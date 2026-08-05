#include <doctest/doctest.h>

import std;
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
