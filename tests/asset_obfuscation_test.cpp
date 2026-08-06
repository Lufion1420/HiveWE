#include <doctest/doctest.h>

import std;
import SLK;
import ModificationTables;
import Hierarchy;
import MDX;
import Utilities;
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

TEST_CASE("enumerate_rename_candidates excludes desktop.ini regardless of case") {
	const fs::path dir = make_scratch_dir("desktop_ini");
	write_file(dir / "desktop.ini");
	write_file(dir / "UI" / "Desktop.INI");
	write_file(dir / "war3mapImported" / "Custom.blp");

	const auto candidates = enumerate_rename_candidates(dir, {}, {});

	CHECK(candidates.size() == 1);
	CHECK(find_by_original(candidates, "war3mapImported/Custom.blp") != nullptr);
	CHECK(find_by_original(candidates, "desktop.ini") == nullptr);
	CHECK(find_by_original(candidates, "UI/Desktop.INI") == nullptr);
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

TEST_CASE("rewrite_mdx_references rewrites a texture path stored inside the model itself") {
	const fs::path dir = make_scratch_dir("rewrite_mdx");

	// minimal_v1000.mdl (shared with mdl_reader_test.cpp) ships one texture: "Textures/Stone.blp".
	const std::string source = read_text_file(fs::path(MDL_FIXTURES_DIR) / "minimal_v1000.mdl");
	auto parsed = mdx::MDX::from_mdl(source);
	REQUIRE(parsed.has_value());
	REQUIRE(parsed.value().textures.size() == 1);
	REQUIRE(parsed.value().textures.front().file_name == "Textures/Stone.blp");
	const auto original_version = parsed.value().version;

	const auto writer = parsed.value().to_mdx(original_version);
	const fs::path mdx_path = dir / "Model.mdx";
	{
		std::ofstream out(mdx_path, std::ios::binary);
		out.write(reinterpret_cast<const char*>(writer.buffer.data()), static_cast<std::streamsize>(writer.buffer.size()));
	}

	RenameCandidate candidate;
	candidate.original_relative_path = "Textures/Stone.blp";
	candidate.new_relative_path = "a3f9c1e2.blp";
	candidate.match_key = asset_match_key("Textures/Stone.blp");

	const AssetObfuscationResult result = rewrite_mdx_references(dir, { candidate });
	CHECK(result.success);

	auto reread_file = read_file(mdx_path);
	REQUIRE(reread_file.has_value());
	const mdx::MDX reread(reread_file.value());
	REQUIRE(reread.textures.size() == 1);
	CHECK(reread.textures.front().file_name == "a3f9c1e2.blp");
	CHECK(reread.version == original_version); // must not silently bump version as a side effect
}

TEST_CASE("rewrite_mdx_references leaves a model untouched when no texture matches a candidate") {
	const fs::path dir = make_scratch_dir("rewrite_mdx_no_match");

	const std::string source = read_text_file(fs::path(MDL_FIXTURES_DIR) / "minimal_v1000.mdl");
	auto parsed = mdx::MDX::from_mdl(source);
	REQUIRE(parsed.has_value());

	const auto writer = parsed.value().to_mdx(parsed.value().version);
	const fs::path mdx_path = dir / "Model.mdx";
	std::vector<char> original_bytes(writer.buffer.begin(), writer.buffer.end());
	{
		std::ofstream out(mdx_path, std::ios::binary);
		out.write(original_bytes.data(), static_cast<std::streamsize>(original_bytes.size()));
	}

	RenameCandidate candidate;
	candidate.original_relative_path = "Textures/Unrelated.blp";
	candidate.new_relative_path = "a3f9c1e2.blp";
	candidate.match_key = asset_match_key("Textures/Unrelated.blp");

	const AssetObfuscationResult result = rewrite_mdx_references(dir, { candidate });
	CHECK(result.success);

	std::ifstream in(mdx_path, std::ios::binary);
	const std::vector<char> after_bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	CHECK(original_bytes == after_bytes); // untouched, byte for byte
}

TEST_CASE("verify_no_dangling_text_references catches a reference in a text format nothing else rewrites") {
	const fs::path dir = make_scratch_dir("verify_dangling_text");
	// war3mapSkin.txt is never parsed/rewritten by any phase - exactly the kind of leftover this
	// safety net exists to catch.
	write_file(dir / "war3mapSkin.txt", "[Ymdl]\nfile=war3mapImported\\Custom.mdx\n");

	RenameCandidate candidate;
	candidate.original_relative_path = "war3mapImported/Custom.mdx";
	candidate.new_relative_path = "a3f9c1e2.mdx";
	candidate.match_key = asset_match_key("war3mapImported/Custom.mdx");

	const AssetObfuscationResult result = verify_no_dangling_text_references(dir, { candidate });
	CHECK_FALSE(result.success);
	CHECK(result.error.find("war3mapSkin.txt") != std::string::npos);
}

TEST_CASE("verify_no_dangling_text_references ignores desktop.ini even if it references a candidate") {
	const fs::path dir = make_scratch_dir("verify_desktop_ini");
	// A real case: Windows Explorer folder-icon metadata that happened to reference a renamed
	// texture's old path. The engine never reads desktop.ini regardless of content, so this must
	// not abort the export the way a real reference in an unhandled format (e.g. war3mapSkin.txt)
	// correctly does in the test above.
	write_file(dir / "desktop.ini", "[.ShellClassInfo]\nIconResource=war3mapImported\\Custom.mdx,0\n");

	RenameCandidate candidate;
	candidate.original_relative_path = "war3mapImported/Custom.mdx";
	candidate.new_relative_path = "a3f9c1e2.mdx";
	candidate.match_key = asset_match_key("war3mapImported/Custom.mdx");

	const AssetObfuscationResult result = verify_no_dangling_text_references(dir, { candidate });
	CHECK(result.success);
}

TEST_CASE("verify_no_dangling_text_references passes when no text file references a candidate") {
	const fs::path dir = make_scratch_dir("verify_no_dangling_text");
	write_file(dir / "war3mapSkin.txt", "[Ymdl]\nfile=Objects\\Unrelated.mdx\n");

	RenameCandidate candidate;
	candidate.original_relative_path = "war3mapImported/Custom.mdx";
	candidate.new_relative_path = "a3f9c1e2.mdx";
	candidate.match_key = asset_match_key("war3mapImported/Custom.mdx");

	const AssetObfuscationResult result = verify_no_dangling_text_references(dir, { candidate });
	CHECK(result.success);
}

TEST_CASE("verify_no_dangling_text_references does not false-positive on a candidate name embedded in a longer filename") {
	// Real case: war3mapSkin.txt legitimately referenced "blank-background.blp" (a real, unrelated
	// UI asset), which a plain substring search flagged as referencing a candidate literally named
	// "ground.blp" - background.blp contains "...ground.blp" as a byte sequence purely by
	// coincidence (back-GROUND.blp), not because anything actually points at the renamed file.
	const fs::path dir = make_scratch_dir("verify_no_false_positive_substring");
	write_file(dir / "war3mapSkin.txt", "[CustomSkin]\nBuildTimeIndicator=UI\\Widgets\\EscMenu\\Human\\blank-background.blp\n");

	RenameCandidate candidate;
	candidate.original_relative_path = "ground.blp";
	candidate.new_relative_path = "a3f9c1e2.blp";
	candidate.match_key = asset_match_key("ground.blp");

	const AssetObfuscationResult result = verify_no_dangling_text_references(dir, { candidate });
	CHECK(result.success);
}

TEST_CASE("verify_no_dangling_text_references still catches a genuine standalone reference") {
	const fs::path dir = make_scratch_dir("verify_genuine_reference_still_caught");
	write_file(dir / "war3mapSkin.txt", "[CustomSkin]\nGoldIcon=ground.blp\n");

	RenameCandidate candidate;
	candidate.original_relative_path = "ground.blp";
	candidate.new_relative_path = "a3f9c1e2.blp";
	candidate.match_key = asset_match_key("ground.blp");

	const AssetObfuscationResult result = verify_no_dangling_text_references(dir, { candidate });
	CHECK_FALSE(result.success);
	CHECK(result.error.find("ground.blp") != std::string::npos);
}

TEST_CASE("verify_no_dangling_text_references ignores binary files entirely, even if bytes coincidentally match") {
	const fs::path dir = make_scratch_dir("verify_dangling_binary");
	// Deliberately a non-text-extension file containing the candidate's path text - must be ignored,
	// since binary assets don't reference other files by path and scanning every byte of every large
	// import would be wasted work (see the function's own doc comment).
	write_file(dir / "Some.mdx", "war3mapImported\\Custom.mdx");

	RenameCandidate candidate;
	candidate.original_relative_path = "war3mapImported/Custom.mdx";
	candidate.new_relative_path = "a3f9c1e2.mdx";
	candidate.match_key = asset_match_key("war3mapImported/Custom.mdx");

	const AssetObfuscationResult result = verify_no_dangling_text_references(dir, { candidate });
	CHECK(result.success);
}

TEST_CASE("apply_renames physically renames every candidate on disk") {
	const fs::path dir = make_scratch_dir("apply_renames");
	write_file(dir / "war3mapImported" / "Custom.mdx", "model bytes");
	write_file(dir / "Textures" / "Icon.blp", "blp bytes");

	std::vector<RenameCandidate> candidates(2);
	candidates[0].original_relative_path = "war3mapImported/Custom.mdx";
	candidates[0].new_relative_path = "a3f9c1e2.mdx";
	candidates[1].original_relative_path = "Textures/Icon.blp";
	candidates[1].new_relative_path = "b7e1d4f0.blp";

	const AssetObfuscationResult result = apply_renames(dir, candidates);
	CHECK(result.success);

	CHECK_FALSE(fs::exists(dir / "war3mapImported" / "Custom.mdx"));
	CHECK_FALSE(fs::exists(dir / "Textures" / "Icon.blp"));
	REQUIRE(fs::exists(dir / "a3f9c1e2.mdx"));
	REQUIRE(fs::exists(dir / "b7e1d4f0.blp"));

	std::ifstream model_in(dir / "a3f9c1e2.mdx", std::ios::binary);
	CHECK(std::string((std::istreambuf_iterator<char>(model_in)), std::istreambuf_iterator<char>()) == "model bytes");
}

TEST_CASE("apply_renames fails without renaming anything already done if a candidate's source is missing") {
	const fs::path dir = make_scratch_dir("apply_renames_missing_source");
	write_file(dir / "war3mapImported" / "Custom.mdx", "model bytes");

	std::vector<RenameCandidate> candidates(2);
	candidates[0].original_relative_path = "war3mapImported/Custom.mdx";
	candidates[0].new_relative_path = "a3f9c1e2.mdx";
	candidates[1].original_relative_path = "DoesNotExist.blp"; // never actually created
	candidates[1].new_relative_path = "b7e1d4f0.blp";

	const AssetObfuscationResult result = apply_renames(dir, candidates);
	CHECK_FALSE(result.success);
	// The first candidate, processed before the failing one, is left renamed - apply_renames() is the
	// last step and only ever runs after every reference has already been rewritten to the new name,
	// so a partial rename here still leaves every *reference* consistent with what's on disk for that
	// one file; run_asset_obfuscation() as a whole still reports failure to the caller either way.
	CHECK(fs::exists(dir / "a3f9c1e2.mdx"));
}
