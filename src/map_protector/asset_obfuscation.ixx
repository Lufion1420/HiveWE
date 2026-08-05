module;

#include <filesystem>

export module AssetObfuscation;

import std;
import SLK;
import ModificationTables;
import StockObjectData;
import Hierarchy;

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

export struct AssetObfuscationResult {
	bool success = false;
	std::string error;
};

/// Rewrites model/icon path fields in a single object-data modification file (e.g. temp_dir /
/// "war3map.w3u" or its war3mapSkin.* counterpart) in place. template_slk/meta_slk must already
/// be the fully-populated stock tables for this category (stock_object_data::load()'s output) -
/// not a bare single-file load - since a custom object's oldid parent, or a field only present
/// via one of the many stock .merge()'d files, needs the complete stock dataset to resolve
/// correctly (see field_to_meta_id()'s doc comment in slk.ixx for why the alias/oldid fallback
/// needs the merged view). Returns changed=true if anything was rewritten (and, in that case, the
/// file has already been re-saved) so the caller only needs to act on failure.
///
/// Exported (not file-local) specifically so it's independently testable against small, hand-built
/// in-memory template/meta SLKs - the same style tests/modification_tables_test.cpp already uses -
/// without needing a real Warcraft III installation's stock data mounted through Hierarchy, which
/// rewrite_object_data_references()'s production stock_object_data::load() call requires.
export struct FileRewriteResult {
	bool success = false;
	std::string error;
	bool changed = false;
};

export FileRewriteResult rewrite_object_data_file(
	const fs::path& file_path,
	const std::string_view file_name,
	const slk::SLK& template_slk,
	const slk::SLK& meta_slk,
	const bool optional_ints,
	const bool skin,
	const std::unordered_map<std::string, const RenameCandidate*>& candidates_by_match_key
) {
	if (!fs::exists(file_path)) {
		return { true, "", false };
	}

	auto shadow_map_result = extract_modification_shadow_map_path(file_path, template_slk, meta_slk, optional_ints);
	if (!shadow_map_result) {
		return { false, shadow_map_result.error(), false };
	}
	ModificationShadowMap shadow_map = std::move(shadow_map_result.value());

	// Merged stock+shadow view, needed for field_to_meta_id()'s alias/oldid resolution (ability
	// field-name aliasing) - matches how src/models/table_model.ixx's field_type() resolves the
	// same classification for the live Object Editor UI.
	slk::SLK scratch = shadow_map_to_slk(template_slk, shadow_map);

	bool changed = false;
	for (auto& [object_id, fields] : shadow_map) {
		for (auto& [field_name, value] : fields) {
			if (field_name == "oldid") {
				continue;
			}

			const auto meta_id = scratch.field_to_meta_id(meta_slk, field_name, object_id);
			if (!meta_id) {
				continue;
			}
			const std::string_view type = meta_slk.data<std::string_view>("type", *meta_id);
			if (type != "model" && type != "icon") {
				continue;
			}

			const auto found = candidates_by_match_key.find(asset_match_key(value));
			if (found == candidates_by_match_key.end()) {
				continue;
			}
			value = found->second->new_relative_path.string();
			changed = true;
		}
	}

	if (changed) {
		scratch = shadow_map_to_slk(template_slk, shadow_map); // rebuild: shadow_map above was mutated in place
		save_modification_file(file_name, scratch, meta_slk, optional_ints, skin);
	}

	return { true, "", changed };
}

/// Rewrites every model/icon object-data field across all 7 categories (units/items/doodads/
/// destructibles/abilities/upgrades/buffs, plus each category's war3mapSkin.* counterpart) that
/// references a renamed candidate, so a unit's icon, an ability's art, a doodad's model override,
/// etc. all point at the new name. Loads its own independent copy of the stock tables via
/// stock_object_data::load() rather than reusing whatever map is currently open in HiveWE - this is
/// what makes Asset Path Obfuscation work for an external-source map that was never loaded into
/// HiveWE's Map object (see map_protection_plan.md's session notes for why that path matters).
///
/// Redirects hierarchy.map_directory to temp_dir for the duration (same established pattern as
/// protection_pipeline.ixx's sanitize_metadata()/inline_map_info_trigger_strings()), since
/// save_modification_file() writes through hierarchy.map_file_write(). Must be called synchronously
/// before any other step that also redirects hierarchy.map_directory.
export AssetObfuscationResult rewrite_object_data_references(const fs::path& temp_dir, const std::vector<RenameCandidate>& candidates) {
	if (candidates.empty()) {
		return { true, "" };
	}

	std::unordered_map<std::string, const RenameCandidate*> candidates_by_match_key;
	for (const RenameCandidate& candidate : candidates) {
		candidates_by_match_key[candidate.match_key] = &candidate;
	}

	struct Category {
		std::string_view file_name;
		std::string_view skin_file_name;
		const stock_object_data::TableSet* tables;
		bool optional_ints;
	};

	const stock_object_data::Tables stock = stock_object_data::load();
	const std::array<Category, 7> categories = { {
		{ "war3map.w3u", "war3mapSkin.w3u", &stock.units, false },
		{ "war3map.w3t", "war3mapSkin.w3t", &stock.items, false },
		{ "war3map.w3a", "war3mapSkin.w3a", &stock.abilities, true },
		{ "war3map.w3b", "war3mapSkin.w3b", &stock.destructibles, false },
		{ "war3map.w3h", "war3mapSkin.w3h", &stock.buffs, false },
		{ "war3map.w3q", "war3mapSkin.w3q", &stock.upgrades, true },
		{ "war3map.w3d", "war3mapSkin.w3d", &stock.doodads, true },
	} };

	const fs::path original_map_directory = hierarchy.map_directory;
	hierarchy.map_directory = temp_dir;

	AssetObfuscationResult result{ true, "" };
	for (const Category& category : categories) {
		for (const auto& [file_name, skin] : { std::pair{ category.file_name, false }, std::pair{ category.skin_file_name, true } }) {
			const FileRewriteResult step = rewrite_object_data_file(
				temp_dir / file_name, file_name, category.tables->data, category.tables->meta, category.optional_ints, skin, candidates_by_match_key
			);
			if (!step.success) {
				result = { false, std::format("Failed to rewrite asset references in {}: {}", file_name, step.error) };
				break;
			}
		}
		if (!result.success) {
			break;
		}
	}

	hierarchy.map_directory = original_map_directory;
	return result;
}
