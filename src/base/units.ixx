module;

#include <absl/strings/str_split.h>
#include <tracy/Tracy.hpp>

export module Units;

import std;
import GLThreadPool;
import Utilities;
import SkinnedMesh;
import SkeletalModelInstance;
import MapInfo;
import MapFormatCheck;
import BinaryReader;
import BinaryWriter;
import ResourceManager;
import Hierarchy;
import Globals;
import Terrain;
import Rects;
import <glm/glm.hpp>;
import <glm/gtc/matrix_transform.hpp>;

namespace fs = std::filesystem;

export struct Unit {
	static inline int auto_increment;

	std::string id;
	int variation = 0;
	glm::vec3 position;
	float angle;
	glm::vec3 scale;

	std::string skin_id;

	// Read from war3mapUnits.doo v12 and up and only written back. Their meaning is unknown, so other values are preserved.
	// The defaults are what a fresh 3.0.1 World Editor save and all stock units have.
	uint32_t group_id = 0xFFFFFFFF;
	std::array<uint32_t, 2> trailer {};

	uint8_t flags = 2;
	int player = 0;

	uint8_t unknown1 = 0;
	uint8_t unknown2 = 0;
	int health = -1;
	int mana = -1;

	int item_table_pointer = -1;
	std::vector<ItemSet> item_sets;

	int gold = 12500;
	float target_acquisition = -1;

	int level = 1;
	int strength = 0;
	int agility = 0;
	int intelligence = 0;

	// Slot, ID
	std::vector<std::pair<uint32_t, std::string>> items;

	// ID, autocast, ability level
	std::vector<std::tuple<std::string, uint32_t, uint32_t>> abilities;

	int random_type = 0;
	/// Everything after random_type as it is in the file, with for type 2 the entry count in front of the entries.
	std::vector<uint8_t> random;

	int custom_color = -1;
	int waygate = -1;
	int creation_number;

	SkeletalModelInstance skeleton;
	std::shared_ptr<SkinnedMesh> mesh;
	glm::vec3 color;

	Unit() {
		creation_number = ++auto_increment;
	}

	static bool uses_water_height(const std::string_view id) {
		if (!units_slk.row_headers.contains(id)) {
			return false;
		}

		const std::string_view movement_type = units_slk.data<std::string_view>("movetp", id);
		return movement_type == "amph" || movement_type == "amphibious" || movement_type == "float" || movement_type == "hover";
	}

	bool uses_water_height() const {
		return uses_water_height(id);
	}

	void update_terrain_height(const Terrain& terrain) {
		position.z = terrain.interpolated_height(position.x, position.y, uses_water_height());
	}

	void update() {
		float model_scale = 0.f;
		float move_height = 0.f;
		if (items_slk.row_headers.contains(id)) {
			model_scale = items_slk.data<float>("scale", id) / 128.f;
			color.r = items_slk.data<float>("colorr", id) / 255.f;
			color.g = items_slk.data<float>("colorg", id) / 255.f;
			color.b = items_slk.data<float>("colorb", id) / 255.f;
		} else {
			model_scale = units_slk.data<float>("modelscale", id) / 128.f;
			move_height = units_slk.data<float>("moveheight", id) / 128.f;
			color.r = units_slk.data<float>("red", id) / 255.f;
			color.g = units_slk.data<float>("green", id) / 255.f;
			color.b = units_slk.data<float>("blue", id) / 255.f;
		}

		const glm::vec3 final_position = position + glm::vec3(0.f, 0.f, move_height);
		const glm::vec3 final_scale = glm::vec3(model_scale);

		skeleton.update_location(final_position, angle, final_scale);
	}
};

export class Units {
	std::unordered_map<std::string, std::shared_ptr<SkinnedMesh>> id_to_mesh;
	std::mutex mesh_mutex;

	static constexpr int write_version = 8;
	static constexpr int write_subversion = 11;

	/// Every id in this file takes up 4 bytes, so a shorter one is padded, as the reader cuts it at the first NUL. A longer one
	/// is refused, as cutting it would name another object. A row of an imported SLK or the object editor's new object dialog can hold one.
	static void write_id(BinaryWriter& writer, const std::string_view id) {
		if (!id_fits(id)) {
			throw std::runtime_error(std::format("war3mapUnits.doo cannot hold the id {}, which is longer than 4 bytes", id));
		}
		std::array<char, 4> bytes {};
		id.copy(bytes.data(), bytes.size());
		writer.write_string(std::string_view(bytes.data(), bytes.size()));
	}

	/// Subversion 9 has no room for these, so a unit that was given one needs subversion 11.
	static bool needs_subversion_11(const Unit& i) {
		return i.item_table_pointer != -1 || i.strength != 0 || i.agility != 0 || i.intelligence != 0;
	}

	//static constexpr int mod_table_write_version = 2;

  public:
	std::vector<Unit> units;
	std::vector<Unit> items;

	/// What was loaded, which is what save starts from. A file of v12 or up keeps its version and subversion, so its fields survive.
	/// Anything older saves as write_version and write_subversion. Each save works out on its own whether a format_subversion of 9
	/// has to be written as 11: see needs_subversion_11.
	int format_version = write_version;
	int format_subversion = write_subversion;

	/// Counts bytes, not characters, as that is what the file has room for.
	static bool id_fits(const std::string_view id) {
		return id.size() <= 4;
	}

	struct OverlongId {
		std::string id;
		std::string field;
		std::string owner_id;
	};

	/// The first id save would refuse, covering every id it hands to write_id, or nothing when they all fit.
	/// Map::save asks this before it writes anything, so the throw in write_id stays a backstop.
	[[nodiscard]]
	std::optional<OverlongId> find_id_over_4_bytes() const {
		for (const std::vector<Unit>* list : { &units, &items }) {
			for (const Unit& i : *list) {
				if (!id_fits(i.id)) {
					return OverlongId { i.id, "type", i.id };
				}
				if (!id_fits(i.skin_id)) {
					return OverlongId { i.skin_id, "skin", i.id };
				}
				for (const ItemSet& set : i.item_sets) {
					for (const auto& [chance, id] : set.items) {
						if (!id_fits(id)) {
							return OverlongId { id, "dropped item", i.id };
						}
					}
				}
				for (const auto& [slot, id] : i.items) {
					if (!id_fits(id)) {
						return OverlongId { id, "inventory item", i.id };
					}
				}
				for (const auto& [id, autocast, level] : i.abilities) {
					if (!id_fits(id)) {
						return OverlongId { id, "ability", i.id };
					}
				}
			}
		}
		return std::nullopt;
	}

	void load(const Terrain& terrain, const MapInfo& info) {
		format_version = write_version;
		format_subversion = write_subversion;

		BinaryReader reader = hierarchy.map_file_read("war3mapUnits.doo").value();

		std::uint32_t version = write_version;
		std::uint32_t subversion = write_subversion;
		read_records(reader, terrain.offset, info, version, subversion, [&](Unit& i) {
			i.update_terrain_height(terrain);

			// Either a unit or an item
			if (units_slk.row_headers.contains(i.id) || i.id == "sloc" || i.id == "uDNR" || i.id == "bDNR") {
				units.push_back(i);
			} else {
				items.push_back(i);
			}

			Unit::auto_increment = std::max(Unit::auto_increment, i.creation_number);
		});

		if (version >= 12) {
			format_version = static_cast<int>(version);
			format_subversion = static_cast<int>(subversion);
		}
	}

	/// Parses war3mapUnits.doo, handing each record to on_record. Constructing a Unit bumps Unit::auto_increment;
	/// apart from that this touches nothing, and reads no SLK and no terrain.
	static void read_records(
		BinaryReader& reader,
		const glm::vec2 terrain_offset,
		const MapInfo& info,
		std::uint32_t& out_version,
		std::uint32_t& out_subversion,
		const std::function<void(Unit&)>& on_record
	) {
		const std::string magic_number = reader.read_string(4);
		if (magic_number != "W3do") {
			std::cout << "Invalid war3mapUnits.w3e file: Magic number is not W3do\n";
		}
		const uint32_t version = reader.read<uint32_t>();
		map_format::require_accepted("war3mapUnits.doo", map_format::units_versions, version);
		out_version = version;

		// The group id and the trailer exist from v12 on. v9 to v11 are refused above.
		const bool has_v12_fields = version >= 12;

		// Subversion
		const int subversion = reader.read<uint32_t>();
		map_format::require_accepted("war3mapUnits.doo", map_format::units_subversions, subversion, "subversion");
		out_subversion = subversion;

		const bool has_skin = info.game_version_major * 100 + info.game_version_minor >= 132;
		if (has_v12_fields && !has_skin) {
			throw std::runtime_error(std::format(
				"war3mapUnits.doo version {} is paired with a war3map.w3i game version of {}.{}, which has not been seen",
				version,
				info.game_version_major,
				info.game_version_minor
			));
		}

		// As BinaryReader::read_string, except that v12 and up refuse what the cut would lose, which could not be saved back.
		const auto read_id = [&](const std::string_view field, const uint32_t record) {
			const auto bytes = reader.read<std::array<char, 4>>();
			std::string id(bytes.data(), bytes.size());
			if (const size_t nul = id.find('\0'); nul != std::string::npos) {
				if (has_v12_fields && id.find_first_not_of('\0', nul) != std::string::npos) {
					throw std::runtime_error(std::format("war3mapUnits.doo record {} has bytes after the first NUL of the {} id", record, field));
				}
				id.resize(nul);
			}
			return id;
		};

		const uint32_t unit_count = reader.read_count(91 + (has_skin ? 4 : 0) + (subversion >= 11 ? 16 : 0) + (has_v12_fields ? 16 : 0));
		for (uint32_t k = 0; k < unit_count; k++) {
			Unit i;
			i.id = read_id("id", k);
			i.variation = reader.read<uint32_t>();
			const glm::vec3 position = reader.read<glm::vec3>();
			if (!std::isfinite(position.x) || !std::isfinite(position.y)) {
				throw std::runtime_error(std::format("Unit {} has a position that is not a number", i.id));
			}
			i.position = (position - glm::vec3(terrain_offset, 0)) / 128.f;
			i.angle = reader.read<float>();
			i.scale = reader.read<glm::vec3>() / 128.f;

			if (has_skin) {
				i.skin_id = read_id("skin", k);
			} else {
				i.skin_id = i.id;
			}

			if (has_v12_fields) {
				i.group_id = reader.read<uint32_t>();
			}

			i.flags = reader.read<uint8_t>();

			i.player = reader.read<uint32_t>();
			if (i.player > 11 && info.editor_version < 6060) {
				i.player += 12;
			}

			i.unknown1 = reader.read<uint8_t>();
			i.unknown2 = reader.read<uint8_t>();

			i.health = reader.read<uint32_t>();
			i.mana = reader.read<uint32_t>();

			if (subversion >= 11) {
				i.item_table_pointer = reader.read<uint32_t>();
			}

			i.item_sets.resize(reader.read_count(4));
			for (auto&& j : i.item_sets) {
				j.items.resize(reader.read_count(8));
				for (auto&& [chance, id] : j.items) {
					id = read_id("item set", k);
					chance = reader.read<uint32_t>();
				}
			}

			i.gold = reader.read<uint32_t>();
			i.target_acquisition = reader.read<float>();

			i.level = reader.read<uint32_t>();

			if (subversion >= 11) {
				i.strength = reader.read<uint32_t>();
				i.agility = reader.read<uint32_t>();
				i.intelligence = reader.read<uint32_t>();
			}

			i.items.resize(reader.read_count(8));
			for (auto&& [slot, id] : i.items) {
				slot = reader.read<uint32_t>();
				id = read_id("inventory", k);
			}

			i.abilities.resize(reader.read_count(12));
			for (auto&& [id, autocast, level] : i.abilities) {
				id = read_id("ability", k);
				autocast = reader.read<uint32_t>();
				level = reader.read<uint32_t>();
			}

			i.random_type = reader.read<uint32_t>();
			switch (i.random_type) {
				case 0:
					i.random = reader.read_vector<uint8_t>(4);
					break;
				case 1:
					i.random = reader.read_vector<uint8_t>(8);
					break;
				case 2: {
					const uint32_t entries = reader.read_count(8);
					const auto count_bytes = std::bit_cast<std::array<uint8_t, 4>>(entries);
					i.random = reader.read_vector<uint8_t>(std::size_t { entries } * 8);
					i.random.insert(i.random.begin(), count_bytes.begin(), count_bytes.end());
					break;
				}
			}

			i.custom_color = reader.read<uint32_t>();
			i.waygate = reader.read<uint32_t>();
			i.creation_number = reader.read<uint32_t>();

			if (has_v12_fields) {
				i.trailer[0] = reader.read<uint32_t>();
				i.trailer[1] = reader.read<uint32_t>();
				// 0 on all stock units, and save writes a 0: anything else would be lost, so it is refused.
				const uint32_t third = reader.read<uint32_t>();
				if (third != 0) {
					throw std::runtime_error(
						std::format("war3mapUnits.doo unit {} has {} as the third value of its trailer, which has not been seen", i.id, third)
					);
				}
			}

			on_record(i);
		}

		if (has_v12_fields && reader.remaining() != 0) {
			throw std::runtime_error(std::format("war3mapUnits.doo version {} has leftover bytes after its last unit: {}", version, reader.remaining()));
		}
	}

	void save(const Terrain& terrain) const {
		BinaryWriter writer;

		// Per save, as the item table and the stats can change at any time: a file loaded as 9 is written as 11 while a unit needs it
		// and as 9 again once none does, with nothing lost. One loaded as 11 stays 11.
		const bool has_subversion_11_fields =
			format_subversion >= 11 || std::ranges::any_of(units, needs_subversion_11) || std::ranges::any_of(items, needs_subversion_11);
		const bool has_v12_fields = format_version >= 12;

		writer.write_string("W3do");
		writer.write<uint32_t>(format_version);
		writer.write<uint32_t>(has_subversion_11_fields ? write_subversion : format_subversion);

		writer.write<uint32_t>(units.size() + items.size());

		auto write_units = [&](const std::vector<Unit>& to_write) {
			for (auto&& i : to_write) {
				write_id(writer, i.id);
				writer.write<uint32_t>(i.variation);
				writer.write<glm::vec3>(i.position * 128.f + glm::vec3(terrain.offset, 0));
				writer.write<float>(i.angle);
				writer.write<glm::vec3>(i.scale * 128.f);

				write_id(writer, i.skin_id);

				if (has_v12_fields) {
					writer.write<uint32_t>(i.group_id);
				}

				writer.write<uint8_t>(i.flags);

				writer.write<uint32_t>(i.player);

				writer.write<uint8_t>(i.unknown1);
				writer.write<uint8_t>(i.unknown2);

				writer.write<uint32_t>(i.health);
				writer.write<uint32_t>(i.mana);

				if (has_subversion_11_fields) {
					writer.write<uint32_t>(i.item_table_pointer);
				}

				writer.write<uint32_t>(i.item_sets.size());
				for (auto&& j : i.item_sets) {
					writer.write<uint32_t>(j.items.size());
					for (auto&& [chance, id] : j.items) {
						write_id(writer, id);
						writer.write<uint32_t>(chance);
					}
				}

				writer.write<uint32_t>(i.gold);
				writer.write<float>(i.target_acquisition);
				writer.write<uint32_t>(i.level);
				if (has_subversion_11_fields) {
					writer.write<uint32_t>(i.strength);
					writer.write<uint32_t>(i.agility);
					writer.write<uint32_t>(i.intelligence);
				}

				writer.write<uint32_t>(i.items.size());
				for (auto&& [slot, id] : i.items) {
					writer.write<uint32_t>(slot);
					write_id(writer, id);
				}

				writer.write<uint32_t>(i.abilities.size());
				for (auto&& [id, autocast, level] : i.abilities) {
					write_id(writer, id);
					writer.write<uint32_t>(autocast);
					writer.write<uint32_t>(level);
				}

				writer.write<uint32_t>(i.random_type);
				writer.write_vector(i.random);

				writer.write<uint32_t>(i.custom_color);
				writer.write<uint32_t>(i.waygate);
				writer.write<uint32_t>(i.creation_number);

				if (has_v12_fields) {
					writer.write<uint32_t>(i.trailer[0]);
					writer.write<uint32_t>(i.trailer[1]);
					writer.write<uint32_t>(0);
				}
			}
		};

		write_units(units);
		write_units(items);

		hierarchy.map_file_write("war3mapUnits.doo", writer.buffer);
	}

	void update_area(const TerrainRectF& area, const Terrain& terrain) {
		for (auto&& i : query_area(area)) {
			i->update_terrain_height(terrain);
			i->update();
		}
	}

	void create() {
		ZoneScoped("Creating Units");
		// Phase 1: Pre-load unique meshes to avoid thread pool starvation.
		// Without this, multiple threads block on shared_future::get() inside ResourceManager
		// while only 1 thread actually constructs the shared mesh.
		{
			std::unordered_set<std::string> seen;
			std::vector<std::future<void>> mesh_futures;

			for (const auto& i : units) {
				if (i.id == "sloc") {
					continue;
				}
				if (seen.insert(i.id).second) {
					std::string id = i.id;
					mesh_futures.push_back(gl_thread_pool.submit([this, id] {
						get_mesh(id);
					}));
				}
			}
			for (const auto& i : items) {
				if (seen.insert(i.id).second) {
					std::string id = i.id;
					mesh_futures.push_back(gl_thread_pool.submit([this, id] {
						get_mesh(id);
					}));
				}
			}

			for (auto& f : mesh_futures) {
				f.get();
			}
		}

		// Phase 2: Init units. All meshes are now cached in id_to_mesh,
		// so get_mesh() returns immediately without blocking in ResourceManager.
		std::vector<std::future<void>> futures;
		futures.reserve(units.size() + items.size());

		for (auto& i : units) {
			if (i.id == "sloc") {
				continue;
			}

			futures.push_back(gl_thread_pool.submit([&] {
				i.mesh = get_mesh(i.id);
				i.skeleton = SkeletalModelInstance(i.mesh->mdx, get_required_animation_names(i.id));
				i.update();
			}));
		}
		for (auto& i : items) {
			futures.push_back(gl_thread_pool.submit([&] {
				i.mesh = get_mesh(i.id);
				i.skeleton = SkeletalModelInstance(i.mesh->mdx, get_required_animation_names(i.id));
				i.update();
			}));
		}

		for (auto& f : futures) {
			f.get();
		}
	}

	// Will assign a unique creation number
	Unit& add_unit(std::string id, glm::vec3 position) {
		// ToDo change this once SkeletalModelInstance doesn't use pointers anymore
		units.push_back(Unit());
		Unit& unit = units.back();
		unit.id = id;
		unit.skin_id = id;
		unit.mesh = get_mesh(id);
		unit.position = position;
		unit.scale = glm::vec3(1.f);
		unit.angle = 0.f;
		unit.random = {1, 0, 0, 0};
		unit.creation_number = ++Unit::auto_increment;
		unit.skeleton = SkeletalModelInstance(unit.mesh->mdx, get_required_animation_names(id));
		unit.update();

		return units.back();
	}

	// Assumes you will set a unique creation number yourself
	Unit& add_unit(Unit unit) {
		units.push_back(unit);
		return units.back();
	}

	void remove_unit(Unit* unit) {
		auto iterator = units.begin() + std::distance(units.data(), unit);
		units.erase(iterator);
	}

	[[nodiscard]]
	Unit* find_start_location(const int player) {
		for (auto& unit : units) {
			if (unit.id == "sloc" && unit.player == player) {
				return &unit;
			}
		}
		return nullptr;
	}

	[[nodiscard]]
	const Unit* find_start_location(const int player) const {
		for (const auto& unit : units) {
			if (unit.id == "sloc" && unit.player == player) {
				return &unit;
			}
		}
		return nullptr;
	}

	[[nodiscard]]
	Unit& add_start_location(const int player, const glm::vec3 position) {
		units.push_back(Unit());
		Unit& unit = units.back();
		unit.id = "sloc";
		unit.skin_id = "sloc";
		unit.position = position;
		unit.scale = glm::vec3(1.f);
		unit.angle = 0.f;
		unit.player = player;
		unit.random = {1, 0, 0, 0};
		unit.creation_number = ++Unit::auto_increment;
		return unit;
	}

	[[nodiscard]]
	std::vector<Unit*> query_area(const TerrainRectF& area) {
		std::vector<Unit*> result;

		for (auto& i : units) {
			if (area.contains(i.position.x, i.position.y)) {
				result.push_back(&i);
			}
		}
		return result;
	}

	void remove_units(const std::unordered_set<Unit*>& list) {
		std::erase_if(units, [&](Unit& unit) {
			return list.contains(&unit);
		});
	}

	void remove_items(const std::unordered_set<Unit*>& list) {
		std::erase_if(items, [&](Unit& item) {
			return list.contains(&item);
		});
	}

	void process_unit_field_change(const std::string& id, const std::string& field) {
		if (field == "file" || field == "attachmentlinkprops") {
			id_to_mesh.erase(id);
			for (auto& i : units) {
				if (i.id == id) {
					i.mesh = get_mesh(id);
					i.skeleton = SkeletalModelInstance(i.mesh->mdx, get_required_animation_names(id));
					i.update();
				}
			}
		}
		if (field == "modelscale" || field == "moveheight" || field == "red" || field == "green" || field == "blue") {
			for (auto& i : units) {
				if (i.id == id) {
					i.update();
				}
			}
		}

		if (field == "colorr" || field == "colorg" || field == "colorb" || field == "scale") {
			for (auto& i : items) {
				if (i.id == id) {
					i.update();
				}
			}
		}
	}

	void process_item_field_change(const std::string& id, const std::string& field) {
		if (field == "colorr" || field == "colorg" || field == "colorb" || field == "scale") {
			for (auto& i : items) {
				if (i.id == id) {
					i.update();
				}
			}
		}
	}

	std::shared_ptr<SkinnedMesh> get_mesh(const std::string& id) {
		{
			std::lock_guard lock(mesh_mutex);
			if (const auto found = id_to_mesh.find(id); found != id_to_mesh.end()) {
				return found->second;
			}
		}

		fs::path mesh_path = units_slk.data<std::string_view>("file", id);
		if (mesh_path.empty()) {
			mesh_path = items_slk.data<std::string_view>("file", id);
		}
		mesh_path.replace_extension(".mdx");

		mesh_path = fs::path(string_replaced(mesh_path.string(), "\\", "/"));

		auto result = resource_manager.load<SkinnedMesh>(mesh_path, "", std::nullopt);
		if (!result) {
			std::println("Missing model file for {} with file path: {} ({})", id, mesh_path.string(), result.error());
			result = resource_manager.load<SkinnedMesh>("Objects/Invalidmodel/Invalidmodel.mdx", "", std::nullopt);
		}

		auto mesh = result.value();
		{
			std::lock_guard lock(mesh_mutex);
			id_to_mesh.emplace(id, mesh);
		}
		return mesh;
	}

	static std::vector<std::string> get_required_animation_names(const std::string& id) {
		std::string animation_names_string = units_slk.data("attachmentlinkprops", id);
		to_lowercase(animation_names_string);
		return absl::StrSplit(animation_names_string, ",", absl::SkipEmpty());
	}
};
