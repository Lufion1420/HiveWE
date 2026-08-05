#include <doctest/doctest.h>

import std;
import SLK;
import ModificationTables;
import Hierarchy;
import AssetObfuscation;

namespace fs = std::filesystem;

namespace {
	fs::path make_scratch_dir(const std::string& name) {
		const fs::path dir = fs::temp_directory_path() / "hivewe_asset_obfuscation_test" / name;
		std::error_code ec;
		fs::remove_all(dir, ec);
		fs::create_directories(dir, ec);
		return dir;
	}

	void write_file(const fs::path& path, std::string_view content = "x") {
		std::error_code ec;
		fs::create_directories(path.parent_path(), ec);
		std::ofstream(path, std::ios::binary) << content;
	}

	const RenameCandidate* find_by_original(const std::vector<RenameCandidate>& candidates, std::string_view original_generic_path) {
		for (const auto& candidate : candidates) {
			if (candidate.original_relative_path.generic_string() == original_generic_path) {
				return &candidate;
			}
		}
		return nullptr;
	}
} // namespace

TEST_CASE("asset_match_key normalizes case, separators, and .mdl/.mdx") {
	CHECK(asset_match_key("war3mapImported\\Foo.MDX") == "war3mapimported/foo.mdx");
	CHECK(asset_match_key("war3mapImported/Bar.mdl") == "war3mapimported/bar.mdx");
	CHECK(asset_match_key("Textures/Icon.blp") == "textures/icon.blp");
}

TEST_CASE("enumerate_rename_candidates excludes blacklisted core files") {
	const fs::path dir = make_scratch_dir("blacklist");
	write_file(dir / "war3map.w3i");
	write_file(dir / "war3map.j");
	write_file(dir / "war3mapImported" / "Custom.blp");

	const std::unordered_set<std::string> blacklist = { "war3map.w3i", "war3map.j" };
	const auto candidates = enumerate_rename_candidates(dir, blacklist, {});

	CHECK(candidates.size() == 1);
	CHECK(find_by_original(candidates, "war3mapImported/Custom.blp") != nullptr);
	CHECK(find_by_original(candidates, "war3map.w3i") == nullptr);
	CHECK(find_by_original(candidates, "war3map.j") == nullptr);
}

TEST_CASE("enumerate_rename_candidates excludes stock-override paths") {
	const fs::path dir = make_scratch_dir("stock_override");
	write_file(dir / "TerrainArt" / "Blight" / "Blight0.blp");
	write_file(dir / "ReplaceableTextures" / "TeamColor" / "TeamColor00.blp");
	write_file(dir / "war3mapImported" / "Custom.blp");

	const auto candidates = enumerate_rename_candidates(dir, {}, {});

	CHECK(candidates.size() == 1);
	CHECK(find_by_original(candidates, "war3mapImported/Custom.blp") != nullptr);
	CHECK(find_by_original(candidates, "TerrainArt/Blight/Blight0.blp") == nullptr);
	CHECK(find_by_original(candidates, "ReplaceableTextures/TeamColor/TeamColor00.blp") == nullptr);
}

TEST_CASE("enumerate_rename_candidates excludes files reported as stock assets") {
	const fs::path dir = make_scratch_dir("stock_asset");
	write_file(dir / "Objects" / "Something.blp");
	write_file(dir / "war3mapImported" / "Custom.blp");

	const auto is_stock_asset = [](const fs::path& path) {
		return path.generic_string() == "Objects/Something.blp";
	};
	const auto candidates = enumerate_rename_candidates(dir, {}, is_stock_asset);

	CHECK(candidates.size() == 1);
	CHECK(find_by_original(candidates, "war3mapImported/Custom.blp") != nullptr);
	CHECK(find_by_original(candidates, "Objects/Something.blp") == nullptr);
}

TEST_CASE("enumerate_rename_candidates generates unique names that preserve extension") {
	const fs::path dir = make_scratch_dir("unique_names");
	for (int i = 0; i < 25; ++i) {
		write_file(dir / std::format("Model{}.mdx", i));
	}

	const auto candidates = enumerate_rename_candidates(dir, {}, {});
	CHECK(candidates.size() == 25);

	std::unordered_set<std::string> new_names;
	for (const auto& candidate : candidates) {
		CHECK(candidate.new_relative_path.extension() == ".mdx");
		CHECK(candidate.new_relative_path == candidate.new_relative_path.filename()); // flat, no subfolder
		new_names.insert(candidate.new_relative_path.string());
	}
	CHECK(new_names.size() == 25); // all unique
}

TEST_CASE("enumerate_rename_candidates on a nonexistent directory returns empty") {
	const auto candidates = enumerate_rename_candidates(fs::temp_directory_path() / "hivewe_asset_obfuscation_test" / "does_not_exist", {}, {});
	CHECK(candidates.empty());
}

namespace {
	// A minimal meta table with one "model"-typed field ("file") and one plain "string"-typed field
	// ("name"), matching the shape of a real UnitMetaData.slk closely enough to exercise
	// field_to_meta_id()/type classification without needing the real stock data.
	slk::SLK make_test_meta() {
		slk::SLK meta;
		meta.add_row("Ymdl");
		meta.set_shadow_data("field", "Ymdl", "file");
		meta.set_shadow_data("type", "Ymdl", "model");
		meta.set_shadow_data("data", "Ymdl", "0");
		meta.add_row("Ynam");
		meta.set_shadow_data("field", "Ynam", "name");
		meta.set_shadow_data("type", "Ynam", "string");
		meta.set_shadow_data("data", "Ynam", "0");
		meta.build_meta_map();
		return meta;
	}

	slk::SLK make_test_template(const std::string& row_id) {
		slk::SLK data;
		data.add_row(row_id);
		return data;
	}
}

TEST_CASE("rewrite_object_data_file rewrites a model-typed field but leaves a string field alone") {
	const fs::path dir = make_scratch_dir("rewrite_model_field");
	const fs::path original_map_directory = hierarchy.map_directory;
	hierarchy.map_directory = dir;

	const slk::SLK meta = make_test_meta();
	const slk::SLK template_slk = make_test_template("hfoo");

	slk::SLK modification_data;
	modification_data.add_row("hfoo");
	modification_data.set_shadow_data("file", "hfoo", "war3mapImported\\Custom.mdx");
	modification_data.set_shadow_data("name", "hfoo", "war3mapImported\\Custom.mdx");
	const std::vector<u8> buffer = build_modification_file_buffer(modification_data, meta, false, false);

	const fs::path w3u_path = dir / "war3map.w3u";
	{
		std::ofstream out(w3u_path, std::ios::binary);
		out.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
	}

	RenameCandidate candidate;
	candidate.original_relative_path = "war3mapImported/Custom.mdx";
	candidate.new_relative_path = "a3f9c1e2.mdx";
	candidate.match_key = asset_match_key("war3mapImported/Custom.mdx");
	const std::unordered_map<std::string, const RenameCandidate*> candidates_by_match_key = { { candidate.match_key, &candidate } };

	const FileRewriteResult result = rewrite_object_data_file(w3u_path, "war3map.w3u", template_slk, meta, false, false, candidates_by_match_key);
	CHECK(result.success);
	CHECK(result.changed);

	const auto reloaded = extract_modification_shadow_map_path(w3u_path, template_slk, meta, false);
	REQUIRE(reloaded.has_value());
	CHECK(reloaded->at("hfoo").at("file") == "a3f9c1e2.mdx");
	CHECK(reloaded->at("hfoo").at("name") == "war3mapImported\\Custom.mdx"); // untouched: not a model/icon field

	hierarchy.map_directory = original_map_directory;
}

TEST_CASE("rewrite_object_data_file leaves the file untouched when no field matches a candidate") {
	const fs::path dir = make_scratch_dir("rewrite_no_match");
	const fs::path original_map_directory = hierarchy.map_directory;
	hierarchy.map_directory = dir;

	const slk::SLK meta = make_test_meta();
	const slk::SLK template_slk = make_test_template("hfoo");

	slk::SLK modification_data;
	modification_data.add_row("hfoo");
	modification_data.set_shadow_data("file", "hfoo", "war3mapImported\\Unrelated.mdx");
	const std::vector<u8> buffer = build_modification_file_buffer(modification_data, meta, false, false);

	const fs::path w3u_path = dir / "war3map.w3u";
	{
		std::ofstream out(w3u_path, std::ios::binary);
		out.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
	}

	RenameCandidate candidate;
	candidate.original_relative_path = "war3mapImported/Custom.mdx";
	candidate.new_relative_path = "a3f9c1e2.mdx";
	candidate.match_key = asset_match_key("war3mapImported/Custom.mdx");
	const std::unordered_map<std::string, const RenameCandidate*> candidates_by_match_key = { { candidate.match_key, &candidate } };

	const FileRewriteResult result = rewrite_object_data_file(w3u_path, "war3map.w3u", template_slk, meta, false, false, candidates_by_match_key);
	CHECK(result.success);
	CHECK_FALSE(result.changed);

	hierarchy.map_directory = original_map_directory;
}

TEST_CASE("rewrite_object_data_file is a no-op success when the file does not exist") {
	const slk::SLK meta = make_test_meta();
	const slk::SLK template_slk = make_test_template("hfoo");
	const FileRewriteResult result = rewrite_object_data_file(
		fs::temp_directory_path() / "hivewe_asset_obfuscation_test" / "does_not_exist.w3u", "war3map.w3u", template_slk, meta, false, false, {}
	);
	CHECK(result.success);
	CHECK_FALSE(result.changed);
}
