export module MapBodyCheck;

import std;
import BinaryReader;
import Doodad;
import Doodads;
import GameCameras;
import Hierarchy;
import INI;
import MapFormatCheck;
import MapInfo;
import ModificationTables;
import Regions;
import Sounds;
import Triggers;
import Units;
import Utilities;
import <glm/glm.hpp>;

namespace fs = std::filesystem;

export enum class BodyScope {
	full,
	/// For a folder that lacks what a map imports: its own UI/TriggerData.txt, which war3map.wtg is parsed against,
	/// and its own ramp models, which decide which texture table a war3map.w3e corner uses. Both wait for full.
	without_triggers,
};

namespace {
	/// The readers below bump these statics of the open map. Restored however the check ends. Nothing here
	/// changes hierarchy.map_directory: see Hierarchy::map_read_override for why.
	class OpenMapStateGuard {
		int doodad_auto_increment = Doodad::auto_increment;
		int unit_auto_increment = Unit::auto_increment;
		int trigger_next_id = Trigger::next_id;

	  public:
		OpenMapStateGuard() = default;
		OpenMapStateGuard(const OpenMapStateGuard&) = delete;
		OpenMapStateGuard& operator=(const OpenMapStateGuard&) = delete;

		~OpenMapStateGuard() {
			Doodad::auto_increment = doodad_auto_increment;
			Unit::auto_increment = unit_auto_increment;
			Trigger::next_id = trigger_next_id;
			Hierarchy::map_read_override = nullptr;
		}
	};

	struct ObjectFile {
		std::string_view file;
		std::string_view skin_file;
		bool optional_ints;
	};

	constexpr std::array<ObjectFile, 2> objects_before_doodads = { {
		{ "war3map.w3d", "war3mapSkin.w3d", true },
		{ "war3map.w3b", "war3mapSkin.w3b", false },
	} };

	constexpr std::array<ObjectFile, 2> objects_before_units = { {
		{ "war3map.w3u", "war3mapSkin.w3u", false },
		{ "war3map.w3t", "war3mapSkin.w3t", false },
	} };

	constexpr std::array<ObjectFile, 3> objects_after_units = { {
		{ "war3map.w3a", "war3mapSkin.w3a", true },
		{ "war3map.w3h", "war3mapSkin.w3h", false },
		{ "war3map.w3q", "war3mapSkin.w3q", true },
	} };

	constexpr std::string_view misc_file = "war3mapMisc.txt";

	auto is_present(const fs::path& path) -> bool {
		// An error counts as present, so that reading the file reports it.
		std::error_code error;
		const bool exists = fs::exists(path, error);
		return exists || error;
	}

	auto read_map_file(const fs::path& path) -> BinaryReader {
		auto file = read_file(path);
		if (!file) {
			throw std::runtime_error(file.error());
		}
		return std::move(*file);
	}

	struct W3eCorners {
		std::uint32_t width = 0;
		std::uint32_t height = 0;
		std::vector<std::uint8_t> ground;
		std::vector<std::uint8_t> cliff;
		std::vector<std::uint8_t> layer;
		std::vector<std::uint8_t> ramp;
		std::vector<std::uint8_t> blight;

		auto index(const std::uint32_t x, const std::uint32_t y) const -> std::size_t {
			return std::size_t { y } * width + x;
		}
	};

	/// The cliff table index Terrain::real_tile_texture reads for a stored index.
	auto cliff_table_index(const std::uint8_t cliff) -> std::uint8_t {
		return cliff == 15 ? 1 : cliff;
	}

	/// Which corners Terrain::update_cliff_meshes marks as under a ramp model, for the whole map as Terrain::create
	/// asks. A ramp only counts when its model exists, which is why this needs the hierarchy.
	auto corners_under_ramps(const W3eCorners& c) -> std::vector<std::uint8_t> {
		std::unordered_map<std::string, bool> model_exists;
		const auto ramp_model_exists = [&](const std::array<std::size_t, 4>& corners, const int base) {
			std::string name;
			for (const std::size_t i : corners) {
				name += static_cast<char>((c.ramp[i] ? 'L' : 'A') + (c.layer[i] - base) * (c.ramp[i] ? -4 : 1));
			}
			const auto [known, inserted] = model_exists.try_emplace(name, false);
			if (inserted) {
				known->second = hierarchy.file_exists("doodads/terrain/clifftrans/clifftrans" + name + "0.mdx");
			}
			return known->second;
		};

		std::vector<std::uint8_t> under_ramp(c.layer.size(), 0);
		for (std::uint32_t i = 0; i + 1 < c.width; i++) {
			for (std::uint32_t j = 0; j + 1 < c.height; j++) {
				const std::size_t bl = c.index(i, j);
				const std::size_t br = c.index(i + 1, j);
				const std::size_t tl = c.index(i, j + 1);
				const std::size_t tr = c.index(i + 1, j + 1);

				if (j + 2 < c.height) {
					const std::size_t ttl = c.index(i, j + 2);
					const std::size_t ttr = c.index(i + 1, j + 2);
					const int ae = std::min(c.layer[bl], c.layer[ttl]);
					const int cf = std::min(c.layer[br], c.layer[ttr]);
					if (c.layer[tl] == ae && c.layer[tr] == cf && c.ramp[bl] == c.ramp[tl] && c.ramp[bl] == c.ramp[ttl]
						&& c.ramp[br] == c.ramp[tr] && c.ramp[br] == c.ramp[ttr] && c.ramp[bl] != c.ramp[br]
						&& ramp_model_exists({ ttl, ttr, br, bl }, std::min(ae, cf))) {
						under_ramp[bl] = 1;
						under_ramp[tl] = 1;
						continue;
					}
				}

				if (i + 2 < c.width) {
					const std::size_t brr = c.index(i + 2, j);
					const std::size_t trr = c.index(i + 2, j + 1);
					const int ae = std::min(c.layer[bl], c.layer[brr]);
					const int bf = std::min(c.layer[tl], c.layer[trr]);
					if (c.layer[br] == ae && c.layer[tr] == bf && c.ramp[bl] == c.ramp[br] && c.ramp[bl] == c.ramp[brr]
						&& c.ramp[tl] == c.ramp[tr] && c.ramp[tl] == c.ramp[trr] && c.ramp[bl] != c.ramp[tl]
						&& ramp_model_exists({ tl, trr, brr, bl }, std::min(ae, bf))) {
						under_ramp[bl] = 1;
						under_ramp[br] = 1;
					}
				}
			}
		}
		return under_ramp;
	}

	/// Follows Terrain::real_tile_texture, which Terrain::create calls for every corner: nearby cliff or ramp model,
	/// then blight, then the corner's own ground index. An index on a path the corner does not take is never read.
	/// ground_textures holds one more than the file's table, for blight; cliff_to_ground_texture holds just the table.
	void check_w3e_texture_indices(const W3eCorners& c, const std::uint32_t ground_table, const std::uint32_t cliff_table) {
		const auto ground_in_range = [&](const std::uint8_t ground) {
			return ground <= ground_table;
		};
		const auto cliff_in_range = [&](const std::uint8_t cliff) {
			return cliff_table_index(cliff) < cliff_table;
		};

		// The usual case, which spares looking up ramp models.
		if (std::ranges::all_of(c.ground, ground_in_range) && std::ranges::all_of(c.cliff, cliff_in_range)) {
			return;
		}

		std::vector<std::uint8_t> is_cliff(c.layer.size(), 0);
		for (std::uint32_t i = 0; i + 1 < c.width; i++) {
			for (std::uint32_t j = 0; j + 1 < c.height; j++) {
				const std::uint8_t layer = c.layer[c.index(i, j)];
				is_cliff[c.index(i, j)] =
					layer != c.layer[c.index(i + 1, j)] || layer != c.layer[c.index(i, j + 1)] || layer != c.layer[c.index(i + 1, j + 1)];
			}
		}
		const std::vector<std::uint8_t> under_ramp = corners_under_ramps(c);

		const auto nearby = [&](const std::vector<std::uint8_t>& flags, const std::uint32_t x, const std::uint32_t y) {
			return flags[c.index(x, y)] || (x > 0 && flags[c.index(x - 1, y)]) || (y > 0 && flags[c.index(x, y - 1)])
				|| (x > 0 && y > 0 && flags[c.index(x - 1, y - 1)]);
		};

		for (std::uint32_t y = 0; y < c.height; y++) {
			for (std::uint32_t x = 0; x < c.width; x++) {
				const std::size_t i = c.index(x, y);
				if (nearby(under_ramp, x, y) || (nearby(is_cliff, x, y) && !c.ramp[i])) {
					if (!cliff_in_range(c.cliff[i])) {
						throw std::runtime_error(
							std::format("The corner at {}, {} uses cliff texture {} of {}", x, y, cliff_table_index(c.cliff[i]), cliff_table)
						);
					}
				} else if (!c.blight[i] && !ground_in_range(c.ground[i])) {
					throw std::runtime_error(std::format("The corner at {}, {} uses ground texture {} of {}", x, y, c.ground[i], ground_table));
				}
			}
		}
	}

	struct TerrainSize {
		std::uint64_t width;
		std::uint64_t height;
	};

	/// Terrain::load creates GPU resources as it parses, so its layout is checked here instead.
	auto check_w3e(BinaryReader& reader, const BodyScope scope) -> TerrainSize {
		if (reader.read_string(4) != "W3E!") {
			throw std::runtime_error("Its magic number is not W3E!");
		}
		const std::uint32_t version = reader.read<std::uint32_t>();
		reader.advance(5); // Tileset, custom tileset flag
		const std::uint32_t ground_table = reader.read_count(4);
		reader.advance(std::size_t { ground_table } * 4);
		const std::uint32_t cliff_table = reader.read_count(4);
		reader.advance(std::size_t { cliff_table } * 4);
		const std::uint64_t width = reader.read<std::uint32_t>();
		const std::uint64_t height = reader.read<std::uint32_t>();

		// Every position is computed from this, and a NaN one reaches interpolated_height, whose clamp lets it through.
		const float offset_x = reader.read<float>();
		const float offset_y = reader.read<float>();
		if (!std::isfinite(offset_x) || !std::isfinite(offset_y)) {
			throw std::runtime_error("Its terrain offset is not a number");
		}

		if (width < 2 || height < 2) {
			throw std::runtime_error(std::format("A terrain of {} by {} is too small", width, height));
		}
		const std::uint64_t corner_size = version >= 12 ? 8 : 7;
		if (width * height > static_cast<std::uint64_t>(reader.remaining()) / corner_size) {
			throw std::runtime_error(std::format("A terrain of {} by {} does not fit in the file", width, height));
		}

		const std::size_t corner_count = width * height;
		W3eCorners corners;
		corners.width = static_cast<std::uint32_t>(width);
		corners.height = static_cast<std::uint32_t>(height);
		corners.ground.resize(corner_count);
		corners.cliff.resize(corner_count);
		corners.layer.resize(corner_count);
		corners.ramp.resize(corner_count);
		corners.blight.resize(corner_count);
		for (std::size_t i = 0; i < corner_count; i++) {
			reader.advance(4); // Height, water height and edge
			if (version >= 12) {
				const std::uint16_t texture_and_flags = reader.read<std::uint16_t>();
				corners.ground[i] = texture_and_flags & 0b00'0011'1111;
				corners.ramp[i] = (texture_and_flags & 0b00'0100'0000) != 0;
				corners.blight[i] = (texture_and_flags & 0b00'1000'0000) != 0;
			} else {
				const std::uint8_t texture_and_flags = reader.read<std::uint8_t>();
				corners.ground[i] = texture_and_flags & 0b0000'1111;
				corners.ramp[i] = (texture_and_flags & 0b0001'0000) != 0;
				corners.blight[i] = (texture_and_flags & 0b0010'0000) != 0;
			}
			reader.advance(1); // Variations
			const std::uint8_t misc = reader.read<std::uint8_t>();
			corners.cliff[i] = (misc & 0b1111'0000) >> 4;
			corners.layer[i] = misc & 0b0000'1111;
		}
		if (scope == BodyScope::full) {
			check_w3e_texture_indices(corners, ground_table, cliff_table);
		}
		return { width, height };
	}

	/// PathingMap::load creates GPU resources as it parses, so its layout is checked here instead.
	void check_wpm(BinaryReader& reader, const TerrainSize terrain) {
		if (reader.read_string(4) != "MP3W") {
			throw std::runtime_error("Its magic number is not MP3W");
		}
		reader.advance(4); // Version
		const std::uint64_t width = reader.read<std::uint32_t>();
		const std::uint64_t height = reader.read<std::uint32_t>();
		if (width * height > static_cast<std::uint64_t>(reader.remaining())) {
			throw std::runtime_error(std::format("A pathing map of {} by {} does not fit in the file", width, height));
		}

		// PathingMap::load replaces a zero-sized map with one that fits. Terrain brushes and Map::resize index a
		// smaller one by the terrain's size, past its end, while a larger one (HiveWE saves terrain * 4) stays in bounds.
		const std::uint64_t needed_width = (terrain.width - 1) * 4;
		const std::uint64_t needed_height = (terrain.height - 1) * 4;
		if (width != 0 && height != 0 && (width < needed_width || height < needed_height)) {
			throw std::runtime_error(
				std::format("A pathing map of {} by {} is smaller than the {} by {} its terrain needs", width, height, needed_width, needed_height)
			);
		}
	}
}

/// Decides whether Map::load would get through this map's files, without touching the map that is open now.
/// Parses in Map::load's order into throwaways with the real parsers, except where those need the GPU. Refuses
/// what Map::load would throw on, and also unbounded counts, non-finite positions and unparseable triggers.
/// Call it before anything that tears down the open map.
export auto check_map_bodies(const fs::path& directory, const BodyScope scope = BodyScope::full) -> std::expected<void, std::string> {
	const OpenMapStateGuard guard;

	std::string_view current_file;
	try {
		const auto parse_if_present = [&](const std::string_view file, const auto& parse) {
			if (is_present(directory / file)) {
				current_file = file;
				BinaryReader reader = read_map_file(directory / file);
				parse(reader);
			}
		};
		const auto parse_objects = [&](const auto& object_files) {
			for (const ObjectFile& object : object_files) {
				for (const std::string_view file : { object.file, object.skin_file }) {
					parse_if_present(file, [&](BinaryReader& reader) {
						check_modification_file_layout(reader, object.optional_ints);
					});
				}
			}
		};

		if (scope == BodyScope::full && is_present(directory / "war3map.wtg")) {
			Hierarchy::map_read_override = &directory;
			Triggers triggers;
			current_file = "war3map.wtg";
			triggers.load();
			if (is_present(directory / "war3map.wct")) {
				current_file = "war3map.wct";
				triggers.load_scripts();
			}
			Hierarchy::map_read_override = nullptr;
		}

		if (is_present(directory / misc_file)) {
			current_file = misc_file;
			const ini::INI misc(directory / misc_file, true);
			if (!misc.section_exists("Misc")) {
				throw std::runtime_error("It has no [Misc] section");
			}
		}

		// Value-initialized as in the Map that Map::load fills: below v28 the file does not set game_version_*.
		MapInfo info {};
		current_file = "war3map.w3i";
		{
			BinaryReader reader = read_map_file(directory / current_file);
			info.load(reader);
		}

		current_file = "war3map.w3e";
		TerrainSize terrain_size {};
		{
			BinaryReader reader = read_map_file(directory / current_file);
			// A map can bring its own ramp models. hierarchy.tileset is left alone: it stays the open map's.
			Hierarchy::map_read_override = &directory;
			terrain_size = check_w3e(reader, scope);
			Hierarchy::map_read_override = nullptr;
		}

		parse_if_present("war3map.wpm", [&](BinaryReader& reader) { check_wpm(reader, terrain_size); });

		parse_objects(objects_before_doodads);

		current_file = "war3map.doo";
		{
			BinaryReader reader = read_map_file(directory / current_file);
			std::vector<Doodad> doodads;
			std::vector<SpecialDoodad> special_doodads;
			std::uint32_t version = 0;
			Doodads::read_records(reader, glm::vec2(0.f), info, doodads, special_doodads, version);
		}

		parse_objects(objects_before_units);

		parse_if_present("war3mapUnits.doo", [&](BinaryReader& reader) {
			std::uint32_t version = 0;
			std::uint32_t subversion = 0;
			Units::read_records(reader, glm::vec2(0.f), info, version, subversion, [](Unit&) {});
		});

		parse_objects(objects_after_units);

		parse_if_present("war3map.w3r", [](BinaryReader& reader) {
			Regions regions;
			regions.load(reader, 0.f, 0.f);
		});
		parse_if_present("war3map.w3c", [&](BinaryReader& reader) {
			GameCameras cameras;
			cameras.load(reader, info.game_version_major, info.game_version_minor, 0.f, 0.f);
		});
		parse_if_present("war3map.w3s", [](BinaryReader& reader) {
			Sounds sounds;
			sounds.load(reader);
		});
	} catch (const std::exception& error) {
		return std::unexpected(
			std::format("This map cannot be opened because {} is damaged or incomplete.\n\n{}: {}", current_file, current_file, error.what())
		);
	}

	return {};
}

/// The whole check a map must pass before the open map is torn down: versions first, then bodies.
export auto check_map_for_opening(const fs::path& directory, const BodyScope scope = BodyScope::full) -> std::expected<void, std::string> {
	if (auto versions = check_map_format_versions(directory); !versions) {
		return versions;
	}
	return check_map_bodies(directory, scope);
}

/// What BodyScope::without_triggers reads beyond checked_map_files, for checking a map still in its archive.
export auto body_only_map_files() -> std::vector<std::string_view> {
	std::vector<std::string_view> files { misc_file };
	const auto add = [&](const auto& object_files) {
		for (const ObjectFile& object : object_files) {
			files.push_back(object.file);
			files.push_back(object.skin_file);
		}
	};
	add(objects_before_doodads);
	add(objects_before_units);
	add(objects_after_units);
	return files;
}
