export module TilesetChange;

import std;
import Hierarchy;

namespace fs = std::filesystem;

/// The logic of Change Tileset without Qt or GL, so tests can drive it: how removed tiles are repainted, how the corner texture
/// indices are remapped, and the policy for custom tile textures. Terrain::change_tileset and the TileSetter dialog call these.

/// The Warcraft III behaviour that is not verified yet (NEEDS_GAME_TEST). Everything unresolved lives here, in tileset_policy().
export struct TilesetPolicy {
	/// How many tiles a map tileset may hold. The w3e corner field has 6 bits (64), but the stock World Editor stops at 16.
	/// Unverified: whether Warcraft III 3.0.1 loads more than 16.
	std::size_t max_tiles = 64;
	/// Whether a custom tile is written as a texture file in the map that replaces the stock tile's texture by path.
	/// Unverified: whether Warcraft III 3.0.1 honours such a map file for a terrain tile.
	bool write_sd_override = true;
	/// Also name the HD form `_hd.w3mod:<dir>/<file>_diffuse.<ext>`. That is an archive name, not a file name Windows can hold, so
	/// the dialog never writes it to the map folder. Best effort only: the user's maps are SD.
	bool name_hd_override = false;
};

export TilesetPolicy tileset_policy(const bool hd) {
	TilesetPolicy policy;
	policy.name_hd_override = hd;
	return policy;
}

/// The texture formats a map file can override a stock tile with, lower case without the dot. The ground texture loader probes `.tga`,
/// then `.blp`, then `.dds`, each through the whole hierarchy (map, then game data). Stock tiles are `.blp`, so a map `.blp` wins at its
/// turn and a map `.tga` wins first, but a map `.dds` never beats the stock `.blp` (the loader stops at the `.blp` before trying `.dds`).
export bool is_tile_texture_extension(const std::string_view extension) {
	return extension == "blp" || extension == "tga";
}

/// The names under which a texture imported for a tile replaces the stock one, from the tile's Terrain.slk `dir` and `file`
/// (`file` without extension) and the imported file's extension (lower case, no dot). Empty when the policy writes nothing.
/// SD: `<dir>/<file>.<ext>`. HD: `_hd.w3mod:<dir>/<file>_diffuse.<ext>`.
export std::vector<std::string> tile_override_names(
	const TilesetPolicy& policy, const std::string_view dir, const std::string_view file, const std::string_view extension
) {
	std::vector<std::string> names;
	if (!policy.write_sd_override || !is_tile_texture_extension(extension)) {
		return names;
	}
	std::string base(dir);
	std::ranges::replace(base, '\\', '/');
	if (!base.empty() && !base.ends_with('/')) {
		base += '/';
	}
	names.push_back(std::format("{}{}.{}", base, file, extension));
	if (policy.name_hd_override) {
		names.push_back(std::format("_hd.w3mod:{}{}_diffuse.{}", base, file, extension));
	}
	return names;
}

/// The HD override name is an archive path (it holds a colon), never a file in the map folder.
export bool is_archive_only_name(const std::string_view name) {
	return name.starts_with("_hd.w3mod:");
}

/// A relative name that stays inside the map folder: not empty, no drive or colon, no leading slash, no root, no `..` component and
/// no character Windows refuses in a file name. The tile dir comes from Terrain.slk, which a map can override, so it is not trusted.
export bool is_writable_map_file_name(const std::string_view name) {
	if (name.empty() || name.front() == '/' || name.front() == '\\') {
		return false;
	}
	for (const char c : name) {
		if (static_cast<unsigned char>(c) < 32 || std::string_view(":*?\"<>|").contains(c)) {
			return false;
		}
	}
	std::size_t start = 0;
	while (start <= name.size()) {
		std::size_t end = name.find_first_of("/\\", start);
		if (end == std::string_view::npos) {
			end = name.size();
		}
		const std::string_view component = name.substr(start, end - start);
		if (component == ".." || (!component.empty() && (component.back() == ' ' || component.back() == '.') && component != ".")) {
			return false;
		}
		start = end + 1;
	}
	const fs::path path(name);
	return !path.is_absolute() && !path.has_root_name() && !path.has_root_directory();
}

/// True when `path` lies under `root` after resolving `..` and links (neither needs to exist).
export bool is_inside_directory(const fs::path& root, const fs::path& path) {
	std::error_code ec;
	const fs::path base = fs::weakly_canonical(root, ec);
	if (ec) {
		return false;
	}
	const fs::path target = fs::weakly_canonical(path, ec);
	if (ec) {
		return false;
	}
	const fs::path relative = target.lexically_relative(base);
	return !relative.empty() && *relative.begin() != ".." && !relative.is_absolute();
}

/// One imported tile texture, ready to be written into the map folder.
export struct TileOverride {
	std::string tile_id;
	/// The tile's Terrain.slk `dir` and `file` (no extension).
	std::string dir;
	std::string file;
	/// Lower case, no dot.
	std::string extension;
	fs::path source;
};

/// Why write_tile_overrides failed. `leftovers` are previous overrides that the rollback could not put back: each is still on disk under
/// its `*.old.hivewe-tmp` name and must be renamed back by hand. Empty means everything is as it was before the call.
export struct TileOverrideError {
	std::string message;
	std::vector<fs::path> leftovers;
};

/// Copies the textures into `map_directory` under the stock tile's path, all or nothing: every file is first copied to a temporary
/// file next to its target, and only when all copies worked are they renamed into place (an existing file is set aside and restored
/// if a later rename fails). On any failure nothing is left changed and the error says why. Returns the files written.
export std::expected<std::vector<fs::path>, TileOverrideError> write_tile_overrides(
	const TilesetPolicy& policy, const fs::path& map_directory, const std::span<const TileOverride> overrides
) {
	// Check every destination before touching the disk. Two tiles with the same dir and file (Windows ignores case) would overwrite
	// each other's temporary file and lose the user's original override.
	{
		std::map<std::string, std::string> seen; // lower case canonical destination -> tile id
		for (const TileOverride& item : overrides) {
			for (const std::string& name : tile_override_names(policy, item.dir, item.file, item.extension)) {
				if (is_archive_only_name(name)) {
					continue;
				}
				const fs::path destination = map_directory / name;
				if (!is_writable_map_file_name(name) || !is_inside_directory(map_directory, destination)) {
					return std::unexpected(TileOverrideError { std::format("The texture path \"{}\" of tile {} is not inside the map folder.", name, item.tile_id), {} });
				}
				std::error_code canonical_error;
				std::string key = fs::weakly_canonical(destination, canonical_error).generic_string();
				std::ranges::transform(key, key.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
				if (const auto [position, inserted] = seen.emplace(key, item.tile_id); !inserted) {
					return std::unexpected(TileOverrideError {
						std::format("Tiles {} and {} would both replace the file \"{}\". Only one tile can use a texture path.", position->second, item.tile_id, name), {} });
				}
			}
		}
	}

	struct Step {
		fs::path destination;
		fs::path temporary;
		fs::path set_aside;
		bool had_original = false;
		bool installed = false;
	};
	std::vector<Step> steps;
	std::vector<fs::path> created_directories;
	std::error_code ec;

	const auto fail = [&](std::string message) -> std::unexpected<TileOverrideError> {
		TileOverrideError error { std::move(message), {} };
		std::error_code ignored;
		for (const Step& step : steps) {
			fs::remove(step.temporary, ignored);
			if (step.installed) {
				fs::remove(step.destination, ignored);
			}
			if (step.had_original && fs::exists(step.set_aside, ignored)) {
				std::error_code restore_error;
				fs::rename(step.set_aside, step.destination, restore_error);
				if (restore_error) {
					error.leftovers.push_back(step.set_aside);
				}
			}
		}
		for (const fs::path& directory : std::views::reverse(created_directories)) {
			fs::remove(directory, ignored); // only removes an empty directory
		}
		return std::unexpected(std::move(error));
	};

	for (const TileOverride& item : overrides) {
		for (const std::string& name : tile_override_names(policy, item.dir, item.file, item.extension)) {
			if (is_archive_only_name(name)) {
				continue;
			}
			Step step;
			step.destination = map_directory / name;
			step.temporary = step.destination;
			step.temporary += map_write_temp_suffix;
			step.set_aside = step.destination;
			step.set_aside += ".old";
			step.set_aside += map_write_temp_suffix;

			// Directories that do not exist yet are removed again on failure.
			std::vector<fs::path> missing;
			for (fs::path directory = step.destination.parent_path(); !directory.empty() && !fs::exists(directory, ec); directory = directory.parent_path()) {
				missing.push_back(directory);
			}
			fs::create_directories(step.destination.parent_path(), ec);
			if (ec) {
				return fail(std::format("Could not create the folder for {}: {}", name, ec.message()));
			}
			created_directories.insert(created_directories.end(), missing.rbegin(), missing.rend());

			steps.push_back(step);
			fs::copy_file(item.source, step.temporary, fs::copy_options::overwrite_existing, ec);
			if (ec) {
				return fail(std::format("Could not copy {} to {}: {}", item.source.filename().string(), name, ec.message()));
			}
		}
	}

	for (Step& step : steps) {
		step.had_original = fs::exists(step.destination, ec);
		if (step.had_original) {
			fs::rename(step.destination, step.set_aside, ec);
			if (ec) {
				step.had_original = false;
				return fail(std::format("Could not replace {}: {}", step.destination.filename().string(), ec.message()));
			}
		}
		fs::rename(step.temporary, step.destination, ec);
		if (ec) {
			return fail(std::format("Could not write {}: {}", step.destination.filename().string(), ec.message()));
		}
		step.installed = true;
	}

	std::vector<fs::path> written;
	for (const Step& step : steps) {
		if (step.had_original) {
			fs::remove(step.set_aside, ec);
		}
		written.push_back(step.destination);
	}
	return written;
}

/// For every tile of the old tileset (same order), its slot in the new tileset: the same tile where it was kept, else the slot of
/// `replacements[tile]`, else slot 0. Empty when the new tileset is empty.
export std::vector<int> build_tile_remap(
	const std::span<const std::string> original,
	const std::span<const std::string> new_ids,
	const std::map<std::string, std::string>& replacements
) {
	std::vector<int> old_to_new;
	if (new_ids.empty()) {
		return old_to_new;
	}
	old_to_new.reserve(original.size());
	for (const std::string& id : original) {
		auto found = std::ranges::find(new_ids, id);
		if (found == new_ids.end()) {
			if (const auto replacement = replacements.find(id); replacement != replacements.end()) {
				found = std::ranges::find(new_ids, replacement->second);
			}
			if (found == new_ids.end()) {
				found = new_ids.begin();
			}
		}
		old_to_new.push_back(static_cast<int>(found - new_ids.begin()));
	}
	return old_to_new;
}

/// Rewrites the ground texture index of every corner. `old_to_new` has one entry per old tile; the index one past the old tiles is
/// the blight texture, which moves to one past the new tiles (`new_count`). Any other index is out of range and becomes slot 0.
export void remap_ground_textures(const std::span<std::uint8_t> corner_ground_texture, const std::span<const int> old_to_new, const std::size_t new_count) {
	for (std::uint8_t& texture : corner_ground_texture) {
		if (texture < old_to_new.size()) {
			texture = static_cast<std::uint8_t>(old_to_new[texture]);
		} else if (texture == old_to_new.size()) {
			texture = static_cast<std::uint8_t>(new_count);
		} else {
			texture = 0;
		}
	}
}
