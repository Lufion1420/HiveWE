module;

#include <QMessageBox>

export module Map;

import std;
import types;
import MDX;
import SLK;
import INI;
import BinaryReader;
import GameCameras;
import Imports;
import MapInfo;
import Doodad;
import Sounds;
import Regions;
import WorldUndoManager;
import TriggerStrings;
import PathingMap;
import ShadowMap;
import Physics;
import Hierarchy;
import Camera;
import Timer;
import Physics;
import ModificationTables;
import StockObjectData;
import RenderManager;
import TableModel;
import Globals;
import Units;
import Doodads;
import Triggers;
import LuaSyntax;
import Terrain;
import GameplayConstants;
import ScriptPreservation;
export import AssetUsage;
import Utilities;
import UnorderedMap;
import "brush.h";
import <glad/glad.h>;
import <bullet/btBulletDynamicsCommon.h>;
import <glm/glm.hpp>;
import <glm/gtc/matrix_transform.hpp>;

namespace fs = std::filesystem;
using namespace std::literals::string_literals;

/// The first object data file Map::save() would fail to write, with every field in it that cannot be written.
export struct UnwritableObjectData {
	std::string_view file_name;
	std::vector<ModificationSerializeError> errors;
};

/// Checks the seven object data tables Map::save() writes (the skin files hold no fields) with the same rules as the writer.
export std::optional<UnwritableObjectData> find_unwritable_object_data() {
	struct Table {
		std::string_view file_name;
		const slk::SLK& slk;
		const slk::SLK& meta_slk;
		bool optional_ints;
	};
	const std::array<Table, 7> tables = { {
		{ "war3map.w3d", doodads_slk, doodads_meta_slk, true },
		{ "war3map.w3b", destructibles_slk, destructibles_meta_slk, false },
		{ "war3map.w3u", units_slk, units_meta_slk, false },
		{ "war3map.w3t", items_slk, items_meta_slk, false },
		{ "war3map.w3a", abilities_slk, abilities_meta_slk, true },
		{ "war3map.w3h", buff_slk, buff_meta_slk, false },
		{ "war3map.w3q", upgrade_slk, upgrade_meta_slk, true },
	} };

	for (const auto& table : tables) {
		auto errors = collect_save_modification_file_errors(table.slk, table.meta_slk, table.optional_ints);
		if (!errors.empty()) {
			return UnwritableObjectData { table.file_name, std::move(errors) };
		}
	}
	return std::nullopt;
}

export class Map: public QObject {
	Q_OBJECT

  public:
	bool loaded = false;
	/// Set by `HiveWE.exe --resave`: save refusals and warnings print to stderr instead of opening a dialog, and missing start
	/// locations refuse the save instead of asking.
	bool headless = false;
	/// Set by save(): the generated script did not compile, so the old war3map.j was kept.
	bool script_compile_failed = false;
	/// Set by load() (see keeps_existing_script): the map's trigger data cannot rebuild its script, so save() keeps the script files
	/// as they are and writes no war3map.wtg, war3map.wct or script, and trigger editing is off. A new Map starts without it.
	bool keeps_script = false;

	TriggerStrings trigger_strings;
	Triggers triggers;
	MapInfo info;
	Terrain terrain;
	PathingMap pathing_map;
	Imports imports;
	Doodads doodads;
	Units units;
	Regions regions;
	GameCameras cameras;
	Sounds sounds;
	GameplayConstants gameplay_constants;
	ShadowMap shadow_map;
	WorldUndoManager world_undo;
	Brush* brush = nullptr;
	Physics physics;

	bool enforce_water_height_limits = true;

	bool render_doodads = true;
	bool render_units = true;
	bool render_pathing = false;
	bool render_brush = true;
	bool render_lighting = true;
	bool render_water = true;
	bool render_click_helpers = false;
	bool render_wireframe = false;
	bool render_debug = false;

	glm::vec3 light_direction = glm::normalize(glm::vec3(1.f, 1.f, -3.f));

	fs::path filesystem_path;
	std::string name;

	/// Set by load(): custom objects left out because their base object is not in the loaded object data (they are dropped on save),
	/// and how many placed units of a skipped custom unit were left out of war3mapUnits.doo with them. HiveWE::load_map shows both.
	std::vector<SkippedObject> skipped_objects;
	size_t skipped_placed_units = 0;

	RenderManager render_manager;

  private:
	std::optional<MapFiles> saved_state;
	/// script_inputs() at the last remember_saved_state(), while keeps_script is set.
	std::optional<MapFiles> saved_script_inputs;

  public:
	void load(const fs::path& path) {
		Timer full_timer;
		Timer timer;
		skipped_objects.clear();
		skipped_placed_units = 0;

		hierarchy.map_directory = path;
		filesystem_path = fs::absolute(path) / "";
		// path::string() throws on MSVC for a folder name outside the system code page, so keep the name as UTF-8.
		const std::u8string folder_name = (--(--filesystem_path.end()))->u8string();
		name = std::string(folder_name.begin(), folder_name.end());

		// ToDo So for the game data files we should actually load from _balance/custom_v0.w3mod/Units, _balance/custom_v1.w3mod/Units, _balance/melee_v0.w3mod/units or /Units depending on the Game Data set and Game Data Versions
		// Maybe just ignore RoC so we only need to choose between _balance/custom_v1.w3mod/Units and /Units
		// Maybe just force everyone to suck it up and use /Units

		// Stock table loading itself lives in StockObjectData::load() (src/base/stock_object_data.ixx)
		// so Map Protector's asset-path obfuscation can get an identical, independent copy of these
		// tables for an external source map that was never loaded into this Map object, without
		// maintaining a second copy of this merge sequence.
		stock_object_data::Tables stock_tables = stock_object_data::load();
		units_slk = std::move(stock_tables.units.data);
		units_meta_slk = std::move(stock_tables.units.meta);
		unit_editor_data = std::move(stock_tables.unit_editor_data);
		items_slk = std::move(stock_tables.items.data);
		items_meta_slk = std::move(stock_tables.items.meta);
		doodads_slk = std::move(stock_tables.doodads.data);
		doodads_meta_slk = std::move(stock_tables.doodads.meta);
		destructibles_slk = std::move(stock_tables.destructibles.data);
		destructibles_meta_slk = std::move(stock_tables.destructibles.meta);
		abilities_slk = std::move(stock_tables.abilities.data);
		abilities_meta_slk = std::move(stock_tables.abilities.meta);
		upgrade_slk = std::move(stock_tables.upgrades.data);
		upgrade_meta_slk = std::move(stock_tables.upgrades.meta);
		buff_slk = std::move(stock_tables.buffs.data);
		buff_meta_slk = std::move(stock_tables.buffs.meta);

		units_table = new TableModel(&units_slk, &units_meta_slk, &trigger_strings);
		items_table = new TableModel(&items_slk, &items_meta_slk, &trigger_strings);
		abilities_table = new TableModel(&abilities_slk, &abilities_meta_slk, &trigger_strings);
		doodads_table = new TableModel(&doodads_slk, &doodads_meta_slk, &trigger_strings);
		destructibles_table = new TableModel(&destructibles_slk, &destructibles_meta_slk, &trigger_strings);
		upgrade_table = new TableModel(&upgrade_slk, &upgrade_meta_slk, &trigger_strings);
		buff_table = new TableModel(&buff_slk, &buff_meta_slk, &trigger_strings);

		std::println("\nSLK loading:\t {:>5}ms", timer.elapsed_ms());
		timer.reset();

		// Trigger strings
		if (hierarchy.map_file_exists("war3map.wts")) {
			trigger_strings.load();
		}

		// Triggers (GUI and JASS)
		if (hierarchy.map_file_exists("war3map.wtg")) {
			triggers.load();

			// Custom text triggers (JASS)
			if (hierarchy.map_file_exists("war3map.wct")) {
				triggers.load_scripts();
			}
		}

		TriggerDataPresence trigger_data;
		trigger_data.wtg = hierarchy.map_file_exists("war3map.wtg");
		trigger_data.wct = hierarchy.map_file_exists("war3map.wct");
		trigger_data.script = std::ranges::any_of(map_script_file_names, [](const std::string_view name) {
			return hierarchy.map_file_exists(fs::path(name));
		});
		keeps_script = keeps_existing_script(trigger_data);
		if (keeps_script) {
			std::println(stderr, "{}", kept_script_notice);
		}

		std::println("Trigger loading: {:>5}ms", timer.elapsed_ms());
		timer.reset();

		gameplay_constants.load();

		info.load();
		profile_reset();
		terrain.load(physics);

		std::println("Terrain loading: {:>5}ms", timer.elapsed_ms());
		profile_print();
		timer.reset();

		// Pathing Map
		if (hierarchy.map_file_exists("war3map.wpm")) {
			pathing_map.load(terrain.width, terrain.height);
		} else {
			pathing_map.resize(terrain.width * 4, terrain.height * 4);
		}

		std::println("Pathing loading: {:>5}ms", timer.elapsed_ms());
		timer.reset();

		// Doodads
		if (hierarchy.map_file_exists("war3map.w3d")) {
			load_modification_file("war3map.w3d", doodads_slk, doodads_meta_slk, true, &skipped_objects);
		}

		if (hierarchy.map_file_exists("war3mapSkin.w3d")) {
			load_modification_file("war3mapSkin.w3d", doodads_slk, doodads_meta_slk, true, &skipped_objects);
		}

		if (hierarchy.map_file_exists("war3map.w3b")) {
			load_modification_file("war3map.w3b", destructibles_slk, destructibles_meta_slk, false, &skipped_objects);
		}

		if (hierarchy.map_file_exists("war3mapSkin.w3b")) {
			load_modification_file("war3mapSkin.w3b", destructibles_slk, destructibles_meta_slk, false, &skipped_objects);
		}

		profile_reset();
		doodads.load(terrain, info);
		doodads.create(terrain, pathing_map);
		glFinish(); // Ensure all GL work submitted on worker contexts is visible to the main context

		std::println("Doodad loading:\t {:>5}ms", timer.elapsed_ms());
		profile_print();
		timer.reset();

		if (hierarchy.map_file_exists("war3map.w3u")) {
			load_modification_file("war3map.w3u", units_slk, units_meta_slk, false, &skipped_objects);
		}

		if (hierarchy.map_file_exists("war3mapSkin.w3u")) {
			load_modification_file("war3mapSkin.w3u", units_slk, units_meta_slk, false, &skipped_objects);
		}

		if (hierarchy.map_file_exists("war3map.w3t")) {
			load_modification_file("war3map.w3t", items_slk, items_meta_slk, false, &skipped_objects);
		}

		if (hierarchy.map_file_exists("war3mapSkin.w3t")) {
			load_modification_file("war3mapSkin.w3t", items_slk, items_meta_slk, false, &skipped_objects);
		}

		// Units/Items
		profile_reset();
		if (hierarchy.map_file_exists("war3mapUnits.doo")) {
			// A placed unit of a skipped custom unit would be filed as an item (no row in units_slk), so it is left out instead.
			std::set<std::string> skipped_unit_ids;
			for (const SkippedObject& entry : skipped_objects) {
				if (entry.file.ends_with(".w3u")) {
					skipped_unit_ids.insert(entry.id);
				}
			}
			skipped_placed_units = units.load(terrain, info, skipped_unit_ids);
			units.create();
			glFinish(); // Ensure all GL work submitted on worker contexts is visible to the main context
		}

		std::println("Unit loading:\t {:>5}ms", timer.elapsed_ms());
		profile_print();
		timer.reset();

		// Abilities
		if (hierarchy.map_file_exists("war3map.w3a")) {
			load_modification_file("war3map.w3a", abilities_slk, abilities_meta_slk, true, &skipped_objects);
		}

		if (hierarchy.map_file_exists("war3mapSkin.w3a")) {
			load_modification_file("war3mapSkin.w3a", abilities_slk, abilities_meta_slk, true, &skipped_objects);
		}

		// Buffs
		if (hierarchy.map_file_exists("war3map.w3h")) {
			load_modification_file("war3map.w3h", buff_slk, buff_meta_slk, false, &skipped_objects);
		}

		if (hierarchy.map_file_exists("war3mapSkin.w3h")) {
			load_modification_file("war3mapSkin.w3h", buff_slk, buff_meta_slk, false, &skipped_objects);
		}

		// Upgrades
		if (hierarchy.map_file_exists("war3map.w3q")) {
			load_modification_file("war3map.w3q", upgrade_slk, upgrade_meta_slk, true, &skipped_objects);
		}

		if (hierarchy.map_file_exists("war3mapSkin.w3q")) {
			load_modification_file("war3mapSkin.w3q", upgrade_slk, upgrade_meta_slk, true, &skipped_objects);
		}

		if (!skipped_objects.empty()) {
			std::string list;
			for (const SkippedObject& entry : skipped_objects) {
				list += std::format("{}{}: {} (based on {})", list.empty() ? "" : ", ", entry.file, entry.id, entry.base_id);
			}
			std::println(stderr, "Skipped {} custom object(s) whose base object is not in the loaded object data (they are dropped on save): {}",
						 skipped_objects.size(), list);
		}

		// Regions
		if (hierarchy.map_file_exists("war3map.w3r")) {
			regions.load(terrain.offset.x, terrain.offset.y);
		}

		// Cameras
		if (hierarchy.map_file_exists("war3map.w3c")) {
			cameras.load(info.game_version_major, info.game_version_minor, terrain.offset.x, terrain.offset.y);
		}

		// Sounds
		if (hierarchy.map_file_exists("war3map.w3s")) {
			sounds.load();
		}

		std::println("Misc loading:\t {:>5}ms", timer.elapsed_ms());
		timer.reset();

		// Shadow map
		if (hierarchy.map_file_exists("war3map.shd")) {
			shadow_map.load(terrain.width - 1, terrain.height - 1);
		} else {
			shadow_map.resize((terrain.width - 1) * 4, (terrain.height - 1) * 4);
		}

		std::println("Shadows loading: {:>5}ms", timer.elapsed_ms());
		timer.reset();

		std::println("Full loading: {:>5}ms", full_timer.elapsed_ms());

		// Center camera
		camera.position = glm::vec3(terrain.width / 2, terrain.height / 2, 0);
		camera.position.z = terrain.interpolated_height(camera.position.x, camera.position.y, true);

		loaded = true;
		remember_saved_state();

		connect(
			units_table,
			&TableModel::dataChanged,
			[&](const QModelIndex& top_left, const QModelIndex& top_right, const QVector<int>& roles) {
				const std::string& id = units_slk.index_to_row.at(top_left.row());
				const std::string& field = units_slk.index_to_column.at(top_left.column());
				units.process_unit_field_change(id, field);
			}
		);

		connect(units_table, &TableModel::rowsAboutToBeRemoved, [&](const QModelIndex& parent, int first, int last) {
			for (size_t i = first; i <= last; i++) {
				const std::string& id = units_slk.index_to_row.at(i);
				std::erase_if(units.units, [&](Unit& unit) {
					return unit.id == id;
				});

				if (brush) {
					brush->unselect_id(id);
				}
			}
		});

		connect(
			items_table,
			&TableModel::dataChanged,
			[&](const QModelIndex& top_left, const QModelIndex& top_right, const QVector<int>& roles) {
				const std::string& id = items_slk.index_to_row.at(top_left.row());
				const std::string& field = items_slk.index_to_column.at(top_left.column());
				units.process_item_field_change(id, field);
			}
		);

		connect(items_table, &TableModel::rowsAboutToBeRemoved, [&](const QModelIndex& parent, int first, int last) {
			for (size_t i = first; i <= last; i++) {
				const std::string& id = items_slk.index_to_row.at(i);
				std::erase_if(units.items, [&](Unit& item) {
					return item.id == id;
				});
			}
		});

		connect(
			doodads_table,
			&TableModel::dataChanged,
			[&](const QModelIndex& top_left, const QModelIndex& top_right, const QVector<int>& roles) {
				const std::string& id = doodads_slk.index_to_row.at(top_left.row());
				const std::string& field = doodads_slk.index_to_column.at(top_left.column());
				doodads.process_doodad_field_change(id, field, terrain);
			}
		);

		connect(doodads_table, &TableModel::rowsAboutToBeRemoved, [&](const QModelIndex& parent, int first, int last) {
			for (size_t i = first; i <= last; i++) {
				const std::string& id = doodads_slk.index_to_row.at(i);
				std::erase_if(doodads.doodads, [&](Doodad& doodad) {
					return doodad.id == id;
				});

				if (brush) {
					brush->unselect_id(id);
				}
			}
		});

		connect(
			destructibles_table,
			&TableModel::dataChanged,
			[&](const QModelIndex& top_left, const QModelIndex& top_right, const QVector<int>& roles) {
				const std::string& id = destructibles_slk.index_to_row.at(top_left.row());
				const std::string& field = destructibles_slk.index_to_column.at(top_left.column());
				doodads.process_destructible_field_change(id, field, terrain);
			}
		);

		connect(destructibles_table, &TableModel::rowsAboutToBeRemoved, [&](const QModelIndex& parent, int first, int last) {
			for (size_t i = first; i <= last; i++) {
				const std::string& id = destructibles_slk.index_to_row.at(i);
				std::erase_if(doodads.doodads, [&](Doodad& destructable) {
					return destructable.id == id;
				});

				if (brush) {
					brush->unselect_id(id);
				}
			}
		});
	}

	/// war3mapUnits.doo holds every id in 4 bytes, so a longer one refuses the save with a dialog. Save calls this before it copies or
	/// writes anything, and Save As before it picks or creates a folder.
	bool ensure_unit_ids_fit() const {
		const auto overlong = units.find_id_over_4_bytes();
		if (!overlong) {
			return true;
		}

		if (headless) {
			std::println(stderr, "Save refused: the {} id \"{}\" of a placed {} is longer than 4 bytes.", overlong->field, overlong->id,
						 overlong->owner_id);
			return false;
		}

		QMessageBox message_box;
		message_box.setIcon(QMessageBox::Warning);
		message_box.setWindowTitle("Id Too Long");
		message_box.setText(QString("The map was not saved. The id \"%1\" is longer than 4 bytes.").arg(QString::fromStdString(overlong->id)));
		message_box.setInformativeText(QString("It is the %1 id of a placed %2. Warcraft III stores these ids in 4 bytes, so give the object an id "
											   "of at most 4 bytes in the Object Editor, or remove it, and save again.")
										   .arg(QString::fromStdString(overlong->field), QString::fromStdString(overlong->owner_id)));
		message_box.setStandardButtons(QMessageBox::Ok);
		message_box.exec();
		return false;
	}

	/// The object data writer throws on a field it cannot map to a meta data id or a value of the wrong type. Save calls this before it
	/// copies or writes anything, so such a field refuses the save with a message instead of stopping it halfway.
	bool ensure_object_data_serializes() const {
		const auto unwritable = find_unwritable_object_data();
		if (!unwritable) {
			return true;
		}

		const auto& first = unwritable->errors.front();
		if (headless) {
			std::println(stderr, "Save refused: {} object {} field {}: {} ({} problem(s) in this file)", unwritable->file_name,
						 first.object_id, first.field, first.message, unwritable->errors.size());
			return false;
		}

		QMessageBox message_box;
		message_box.setIcon(QMessageBox::Warning);
		message_box.setWindowTitle("Object Data Cannot Be Saved");
		message_box.setText(QString("The map was not saved. %1 has %2 field(s) that cannot be written.")
								.arg(QString::fromUtf8(unwritable->file_name.data(), unwritable->file_name.size()))
								.arg(unwritable->errors.size()));
		message_box.setInformativeText(QString("First: object %1, field %2: %3")
										   .arg(QString::fromStdString(first.object_id), QString::fromStdString(first.field),
												QString::fromStdString(first.message)));
		message_box.setStandardButtons(QMessageBox::Ok);
		message_box.exec();
		return false;
	}

	/// What save() would write from memory now, for the unsaved changes check. Object data and gameplay constants are taken as
	/// their edits, so a field that cannot be written does not make this fail. The map script is left out, as it is made from this
	/// data. Writes and changes nothing. Empty when a writer throws, for example on a unit id over 4 bytes.
	/// A new writer in save() must be added here, or the editor will not ask to save its edits.
	[[nodiscard]]
	std::optional<MapFiles> serialized_state() const {
		MapFiles files;
		try {
			const MapWriteCapture capture(files);
			pathing_map.save();
			terrain.save();
			shadow_map.save();
			doodads.save(terrain);
			units.save(terrain);
			regions.save(terrain.offset.x, terrain.offset.y);
			info.save(terrain.tileset);
			trigger_strings.save();
			// As in save(): a kept script means no trigger files are written.
			if (!keeps_script) {
				triggers.save();
				triggers.save_scripts();
			}
			// war3map.imp lists the map folder, which the Asset Manager and others change directly.
			if (!filesystem_path.empty()) {
				imports.save(filesystem_path);
			}

			// Object data and gameplay constants are taken as their edits below, under the names their writers use. Their save()
			// writers also go through map_file_write now (GameplayConstants::save too), so calling one under this capture would
			// replace the edit entry with the file bytes: keep them out of the list above.
			const std::array<std::pair<std::string_view, const slk::SLK*>, 8> edits = { {
				{ "war3map.w3d", &doodads_slk },
				{ "war3map.w3b", &destructibles_slk },
				{ "war3map.w3u", &units_slk },
				{ "war3map.w3t", &items_slk },
				{ "war3map.w3a", &abilities_slk },
				{ "war3map.w3h", &buff_slk },
				{ "war3map.w3q", &upgrade_slk },
				{ "war3mapMisc.txt", &gameplay_constants.data },
			} };
			for (const auto& [file_name, table] : edits) {
				const std::string data = table->serialized_shadow_data();
				files[fs::path(file_name)].assign(data.begin(), data.end());
			}
		} catch (const std::exception& e) {
			std::println(stderr, "Unsaved changes check: the map data cannot be written: {}", e.what());
			return std::nullopt;
		}
		return files;
	}

	/// Call when the map folder and memory agree (after a load or a successful save).
	void remember_saved_state() {
		saved_state = serialized_state();
		saved_script_inputs.reset();
		if (keeps_script) {
			try {
				saved_script_inputs = current_script_inputs();
			} catch (const std::exception& e) {
				std::println(stderr, "Kept script check: the map data cannot be written: {}", e.what());
			}
		}
	}

	/// For a change the user did not make (Players/Forces filling in missing slots and forces when it opens): runs `apply`, and what
	/// was unchanged before still counts as unchanged afterwards, both for the unsaved changes check and for the kept-script warning.
	/// Edits made before stay pending in each, so only a load or a save (remember_saved_state) clears them.
	void apply_as_unchanged(const std::function<void()>& apply) {
		const bool files_saved = !differs_from_saved_state();
		const bool script_inputs_saved = keeps_script && changes_the_kept_script_misses().empty();
		apply();
		if (files_saved) {
			saved_state = serialized_state();
		}
		if (script_inputs_saved) {
			try {
				saved_script_inputs = current_script_inputs();
			} catch (const std::exception& e) {
				std::println(stderr, "Kept script check: the map data cannot be written: {}", e.what());
				saved_script_inputs.reset();
			}
		}
	}

	/// script_inputs() for the map in memory, through the real writers. Throws when a writer does.
	[[nodiscard]]
	MapFiles current_script_inputs() const {
		MapFiles writes;
		{
			const MapWriteCapture capture(writes);
			units.save(terrain);
			regions.save(terrain.offset.x, terrain.offset.y);
		}
		return script_inputs(writes, info);
	}

	/// For a kept script: what changed since the last save that the script creates or sets up, so the game will not show it.
	/// Empty when nothing did. Says "the map data" when it cannot tell.
	[[nodiscard]]
	std::vector<std::string> changes_the_kept_script_misses() const {
		try {
			if (!saved_script_inputs) {
				return { "the map data" };
			}
			return changed_script_inputs(*saved_script_inputs, current_script_inputs());
		} catch (const std::exception&) {
			return { "the map data" };
		}
	}

	/// True when save() would now write something other than at the last remember_saved_state(). Also true when it cannot tell.
	[[nodiscard]]
	bool differs_from_saved_state() const {
		const auto current = serialized_state();
		return !current || !saved_state || *current != *saved_state;
	}

	bool save(const fs::path& path) {
		Timer timer;
		if (!ensure_unit_ids_fit()) {
			return false;
		}
		if (!ensure_object_data_serializes()) {
			return false;
		}

		if (!fs::equivalent(path, filesystem_path)) {
			try {
				fs::copy(filesystem_path, fs::absolute(path), fs::copy_options::recursive);
				// A temporary file a crashed save left in the source folder is not part of the map.
				remove_map_write_temp_files(fs::absolute(path));
			} catch (fs::filesystem_error& e) {
				if (headless) {
					std::println(stderr, "Save failed: copying the map folder: {}", e.what());
					return false;
				}
				QMessageBox msgbox;
				msgbox.setText(e.what());
				msgbox.exec();
				return false;
			}
			filesystem_path = fs::absolute(path) / "";
			const std::u8string folder_name = (*--(--filesystem_path.end())).u8string();
			name = std::string(folder_name.begin(), folder_name.end());
		}

		if (!ensure_player_start_locations()) {
			return false;
		}

		sync_player_start_locations_to_map_info();

		if (keeps_script) {
			warn_about_changes_the_kept_script_misses();
		}

		pathing_map.save();
		terrain.save();
		shadow_map.save();

		save_modification_file("war3map.w3d", doodads_slk, doodads_meta_slk, true, false);
		save_modification_file("war3mapSkin.w3d", doodads_slk, doodads_meta_slk, true, true);
		save_modification_file("war3map.w3b", destructibles_slk, destructibles_meta_slk, false, false);
		save_modification_file("war3mapSkin.w3b", destructibles_slk, destructibles_meta_slk, false, true);
		doodads.save(terrain);

		save_modification_file("war3map.w3u", units_slk, units_meta_slk, false, false);
		save_modification_file("war3mapSkin.w3u", units_slk, units_meta_slk, false, true);
		save_modification_file("war3map.w3t", items_slk, items_meta_slk, false, false);
		save_modification_file("war3mapSkin.w3t", items_slk, items_meta_slk, false, true);
		units.save(terrain);

		save_modification_file("war3map.w3a", abilities_slk, abilities_meta_slk, true, false);
		save_modification_file("war3mapSkin.w3a", abilities_slk, abilities_meta_slk, true, true);

		save_modification_file("war3map.w3h", buff_slk, buff_meta_slk, false, false);
		save_modification_file("war3mapSkin.w3h", buff_slk, buff_meta_slk, false, true);
		save_modification_file("war3map.w3q", upgrade_slk, upgrade_meta_slk, true, false);
		save_modification_file("war3mapSkin.w3q", upgrade_slk, upgrade_meta_slk, true, true);
		regions.save(terrain.offset.x, terrain.offset.y);

		info.save(terrain.tileset);
		trigger_strings.save();

		// A kept script stays byte for byte (Save As copied it with the folder), and no trigger files are written next to it.
		std::expected<void, std::string> result;
		ScriptMode mode = ScriptMode::jass;
		if (!keeps_script) {
			triggers.save();
			triggers.save_scripts();
			if (info.lua) {
				mode = ScriptMode::lua;
			}
			result = triggers.generate_map_script(terrain, units, doodads, info, sounds, regions, cameras, mode);
		}
		script_compile_failed = !result.has_value();
		if (!result.has_value() && headless) {
			std::println(stderr, "Script compilation errors:\n{}", result.error());
		} else if (!result.has_value() && mode == ScriptMode::lua) {
			// The other map files are saved either way; only war3map.lua keeps its previous version. The error already names line and region.
			const bool syntax_error = result.error().starts_with(lua_syntax_error_prefix);
			QMessageBox::warning(
				nullptr,
				"Lua script",
				QString(syntax_error ? "war3map.lua was not updated; the other map files are still saved.\n\n" : "The map script could not be written: ")
					+ QString::fromStdString(result.error()),
				QMessageBox::StandardButton::Ok
			);
		} else if (!result.has_value()) {
			QMessageBox::information(
				nullptr,
				"vJass output",
				"There were compilation errors:\n" + QString::fromStdString(result.error()),
				QMessageBox::StandardButton::Ok
			);
		}

		gameplay_constants.save();

		imports.save(filesystem_path);

		std::println("Saving took: {:>5}ms", timer.elapsed_ms());

		return true;
	}

	void update(double delta, int width, int height) {
		if (!loaded) {
			return;
		}

		camera.position.z = terrain.interpolated_height(camera.position.x, camera.position.y, true);
		camera.update(delta);

		// Update current water texture index
		terrain.current_texture += std::max(0.0, terrain.animation_rate * delta);
		if (terrain.current_texture >= terrain.water_textures_nr) {
			terrain.current_texture = 0;
		}

		/*auto current_time = std::chrono::steady_clock::now().time_since_epoch();
		auto seconds = std::chrono::duration_cast<std::chrono::milliseconds>(current_time).count() / 1000.f;
		light_direction = glm::normalize(glm::vec3(std::cos(seconds), std::sin(seconds), -2.f));*/

		// Map mouse coordinates to world coordinates
		if (input_handler.mouse != input_handler.previous_mouse) {
			glm::vec3 window = {input_handler.mouse.x, height - input_handler.mouse.y, 1.f};
			glm::vec3 pos = glm::unProject(window, camera.view, camera.projection, glm::vec4(0, 0, width, height));
			glm::vec3 origin = camera.position - camera.direction * camera.distance;
			glm::vec3 direction = glm::normalize(pos - origin);
			glm::vec3 toto = origin + direction * 2000.f;

			btVector3 from(origin.x, origin.y, origin.z);
			btVector3 to(toto.x, toto.y, toto.z);

			btCollisionWorld::ClosestRayResultCallback res(from, to);
			res.m_collisionFilterGroup = 32;
			res.m_collisionFilterMask = 32;
			physics.dynamicsWorld->rayTest(from, to, res);

			if (res.hasHit()) {
				auto& hit = res.m_hitPointWorld;
				input_handler.previous_mouse_world = input_handler.mouse_world;
				input_handler.mouse_world = glm::vec3(hit.x(), hit.y(), hit.z());
			}
		}

		// Animate units
		std::for_each(std::execution::par_unseq, units.units.begin(), units.units.end(), [&](Unit& i) {
			if (i.id == "sloc") {
				return;
			} // ToDo handle starting locations

			mdx::Extent& extent = i.mesh->mdx->sequences[i.skeleton.sequence_index].extent;
			if (!camera.inside_frustrum_transform(extent.minimum, extent.maximum, i.skeleton.matrix)) {
				return;
			}

			i.skeleton.update(delta);
		});

		// Animate items
		for (auto& i : units.items) {
			i.skeleton.update(delta);
		}

		// Animate doodads
		std::for_each(std::execution::par_unseq, doodads.doodads.begin(), doodads.doodads.end(), [&](Doodad& i) {
			mdx::Extent& extent = i.mesh->mdx->sequences[i.skeleton.sequence_index].extent;
			if (!camera.inside_frustrum_transform(extent.minimum, extent.maximum, i.skeleton.matrix)) {
				return;
			}

			i.skeleton.update(delta);
		});
	}

	void render() {
		// While switching maps it may happen that render is called before loading has finished.
		if (!loaded) {
			return;
		}

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glPolygonMode(GL_FRONT_AND_BACK, render_wireframe ? GL_LINE : GL_FILL);

		terrain.render_ground(render_pathing, render_lighting, light_direction, brush, pathing_map);

		if (render_doodads) {
			for (const auto& i : doodads.doodads) {
				render_manager.queue_render(*i.mesh, i.skeleton, i.color, 0);
				if (render_click_helpers) {
					const bool is_doodad = doodads_slk.row_headers.contains(i.id);
					const slk::SLK& slk = is_doodad ? doodads_slk : destructibles_slk;
					if (slk.data<bool>("useclickhelper", i.id)) {
						render_manager.queue_click_helper(i.skeleton.matrix);
					}
				}
			}
			for (const auto& i : doodads.special_doodads) {
				render_manager.queue_render(*i.mesh, i.skeleton, glm::vec3(1.f), 0);
			}
		}

		if (render_units) {
			for (auto& i : units.units) {
				if (i.id == "sloc") {
					auto color = RenderManager::player_color(i.player);
					color.a = 235;
					render_manager.queue_start_location_marker(i.position, color);
					continue;
				}

				render_manager.queue_render(*i.mesh, i.skeleton, i.color, i.player);
			}
			for (auto& i : units.items) {
				render_manager.queue_render(*i.mesh, i.skeleton, i.color, i.player);
			}
		}

		if (render_brush && brush) {
			brush->render();
		}

		render_manager.render(render_lighting, light_direction);
		if (render_water) {
			terrain.render_water();
		}

		// physics.dynamicsWorld->debugDrawWorld();
		// physics.draw->render();
	}

	/// Resizes the entire map by expanding/shirnking it from all sides
	/// Handles terrain, pathing map, shadow map and preplaced objects
	/// Also, as per vanilla WE behaviour, clears the entire world undo stack
	void resize(int delta_left, int delta_right, int delta_top, int delta_bottom);

	/// Sets the playable area (shadowed map bounds)
	/// Handles the terrain flags, camera bounds and map info
	/// Also updates the pathing map and deletes units/items which are now out of bounds
	void set_playable_area(int unplayable_left, int unplayable_right, int unplayable_top, int unplayable_bottom);

	std::string get_unique_id(bool first_uppercase) {
		std::random_device rd;
		std::mt19937 mt(rd());
		std::uniform_int_distribution<int> dist(0, 25);
	again:

		std::string id =
			""s + char((first_uppercase ? 'A' : 'a') + dist(mt)) + char('a' + dist(mt)) + char('a' + dist(mt)) + char('a' + dist(mt));

		if (units_slk.row_headers.contains(id) || items_slk.row_headers.contains(id) || abilities_slk.row_headers.contains(id)
			|| doodads_slk.row_headers.contains(id) || destructibles_slk.row_headers.contains(id) || upgrade_slk.row_headers.contains(id)
			|| buff_slk.row_headers.contains(id)) {
			std::print("Generated an existing ID: {} what're the odds\n", id);
			goto again;
		}

		return id;
	}

	/// Returns a list containing all the custom resources in the map folder with what refers to them. `editor_text` is text typed into
	/// open editors that the map may not hold yet. Note that this isn't exhaustive: map code can compose a path while the game runs,
	/// for example by appending two strings.
	std::vector<FileUsage> get_file_usage(std::vector<AssetReference> editor_text = {}) const;

	/// Whether a file nothing refers to is still used because the game picks it up by its path.
	[[nodiscard]]
	bool is_override_file(const std::string& path) const;

  private:
	/// One warning per save, which never stops it: the edits are saved, but the kept script does not create or set them up.
	void warn_about_changes_the_kept_script_misses() const {
		const std::vector<std::string> changed = changes_the_kept_script_misses();
		if (changed.empty()) {
			return;
		}
		std::string list;
		for (const std::string& part : changed) {
			list += (list.empty() ? "" : ", ") + part;
		}
		const std::string text = std::format(
			"This map keeps its existing script, so changes to {} since the last save are written to the map files but the script "
			"does not include them. In the game, the old script still runs.",
			list
		);
		if (headless) {
			std::println(stderr, "Warning: {}", text);
			return;
		}
		QMessageBox::warning(nullptr, "Script Not Updated", QString::fromStdString(text), QMessageBox::StandardButton::Ok);
	}

	bool player_requires_start_location(const PlayerData& player) const {
		return player.internal_number >= 0 && player.internal_number < 12
			&& (player.type == PlayerType::human || player.type == PlayerType::computer);
	}

	glm::vec3 default_player_start_position(const PlayerData& player, size_t fallback_index) const {
		glm::vec2 position = (player.starting_position - terrain.offset) / 128.f;
		const bool inside_bounds = position.x >= 0.f && position.y >= 0.f && position.x <= terrain.width - 1 && position.y <= terrain.height - 1;

		if (!inside_bounds) {
			const glm::vec2 center(terrain.width * 0.5f, terrain.height * 0.5f);
			const float radius = std::max(4.f, std::min(terrain.width, terrain.height) * 0.25f);
			const float angle =
				static_cast<float>(fallback_index) * static_cast<float>(std::numbers::pi_v<float> * 2.0) / std::max<size_t>(1, info.players.size());
			position = center + glm::vec2(std::cos(angle), std::sin(angle)) * radius;
			position.x = std::clamp(position.x, 0.f, static_cast<float>(terrain.width - 1));
			position.y = std::clamp(position.y, 0.f, static_cast<float>(terrain.height - 1));
		}

		return {position.x, position.y, terrain.interpolated_height(position.x, position.y, true)};
	}

	void sync_player_start_locations_to_map_info() {
		for (auto& player : info.players) {
			if (const Unit* start_location = units.find_start_location(player.internal_number)) {
				player.starting_position = glm::vec2(start_location->position) * 128.f + terrain.offset;
			}
		}
	}

	bool ensure_player_start_locations() {
		std::vector<const PlayerData*> missing_players;
		for (const auto& player : info.players) {
			if (!player_requires_start_location(player)) {
				continue;
			}

			if (!units.find_start_location(player.internal_number)) {
				missing_players.push_back(&player);
			}
		}

		if (missing_players.empty()) {
			return true;
		}

		QString player_names;
		for (const auto* player : missing_players) {
			const std::string name(trigger_strings.string(player->name));
			if (!player_names.isEmpty()) {
				player_names += '\n';
			}
			player_names += QString("Player %1: ").arg(player->internal_number + 1) + QString::fromStdString(name);
		}

		if (headless) {
			std::println(stderr, "Save refused: active player slots are missing start locations:\n{}", player_names.toStdString());
			return false;
		}

		QMessageBox message_box;
		message_box.setIcon(QMessageBox::Question);
		message_box.setWindowTitle("Missing Start Locations");
		message_box.setText("Some active player slots are missing start locations.");
		message_box.setInformativeText("Create missing start locations automatically?");
		message_box.setDetailedText(player_names);
		message_box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
		message_box.setDefaultButton(QMessageBox::Yes);

		if (message_box.exec() != QMessageBox::Yes) {
			return false;
		}

		for (size_t i = 0; i < missing_players.size(); ++i) {
			const PlayerData& player = *missing_players[i];
			[[maybe_unused]] Unit& start_location = units.add_start_location(player.internal_number, default_player_start_position(player, i));
		}

		return true;
	}

	int update_object_positions(int delta_left, int delta_right, int delta_top, int delta_bottom);
	void reset_map_edge_pathing(
		int old_left,
		int old_right,
		int old_top,
		int old_bottom,
		int new_left,
		int new_right,
		int new_top,
		int new_bottom
	);
};

#include "map.moc"
