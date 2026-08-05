module;

#include <filesystem>

export module AssetObfuscation;

import std;

namespace fs = std::filesystem;

/// Normalizes a path string for case/separator-insensitive comparison against other paths (never
/// for display or for the string actually written back into map data - see RenameCandidate below).
/// Mirrors Map::get_file_usage()'s match_key lambda (src/base/map/unused_files.cpp) exactly: WC3
/// paths are case-insensitive, and a model may be referenced as .mdl in one place and imported as
/// .mdx in another (or vice versa), so both the case and that specific extension pair are unified.
export std::string asset_match_key(std::string path) {
	std::ranges::transform(path, path.begin(), [](const unsigned char c) {
		return c == '\\' ? '/' : static_cast<char>(std::tolower(c));
	});
	if (path.ends_with(".mdl")) {
		path = path.substr(0, path.size() - 4) + ".mdx";
	}
	return path;
}

/// Files placed under these stock game-asset roots override game content by path (terrain tiles,
/// cliffs, team colours, ...) - the override only works because the path matches a specific stock
/// name, so renaming one would silently break the override rather than just an ordinary reference.
/// Mirrors Map::get_file_usage()'s is_stock_override_path lambda.
bool is_stock_override_path(const std::string& lowercase_forward_slash_path) {
	static constexpr std::array<std::string_view, 2> roots = { "terrainart/", "replaceabletextures/" };
	return std::ranges::any_of(roots, [&](std::string_view root) { return lowercase_forward_slash_path.starts_with(root); });
}

/// A single loose file under temp_dir that Asset Path Obfuscation will rename. new_relative_path is
/// always a flat, single-segment name directly under the archive root (e.g. "a3f9c1e2.mdx") -
/// nothing about WC3's asset loading depends on folder structure, only on every reference to a file
/// agreeing on its path, so flattening is free obfuscation with no extra correctness burden: later
/// phases rewrite every reference (including paths stored inside another model's own MDX chunks) to
/// this exact same string, not a folder-relative one.
export struct RenameCandidate {
	fs::path original_relative_path;
	fs::path new_relative_path;
	std::string match_key;
};

namespace {
	std::string generate_garbage_name(const fs::path& extension, std::mt19937& rng, const std::unordered_set<std::string>& taken) {
		static constexpr std::string_view hex_digits = "0123456789abcdef";
		std::uniform_int_distribution<int> digit(0, static_cast<int>(hex_digits.size()) - 1);

		std::string lowercase_extension = extension.string();
		std::ranges::transform(lowercase_extension, lowercase_extension.begin(), [](const unsigned char c) { return std::tolower(c); });

		for (;;) {
			std::string name;
			for (int i = 0; i < 8; ++i) {
				name += hex_digits[digit(rng)];
			}
			name += lowercase_extension;
			if (!taken.contains(name)) {
				return name;
			}
		}
	}
} // namespace

/// Enumerates every loose file under temp_dir that Asset Path Obfuscation is allowed to rename.
///
/// Excludes:
/// - Anything in never_rename_file_names (Imports::blacklist in production - core war3map.*/
///   war3campaign.* filenames the engine looks up by hardcoded name, plus war3mapSkin.txt, which
///   is already in that same blacklist and which HiveWE doesn't parse anywhere so is left untouched
///   rather than risk desyncing a format nothing here understands).
/// - Stock-override paths (TerrainArt/, ReplaceableTextures/), which must keep their exact stock
///   name for the override itself to work.
/// - Anything is_stock_asset() reports as also existing as a genuine stock/base-game asset at the
///   same relative path - a broader safety net beyond the two known override roots. Injected rather
///   than hard-coded to the global Hierarchy singleton so this stays independently testable; the
///   pipeline's production call site passes hierarchy.game_file_exists().
///
/// Sound files are excluded by the caller (not filtered here) since exclusion in v1 is driven by
/// which files are referenced by a Sound object, which this file-system-only enumeration can't know
/// - that cross-reference is resolved once the object-data/w3s reference-discovery phases exist.
export std::vector<RenameCandidate> enumerate_rename_candidates(
	const fs::path& temp_dir,
	const std::unordered_set<std::string>& never_rename_file_names,
	const std::function<bool(const fs::path&)>& is_stock_asset
) {
	std::vector<RenameCandidate> candidates;
	std::unordered_set<std::string> taken_names;
	std::mt19937 rng{ std::random_device{}() };

	if (!fs::exists(temp_dir)) {
		return candidates;
	}

	for (const auto& entry : fs::recursive_directory_iterator(temp_dir)) {
		if (!entry.is_regular_file()) {
			continue;
		}

		const fs::path relative_path = entry.path().lexically_relative(temp_dir);
		const std::string file_name = entry.path().filename().string();
		if (never_rename_file_names.contains(file_name)) {
			continue;
		}

		const std::string key = asset_match_key(relative_path.string());
		if (is_stock_override_path(key)) {
			continue;
		}
		if (is_stock_asset && is_stock_asset(relative_path)) {
			continue;
		}

		RenameCandidate candidate;
		candidate.original_relative_path = relative_path;
		candidate.match_key = key;
		candidate.new_relative_path = generate_garbage_name(entry.path().extension(), rng, taken_names);
		taken_names.insert(candidate.new_relative_path.string());
		candidates.push_back(std::move(candidate));
	}

	return candidates;
}
