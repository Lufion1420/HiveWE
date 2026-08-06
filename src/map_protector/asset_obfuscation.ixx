module;

#include <filesystem>

export module AssetObfuscation;

import std;
import SLK;
import ModificationTables;
import StockObjectData;
import Hierarchy;
import MapInfo;
import MDX;
import BinaryWriter;
import Utilities;

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

/// Rewrites war3map.w3i's own literal-path fields (loading_screen_model, prologue_screen_model) if
/// either matches a renamed candidate. Reuses the exact MapInfo load/tileset-byte/save pattern
/// already implemented in protection_pipeline.ixx's sanitize_metadata()/inline_map_info_trigger_strings()
/// - MapInfo::save() needs a tileset byte MapInfo::load() itself discards, so it's read directly
/// from war3map.w3e's header instead (same layout Terrain::load() parses).
export AssetObfuscationResult rewrite_map_info_references(const fs::path& temp_dir, const std::vector<RenameCandidate>& candidates) {
	if (candidates.empty() || !fs::exists(temp_dir / "war3map.w3i")) {
		return { true, "" };
	}

	std::unordered_map<std::string, const RenameCandidate*> candidates_by_match_key;
	for (const RenameCandidate& candidate : candidates) {
		candidates_by_match_key[candidate.match_key] = &candidate;
	}

	const fs::path original_map_directory = hierarchy.map_directory;
	hierarchy.map_directory = temp_dir;

	char tileset = 'L';
	if (auto w3e = hierarchy.map_file_read("war3map.w3e"); w3e && w3e->read_string(4) == "W3E!") {
		w3e->advance(4); // format version
		tileset = w3e->read<char>();
	}

	AssetObfuscationResult result{ true, "" };
	try {
		MapInfo info;
		info.load();

		bool changed = false;
		const auto rewrite_field = [&](std::string& field) {
			const auto found = candidates_by_match_key.find(asset_match_key(field));
			if (found != candidates_by_match_key.end()) {
				field = found->second->new_relative_path.string();
				changed = true;
			}
		};
		rewrite_field(info.loading_screen_model);
		rewrite_field(info.prologue_screen_model);

		if (changed) {
			info.save(tileset);
		}
	} catch (const std::exception& e) {
		result = { false, std::string("Failed to rewrite asset references in war3map.w3i: ") + e.what() };
	}

	hierarchy.map_directory = original_map_directory;
	return result;
}

/// Rewrites paths stored *inside* MDX model files themselves - a model's own texture list
/// (Texture::file_name), attachment paths (Attachment::path), and Emitter1 particle paths
/// (ParticleEmitter1::path) are literal path strings independent of whatever file the model itself
/// was renamed to. Reads/writes temp_dir files directly via plain file I/O rather than through
/// Hierarchy, since every .mdx worth checking here is already a loose file physically present in
/// temp_dir (guaranteed by enumerate_rename_candidates()'s walk) - no override/CASC lookup needed.
///
/// No cross-file ordering dependency: every MDX is rewritten independently against the same fixed
/// candidate table built once in enumerate_rename_candidates(), so it doesn't matter whether model A
/// (which might reference model B as an attachment) is visited before or after model B - neither
/// rewrite depends on the other's *new* name being already known, only on the fixed original->new
/// mapping. Explicitly re-serializes at the model's own original version (not to_mdx()'s
/// LATEST_MDX_VERSION default) so an untouched model's version isn't silently bumped as a side
/// effect of an unrelated rename. Deliberately does not call MDX::validate() - that's meant for
/// editor-authored models being exported and can restructure things (e.g. add a bone if none exist);
/// this step should only ever change the handful of path strings it found a candidate for.
export AssetObfuscationResult rewrite_mdx_references(const fs::path& temp_dir, const std::vector<RenameCandidate>& candidates) {
	if (candidates.empty()) {
		return { true, "" };
	}

	std::unordered_map<std::string, const RenameCandidate*> candidates_by_match_key;
	for (const RenameCandidate& candidate : candidates) {
		candidates_by_match_key[candidate.match_key] = &candidate;
	}

	const auto rewrite_if_match = [&](std::string& value) {
		const auto found = candidates_by_match_key.find(asset_match_key(value));
		if (found != candidates_by_match_key.end()) {
			value = found->second->new_relative_path.string();
			return true;
		}
		return false;
	};

	if (!fs::exists(temp_dir)) {
		return { true, "" };
	}

	for (const auto& entry : fs::recursive_directory_iterator(temp_dir)) {
		if (!entry.is_regular_file() || !asset_match_key(entry.path().filename().string()).ends_with(".mdx")) {
			continue;
		}

		try {
			auto file_result = read_file(entry.path());
			if (!file_result) {
				return { false, std::format("Failed to read '{}': {}", entry.path().filename().string(), file_result.error()) };
			}
			mdx::MDX model(file_result.value());

			bool changed = false;
			for (mdx::Texture& texture : model.textures) {
				std::string path_string = texture.file_name.string();
				if (rewrite_if_match(path_string)) {
					texture.file_name = path_string;
					changed = true;
				}
			}
			for (auto& attachment : model.attachments) {
				changed |= rewrite_if_match(attachment.path);
			}
			for (auto& emitter : model.emitters1) {
				changed |= rewrite_if_match(emitter.path);
			}

			if (changed) {
				const BinaryWriter writer = model.to_mdx(model.version);
				std::ofstream out(entry.path(), std::ios::binary | std::ios::trunc);
				out.write(reinterpret_cast<const char*>(writer.buffer.data()), static_cast<std::streamsize>(writer.buffer.size()));
			}
		} catch (const std::exception& e) {
			return { false, std::format("Failed to rewrite asset references in '{}': {}", entry.path().filename().string(), e.what()) };
		}
	}

	return { true, "" };
}

/// Safety-net scan run after all the structured rewrite phases (object data, war3map.w3i, MDX-
/// internal paths, script literals) have already run: catches a reference living somewhere none of
/// those phases understands - most concretely war3mapSkin.txt (can legitimately hold path overrides
/// per WC3's format, but nothing in this codebase parses it, so it's excluded from the rename
/// candidate pool entirely and never rewritten) or any other stray text file a map happens to ship.
///
/// Deliberately scoped to text-like files only (.txt/.ini/.j/.lua), not a blind byte-scan of every
/// binary asset in the map: a BLP/MDX/WAV file's *own* bytes were already exhaustively covered by
/// the structured phases above (MDX contents specifically, by rewrite_mdx_references()) or simply
/// can't reference another file's path in the first place, so scanning every byte of every binary
/// asset would cost real time on a large map (many-MB imports are exactly this user's profile) for
/// no realistic gain. If this scan ever needs widening, do it deliberately, not as a silent blanket
/// byte-scan.
export AssetObfuscationResult verify_no_dangling_text_references(const fs::path& temp_dir, const std::vector<RenameCandidate>& candidates) {
	if (candidates.empty() || !fs::exists(temp_dir)) {
		return { true, "" };
	}

	static constexpr std::array<std::string_view, 4> text_extensions = { ".txt", ".ini", ".j", ".lua" };

	for (const auto& entry : fs::recursive_directory_iterator(temp_dir)) {
		if (!entry.is_regular_file()) {
			continue;
		}
		std::string extension = entry.path().extension().string();
		std::ranges::transform(extension, extension.begin(), [](const unsigned char c) { return std::tolower(c); });
		if (!std::ranges::contains(text_extensions, extension)) {
			continue;
		}

		std::string content;
		{
			std::ifstream in(entry.path(), std::ios::binary);
			content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		}
		std::ranges::transform(content, content.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });

		for (const RenameCandidate& candidate : candidates) {
			std::string backward_key = candidate.match_key;
			std::ranges::replace(backward_key, '/', '\\');
			if (content.find(candidate.match_key) != std::string::npos || content.find(backward_key) != std::string::npos) {
				return {
					false,
					std::format(
						"'{}' still references '{}', which is not a reference kind this pipeline rewrites - aborting rather "
						"than renaming a file something else still points at by its old name.",
						entry.path().filename().string(), candidate.original_relative_path.string()
					)
				};
			}
		}
	}

	return { true, "" };
}

/// Physically renames every candidate on disk. Must run last, strictly after every rewrite phase and
/// the verification scan above have already succeeded - every reference elsewhere in the map already
/// points at new_relative_path by this point, so renaming any earlier would make those references
/// (briefly, but for real if any step in between failed) point at nothing.
export AssetObfuscationResult apply_renames(const fs::path& temp_dir, const std::vector<RenameCandidate>& candidates) {
	for (const RenameCandidate& candidate : candidates) {
		std::error_code ec;
		fs::rename(temp_dir / candidate.original_relative_path, temp_dir / candidate.new_relative_path, ec);
		if (ec) {
			return {
				false,
				std::format(
					"Failed to rename '{}' to '{}': {}", candidate.original_relative_path.string(), candidate.new_relative_path.string(), ec.message()
				)
			};
		}
	}
	return { true, "" };
}
