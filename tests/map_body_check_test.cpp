// doctest's own forward declaration of std::char_traits breaks std::string here otherwise.
#define DOCTEST_CONFIG_USE_STD_HEADERS
#include <doctest/doctest.h>
#define __STORMLIB_NO_STATIC_LINK__
#include "StormLib.h"

import std;
import MPQ;
import Doodad;
import Doodads;
import GameCameras;
import Globals;
import Hierarchy;
import ModificationTables;
import SLK;
import StockObjectData;
import <glm/glm.hpp>;
import MapArchiveCheck;
import MapBodyCheck;
import MapFormatCheck;
import MapInfo;
import Regions;
import Terrain;
import Triggers;
import Units;
import types;
import no_init_allocator;
import BinaryReader;

namespace fs = std::filesystem;

namespace {
	constexpr std::uint32_t damaged_count = 0xFFFF4700;

	fs::path make_scratch_dir(const std::string& name) {
		const fs::path dir = fs::temp_directory_path() / "hivewe_map_body_check_test" / name;
		std::error_code error;
		fs::remove_all(dir, error);
		fs::create_directories(dir);
		return dir;
	}

	void write_all(const fs::path& path, const std::string& content) {
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		out << content;
	}

	std::string read_all(const fs::path& path) {
		std::ifstream in(path, std::ios::binary);
		return { std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
	}

	std::string le32(const std::uint32_t value) {
		std::string bytes(4, '\0');
		for (int i = 0; i < 4; i++) {
			bytes[i] = static_cast<char>((value >> (8 * i)) & 0xFF);
		}
		return bytes;
	}

	bool contains(const std::string& text, const std::string& part) {
		return text.find(part) != std::string::npos;
	}

	std::string env(const char* const name) {
#pragma warning(suppress : 4996) // getenv: read on a single-threaded test.
		const char* const value = std::getenv(name);
		return value == nullptr ? std::string() : std::string(value);
	}

	/// A file under construction that remembers where each record count went, so a test can damage each one.
	struct Bytes {
		std::string data;
		std::vector<std::size_t> counts;

		void u32(const std::uint32_t value = 0) {
			data += le32(value);
		}
		void u32s(const int amount) {
			data += std::string(4 * static_cast<std::size_t>(amount), '\0');
		}
		void count(const std::uint32_t value) {
			counts.push_back(data.size());
			u32(value);
		}
		void str(const std::string& text) {
			data += text;
			data += '\0';
		}
		void raw(const std::string& text) {
			data += text;
		}
	};

	std::string with_u32_at(std::string bytes, const std::size_t offset, const std::uint32_t value) {
		bytes.replace(offset, 4, le32(value));
		return bytes;
	}

	/// Holds one record per count, and is exactly as long as its counts say.
	Bytes w3i_file(const std::uint32_t version) {
		const bool v39 = version == 39;
		Bytes b;
		b.u32(version);
		b.u32(1); // map version
		b.u32(v39 ? 6140 : 6116); // editor
		b.u32(2);
		b.u32(0);
		b.u32(3);
		b.u32(22978);
		b.str("name");
		b.str("author");
		b.str("description");
		b.str("players");
		b.u32s(8 + 4 + 2); // cameras, complements, playable size
		b.u32(); // flags
		b.raw("L");
		b.u32(); // loading screen number
		if (v39) {
			b.u32(); // race hud
		}
		b.str("");
		b.str("");
		b.str("");
		b.str("");
		b.u32(); // game data set
		b.str("");
		b.str("");
		b.str("");
		b.str("");
		b.u32s(5); // fog
		if (v39) {
			b.u32s(6);
		}
		b.u32(); // weather
		b.str("");
		b.raw("F");
		b.u32(); // water
		b.u32(); // lua
		b.u32s(2 + 3); // modes, game data version, camera distances
		if (v39) {
			b.u32s(10);
		}

		b.count(1); // players
		b.u32s(3);
		if (v39) {
			b.u32(); // hud skin
		}
		b.u32();
		b.str("Player");
		b.u32s(2 + 2 + 2);

		b.count(1); // forces
		b.u32s(2);
		b.str("Force");

		b.count(1); // upgrades
		b.u32();
		b.raw("Rhme");
		b.u32s(2);

		b.count(1); // tech
		b.u32();
		b.raw("Rhme");

		b.count(1); // random unit tables
		b.u32();
		b.str("Table");
		b.count(1); // positions
		b.u32();
		b.count(1); // lines
		b.u32();
		b.raw("hfoo");

		b.count(1); // random item tables
		b.u32();
		b.str("Table");
		b.count(1); // item sets
		b.count(1); // items
		b.u32();
		b.raw("ratc");
		return b;
	}

	struct PlacedFile {
		Bytes bytes;
		std::size_t x_offset = 0;
		std::size_t flags_offset = 0;
		std::size_t roll_offset = 0;
		std::size_t special_version_offset = 0;
	};

	PlacedFile doo_file(const std::uint32_t version) {
		PlacedFile file;
		Bytes& b = file.bytes;
		b.raw("W3do");
		b.u32(version);
		b.u32(11);
		b.count(1); // doodads
		b.raw("LTlt");
		b.u32();
		file.x_offset = b.data.size();
		b.u32s(3 + 1 + 3); // position, angle, scale
		b.raw("LTlt"); // skin
		if (version >= 12) {
			b.u32(0xFFFFFFFF); // group id
		}
		file.flags_offset = b.data.size();
		b.raw(std::string(2, '\0')); // flags, life
		if (version >= 8) {
			b.u32(); // item table
			b.count(1); // item sets
			b.count(1); // items
			b.raw("ratc");
			b.u32();
		}
		if (version >= 13) {
			b.u32(0xFFFFFFFF); // unknown
		}
		b.u32(7); // creation number
		if (version >= 12) {
			file.roll_offset = b.data.size();
			b.u32s(2); // roll, pitch
			b.count(1); // lights
			b.raw(std::string(36, '\0'));
		}
		file.special_version_offset = b.data.size();
		b.u32(); // special format version
		b.count(1); // special doodads
		b.raw("LTlt");
		b.u32s(1 + 2);
		return file;
	}

	PlacedFile units_file(const std::uint32_t version, const std::uint32_t random_type = 2) {
		PlacedFile file;
		Bytes& b = file.bytes;
		b.raw("W3do");
		b.u32(version);
		b.u32(11);
		b.count(1); // units
		b.raw("hfoo");
		b.u32();
		file.x_offset = b.data.size();
		b.u32s(3 + 1 + 3); // position, angle, scale
		b.raw("hfoo"); // skin
		if (version >= 12) {
			b.u32(0xFFFFFFFF); // group id
		}
		b.raw(std::string(1, '\0')); // flags
		b.u32(); // player
		b.raw(std::string(2, '\0'));
		b.u32s(2); // health, mana
		b.u32(); // item table
		b.count(1); // item sets
		b.count(1); // items
		b.raw("ratc");
		b.u32();
		b.u32s(3); // gold, acquisition, level
		b.u32s(3); // strength, agility, intelligence
		b.count(1); // inventory
		b.u32();
		b.raw("ratc");
		b.count(1); // abilities
		b.raw("Ahbz");
		b.u32s(2);
		b.u32(random_type);
		if (random_type == 2) {
			b.count(1); // random entries
			b.u32s(2);
		} else if (random_type == 1) {
			b.u32s(2);
		} else if (random_type == 0) {
			b.u32();
		}
		b.u32s(2); // color, waygate
		b.u32(9); // creation number
		if (version >= 12) {
			b.u32s(3); // trailer
		}
		return file;
	}

	Bytes w3r_file(const std::uint32_t version = 5) {
		Bytes b;
		b.u32(version);
		b.count(1);
		b.u32s(4);
		b.str("Region");
		b.u32();
		b.raw("RLlr");
		b.str("");
		b.raw(std::string(3, '\0') + '\xFF'); // colour, then the alpha that save always writes
		if (version >= 7) {
			b.u32s(2); // camera blocker, alpha tile minimap colour
		}
		return b;
	}

	Bytes w3c_file(const std::uint32_t version = 0) {
		Bytes b;
		b.u32(version);
		b.count(1);
		b.u32s(10 + 3);
		if (version >= 3) {
			b.u32s(3); // depth of field distance and scale, absolute z
		}
		b.str("Camera");
		if (version >= 3) {
			b.u32(); // camera type
		}
		return b;
	}

	Bytes w3s_file(const std::string& name = "gg_snd") {
		Bytes b;
		b.u32(3);
		b.count(1);
		b.str(name);
		b.str("file.wav");
		b.str("");
		b.u32s(1 + 3 + 2 + 2 + 3 + 2 + 1 + 3);
		for (int i = 0; i < 3; i++) {
			b.str("");
		}
		for (int i = 0; i < 2; i++) {
			b.u32();
			b.str("");
		}
		b.u32();
		for (int i = 0; i < 4; i++) {
			b.str("");
		}
		b.u32();
		return b;
	}

	struct W3eFile {
		Bytes bytes;
		std::size_t width_offset = 0;
	};

	W3eFile w3e_file() {
		W3eFile file;
		Bytes& b = file.bytes;
		b.raw("W3E!");
		b.u32(12);
		b.raw("L");
		b.u32();
		b.count(1);
		b.raw("Ldrt");
		b.count(1);
		b.raw("CLdi");
		file.width_offset = b.data.size();
		b.count(2);
		b.count(2);
		b.u32s(2);
		b.raw(std::string(4 * 8, '\0'));
		return file;
	}

	Bytes wpm_file() {
		Bytes b;
		b.raw("MP3W");
		b.u32(0);
		b.count(4); // The size a terrain of 2 by 2, as in w3e_file, needs
		b.count(4);
		b.raw(std::string(16, '\0'));
		return b;
	}

	void add_test_action(Bytes& b, const bool child) {
		b.u32(2); // type
		if (child) {
			b.u32(); // group
		}
		b.str("TestAction");
		b.u32(1); // enabled
		// The one parameter TestAction has
		b.u32(3);
		b.str("value");
		b.u32(); // no sub parameter
		b.u32(); // not an array
	}

	void add_eca_with_child(Bytes& b, const std::uint32_t version) {
		add_test_action(b, false);
		if (version == 7) {
			b.count(1); // child ECAs
			add_test_action(b, true);
			b.u32(); // the child has no children
		}
	}

	Bytes wtg_131_file() {
		Bytes b;
		b.raw("WTG!");
		b.u32(0x80000004);
		b.u32(7);
		for (int i = 0; i < 7; i++) {
			b.u32();
			b.count(1); // deleted ids
			b.u32();
		}
		b.u32s(2);
		b.u32(2); // trig_def_ver
		b.count(1); // variables
		b.str("variable");
		b.str("integer");
		b.u32s(4);
		b.str("");
		b.u32(5);
		b.u32();
		b.count(3); // elements
		b.u32(4); // category
		b.u32(1);
		b.str("Category");
		b.u32s(3);
		b.u32(8); // trigger
		b.str("Trigger");
		b.str("");
		b.u32();
		b.u32(2);
		b.u32s(5);
		b.count(1); // ECAs
		add_eca_with_child(b, 7);
		b.u32(64); // variable element
		b.u32(5);
		b.str("variable");
		b.u32();
		return b;
	}

	Bytes wtg_old_file(const std::uint32_t version) {
		Bytes b;
		b.raw("WTG!");
		b.u32(version);
		b.count(1); // categories
		b.u32(1);
		b.str("Category");
		if (version == 7) {
			b.u32();
		}
		b.u32();
		b.count(1); // variables
		b.str("variable");
		b.str("integer");
		b.u32s(version == 7 ? 4 : 3);
		b.str("");
		b.count(1); // triggers
		b.str("Trigger");
		b.str("");
		if (version == 7) {
			b.u32();
		}
		b.u32s(5);
		b.count(1); // ECAs
		add_eca_with_child(b, version);
		return b;
	}

	Bytes wct_file() {
		Bytes b;
		b.u32(0x80000004);
		b.u32(1);
		b.str("comment");
		b.count(3); // global script size
		b.raw("abc");
		b.count(2); // trigger script size
		b.raw("de");
		return b;
	}

	Bytes object_file(const bool optional_ints) {
		Bytes b;
		b.u32(3);
		for (const std::string& modified : { std::string(4, '\0'), std::string("h000") }) {
			b.count(1); // objects
			b.raw("hfoo");
			b.raw(modified);
			b.u32s(2); // set count, set flag
			b.count(1); // modifications
			b.raw("umvs");
			b.u32(0); // int
			if (optional_ints) {
				b.u32s(2);
			}
			b.u32(5);
			b.u32();
		}
		return b;
	}

	std::string wtg_with_nested_parameters(const int nesting) {
		Bytes b;
		b.raw("WTG!");
		b.u32(4);
		b.u32(); // categories
		b.u32();
		b.u32(); // variables
		b.u32(1); // triggers
		b.str("Trigger");
		b.str("");
		b.u32s(5);
		b.u32(1); // ECAs
		b.u32(2);
		b.str("TestAction");
		b.u32(1);
		for (int i = 1; i <= nesting; i++) {
			b.u32(1); // type
			b.str("");
			b.u32(); // no sub parameter
			b.u32(i < nesting ? 1 : 0); // an array index parameter follows
		}
		return b.data;
	}

	std::string wtg_with_nested_ecas(const int nesting) {
		Bytes b;
		b.raw("WTG!");
		b.u32(7);
		b.u32(); // categories
		b.u32();
		b.u32(); // variables
		b.u32(1); // triggers
		b.str("Trigger");
		b.str("");
		b.u32s(6);
		b.u32(1); // ECAs
		for (int i = 0; i <= nesting; i++) {
			b.u32(2);
			if (i > 0) {
				b.u32(); // group
			}
			b.str("TestNoArgumentAction");
			b.u32(1);
			b.u32(i < nesting ? 1 : 0); // child ECAs
		}
		return b.data;
	}

	/// The minimum TriggerData for the test map's war3map.wtg and the crafted ones above.
	constexpr std::string_view trigger_data = R"([TriggerEvents]
MapInitializationEvent=0,nothing
TriggerRegisterEnterRectSimple=0,rect
[TriggerConditions]
[TriggerActions]
TestAction=0,integer
TestNoArgumentAction=0,nothing
TriggerSleepAction=0,real
MeleeStartingVisibility=0,nothing
MeleeStartingHeroLimit=0,nothing
MeleeGrantHeroItems=0,nothing
MeleeStartingResources=0,nothing
MeleeClearExcessUnits=0,nothing
MeleeStartingUnits=0,nothing
MeleeStartingAI=0,nothing
MeleeInitVictoryDefeat=0,nothing
[TriggerCalls]
TestCall=0,0,integer,integer
)";

	/// The test binary has no game data, so the map itself supplies what open_file finds there in the real app.
	void add_trigger_definitions(const fs::path& map) {
		fs::create_directories(map / "UI");
		write_all(map / "UI" / "TriggerData.txt", std::string(trigger_data));
		write_all(map / "UI" / "TriggerStrings.txt", "[TriggerActionStrings]\n");
		world_edit_strings.ini_data["WorldEditStrings"]; // Triggers::load substitutes from this.
	}

	/// A copy of the bundled test map, which is never touched itself, that can parse its own triggers.
	fs::path copy_test_map(const std::string& name) {
		const fs::path dir = make_scratch_dir(name);
		fs::copy(TEST_MAP_DIR, dir, fs::copy_options::recursive);
		add_trigger_definitions(dir);
		return dir;
	}

	/// A map in which every file holds records, so every count has something to be wrong about.
	fs::path make_crafted_map(const std::string& name) {
		const fs::path dir = make_scratch_dir(name);
		add_trigger_definitions(dir);
		write_all(dir / "war3map.w3i", w3i_file(33).data);
		write_all(dir / "war3map.w3e", w3e_file().bytes.data);
		write_all(dir / "war3map.wpm", wpm_file().data);
		write_all(dir / "war3map.doo", doo_file(8).bytes.data);
		write_all(dir / "war3mapUnits.doo", units_file(8).bytes.data);
		write_all(dir / "war3map.w3r", w3r_file().data);
		write_all(dir / "war3map.w3c", w3c_file().data);
		write_all(dir / "war3map.w3s", w3s_file().data);
		write_all(dir / "war3map.wtg", wtg_131_file().data);
		write_all(dir / "war3map.wct", wct_file().data);
		write_all(dir / "war3map.w3u", object_file(false).data);
		write_all(dir / "war3map.w3a", object_file(true).data);
		return dir;
	}

	/// Stands in for an open map: distinctive values that a check must leave exactly as they are.
	class SentinelOpenMap {
		fs::path previous_directory = hierarchy.map_directory;
		int previous_doodad = Doodad::auto_increment;
		int previous_unit = Unit::auto_increment;
		int previous_trigger = Trigger::next_id;
		char previous_tileset = hierarchy.tileset;

	  public:
		SentinelOpenMap() {
			hierarchy.map_directory = "sentinel/open map";
			Doodad::auto_increment = 1234;
			Unit::auto_increment = 2345;
			Trigger::next_id = 3456;
			hierarchy.tileset = 'Q';
		}
		~SentinelOpenMap() {
			hierarchy.map_directory = previous_directory;
			Doodad::auto_increment = previous_doodad;
			Unit::auto_increment = previous_unit;
			Trigger::next_id = previous_trigger;
			hierarchy.tileset = previous_tileset;
		}
		void check_unchanged() const {
			CHECK(hierarchy.map_directory == fs::path("sentinel/open map"));
			CHECK(Doodad::auto_increment == 1234);
			CHECK(Unit::auto_increment == 2345);
			CHECK(Trigger::next_id == 3456);
			CHECK(hierarchy.tileset == 'Q');
			CHECK(Hierarchy::map_read_override == nullptr);
		}
	};

	void check_refused(const fs::path& dir, const std::string& file, const BodyScope scope = BodyScope::full) {
		const auto result = check_map_bodies(dir, scope);
		REQUIRE_MESSAGE(!result.has_value(), file);
		CHECK_MESSAGE(contains(result.error(), file), result.error());
	}

	void check_accepted(const fs::path& dir, const BodyScope scope = BodyScope::full) {
		const auto result = check_map_bodies(dir, scope);
		CHECK_MESSAGE(result.has_value(), (result ? std::string() : result.error()));
	}

	/// Every prefix of a file refuses the map, and the whole file does not.
	void check_every_prefix_refused(const fs::path& dir, const std::string& file) {
		const std::string whole = read_all(dir / file);
		REQUIRE_FALSE(whole.empty());
		for (std::size_t size = 0; size < whole.size(); size++) {
			write_all(dir / file, whole.substr(0, size));
			CAPTURE(file);
			CAPTURE(size);
			const auto result = check_map_bodies(dir, BodyScope::without_triggers);
			REQUIRE_FALSE(result.has_value());
			CHECK(contains(result.error(), file));
		}
		write_all(dir / file, whole);
		check_accepted(dir, BodyScope::without_triggers);
	}

	/// The file as built is accepted, and damaging any one of its counts refuses the map.
	void check_every_count_refused(const fs::path& dir, const std::string& file, const Bytes& bytes) {
		write_all(dir / file, bytes.data);
		check_accepted(dir);
		REQUIRE_FALSE(bytes.counts.empty());
		for (const std::size_t offset : bytes.counts) {
			CAPTURE(file);
			CAPTURE(offset);
			write_all(dir / file, with_u32_at(bytes.data, offset, damaged_count));
			const auto result = check_map_bodies(dir);
			REQUIRE_FALSE(result.has_value());
			CHECK(contains(result.error(), file));
		}
		write_all(dir / file, bytes.data);
	}

	/// How deep the parser's recursion went for what it built, counted the way max_parse_depth counts.
	int deepest_nesting(const TriggerParameter& parameter, int depth);

	int deepest_nesting(const ECA& eca, const int depth) {
		int deepest = depth;
		for (const TriggerParameter& parameter : eca.parameters) {
			deepest = std::max(deepest, deepest_nesting(parameter, depth + 1));
		}
		for (const ECA& child : eca.ecas) {
			deepest = std::max(deepest, deepest_nesting(child, depth + 1));
		}
		return deepest;
	}

	int deepest_nesting(const TriggerParameter& parameter, const int depth) {
		int deepest = depth;
		if (parameter.has_sub_parameter) {
			for (const TriggerParameter& sub : parameter.sub_parameter.parameters) {
				deepest = std::max(deepest, deepest_nesting(sub, depth + 1));
			}
		}
		if (parameter.is_array) {
			deepest = std::max(deepest, deepest_nesting(parameter.parameters.front(), depth + 1));
		}
		return deepest;
	}

	void make_archive(const fs::path& archive, const fs::path& source_directory, const std::vector<std::string>& names) {
		HANDLE handle = nullptr;
		SFILE_CREATE_MPQ create {};
		create.cbSize = sizeof(SFILE_CREATE_MPQ);
		create.dwStreamFlags = STREAM_PROVIDER_FLAT | BASE_PROVIDER_FILE;
		create.dwFileFlags1 = MPQ_FILE_DEFAULT_INTERNAL;
		create.dwSectorSize = 0x1000;
		create.dwMaxFileCount = static_cast<DWORD>(names.size() + 2);
		REQUIRE(SFileCreateArchive2(archive.c_str(), &create, &handle));
		for (const std::string& name : names) {
			const fs::path source = source_directory / name;
			REQUIRE(SFileAddFileEx(handle, source.c_str(), name.c_str(), MPQ_FILE_COMPRESS, MPQ_COMPRESSION_ZLIB, MPQ_COMPRESSION_NEXT_SAME));
		}
		REQUIRE(SFileCloseArchive(handle));
	}

	/// Points the hierarchy's local-files lookup at TriggerData outside the map, as CASC is in the real app.
	class LocalTriggerDefinitions {
		fs::path previous_root = hierarchy.root_directory;
		bool previous_local = hierarchy.local_files;

	  public:
		explicit LocalTriggerDefinitions(const fs::path& root) {
			hierarchy.root_directory = root;
			hierarchy.local_files = true;
		}
		~LocalTriggerDefinitions() {
			hierarchy.root_directory = previous_root;
			hierarchy.local_files = previous_local;
		}
	};

	/// Folder name to map name from index.json's flat {"0000": "war3.w3mod:campaign\\..."}, with / for \\.
	std::map<std::string, std::string> read_sweep_index(const fs::path& path) {
		std::map<std::string, std::string> index;
		const std::string text = read_all(path);
		const std::regex entry("\"([0-9]{4})\": \"([^\"]*)\"");
		for (auto it = std::sregex_iterator(text.begin(), text.end(), entry); it != std::sregex_iterator(); ++it) {
			index[(*it)[1].str()] = std::regex_replace((*it)[2].str(), std::regex("\\\\\\\\"), "/");
		}
		return index;
	}

	std::uint32_t leading_u32(const fs::path& path) {
		const std::string bytes = read_all(path);
		std::uint32_t value = 0;
		if (bytes.size() >= 4) {
			std::memcpy(&value, bytes.data(), 4);
		}
		return value;
	}

	/// The doodad version a war3map.doo says it is.
	std::uint32_t doo_version(const fs::path& path) {
		const std::string bytes = read_all(path);
		std::uint32_t value = 0;
		if (bytes.size() >= 8) {
			std::memcpy(&value, bytes.data() + 4, 4);
		}
		return value;
	}

	/// A copy of a map the version check refuses, with the files in a format no parser exists for replaced or
	/// dropped, so that the check can reach the bodies of everything else in it. A war3map.doo or war3mapUnits.doo
	/// of v12 or v13 has a parser now, so it stays, and these passes read its real body.
	fs::path copy_with_readable_formats_only(const fs::path& directory) {
		const fs::path copy = make_scratch_dir("stock_readable_formats");
		for (const auto& entry : fs::directory_iterator(directory)) {
			if (entry.is_regular_file()) {
				fs::copy_file(entry.path(), copy / entry.path().filename());
			}
		}
		if (const std::string doo = read_all(directory / "war3map.doo"); doo.size() < 8 || !map_format::is_accepted(map_format::doo_versions, static_cast<std::uint8_t>(doo[4]))) {
			write_all(copy / "war3map.doo", "W3do" + le32(8) + le32(11) + le32(0) + le32(0) + le32(0));
		}
		if (const std::string units = read_all(directory / "war3mapUnits.doo"); units.size() < 8 || !map_format::is_accepted(map_format::units_versions, static_cast<std::uint8_t>(units[4]))) {
			fs::remove(copy / "war3mapUnits.doo");
		}
		if (!map_format::is_accepted(map_format::w3r_versions, leading_u32(directory / "war3map.w3r"))) {
			fs::remove(copy / "war3map.w3r");
		}
		if (!map_format::is_accepted(map_format::w3c_versions, leading_u32(directory / "war3map.w3c"))) {
			fs::remove(copy / "war3map.w3c");
		}
		return copy;
	}

	void sweep_stock_maps(const fs::path& root, const std::string_view label) {
		const std::map<std::string, std::string> index = read_sweep_index(root / "index.json");
		REQUIRE_FALSE(index.empty());

		std::map<std::string, int> refusals_by_kind;
		std::vector<std::string> unexpected;
		std::size_t accepted = 0;
		std::size_t refused = 0;
		std::size_t readable_parts_passed = 0;
		std::size_t readable_parts_with_real_doo = 0;
		std::size_t readable_parts_with_real_units = 0;
		std::size_t refusals_naming_units = 0;
		int deepest = 0;
		std::string deepest_map;
		const int trigger_next_id = Trigger::next_id;

		for (const auto& [key, name] : index) {
			const fs::path directory = root / key;
			if (const auto result = check_map_for_opening(directory); result) {
				accepted++;
			} else {
				refused++;
				refusals_naming_units += contains(result.error(), "war3mapUnits.doo") ? 1 : 0;
				const bool version_refusal = contains(result.error(), "does not support") || contains(result.error(), "newer version of HiveWE");
				const bool known_trigger_refusal = contains(name, "reforged/roc/undead02") && contains(result.error(), "war3map.wtg");
				refusals_by_kind[version_refusal ? "version" : known_trigger_refusal ? "undead02 wtg" : "other"]++;
				std::string one_line = result.error();
				std::ranges::replace(one_line, '\n', ' ');
				std::println("{} refused {} ({}): {}", label, key, name, one_line);
				if (!version_refusal && !known_trigger_refusal) {
					unexpected.push_back(name);
				}

				if (version_refusal) {
					const fs::path readable_copy = copy_with_readable_formats_only(directory);
					if (const auto readable = check_map_bodies(readable_copy); readable) {
						readable_parts_passed++;
						// From the copy, since that is what was checked: a stub in it would not be a real doo at all.
						const std::uint32_t real_doo = doo_version(directory / "war3map.doo");
						if ((real_doo == 12 || real_doo == 13) && read_all(readable_copy / "war3map.doo") == read_all(directory / "war3map.doo")) {
							readable_parts_with_real_doo++;
						} else {
							unexpected.push_back(name);
						}
						const std::uint32_t real_units = doo_version(directory / "war3mapUnits.doo");
						if ((real_units == 12 || real_units == 13) && read_all(readable_copy / "war3mapUnits.doo") == read_all(directory / "war3mapUnits.doo")) {
							readable_parts_with_real_units++;
						} else {
							unexpected.push_back(name);
						}
					} else {
						refusals_naming_units += contains(readable.error(), "war3mapUnits.doo") ? 1 : 0;
						std::string readable_line = readable.error();
						std::ranges::replace(readable_line, '\n', ' ');
						std::println("{} readable parts of {} ({}) refused: {}", label, key, name, readable_line);
						const bool known = contains(name, "reforged/roc/undead02") && contains(readable.error(), "war3map.wtg");
						refusals_by_kind[known ? "undead02 wtg, behind a version refusal" : "other, behind a version refusal"]++;
						if (!known) {
							unexpected.push_back(name);
						}
					}
				}
			}

			// Measured on what the real parser built, separately from the check.
			if (fs::exists(directory / "war3map.wtg")) {
				Hierarchy::map_read_override = &directory;
				Triggers triggers;
				try {
					triggers.load();
				} catch (const std::exception&) {
				}
				Hierarchy::map_read_override = nullptr;
				Trigger::next_id = trigger_next_id;
				for (const Trigger& trigger : triggers.triggers) {
					for (const ECA& eca : trigger.ecas) {
						if (const int depth = deepest_nesting(eca, 0); depth > deepest) {
							deepest = depth;
							deepest_map = name;
						}
					}
				}
			}
		}

		std::println("{}: stock maps checked: {}, accepted {}, refused {}", label, index.size(), accepted, refused);
		std::println("{}: version refusals whose readable parts passed the body check: {}", label, readable_parts_passed);
		std::println("{}: of those, passes that parsed a real war3map.doo v12 or v13: {}", label, readable_parts_with_real_doo);
		std::println("{}: of those, passes that parsed a real war3mapUnits.doo v12 or v13: {}", label, readable_parts_with_real_units);
		std::println("{}: refusals naming war3mapUnits.doo: {}", label, refusals_naming_units);
		for (const auto& [kind, amount] : refusals_by_kind) {
			std::println("{}: refused as {}: {}", label, kind, amount);
		}
		std::println("{}: deepest trigger nesting {} in {}", label, deepest, deepest_map);
		CHECK_MESSAGE(unexpected.empty(), std::format("{} unexpected refusals", unexpected.size()));
		CHECK(deepest <= Triggers::max_parse_depth);

		// The stock maps of 3.0.1, so that an extraction that lost maps or a stubbed doo cannot pass unnoticed.
		CHECK(index.size() == 466);
		// With doo, units, w3r v7 and w3c v3 read, no stock map is refused for a version any more, so the
		// readable-parts pass has nothing to do and undead02's trigger refusal is reached directly.
		CHECK(accepted == 465);
		CHECK(refused == 1);
		CHECK(refusals_by_kind["version"] == 0);
		CHECK(refusals_by_kind["undead02 wtg"] == 1);
		CHECK(readable_parts_passed == 0);
		CHECK(readable_parts_with_real_doo == 0);
		CHECK(readable_parts_with_real_units == 0);
		CHECK(refusals_naming_units == 0);
		CHECK(refusals_by_kind["undead02 wtg, behind a version refusal"] == 0);
	}
}

TEST_CASE("a unit count above INT_MAX is refused and not read as no units") {
	const fs::path dir = make_scratch_dir("units_int_max");
	write_all(dir / "war3mapUnits.doo", "W3do" + le32(8) + le32(11) + le32(damaged_count));
	const fs::path previous = hierarchy.map_directory;
	hierarchy.map_directory = dir;
	static const Terrain* terrain = new Terrain(); // Never destroyed: its destructor calls GL.
	MapInfo info {};
	Units units;
	CHECK_THROWS(units.load(*terrain, info));
	hierarchy.map_directory = previous;
}

TEST_CASE("a v33 count that cannot fit is refused before anything is allocated") {
	const fs::path dir = make_scratch_dir("w3i_v33_count");
	const Bytes w3i = w3i_file(33);
	write_all(dir / "war3map.w3i", with_u32_at(w3i.data, w3i.counts[0], damaged_count));
	const fs::path previous = hierarchy.map_directory;
	hierarchy.map_directory = dir;
	MapInfo info {};
	CHECK_THROWS_AS(info.load(), std::runtime_error);
	hierarchy.map_directory = previous;
}

TEST_CASE("BinaryReader bounds") {
	const auto reader_of = [](const std::string& text) {
		std::vector<u8, default_init_allocator<u8>> buffer(text.size());
		std::memcpy(buffer.data(), text.data(), text.size());
		return BinaryReader(std::move(buffer));
	};

	SUBCASE("read_vector is not fooled by a size that wraps") {
		BinaryReader reader = reader_of(std::string(8, 'x'));
		CHECK_THROWS_AS((void)reader.read_vector<std::uint32_t>((std::numeric_limits<std::size_t>::max() / 4) + 2), std::out_of_range);
		CHECK_THROWS_AS((void)reader.read_vector<std::uint8_t>(std::numeric_limits<std::size_t>::max() - 2), std::out_of_range);
		CHECK(reader.read_vector<std::uint32_t>(2).size() == 2);
	}

	SUBCASE("advance_c_string stops at the end of the buffer") {
		BinaryReader unterminated = reader_of("abcd");
		CHECK_THROWS_AS(unterminated.advance_c_string(), std::out_of_range);
		CHECK(unterminated.position == 0);

		BinaryReader at_end = reader_of("a");
		at_end.advance(1);
		CHECK_THROWS_AS(at_end.advance_c_string(), std::out_of_range);

		BinaryReader terminated = reader_of(std::string("ab\0c\0", 5));
		terminated.advance_c_string();
		CHECK(terminated.position == 3);
		terminated.advance_c_string();
		CHECK(terminated.position == 5);
	}

	SUBCASE("read_count takes what fits and refuses what does not") {
		BinaryReader fits = reader_of(le32(2) + std::string(8, 'x'));
		CHECK(fits.read_count(4) == 2);

		BinaryReader too_many = reader_of(le32(3) + std::string(8, 'x'));
		CHECK_THROWS_AS((void)too_many.read_count(4), std::out_of_range);

		BinaryReader damaged = reader_of(le32(damaged_count));
		CHECK_THROWS_AS((void)damaged.read_count(1), std::out_of_range);
	}
}

TEST_CASE("a copy of the test map passes and the check changes nothing") {
	const fs::path dir = copy_test_map("control");
	const SentinelOpenMap open_map;

	std::map<std::string, std::string> before;
	for (const auto& entry : fs::recursive_directory_iterator(dir)) {
		if (entry.is_regular_file()) {
			before[entry.path().string()] = read_all(entry.path());
		}
	}

	check_accepted(dir);
	open_map.check_unchanged();

	const auto combined = check_map_for_opening(dir);
	CHECK_MESSAGE(combined.has_value(), (combined ? std::string() : combined.error()));
	open_map.check_unchanged();

	for (const auto& [path, content] : before) {
		CHECK(read_all(path) == content);
	}
	std::size_t files = 0;
	for (const auto& entry : fs::recursive_directory_iterator(dir)) {
		files += entry.is_regular_file() ? 1 : 0;
	}
	CHECK(files == before.size());
}

TEST_CASE("the test TriggerData parses the test map's triggers for real") {
	const fs::path dir = copy_test_map("control_triggers");
	const SentinelOpenMap open_map;
	Hierarchy::map_read_override = &dir;
	Triggers triggers;
	triggers.load();
	Hierarchy::map_read_override = nullptr;
	REQUIRE(triggers.triggers.size() == 1);
	CHECK(triggers.triggers[0].ecas.size() == 11);
	CHECK(triggers.triggers[0].ecas[0].name == "TriggerSleepAction");
	CHECK(triggers.triggers[0].ecas[10].name == "MeleeInitVictoryDefeat");
}

TEST_CASE("a refusal changes nothing whichever file it stops at") {
	const SentinelOpenMap open_map;
	const std::vector<std::string> files = { "war3map.wtg", "war3map.wct", "war3map.w3i", "war3map.w3e", "war3map.w3u", "war3map.doo",
											 "war3mapUnits.doo", "war3map.w3a", "war3map.w3r", "war3map.w3c", "war3map.w3s" };
	for (const std::string& file : files) {
		CAPTURE(file);
		const fs::path dir = make_crafted_map("refusal_state");
		const std::string whole = read_all(dir / file);
		write_all(dir / file, whole.substr(0, whole.size() - 1));
		check_refused(dir, file);
		open_map.check_unchanged();
	}
}

TEST_CASE("the readers really do bump the statics so the state checks above mean something") {
	const fs::path dir = make_crafted_map("statics_bump");
	const SentinelOpenMap open_map;

	const std::string bytes = read_all(dir / "war3mapUnits.doo");
	std::vector<u8, default_init_allocator<u8>> buffer(bytes.size());
	std::memcpy(buffer.data(), bytes.data(), bytes.size());
	BinaryReader reader(std::move(buffer));
	MapInfo info {};
	info.game_version_major = 2;
	int records = 0;
	std::uint32_t version = 0;
	std::uint32_t subversion = 0;
	Units::read_records(reader, {}, info, version, subversion, [&](Unit&) { records++; });
	CHECK(records == 1);
	CHECK(version == 8);
	CHECK(subversion == 11);
	CHECK(Unit::auto_increment != 2345);

	Hierarchy::map_read_override = &dir;
	Triggers triggers;
	triggers.load();
	Hierarchy::map_read_override = nullptr;
	CHECK(Trigger::next_id != 3456);
}

TEST_CASE("v39 maps with damaged bodies pass the version check and are refused here") {
	const std::string valid = w3i_file(39).data;
	const std::array<std::pair<std::string, std::string>, 3> damaged = { {
		{ "12 bytes", le32(39) + le32(1) + le32(6140) },
		{ "one trailing NUL", valid + std::string(1, '\0') },
		{ "huge player count", with_u32_at(valid, w3i_file(39).counts[0], damaged_count) },
	} };

	for (const std::uint32_t doo : { 7u, 8u }) {
		for (const std::uint32_t units : { 7u, 8u }) {
			for (const auto& [label, bytes] : damaged) {
				CAPTURE(label);
				CAPTURE(doo);
				CAPTURE(units);
				const fs::path dir = make_crafted_map("v39_damaged");
				write_all(dir / "war3map.doo", doo_file(doo).bytes.data);
				write_all(dir / "war3mapUnits.doo", units_file(units).bytes.data);
				write_all(dir / "war3map.w3i", bytes);
				CHECK(check_map_format_versions(dir).has_value());
				const SentinelOpenMap open_map;
				const auto result = check_map_for_opening(dir);
				REQUIRE_FALSE(result.has_value());
				CHECK(contains(result.error(), "war3map.w3i"));
				open_map.check_unchanged();
			}
		}
	}

	// And the valid one is not refused, so the three above were refused for what is wrong with them.
	const fs::path dir = make_crafted_map("v39_valid");
	write_all(dir / "war3map.w3i", valid);
	check_accepted(dir);
}

TEST_CASE("a damaged count is refused in every file") {
	const fs::path dir = make_crafted_map("counts");

	SUBCASE("war3map.w3i v33") {
		check_every_count_refused(dir, "war3map.w3i", w3i_file(33));
	}
	SUBCASE("war3map.w3i v39") {
		check_every_count_refused(dir, "war3map.w3i", w3i_file(39));
	}
	SUBCASE("war3map.w3e") {
		check_every_count_refused(dir, "war3map.w3e", w3e_file().bytes);
	}
	SUBCASE("war3map.wpm") {
		check_every_count_refused(dir, "war3map.wpm", wpm_file());
	}
	SUBCASE("war3map.doo") {
		check_every_count_refused(dir, "war3map.doo", doo_file(7).bytes);
		check_every_count_refused(dir, "war3map.doo", doo_file(8).bytes);
		check_every_count_refused(dir, "war3map.doo", doo_file(12).bytes);
		check_every_count_refused(dir, "war3map.doo", doo_file(13).bytes);
	}
	SUBCASE("war3mapUnits.doo") {
		check_every_count_refused(dir, "war3mapUnits.doo", units_file(7).bytes);
		check_every_count_refused(dir, "war3mapUnits.doo", units_file(8).bytes);
	}
	SUBCASE("war3map.w3r") {
		check_every_count_refused(dir, "war3map.w3r", w3r_file());
		check_every_count_refused(dir, "war3map.w3r", w3r_file(7));
	}
	SUBCASE("war3map.w3c") {
		check_every_count_refused(dir, "war3map.w3c", w3c_file());
		check_every_count_refused(dir, "war3map.w3c", w3c_file(3));
	}
	SUBCASE("war3map.w3s") {
		check_every_count_refused(dir, "war3map.w3s", w3s_file());
	}
	SUBCASE("war3map.wtg") {
		check_every_count_refused(dir, "war3map.wtg", wtg_131_file());
	}
	SUBCASE("war3map.wtg before 1.31") {
		fs::remove(dir / "war3map.wct"); // Written for the 1.31 trigger, and not what is under test.
		check_every_count_refused(dir, "war3map.wtg", wtg_old_file(4));
		check_every_count_refused(dir, "war3map.wtg", wtg_old_file(7));
	}
	SUBCASE("war3map.wct") {
		check_every_count_refused(dir, "war3map.wct", wct_file());
	}
	SUBCASE("object data") {
		check_every_count_refused(dir, "war3map.w3u", object_file(false));
		check_every_count_refused(dir, "war3map.w3a", object_file(true));
	}
}

TEST_CASE("object files use the optional ints their kind has") {
	const fs::path dir = make_crafted_map("optional_ints");
	for (const std::string file : { "war3map.w3u", "war3map.w3t", "war3map.w3b", "war3map.w3h", "war3mapSkin.w3u" }) {
		CAPTURE(file);
		write_all(dir / file, object_file(false).data);
		check_accepted(dir);
		write_all(dir / file, object_file(true).data);
		check_refused(dir, file);
		fs::remove(dir / file);
	}
	for (const std::string file : { "war3map.w3d", "war3map.w3a", "war3map.w3q", "war3mapSkin.w3a" }) {
		CAPTURE(file);
		write_all(dir / file, object_file(true).data);
		check_accepted(dir);
		write_all(dir / file, object_file(false).data);
		check_refused(dir, file);
		fs::remove(dir / file);
	}
}

TEST_CASE("every prefix of the test map's files is refused") {
	const fs::path dir = copy_test_map("prefixes");
	for (const std::string file : { "war3map.doo", "war3mapUnits.doo", "war3map.w3r", "war3map.w3c", "war3map.w3u", "war3map.w3a", "war3map.w3b" }) {
		check_every_prefix_refused(dir, file);
	}
}

TEST_CASE("a truncated or malformed war3map.w3e is refused") {
	const fs::path dir = copy_test_map("w3e");
	const std::string whole = read_all(dir / "war3map.w3e");

	for (const std::size_t size : { std::size_t { 0 }, std::size_t { 8 }, std::size_t { 60 }, whole.size() / 2, whole.size() - 1 }) {
		CAPTURE(size);
		write_all(dir / "war3map.w3e", whole.substr(0, size));
		check_refused(dir, "war3map.w3e");
	}

	const std::size_t width_offset = w3e_file().width_offset;
	const std::string crafted = w3e_file().bytes.data;
	write_all(dir / "war3map.w3e", crafted);
	check_accepted(dir);
	for (const std::uint32_t too_small : { 0u, 1u }) {
		write_all(dir / "war3map.w3e", with_u32_at(crafted, width_offset, too_small));
		check_refused(dir, "war3map.w3e");
		write_all(dir / "war3map.w3e", with_u32_at(crafted, width_offset + 4, too_small));
		check_refused(dir, "war3map.w3e");
	}
}

TEST_CASE("a terrain offset that is not finite is refused") {
	const fs::path dir = make_crafted_map("w3e_offset");
	const std::size_t offset_x = w3e_file().width_offset + 8;
	const std::string crafted = w3e_file().bytes.data;

	for (const float value : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() }) {
		for (const std::size_t offset : { offset_x, offset_x + 4 }) {
			CAPTURE(offset);
			std::string bytes = crafted;
			std::memcpy(bytes.data() + offset, &value, sizeof(value));
			write_all(dir / "war3map.w3e", bytes);
			check_refused(dir, "war3map.w3e");
		}
	}

	std::string finite = crafted;
	const float huge = std::numeric_limits<float>::max();
	std::memcpy(finite.data() + offset_x, &huge, sizeof(huge));
	write_all(dir / "war3map.w3e", finite);
	check_accepted(dir);
}

namespace {
	struct W3eCorner {
		std::uint8_t ground = 0;
		std::uint8_t cliff = 0;
		std::uint8_t layer = 2;
		bool ramp = false;
		bool blight = false;
	};

	std::string w3e_with_corners(
		const std::uint32_t version,
		const std::uint32_t ground_table,
		const std::uint32_t cliff_table,
		const std::uint32_t width,
		const std::vector<W3eCorner>& corners
	) {
		Bytes b;
		b.raw("W3E!");
		b.u32(version);
		b.raw("L");
		b.u32();
		b.u32(ground_table);
		for (std::uint32_t i = 0; i < ground_table; i++) {
			b.raw("Ldrt");
		}
		b.u32(cliff_table);
		for (std::uint32_t i = 0; i < cliff_table; i++) {
			b.raw("CLdi");
		}
		b.u32(width);
		b.u32(static_cast<std::uint32_t>(corners.size()) / width);
		b.u32s(2);
		for (const W3eCorner& corner : corners) {
			b.raw(std::string(4, '\0')); // height, water
			if (version >= 12) {
				const std::uint16_t flags = corner.ground | (corner.ramp ? 0x40 : 0) | (corner.blight ? 0x80 : 0);
				b.data += static_cast<char>(flags & 0xFF);
				b.data += static_cast<char>(flags >> 8);
			} else {
				b.data += static_cast<char>(corner.ground | (corner.ramp ? 0x10 : 0) | (corner.blight ? 0x20 : 0));
			}
			b.data += '\0'; // variation
			b.data += static_cast<char>(corner.cliff << 4 | corner.layer);
		}
		return b.data;
	}

	/// A 3 by 3 terrain whose bottom-left corner is a cliff, which makes corners 0, 1, 3 and 4 use the cliff table.
	std::vector<W3eCorner> corners_with_cliff() {
		std::vector<W3eCorner> corners(9);
		corners[0].layer = 3;
		return corners;
	}

	/// Where Terrain::real_tile_texture sends the corner decides which table it indexes.
	void check_w3e_verdict(const fs::path& dir, const std::string& bytes, const bool accepted) {
		fs::remove(dir / "war3map.wpm"); // Sized for another terrain, and not what is under test.
		write_all(dir / "war3map.w3e", bytes);
		if (accepted) {
			check_accepted(dir);
		} else {
			check_refused(dir, "war3map.w3e");
		}
	}
}

TEST_CASE("a ground texture index is refused only where the load can index past the table") {
	const fs::path dir = make_crafted_map("w3e_ground");
	std::vector<W3eCorner> corners(9);

	SUBCASE("the table has a slot for blight after the ground textures") {
		corners[8].ground = 2;
		check_w3e_verdict(dir, w3e_with_corners(12, 3, 1, 3, corners), true); // Last ground texture
		corners[8].ground = 3;
		check_w3e_verdict(dir, w3e_with_corners(12, 3, 1, 3, corners), true); // The blight slot
		corners[8].ground = 4;
		check_w3e_verdict(dir, w3e_with_corners(12, 3, 1, 3, corners), false);
		corners[8].ground = 63;
		check_w3e_verdict(dir, w3e_with_corners(12, 3, 1, 3, corners), false);
	}
	SUBCASE("in the older corner layout too") {
		corners[8].ground = 3;
		check_w3e_verdict(dir, w3e_with_corners(11, 3, 1, 3, corners), true);
		corners[8].ground = 4;
		check_w3e_verdict(dir, w3e_with_corners(11, 3, 1, 3, corners), false);
	}
	SUBCASE("a blighted corner uses the blight texture and never its own index") {
		corners[8].ground = 63;
		corners[8].blight = true;
		check_w3e_verdict(dir, w3e_with_corners(12, 3, 1, 3, corners), true);
	}
	SUBCASE("a corner next to a cliff uses the cliff table and never its own index") {
		corners = corners_with_cliff();
		corners[4].ground = 63;
		check_w3e_verdict(dir, w3e_with_corners(12, 3, 1, 3, corners), true);
	}
	SUBCASE("a ramp corner next to a cliff does use its own index") {
		corners = corners_with_cliff();
		corners[4].ground = 63;
		corners[4].ramp = true;
		check_w3e_verdict(dir, w3e_with_corners(12, 3, 1, 3, corners), false);
	}
}

TEST_CASE("a cliff texture index is refused only where the load can index past the table") {
	const fs::path dir = make_crafted_map("w3e_cliff");
	std::vector<W3eCorner> corners = corners_with_cliff();

	SUBCASE("the last valid index and the first invalid one") {
		corners[4].cliff = 1;
		check_w3e_verdict(dir, w3e_with_corners(12, 3, 2, 3, corners), true);
		corners[4].cliff = 2;
		check_w3e_verdict(dir, w3e_with_corners(12, 3, 2, 3, corners), false);
		corners[4].cliff = 14;
		check_w3e_verdict(dir, w3e_with_corners(12, 3, 2, 3, corners), false);
	}
	SUBCASE("15 means 1 so it needs a table of two") {
		corners[4].cliff = 15;
		check_w3e_verdict(dir, w3e_with_corners(12, 3, 2, 3, corners), true);
		check_w3e_verdict(dir, w3e_with_corners(12, 3, 1, 3, corners), false);
	}
	SUBCASE("a corner away from any cliff never reads its index") {
		corners[8].cliff = 14;
		check_w3e_verdict(dir, w3e_with_corners(12, 3, 1, 3, corners), true);
	}
	SUBCASE("a ramp corner next to a cliff never reads its index") {
		corners[4].cliff = 14;
		corners[4].ramp = true;
		check_w3e_verdict(dir, w3e_with_corners(12, 3, 1, 3, corners), true);
	}
	SUBCASE("an empty table cannot serve a cliff") {
		check_w3e_verdict(dir, w3e_with_corners(12, 3, 0, 3, corners), false);
		std::vector<W3eCorner> flat(9);
		check_w3e_verdict(dir, w3e_with_corners(12, 3, 0, 3, flat), true);
	}
}

TEST_CASE("a corner under a ramp model uses the cliff table only when that model exists") {
	const fs::path dir = make_crafted_map("w3e_romp");
	const fs::path models = dir / "doodads" / "terrain" / "clifftrans";
	fs::create_directories(models);

	const auto check_ramp = [&](const std::vector<W3eCorner>& corners, const std::string& model) {
		CAPTURE(model);
		check_w3e_verdict(dir, w3e_with_corners(12, 3, 1, 3, corners), true);
		write_all(models / ("clifftrans" + model + "0.mdx"), "x");
		check_w3e_verdict(dir, w3e_with_corners(12, 3, 1, 3, corners), false);
		fs::remove(models / ("clifftrans" + model + "0.mdx"));
	};

	SUBCASE("a vertical ramp") {
		std::vector<W3eCorner> corners(9);
		corners[0].ramp = corners[3].ramp = corners[6].ramp = true; // The left column
		corners[0].cliff = 14;
		check_ramp(corners, "LAAL");
	}
	SUBCASE("a horizontal ramp") {
		std::vector<W3eCorner> corners(9);
		corners[0].ramp = corners[1].ramp = corners[2].ramp = true; // The bottom row
		corners[0].cliff = 14;
		check_ramp(corners, "AALL");
	}
}

TEST_CASE("texture indices wait for the full scope since an archive scratch folder lacks the map's ramp models") {
	const fs::path dir = make_crafted_map("w3e_scope");
	fs::remove(dir / "war3map.wpm");

	std::vector<W3eCorner> corners(9);
	corners[8].ground = 63;
	write_all(dir / "war3map.w3e", w3e_with_corners(12, 3, 1, 3, corners));
	check_accepted(dir, BodyScope::without_triggers);
	check_refused(dir, "war3map.w3e", BodyScope::full);

	corners = corners_with_cliff();
	corners[4].cliff = 14;
	write_all(dir / "war3map.w3e", w3e_with_corners(12, 3, 1, 3, corners));
	check_accepted(dir, BodyScope::without_triggers);
	check_refused(dir, "war3map.w3e", BodyScope::full);

	// The checks that need no model hold in both scopes.
	const std::string crafted = w3e_file().bytes.data;
	const float nan = std::numeric_limits<float>::quiet_NaN();
	std::string bad_offset = crafted;
	std::memcpy(bad_offset.data() + w3e_file().width_offset + 8, &nan, sizeof(nan));
	write_all(dir / "war3map.w3e", bad_offset);
	check_refused(dir, "war3map.w3e", BodyScope::without_triggers);

	write_all(dir / "war3map.w3e", crafted.substr(0, crafted.size() - 1));
	check_refused(dir, "war3map.w3e", BodyScope::without_triggers);

	write_all(dir / "war3map.w3e", crafted);
	Bytes small_wpm;
	small_wpm.raw("MP3W");
	small_wpm.u32(0);
	small_wpm.u32(3);
	small_wpm.u32(4);
	small_wpm.raw(std::string(12, '\0'));
	write_all(dir / "war3map.wpm", small_wpm.data);
	check_refused(dir, "war3map.wpm", BodyScope::without_triggers);
}

TEST_CASE("get_terrain_pathing gives no tile pathing for a ground index past the tile table") {
	constexpr std::uint8_t unwalkable = 0b00000010;
	constexpr std::uint8_t unbuildable = 0b00001000;
	constexpr std::uint8_t blight = 0b00100000;
	constexpr std::uint8_t unfloatable = 0b01000000;

	static Terrain* const terrain = new Terrain(); // Never destroyed: its destructor calls GL.
	terrain->width = 3;
	terrain->height = 3;
	terrain->resize_corner_arrays(9);
	terrain->tileset_ids = { "Ldrt", "Ldro" };
	terrain->pathing_options.clear();
	terrain->pathing_options["Ldrt"].unwalkable = true;
	terrain->pathing_options["Ldrt"].unbuildable = true;

	const auto cell_mask = [&](const std::uint8_t ground, const bool tile_pathing = true) {
		terrain->corner_ground_texture[0] = ground;
		return terrain->get_terrain_pathing(0, 0, tile_pathing, false, false);
	};

	SUBCASE("valid indices use their tile as before") {
		CHECK(cell_mask(0) == (unwalkable | unbuildable | unfloatable));
		CHECK(cell_mask(1) == unfloatable);
		CHECK(terrain->pathing_options.contains("Ldro")); // A tile without options still gets its default entry.
	}
	SUBCASE("at and above the count the result is that of no tile pathing and no entry appears") {
		const std::uint8_t no_tile_pathing = cell_mask(0, false);
		CHECK(no_tile_pathing == unfloatable);
		for (const std::uint8_t ground : { std::uint8_t { 2 }, std::uint8_t { 3 }, std::uint8_t { 63 }, std::uint8_t { 255 } }) {
			CAPTURE(int { ground });
			CHECK(cell_mask(ground) == no_tile_pathing);
		}
		CHECK(terrain->pathing_options.size() == 1);
	}
	SUBCASE("a blighted corner and a cliff corner are bounded too") {
		terrain->corner_blight[0] = true;
		CHECK(cell_mask(2) == (blight | unfloatable));
		CHECK(cell_mask(63) == (blight | unfloatable));
		terrain->corner_blight[0] = false;

		terrain->corner_cliff[0] = true;
		terrain->corner_ground_texture[0] = 2;
		const std::uint8_t cliff_mask = terrain->get_terrain_pathing(0, 0, true, true, false);
		CHECK(cliff_mask == terrain->get_terrain_pathing(0, 0, false, true, false));
		terrain->corner_cliff[0] = false;
		CHECK(terrain->pathing_options.size() == 1);
	}
}

TEST_CASE("dimensions whose product wraps do not pass as fitting") {
	const fs::path dir = make_crafted_map("wrapping_dimensions");

	Bytes wpm;
	wpm.raw("MP3W");
	wpm.u32(0);
	wpm.u32(0x10000);
	wpm.u32(0x10000);
	write_all(dir / "war3map.wpm", wpm.data);
	check_refused(dir, "war3map.wpm");
	write_all(dir / "war3map.wpm", wpm_file().data);

	const std::size_t width_offset = w3e_file().width_offset;
	const std::string w3e = with_u32_at(w3e_file().bytes.data, width_offset, 0xFFFFFFFF);
	write_all(dir / "war3map.w3e", with_u32_at(w3e, width_offset + 4, 0xFFFFFFFF));
	check_refused(dir, "war3map.w3e");
}

TEST_CASE("a war3map.wpm smaller than the terrain needs is refused") {
	const fs::path dir = make_crafted_map("wpm_size"); // A terrain of 2 by 2, so 4 by 4 pathing cells.
	const auto with_size = [](const std::uint32_t width, const std::uint32_t height) {
		Bytes wpm;
		wpm.raw("MP3W");
		wpm.u32(0);
		wpm.u32(width);
		wpm.u32(height);
		wpm.raw(std::string(std::size_t { width } * height, '\0'));
		return wpm.data;
	};
	const auto check_size = [&](const std::uint32_t width, const std::uint32_t height, const bool accepted) {
		CAPTURE(width);
		CAPTURE(height);
		write_all(dir / "war3map.wpm", with_size(width, height));
		if (accepted) {
			check_accepted(dir);
		} else {
			check_refused(dir, "war3map.wpm");
		}
	};

	check_size(3, 4, false);
	check_size(4, 3, false);
	check_size(1, 1, false);
	check_size(8, 3, false); // Large enough in total, not in height.
	check_size(4, 4, true); // Exactly (terrain - 1) * 4
	check_size(8, 8, true); // terrain * 4, which HiveWE saves for a map that had no war3map.wpm
	check_size(5, 9, true);
	check_size(0, 0, true); // PathingMap::load resizes these to fit
	check_size(0, 3, true);
	check_size(3, 0, true);

	fs::remove(dir / "war3map.wpm");
	check_accepted(dir);

	// The terrain of the same candidate map decides, not a fixed size.
	const fs::path test_map = copy_test_map("wpm_size_test_map");
	check_accepted(test_map);
	write_all(test_map / "war3map.wpm", with_size(252, 256));
	check_refused(test_map, "war3map.wpm");
	write_all(test_map / "war3map.wpm", with_size(256, 252));
	check_refused(test_map, "war3map.wpm");
}

TEST_CASE("a zero-sized war3map.wpm stays accepted") {
	const fs::path dir = make_crafted_map("wpm_zero");
	Bytes wpm;
	wpm.raw("MP3W");
	wpm.u32(0);
	wpm.u32(0);
	wpm.u32(0);
	write_all(dir / "war3map.wpm", wpm.data);
	check_accepted(dir);
}

TEST_CASE("a war3map.w3s cut in the middle of a string is refused") {
	const fs::path dir = make_crafted_map("w3s_string");

	// In a string read_c_string reads.
	const std::string long_name = w3s_file(std::string(500, 'x')).data;
	write_all(dir / "war3map.w3s", long_name.substr(0, 8 + 300));
	check_refused(dir, "war3map.w3s");

	// In a string advance_c_string skips: what is left of the file holds no NUL at all.
	Bytes bytes = w3s_file();
	const std::size_t last_strings = bytes.data.size() - 8;
	bytes.data.resize(last_strings);
	bytes.raw("xxx");
	write_all(dir / "war3map.w3s", bytes.data);
	check_refused(dir, "war3map.w3s");
}

TEST_CASE("war3mapMisc.txt without a Misc section is refused") {
	const fs::path dir = make_crafted_map("misc");
	write_all(dir / "war3mapMisc.txt", "[Misc]\nGoldTextHeight=1\n");
	check_accepted(dir);
	write_all(dir / "war3mapMisc.txt", "[Other]\nGoldTextHeight=1\n");
	check_refused(dir, "war3mapMisc.txt");
	write_all(dir / "war3mapMisc.txt", "");
	check_refused(dir, "war3mapMisc.txt");
	write_all(dir / "war3mapMisc.txt", "GoldTextHeight=1\n");
	check_refused(dir, "war3mapMisc.txt");
}

TEST_CASE("a position that is not a number is refused") {
	const fs::path dir = make_crafted_map("nan");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float infinity = std::numeric_limits<float>::infinity();

	const auto with_float = [](const std::string& bytes, const std::size_t offset, const float value) {
		std::string result = bytes;
		std::memcpy(result.data() + offset, &value, sizeof(value));
		return result;
	};

	for (const float value : { nan, infinity, -infinity }) {
		const PlacedFile units = units_file(8);
		write_all(dir / "war3mapUnits.doo", with_float(units.bytes.data, units.x_offset, value));
		check_refused(dir, "war3mapUnits.doo");
		write_all(dir / "war3mapUnits.doo", with_float(units.bytes.data, units.x_offset + 4, value));
		check_refused(dir, "war3mapUnits.doo");
		write_all(dir / "war3mapUnits.doo", units.bytes.data);

		const PlacedFile doodads = doo_file(8);
		write_all(dir / "war3map.doo", with_float(doodads.bytes.data, doodads.x_offset, value));
		check_refused(dir, "war3map.doo");
		write_all(dir / "war3map.doo", with_float(doodads.bytes.data, doodads.x_offset + 4, value));
		check_refused(dir, "war3map.doo");
		write_all(dir / "war3map.doo", doodads.bytes.data);
	}
	check_accepted(dir);
}

namespace {
	/// A fresh World Editor 3.0.1 save, editor 7003, with two doodads. Not fixed-Z, so it is only read here.
	constexpr std::string_view fresh_world_editor_doo =
		"5733646f0d0000000b000000020000004c54627800000000000020c3000020c3000000006037bc40436b943f436b943f436b943f4c546278ffffffff0264"
		"ffffffff00000000ffffffff000000000000000000000000000000004c546734000000000000d0430000004200000000db0f49400000803f0000803f0000"
		"803f4c546734ffffffff0264ffffffff00000000ffffffff010000000000000000000000000000000000000000000000";

	std::string from_hex(const std::string_view hex) {
		std::string bytes;
		for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
			bytes += static_cast<char>(std::stoi(std::string(hex.substr(i, 2)), nullptr, 16));
		}
		return bytes;
	}

	BinaryReader reader_over(const std::string& bytes) {
		std::vector<u8, default_init_allocator<u8>> buffer(bytes.size());
		std::memcpy(buffer.data(), bytes.data(), bytes.size());
		return BinaryReader(std::move(buffer));
	}

	std::string le_f32(const float value) {
		std::string bytes(4, '\0');
		std::memcpy(bytes.data(), &value, 4);
		return bytes;
	}

	std::string with_f32_at(std::string bytes, const std::size_t offset, const float value) {
		std::memcpy(bytes.data() + offset, &value, 4);
		return bytes;
	}

	std::string with_byte_at(std::string bytes, const std::size_t offset, const std::uint8_t value) {
		bytes[offset] = static_cast<char>(value);
		return bytes;
	}

	std::uint32_t read_u32_at(const std::string& bytes, const std::size_t offset) {
		std::uint32_t value = 0;
		std::memcpy(&value, bytes.data() + offset, 4);
		return value;
	}

	MapInfo game_version(const int major, const int minor) {
		MapInfo info {};
		info.game_version_major = major;
		info.game_version_minor = minor;
		return info;
	}

	struct ScopedMapDirectory {
		fs::path previous = hierarchy.map_directory;
		explicit ScopedMapDirectory(const fs::path& dir) {
			hierarchy.map_directory = dir;
		}
		~ScopedMapDirectory() {
			hierarchy.map_directory = previous;
		}
	};

	/// Terrain's destructor calls GL, which does not exist here, so the instance is kept for the process. Saving a fixed-Z
	/// doodad reads only the offset. Its corners are for the ones that are not: 2 by 2 and flat at height 0.
	const Terrain& terrain_without_gl() {
		static const Terrain* const terrain = [] {
			Terrain* const created = new Terrain();
			created->width = 2;
			created->height = 2;
			created->resize_corner_arrays(4);
			return created;
		}();
		return *terrain;
	}

	/// Holds a value other than the default in every field, and fixed-Z doodads, so that a save reads no terrain.
	std::string doo_every_field(const std::uint32_t version, const int lights = 2) {
		const auto add_light = [](Bytes& b, const std::uint32_t index, const std::array<float, 6>& floats) {
			b.u32(index);
			b.u32(index + 1);
			b.raw("\x10\x20\x30\x40");
			for (const float value : floats) {
				b.raw(le_f32(value));
			}
		};

		Bytes b;
		b.raw("W3do");
		b.u32(version);
		b.u32(11);
		b.u32(2);

		b.raw("LTlt");
		b.u32(3);
		for (const float value : { 128.f, -256.f, 512.5f, 1.5f, 0.5f, 2.f, 3.f }) {
			b.raw(le_f32(value)); // position, angle, scale
		}
		b.raw("LTsk");
		if (version >= 12) {
			b.u32(0x12345678); // group id
		}
		b.raw(std::string(1, '\x0E')); // state 2, fixed-Z and the bit nobody knows
		b.raw(std::string(1, '\x4D')); // life
		b.u32(5); // item table
		b.u32(1); // item sets
		b.u32(1); // items
		b.raw("ratc");
		b.u32(100);
		if (version >= 13) {
			b.u32(0x07654321); // unknown
		}
		b.u32(41); // creation number
		if (version >= 12) {
			b.raw(le_f32(0.25f));
			b.raw(le_f32(-0.5f));
			b.u32(lights);
			for (int i = 0; i < lights; i++) {
				add_light(b, i, { 1.f, 2.f, 3.f, 4.f, 5.f, 6.f });
			}
		}

		b.raw("YTlb");
		b.u32();
		for (const float value : { 0.f, 1024.f, 0.f, 0.f, 1.f, 1.f, 1.f }) {
			b.raw(le_f32(value));
		}
		b.raw("YTlb");
		if (version >= 12) {
			b.u32(0xFFFFFFFF);
		}
		b.raw(std::string(1, '\xF7')); // state 3, fixed-Z and unknown high bits
		b.raw(std::string(1, '\x64'));
		b.u32(0xFFFFFFFF);
		b.u32();
		if (version >= 13) {
			b.u32(0xFFFFFFFF);
		}
		b.u32(42);
		if (version >= 12) {
			b.u32s(2);
			b.u32();
		}

		b.u32(); // special format version
		b.u32(1);
		b.raw("ZZab");
		b.u32(2);
		b.u32(3);
		b.u32(static_cast<std::uint32_t>(-4));
		return b.data;
	}

	/// Loads and saves through the real paths and returns what the save wrote.
	std::string load_and_save(const std::string& name, const std::string& bytes, const MapInfo& info, const std::function<void(Doodads&)>& check) {
		const fs::path dir = make_scratch_dir(name);
		write_all(dir / "war3map.doo", bytes);
		const ScopedMapDirectory open_map(dir);
		Doodads doodads;
		REQUIRE(doodads.load(terrain_without_gl(), info));
		check(doodads);
		doodads.save(terrain_without_gl());
		return read_all(dir / "war3map.doo");
	}

	std::string load_and_save(const std::string& name, const std::string& bytes, const MapInfo& info) {
		return load_and_save(name, bytes, info, [](Doodads&) {});
	}

	/// A crafted map whose war3map.doo is the given file.
	fs::path crafted_map_with_doo(const std::string& name, const std::string& doo) {
		const fs::path dir = make_crafted_map(name);
		write_all(dir / "war3map.doo", doo);
		return dir;
	}

	void check_doo_refused_saying(const fs::path& dir, const std::vector<std::string>& parts) {
		const auto result = check_map_bodies(dir);
		REQUIRE_FALSE(result.has_value());
		CHECK_MESSAGE(contains(result.error(), "war3map.doo"), result.error());
		for (const std::string& part : parts) {
			CHECK_MESSAGE(contains(result.error(), part), result.error());
		}
	}
}

TEST_CASE("a fresh World Editor v13 war3map.doo reads as what it holds") {
	const std::string bytes = from_hex(fresh_world_editor_doo);
	REQUIRE(bytes.size() == 172);

	BinaryReader reader = reader_over(bytes);
	std::vector<Doodad> doodads;
	std::vector<SpecialDoodad> special_doodads;
	std::uint32_t version = 0;
	REQUIRE(Doodads::read_records(reader, {}, game_version(3, 0), doodads, special_doodads, version));
	CHECK(version == 13);
	CHECK(reader.remaining() == 0);
	CHECK(special_doodads.empty());

	REQUIRE(doodads.size() == 2);
	CHECK(doodads[0].id == "LTbx");
	CHECK(doodads[0].skin_id == "LTbx");
	CHECK(doodads[1].id == "LTg4");
	CHECK(doodads[1].skin_id == "LTg4");
	CHECK(doodads[0].position.x == -1.25f);
	CHECK(doodads[1].position.y == 0.25f);
	for (int i = 0; i < 2; i++) {
		CAPTURE(i);
		CHECK(doodads[i].creation_number == i);
		CHECK(doodads[i].file_flags == 0x02);
		CHECK(doodads[i].flags() == 0x02);
		CHECK(doodads[i].life == 100);
		CHECK(doodads[i].group_id == 0xFFFFFFFF);
		CHECK(doodads[i].unknown_v13 == -1);
		CHECK(doodads[i].stored_roll == 0.f);
		CHECK(doodads[i].stored_pitch == 0.f);
		CHECK(doodads[i].lights.empty());
	}

	const fs::path dir = crafted_map_with_doo("doo_fresh", bytes);
	check_accepted(dir);

	const ScopedMapDirectory open_map(dir);
	Doodads loaded;
	REQUIRE(loaded.load(terrain_without_gl(), game_version(3, 0)));
	CHECK(loaded.format_version == 13);
}

TEST_CASE("the test terrain has an offset of 0, which the byte-exact round trips below rely on") {
	CHECK(terrain_without_gl().offset.x == 0.f);
	CHECK(terrain_without_gl().offset.y == 0.f);
}

TEST_CASE("a v12 or v13 war3map.doo saves as the same bytes") {
	for (const std::uint32_t version : { 12u, 13u }) {
		CAPTURE(version);
		const std::string bytes = doo_every_field(version);
		const std::string saved = load_and_save("doo_round_trip", bytes, game_version(3, 0), [&](Doodads& doodads) {
			CHECK(doodads.format_version == static_cast<int>(version));
			REQUIRE(doodads.doodads.size() == 2);
			const Doodad& first = doodads.doodads[0];
			CHECK(first.group_id == 0x12345678u);
			CHECK(first.unknown_v13 == (version >= 13 ? 0x07654321 : -1));
			CHECK(first.stored_roll == 0.25f);
			CHECK(first.stored_pitch == -0.5f);
			REQUIRE(first.lights.size() == 2);
			CHECK(first.lights[1].index == 1u);
			CHECK(first.lights[1].is_shadow_casting == 2u);
			const std::array<std::uint8_t, 4> color = { 0x10, 0x20, 0x30, 0x40 };
			CHECK(first.lights[1].color == color);
			CHECK(first.lights[1].intensity == 1.f);
			CHECK(first.lights[1].shadow_start == 2.f);
			CHECK(first.lights[1].shadow_end == 3.f);
			CHECK(first.lights[1].quadratic_falloff == 4.f);
			CHECK(first.lights[1].linear_falloff == 5.f);
			CHECK(first.lights[1].damping == 6.f);
			CHECK(first.flags() == 14);
			CHECK(doodads.doodads[1].flags() == 0xF7);
		});
		CHECK(saved == bytes);
	}
}

TEST_CASE("flags keeps the file's byte unless the doodad changed") {
	const auto loaded_with = [](const std::uint8_t flags) {
		Doodad doodad;
		doodad.file_flags = flags;
		doodad.state = Doodad::state_from_flags(flags);
		doodad.fixed_z = flags & Doodad::fixed_z_flag;
		return doodad;
	};

	SUBCASE("unchanged") {
		for (const std::uint8_t flags : { std::uint8_t { 3 }, std::uint8_t { 7 }, std::uint8_t { 10 }, std::uint8_t { 14 }, std::uint8_t { 0xF7 } }) {
			CAPTURE(int { flags });
			CHECK(int { loaded_with(flags).flags() } == int { flags });
		}
	}
	SUBCASE("a changed state takes effect and keeps the other bits") {
		Doodad doodad = loaded_with(14);
		doodad.state = Doodad::State::invisible_non_solid;
		CHECK(int { doodad.flags() } == 12);
		doodad.state = Doodad::State::visible_non_solid;
		CHECK(int { doodad.flags() } == 13);

		Doodad state_three = loaded_with(3);
		state_three.state = Doodad::State::visible_non_solid;
		CHECK(int { state_three.flags() } == 1);
		state_three.state = Doodad::State::visible_solid;
		CHECK(int { state_three.flags() } == 3);
	}
	SUBCASE("a changed height flag takes effect") {
		Doodad fixed = loaded_with(14);
		fixed.snap_to_terrain(terrain_without_gl());
		CHECK(int { fixed.flags() } == 10);

		Doodad following = loaded_with(3);
		following.set_absolute_height(2.f);
		CHECK(int { following.flags() } == 7);
	}
	SUBCASE("a placed doodad has the brush's state and no other bits") {
		Doodad doodad;
		CHECK(int { doodad.flags() } == 2);
		doodad.state = Doodad::State::invisible_non_solid;
		CHECK(int { doodad.flags() } == 0);
		doodad.state = Doodad::State::visible_non_solid;
		CHECK(int { doodad.flags() } == 1);
		doodad.fixed_z = true;
		CHECK(int { doodad.flags() } == 5);
		CHECK(doodad.group_id == 0xFFFFFFFF);
		CHECK(doodad.unknown_v13 == -1);
		CHECK(doodad.stored_roll == 0.f);
		CHECK(doodad.stored_pitch == 0.f);
		CHECK(doodad.lights.empty());
	}
}

TEST_CASE("a v8 war3map.doo stays v8 with no new fields and keeps its flags") {
	const PlacedFile v8 = doo_file(8);
	for (const std::uint8_t flags : { std::uint8_t { 2 }, std::uint8_t { 3 }, std::uint8_t { 7 }, std::uint8_t { 10 }, std::uint8_t { 14 } }) {
		CAPTURE(int { flags });
		const std::string bytes = with_byte_at(v8.bytes.data, v8.flags_offset, flags);
		const std::string saved = load_and_save("doo_v8", bytes, game_version(2, 0), [&](Doodads& doodads) {
			CHECK(doodads.format_version == 8);
		});
		CHECK(saved == bytes);
	}
}

TEST_CASE("what a war3map.doo saves as depends on what it was") {
	const auto check_saved_as_8_11 = [](const std::string& saved) {
		CHECK(read_u32_at(saved, 4) == 8u);
		CHECK(read_u32_at(saved, 8) == 11u);
	};

	SUBCASE("v7 saves as v8") {
		const std::string v7 = doo_file(7).bytes.data;
		const std::string saved = load_and_save("doo_saves_v7", v7, game_version(2, 0));
		check_saved_as_8_11(saved);
		CHECK(saved.size() == v7.size() + 8); // Only the item table and the item set count are added.
	}
	SUBCASE("a v8 subversion is written as 11") {
		const std::string v8 = with_u32_at(doo_file(8).bytes.data, 8, 9);
		check_saved_as_8_11(load_and_save("doo_saves_v8", v8, game_version(2, 0)));
	}
	SUBCASE("a new map saves as v8") {
		const fs::path dir = make_scratch_dir("doo_saves_new");
		const ScopedMapDirectory open_map(dir);
		Doodads doodads;
		CHECK(doodads.format_version == 8);
		doodads.save(terrain_without_gl());
		check_saved_as_8_11(read_all(dir / "war3map.doo"));
	}
	SUBCASE("a file that fails on its magic number leaves no stale version behind") {
		const fs::path dir = make_scratch_dir("doo_saves_stale");
		const ScopedMapDirectory open_map(dir);
		Doodads doodads;
		write_all(dir / "war3map.doo", doo_every_field(13));
		REQUIRE(doodads.load(terrain_without_gl(), game_version(3, 0)));
		CHECK(doodads.format_version == 13);
		write_all(dir / "war3map.doo", "XXXX");
		CHECK_FALSE(doodads.load(terrain_without_gl(), game_version(3, 0)));
		CHECK(doodads.format_version == 8);
	}
}

TEST_CASE("a v12 file read with v13 gates and the reverse are refused") {
	const auto check_swapped = [](const std::string& bytes, const std::uint32_t wrong_version) {
		const fs::path dir = crafted_map_with_doo("doo_wrong_gate", bytes);
		check_accepted(dir);
		write_all(dir / "war3map.doo", with_u32_at(bytes, 4, wrong_version));
		check_refused(dir, "war3map.doo");
	};

	for (const int lights : { 0, 1, 2 }) {
		CAPTURE(lights);
		check_swapped(doo_every_field(12, lights), 13);
		check_swapped(doo_every_field(13, lights), 12);
	}
	check_swapped(doo_file(12).bytes.data, 13);
	check_swapped(doo_file(13).bytes.data, 12);
	check_swapped(from_hex(fresh_world_editor_doo), 12);
}

TEST_CASE("every prefix of a v12 or v13 war3map.doo is refused") {
	for (const std::uint32_t version : { 12u, 13u }) {
		CAPTURE(version);
		const fs::path dir = crafted_map_with_doo("doo_prefix", doo_every_field(version));
		check_every_prefix_refused(dir, "war3map.doo");
	}
}

TEST_CASE("the smallest v12 and v13 records are exactly what the doodad count is measured against") {
	// The count is refused for the record size alone, with the file holding just too little for it. A file that big
	// would also fail further on, so the message is what tells that the count check caught it.
	for (const auto& [version, smallest] : { std::pair { 12u, 70u }, std::pair { 13u, 74u } }) {
		CAPTURE(version);
		const std::string header = "W3do" + le32(version) + le32(11) + le32(2);
		BinaryReader too_small = reader_over(header + std::string(2 * smallest - 1, '\0'));
		std::vector<Doodad> doodads;
		std::vector<SpecialDoodad> special_doodads;
		std::uint32_t read_version = 0;
		CHECK_THROWS_WITH_AS(
			(void)Doodads::read_records(too_small, {}, game_version(3, 0), doodads, special_doodads, read_version),
			doctest::Contains("does not fit in the file"),
			std::out_of_range
		);

		// One byte more and the count passes. All that is missing then is the trailer after the two zeroed records.
		BinaryReader big_enough = reader_over(header + std::string(2 * smallest, '\0'));
		CHECK_THROWS_WITH_AS(
			(void)Doodads::read_records(big_enough, {}, game_version(3, 0), doodads, special_doodads, read_version),
			doctest::Contains("out of range"),
			std::out_of_range
		);
	}
}

TEST_CASE("v12 and v13 refusals reach the user naming war3map.doo") {
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float infinity = std::numeric_limits<float>::infinity();

	for (const std::uint32_t version : { 12u, 13u }) {
		CAPTURE(version);
		const PlacedFile file = doo_file(version);
		const std::string& valid = file.bytes.data;
		const fs::path dir = crafted_map_with_doo("doo_refusals", valid);
		check_accepted(dir);

		// A light count that cannot fit
		write_all(dir / "war3map.doo", with_u32_at(valid, file.roll_offset + 8, 2));
		check_doo_refused_saying(dir, { "A count of 2 does not fit" });

		// A roll or pitch that is not a number
		for (const float value : { nan, infinity, -infinity }) {
			for (const std::size_t offset : { file.roll_offset, file.roll_offset + 4 }) {
				CAPTURE(offset);
				write_all(dir / "war3map.doo", with_f32_at(valid, offset, value));
				check_doo_refused_saying(dir, { "roll or pitch" });
			}
		}

		// A subversion that has not been seen
		for (const std::uint32_t subversion : { 0u, 9u, 10u, 12u }) {
			write_all(dir / "war3map.doo", with_u32_at(valid, 8, subversion));
			check_doo_refused_saying(dir, { "subversion " + std::to_string(subversion), "has not been seen" });
		}

		// A special version that has not been seen
		write_all(dir / "war3map.doo", with_u32_at(valid, file.special_version_offset, 1));
		check_doo_refused_saying(dir, { "special doodad version 1", "has not been seen" });

		// Bytes left after the last special doodad
		write_all(dir / "war3map.doo", valid + std::string(1, '\0'));
		check_doo_refused_saying(dir, { "leftover bytes after its last special doodad: 1" });
		write_all(dir / "war3map.doo", valid + std::string(4, '\0'));
		check_doo_refused_saying(dir, { "leftover bytes after its last special doodad: 4" });

		// A w3i game version below 1.32
		write_all(dir / "war3map.doo", valid);
		check_accepted(dir);
		const std::string w3i = w3i_file(33).data;
		write_all(dir / "war3map.w3i", with_u32_at(with_u32_at(w3i, 12, 1), 16, 31));
		check_doo_refused_saying(dir, { "1.31", "has not been seen" });
		write_all(dir / "war3map.w3i", with_u32_at(with_u32_at(w3i, 12, 1), 16, 32));
		check_accepted(dir);
	}
}

TEST_CASE("v7 and v8 get none of the v12 and v13 refusals") {
	for (const std::uint32_t version : { 7u, 8u }) {
		CAPTURE(version);
		const PlacedFile file = doo_file(version);
		const fs::path dir = crafted_map_with_doo("doo_old_lenient", file.bytes.data);
		check_accepted(dir);

		write_all(dir / "war3map.doo", file.bytes.data + std::string(4, '\0'));
		check_accepted(dir);
		write_all(dir / "war3map.doo", with_u32_at(file.bytes.data, 8, 9));
		check_accepted(dir);
		write_all(dir / "war3map.doo", with_u32_at(file.bytes.data, file.special_version_offset, 1));
		check_accepted(dir);

		// Below 1.32 the record has no skin. The other files of a crafted map assume one, so this reads just the doo.
		std::string no_skin = file.bytes.data;
		no_skin.erase(file.flags_offset - 4, 4);
		BinaryReader reader = reader_over(no_skin);
		std::vector<Doodad> doodads;
		std::vector<SpecialDoodad> special_doodads;
		std::uint32_t read_version = 0;
		CHECK(Doodads::read_records(reader, {}, game_version(1, 31), doodads, special_doodads, read_version));
		REQUIRE(doodads.size() == 1);
		CHECK(doodads[0].skin_id == doodads[0].id);
		CHECK(reader.remaining() == 0);
	}
}

TEST_CASE("float bit patterns in a v12 or v13 war3map.doo survive a save") {
	constexpr std::uint32_t negative_zero = 0x80000000;
	constexpr std::array<std::array<std::uint32_t, 6>, 2> patterns = { {
		{ 0x7FA00001, 0xFFA12345, 0x7FC00001, 0xFFC12345, negative_zero, 0x7F800000 }, // signalling, quiet, -0, +inf
		{ 0xFF800000, 0x7F800001, 0x7FFFFFFF, 0xFFFFFFFF, 0x00000001, 0x80000001 }, // -inf, edge NaNs, denormals
	} };

	for (const std::uint32_t version : { 12u, 13u }) {
		CAPTURE(version);
		const PlacedFile file = doo_file(version);
		const std::size_t light_floats = file.roll_offset + 8 + 4 + 12; // Past roll, pitch, light count and the light's first 3 fields.

		std::string zero_angles = with_u32_at(file.bytes.data, file.roll_offset, negative_zero);
		zero_angles = with_u32_at(zero_angles, file.roll_offset + 4, negative_zero);
		check_accepted(crafted_map_with_doo("doo_bits_roll", zero_angles));
		const std::string saved_angles = load_and_save("doo_bits_roll_save", zero_angles, game_version(3, 0));
		CHECK(read_u32_at(saved_angles, file.roll_offset) == negative_zero);
		CHECK(read_u32_at(saved_angles, file.roll_offset + 4) == negative_zero);

		for (const auto& floats : patterns) {
			std::string bytes = file.bytes.data;
			for (std::size_t i = 0; i < floats.size(); i++) {
				bytes = with_u32_at(bytes, light_floats + i * 4, floats[i]);
			}
			check_accepted(crafted_map_with_doo("doo_bits_light", bytes));
			const std::string saved = load_and_save("doo_bits_light_save", bytes, game_version(3, 0));
			REQUIRE(saved.size() == bytes.size());
			for (std::size_t i = 0; i < floats.size(); i++) {
				CAPTURE(i);
				CHECK(read_u32_at(saved, light_floats + i * 4) == floats[i]);
			}
		}
	}
}

TEST_CASE("the template a new map starts from has a war3map.doo that saves as v8") {
	// HiveWE loads this folder for a new map and only clears the doodads, so format_version comes from this file.
	const std::string bytes = read_all(fs::path(TEST_MAP_DIR) / "war3map.doo");
	REQUIRE(bytes.size() >= 8);
	CHECK(bytes.substr(0, 4) == "W3do");
	const std::uint32_t version = read_u32_at(bytes, 4);
	CAPTURE(version);
	CHECK((version == 7 || version == 8));
}

TEST_CASE("values whose meaning is unknown are preserved and not refused") {
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float infinity = std::numeric_limits<float>::infinity();

	for (const std::uint32_t version : { 12u, 13u }) {
		CAPTURE(version);

		// Non-finite light floats
		std::string light_floats = doo_every_field(version, 1);
		const std::size_t first_float = light_floats.find(std::string("\x10\x20\x30\x40")) + 4;
		light_floats = with_f32_at(light_floats, first_float, nan);
		light_floats = with_f32_at(light_floats, first_float + 4, infinity);
		light_floats = with_f32_at(light_floats, first_float + 8, -infinity);
		check_accepted(crafted_map_with_doo("doo_light_floats", light_floats));
		CHECK(load_and_save("doo_light_floats_save", light_floats, game_version(3, 0)) == light_floats);

		// More than 2 lights
		const std::string many_lights = doo_every_field(version, 5);
		check_accepted(crafted_map_with_doo("doo_many_lights", many_lights));
		CHECK(load_and_save("doo_many_lights_save", many_lights, game_version(3, 0)) == many_lights);

		// Unknown flag bits
		const PlacedFile file = doo_file(version);
		for (const std::uint8_t flags : { std::uint8_t { 0x0C }, std::uint8_t { 0x0F }, std::uint8_t { 0xFF }, std::uint8_t { 0xF4 } }) {
			CAPTURE(int { flags });
			const std::string bytes = with_byte_at(file.bytes.data, file.flags_offset, flags);
			check_accepted(crafted_map_with_doo("doo_flag_bits", bytes));
			CHECK(load_and_save("doo_flag_bits_save", bytes, game_version(3, 0)) == bytes);
		}
	}
}

TEST_CASE("trigger nesting is limited deep enough for stock and shallow enough for the stack") {
	const fs::path dir = make_crafted_map("nesting");
	fs::remove(dir / "war3map.wct");

	write_all(dir / "war3map.wtg", wtg_with_nested_parameters(Triggers::max_parse_depth));
	check_accepted(dir);
	write_all(dir / "war3map.wtg", wtg_with_nested_parameters(Triggers::max_parse_depth + 1));
	check_refused(dir, "war3map.wtg");

	write_all(dir / "war3map.wtg", wtg_with_nested_ecas(Triggers::max_parse_depth));
	check_accepted(dir);
	write_all(dir / "war3map.wtg", wtg_with_nested_ecas(Triggers::max_parse_depth + 1));
	check_refused(dir, "war3map.wtg");

	// Would overflow the stack without the limit, which no catch could recover from.
	write_all(dir / "war3map.wtg", wtg_with_nested_parameters(500'000));
	check_refused(dir, "war3map.wtg");
	write_all(dir / "war3map.wtg", wtg_with_nested_ecas(500'000));
	check_refused(dir, "war3map.wtg");
}

TEST_CASE("wtg and wct failures refuse the map in full scope and are skipped without triggers") {
	const fs::path dir = make_crafted_map("trigger_scope");
	const std::string wtg = read_all(dir / "war3map.wtg");

	write_all(dir / "war3map.wtg", wtg.substr(0, wtg.size() / 2));
	check_refused(dir, "war3map.wtg");
	check_accepted(dir, BodyScope::without_triggers);
	write_all(dir / "war3map.wtg", wtg);

	const std::string wct = read_all(dir / "war3map.wct");
	write_all(dir / "war3map.wct", wct.substr(0, wct.size() - 1));
	check_refused(dir, "war3map.wct");
	check_accepted(dir, BodyScope::without_triggers);

	// Map::load reads the wct only when there is a wtg.
	fs::remove(dir / "war3map.wtg");
	check_accepted(dir);
}

TEST_CASE("data that is odd but not damage stays accepted") {
	const fs::path dir = make_crafted_map("not_damage");

	SUBCASE("a war3map.wct with trailing zeros") {
		write_all(dir / "war3map.wct", wct_file().data + std::string(16, '\0'));
		check_accepted(dir);
	}
	SUBCASE("unit random types with and without a payload") {
		for (const std::uint32_t type : { 0xFFFFFFFFu, 641u, 0u, 1u, 2u }) {
			CAPTURE(type);
			write_all(dir / "war3mapUnits.doo", units_file(8, type).bytes.data);
			check_accepted(dir);
		}
	}
	SUBCASE("a v33 war3map.w3i cut after its forces under the kept leniency") {
		const fs::path test_map = copy_test_map("not_damage_w3i");
		const std::string whole = read_all(test_map / "war3map.w3i");
		REQUIRE(whole.size() == 309);
		write_all(test_map / "war3map.w3i", whole.substr(0, 293));
		check_accepted(test_map);
		write_all(test_map / "war3map.w3i", whole.substr(0, 292));
		check_refused(test_map, "war3map.w3i");
	}
	SUBCASE("trailing bytes in the files with no rule against them") {
		for (const std::string file : { "war3map.doo", "war3mapUnits.doo", "war3map.w3r", "war3map.w3c", "war3map.w3s", "war3map.wpm", "war3map.w3e", "war3map.w3u" }) {
			CAPTURE(file);
			const std::string whole = read_all(dir / file);
			write_all(dir / file, whole + std::string(4, '\0'));
			check_accepted(dir);
			write_all(dir / file, whole);
		}
	}
}

TEST_CASE("the read override applies to this thread only") {
	const fs::path dir = make_scratch_dir("override");
	write_all(dir / "file.txt", "x");
	const SentinelOpenMap open_map;

	CHECK_FALSE(hierarchy.map_file_read("file.txt").has_value());

	Hierarchy::map_read_override = &dir;
	CHECK(hierarchy.map_file_read("file.txt").has_value());
	bool other_thread_reads_it = true;
	std::thread other([&] { other_thread_reads_it = hierarchy.map_file_read("file.txt").has_value(); });
	other.join();
	CHECK_FALSE(other_thread_reads_it);
	Hierarchy::map_read_override = nullptr;

	CHECK_FALSE(hierarchy.map_file_read("file.txt").has_value());
}

TEST_CASE("the archive check reads bodies without judging triggers") {
	const fs::path source = make_crafted_map("archive_source");
	write_all(source / "war3map.w3t", object_file(false).data);
	const std::vector<std::string> names = {
		"war3map.w3i", "war3map.w3e", "war3map.wpm", "war3map.doo", "war3mapUnits.doo", "war3map.w3r", "war3map.w3c",
		"war3map.w3s", "war3map.wtg", "war3map.wct", "war3map.w3u", "war3map.w3a", "war3map.w3t",
	};

	const auto check = [&](const std::string& name) {
		const fs::path archive = make_scratch_dir("archive_" + name) / "map.w3x";
		make_archive(archive, source, names);
		const fs::path scratch = make_scratch_dir("archive_scratch_" + name);
		mpq::MPQ opened;
		REQUIRE(opened.open(archive, MPQ_OPEN_READ_ONLY));
		return check_archive_for_opening(opened, scratch);
	};

	SUBCASE("a valid archive passes") {
		const auto result = check("valid");
		CHECK_MESSAGE(result.has_value(), (result ? std::string() : result.error()));
	}
	SUBCASE("a truncated body of a checked file is refused") {
		const std::string doo = read_all(source / "war3map.doo");
		write_all(source / "war3map.doo", doo.substr(0, doo.size() - 1));
		const auto result = check("doo");
		REQUIRE_FALSE(result.has_value());
		CHECK(contains(result.error(), "war3map.doo"));
	}
	SUBCASE("a truncated object file that the version check does not extract is refused") {
		const std::string w3t = read_all(source / "war3map.w3t");
		write_all(source / "war3map.w3t", w3t.substr(0, w3t.size() - 1));
		const auto result = check("w3t");
		REQUIRE_FALSE(result.has_value());
		CHECK(contains(result.error(), "war3map.w3t"));
	}
	SUBCASE("a wrong version still reads as one") {
		const std::string w3i = read_all(source / "war3map.w3i");
		write_all(source / "war3map.w3i", with_u32_at(w3i, 0, 99));
		const auto result = check("version");
		REQUIRE_FALSE(result.has_value());
		CHECK(contains(result.error(), "newer version of HiveWE"));
	}
	SUBCASE("a broken war3map.wtg waits for the check on the unpacked folder") {
		const std::string wtg = read_all(source / "war3map.wtg");
		write_all(source / "war3map.wtg", wtg.substr(0, 20));
		const auto result = check("wtg");
		CHECK_MESSAGE(result.has_value(), (result ? std::string() : result.error()));
	}
}

TEST_CASE("every extracted stock map below HIVEWE_STOCK_BODY_ROOT is accepted except known refusals") {
	const std::string root_text = env("HIVEWE_STOCK_BODY_ROOT");
	if (root_text.empty()) {
		MESSAGE("skipped: HIVEWE_STOCK_BODY_ROOT is not set");
		return;
	}
	const fs::path root = root_text;
	const fs::path definitions = make_scratch_dir("stock_definitions");
	fs::create_directories(definitions / "UI");
	fs::copy_file(root / "TriggerData.txt", definitions / "UI" / "TriggerData.txt");
	write_all(definitions / "UI" / "TriggerStrings.txt", "[TriggerActionStrings]\n");
	world_edit_strings.ini_data["WorldEditStrings"];

	const SentinelOpenMap open_map;
	const LocalTriggerDefinitions local(definitions);
	sweep_stock_maps(root, "extracted TriggerData");
	open_map.check_unchanged();
}

namespace {
	/// Walks both files of a v12 or v13 war3map.doo by itself rather than through the reader, which wrote one of them.
	/// Every byte must match except the position, which rounds at a non-zero terrain offset, and flag bit 0x04.
	/// Returns what differs first, or nothing.
	std::string first_raw_doo_difference(const std::string& original, const std::string& saved, const std::uint32_t version) {
		if (original.size() != saved.size()) {
			return std::format("size {} became {}", original.size(), saved.size());
		}

		std::size_t at = 0;
		std::string difference;
		const auto same = [&](const std::size_t size, const std::string_view field) {
			if (!difference.empty()) {
				return;
			}
			if (size > original.size() - at) {
				difference = std::format("{} at {} runs past the end", field, at);
			} else if (original.compare(at, size, saved, at, size) != 0) {
				difference = std::format("{} at {} changed", field, at);
			} else {
				at += size;
			}
		};
		const auto count_at = [&] {
			return difference.empty() && original.size() - at >= 4 ? read_u32_at(original, at) : 0;
		};
		const auto count_field = [&](const std::string_view field) {
			const std::uint32_t count = count_at();
			same(4, field);
			return count;
		};
		const auto skip = [&](const std::size_t size, const std::string_view field) {
			if (difference.empty() && size > original.size() - at) {
				difference = std::format("{} at {} runs past the end", field, at);
			} else if (difference.empty()) {
				at += size;
			}
		};

		same(12, "header");
		const std::uint32_t doodads = count_field("doodad count");
		for (std::uint32_t i = 0; i < doodads && difference.empty(); i++) {
			same(8, "id, variation");
			skip(12, "position");
			same(4 + 12 + 4, "angle, scale, skin");
			same(4, "group_id");
			if (difference.empty() && at < original.size()
				&& (original[at] & ~Doodad::fixed_z_flag) != (saved[at] & ~Doodad::fixed_z_flag)) {
				difference = std::format("flags at {} changed", at);
			}
			skip(1, "flags");
			same(1, "life");
			same(4, "item table");
			const std::uint32_t sets = count_field("item set count");
			for (std::uint32_t j = 0; j < sets && difference.empty(); j++) {
				const std::uint32_t items = count_field("item count");
				same(std::size_t { items } * 8, "items");
			}
			if (version >= 13) {
				same(4, "unk");
			}
			same(4, "creation number");
			same(4, "roll");
			same(4, "pitch");
			const std::uint32_t lights = count_field("light count");
			same(std::size_t { lights } * 36, "lights");
		}
		same(original.size() - at, "special doodads");
		return difference;
	}
}

TEST_CASE("the raw war3map.doo comparison notices each field it is there for") {
	for (const std::uint32_t version : { 12u, 13u }) {
		CAPTURE(version);
		const PlacedFile file = doo_file(version);
		const std::string& valid = file.bytes.data;
		CHECK(first_raw_doo_difference(valid, valid, version).empty());

		// What it must let through.
		CHECK(first_raw_doo_difference(valid, with_byte_at(valid, file.flags_offset, Doodad::fixed_z_flag), version).empty());
		CHECK(first_raw_doo_difference(valid, with_u32_at(valid, file.x_offset, 0x80000000), version).empty());

		// -0 and another NaN are what a comparison of float values would not see.
		const auto check_noticed = [&](const std::string& changed, const std::string& field) {
			CAPTURE(field);
			CHECK(contains(first_raw_doo_difference(valid, changed, version), field));
		};
		check_noticed(with_byte_at(valid, file.flags_offset, 0x08), "flags");
		check_noticed(with_u32_at(valid, file.flags_offset - 4, 0), "group_id");
		check_noticed(with_u32_at(valid, file.roll_offset, 0x80000000), "roll");
		check_noticed(with_u32_at(valid, file.roll_offset + 4, 0x80000000), "pitch");
		check_noticed(with_u32_at(valid, file.roll_offset + 12 + 12, 0x7FA00001), "lights");
		check_noticed(with_u32_at(valid, file.roll_offset + 12 + 32, 0x7FC00001), "lights");
		check_noticed(valid + std::string(1, '\0'), "size");
		if (version >= 13) {
			check_noticed(with_u32_at(valid, file.roll_offset - 8, 0), "unk");
		}

		// A NaN pair that compares as equal floats in the original and the saved file is still a difference.
		const std::string one_nan = with_u32_at(valid, file.roll_offset + 24, 0x7FC00001);
		CHECK(contains(first_raw_doo_difference(one_nan, with_u32_at(valid, file.roll_offset + 24, 0x7FC00002), version), "lights"));
	}
}

TEST_CASE("every stock war3map.doo v12 or v13 below HIVEWE_STOCK_BODY_ROOT survives a save") {
	const std::string root_text = env("HIVEWE_STOCK_BODY_ROOT");
	if (root_text.empty()) {
		MESSAGE("skipped: HIVEWE_STOCK_BODY_ROOT is not set");
		return;
	}
	const fs::path root = root_text;

	const auto same_float = [](const float a, const float b) {
		return a == b || (std::isnan(a) && std::isnan(b));
	};
	const auto same_doodad = [&](const Doodad& a, const Doodad& b) {
		bool same = a.id == b.id && a.skin_id == b.skin_id && a.variation == b.variation && a.life == b.life
			&& a.item_table_pointer == b.item_table_pointer && a.creation_number == b.creation_number && a.group_id == b.group_id
			&& a.unknown_v13 == b.unknown_v13 && a.state == b.state && (a.file_flags & ~Doodad::fixed_z_flag) == (b.file_flags & ~Doodad::fixed_z_flag)
			&& same_float(a.position.x, b.position.x) && same_float(a.position.y, b.position.y) && same_float(a.position.z, b.position.z)
			&& same_float(a.angle, b.angle) && same_float(a.scale.x, b.scale.x) && same_float(a.scale.y, b.scale.y)
			&& same_float(a.scale.z, b.scale.z) && same_float(a.stored_roll, b.stored_roll) && same_float(a.stored_pitch, b.stored_pitch)
			&& a.item_sets.size() == b.item_sets.size() && a.lights.size() == b.lights.size();
		for (std::size_t i = 0; same && i < a.item_sets.size(); i++) {
			same = a.item_sets[i].items == b.item_sets[i].items;
		}
		for (std::size_t i = 0; same && i < a.lights.size(); i++) {
			const DoodadLight& x = a.lights[i];
			const DoodadLight& y = b.lights[i];
			same = x.index == y.index && x.is_shadow_casting == y.is_shadow_casting && x.color == y.color && same_float(x.intensity, y.intensity)
				&& same_float(x.shadow_start, y.shadow_start) && same_float(x.shadow_end, y.shadow_end)
				&& same_float(x.quadratic_falloff, y.quadratic_falloff) && same_float(x.linear_falloff, y.linear_falloff)
				&& same_float(x.damping, y.damping);
		}
		return same;
	};

	const fs::path scratch = make_scratch_dir("stock_doo_round_trip");
	const ScopedMapDirectory open_map(scratch);

	std::map<std::uint32_t, std::size_t> files_by_version;
	std::size_t doodads_compared = 0;
	std::size_t lights_compared = 0;
	std::vector<std::string> failures;

	const std::map<std::string, std::string> index = read_sweep_index(root / "index.json");
	for (const auto& [key, name] : index) {
		// 0 is a file that is missing or too short. No stock map may be skipped for that.
		const std::uint32_t version = doo_version(root / key / "war3map.doo");
		if (version == 8) {
			files_by_version[version]++;
			continue;
		}
		if (version != 12 && version != 13) {
			failures.push_back(std::format("{}: war3map.doo version {}", name, version));
			continue;
		}
		CAPTURE(name);

		MapInfo info {};
		{
			BinaryReader reader = reader_over(read_all(root / key / "war3map.w3i"));
			info.load(reader);
		}

		const std::string original = read_all(root / key / "war3map.doo");
		write_all(scratch / "war3map.doo", original);

		Doodads first;
		REQUIRE(first.load(terrain_without_gl(), info));
		REQUIRE(first.format_version == static_cast<int>(version));
		for (Doodad& doodad : first.doodads) {
			doodad.fixed_z = true; // Saving a doodad that follows the terrain would read this test's terrain instead.
		}
		first.save(terrain_without_gl());

		const std::string saved = read_all(scratch / "war3map.doo");
		Doodads second;
		REQUIRE(second.load(terrain_without_gl(), info));
		REQUIRE(second.format_version == static_cast<int>(version));

		const std::string raw_difference = first_raw_doo_difference(original, saved, version);
		if (!raw_difference.empty()) {
			failures.push_back(std::format("{}: raw bytes: {}", name, raw_difference));
		}

		bool same = first.doodads.size() == second.doodads.size() && first.special_doodads.size() == second.special_doodads.size();
		for (std::size_t i = 0; same && i < first.doodads.size(); i++) {
			same = same_doodad(first.doodads[i], second.doodads[i]);
			lights_compared += first.doodads[i].lights.size();
			doodads_compared++;
		}
		for (std::size_t i = 0; same && i < first.special_doodads.size(); i++) {
			const SpecialDoodad& a = first.special_doodads[i];
			const SpecialDoodad& b = second.special_doodads[i];
			same = a.id == b.id && a.variation == b.variation && a.old_position.x == b.old_position.x && a.old_position.y == b.old_position.y;
		}
		if (!same) {
			failures.push_back(name);
		}
		files_by_version[version]++;
	}

	std::println(
		"stock doo round trip: v12 files {}, v13 files {}, doodads compared {}, lights compared {}, failures {}",
		files_by_version[12],
		files_by_version[13],
		doodads_compared,
		lights_compared,
		failures.size()
	);
	for (const std::string& failure : failures) {
		std::println("stock doo round trip failed: {}", failure);
	}
	CHECK(index.size() == 466);
	CHECK(files_by_version[8] == 275);
	CHECK(files_by_version[12] == 130);
	CHECK(files_by_version[13] == 61);
	CHECK(doodads_compared == 546722);
	CHECK(lights_compared == 4597);
	CHECK_MESSAGE(failures.empty(), std::format("{} stock files changed on save", failures.size()));
}

TEST_CASE("HIVEWE_BODY_CHECK_CASC gives real TriggerData from the game for the sweep and for timing") {
	const std::string warcraft = env("HIVEWE_BODY_CHECK_CASC");
	if (warcraft.empty()) {
		MESSAGE("skipped: HIVEWE_BODY_CHECK_CASC is not set");
		return;
	}
	REQUIRE(hierarchy.open_casc(warcraft));
	world_edit_strings = {};
	world_edit_strings.load("UI/WorldEditStrings.txt");
	std::println("casc: hd {} teen {} local files {}", hierarchy.hd, hierarchy.teen, hierarchy.local_files);

	const auto timed = [](const std::string_view label, const auto& check) {
		std::vector<double> times;
		for (int i = 0; i < 8; i++) {
			const auto start = std::chrono::steady_clock::now();
			const auto result = check();
			times.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
			CHECK_MESSAGE(result.has_value(), (result ? std::string() : result.error()));
		}
		std::println(
			"{}: first {:.2f} ms, then best {:.2f} ms worst {:.2f} ms",
			label,
			times[0],
			*std::ranges::min_element(times | std::views::drop(1)),
			*std::ranges::max_element(times | std::views::drop(1))
		);
	};

	const fs::path test_map = make_scratch_dir("casc_timing");
	fs::copy(TEST_MAP_DIR, test_map, fs::copy_options::recursive);
	timed("test map, versions only", [&] { return check_map_format_versions(test_map); });
	timed("test map, full scope", [&] { return check_map_for_opening(test_map); });
	timed("test map, without triggers", [&] { return check_map_for_opening(test_map, BodyScope::without_triggers); });

	const std::string root_text = env("HIVEWE_STOCK_BODY_ROOT");
	if (!root_text.empty()) {
		const fs::path root = root_text;
		// The largest stock map that passes, by the bytes the check reads, as the upper end of what it costs.
		fs::path largest;
		std::uintmax_t largest_size = 0;
		for (const auto& [key, name] : read_sweep_index(root / "index.json")) {
			if (check_map_for_opening(root / key)) {
				std::uintmax_t size = 0;
				for (const auto& entry : fs::directory_iterator(root / key)) {
					const std::string extension = entry.path().extension().string();
					size += extension == ".shd" || extension == ".wts" || extension == ".imp" ? 0 : entry.file_size();
				}
				if (size > largest_size) {
					largest_size = size;
					largest = root / key;
				}
			}
		}
		std::println("largest accepted stock map: {}, {} bytes read by the check", largest.string(), largest_size);
		timed("largest accepted stock map, full scope", [&] { return check_map_for_opening(largest); });
		const SentinelOpenMap open_map;
		sweep_stock_maps(root, "game TriggerData");
		open_map.check_unchanged();
	}
}

namespace {
	/// A war3mapUnits.doo of one record with a value other than the default in every field, except the z, which Units::load
	/// re-derives from the flat test terrain. The sub 11 fields are only there when the subversion has them.
	struct UnitFileSpec {
		std::uint32_t version = 13;
		std::uint32_t subversion = 11;
		std::string id = "hfoo";
		std::string skin = "hsk1";
		std::string item_set_id = "ratc";
		std::string inventory_id = "rat6";
		std::string ability_id = "Ahbz";
		std::uint32_t group_id = 0x12345678;
		std::uint32_t item_table = 5;
		std::array<std::uint32_t, 3> stats = { 4, 5, 6 };
		std::uint32_t random_type = 0;
		std::string random = le32(1);
		std::array<std::uint32_t, 3> trailer = { 0, 0, 0 };
	};

	std::string unit_file(const UnitFileSpec& spec) {
		Bytes b;
		b.raw("W3do");
		b.u32(spec.version);
		b.u32(spec.subversion);
		b.u32(1);

		b.raw(spec.id);
		b.u32(3); // variation
		for (const float value : { 64.f, 64.f, 0.f, 1.5f, 128.f, 256.f, 384.f }) {
			b.raw(le_f32(value)); // position, angle, scale
		}
		b.raw(spec.skin);
		if (spec.version >= 12) {
			b.u32(spec.group_id);
		}
		b.raw(std::string(1, '\x0A')); // flags
		b.u32(4); // player
		b.raw("\x07\x09"); // unknown bytes
		b.u32(50); // health
		b.u32(22); // mana
		if (spec.subversion >= 11) {
			b.u32(spec.item_table);
		}
		b.u32(1); // item sets
		b.u32(1); // items
		b.raw(spec.item_set_id);
		b.u32(100); // chance
		b.u32(123); // gold
		b.raw(le_f32(200.f)); // acquisition
		b.u32(3); // level
		if (spec.subversion >= 11) {
			for (const std::uint32_t stat : spec.stats) {
				b.u32(stat);
			}
		}
		b.u32(1); // inventory
		b.u32(2); // slot
		b.raw(spec.inventory_id);
		b.u32(1); // abilities
		b.raw(spec.ability_id);
		b.u32(1); // autocast
		b.u32(2); // level
		b.u32(spec.random_type);
		b.raw(spec.random);
		b.u32(3); // color
		b.u32(0xFFFFFFFF); // waygate
		b.u32(41); // creation number
		if (spec.version >= 12) {
			for (const std::uint32_t value : spec.trailer) {
				b.u32(value);
			}
		}
		return b.data;
	}

	/// Loads and saves through the real paths and returns what the save wrote.
	std::string load_and_save_units(
		const std::string& name,
		const std::string& bytes,
		const MapInfo& info,
		const std::function<void(Units&)>& check,
		const Terrain& terrain = terrain_without_gl()
	) {
		const fs::path dir = make_scratch_dir(name);
		write_all(dir / "war3mapUnits.doo", bytes);
		const ScopedMapDirectory open_map(dir);
		Units units;
		units.load(terrain, info);
		check(units);
		units.save(terrain);
		return read_all(dir / "war3mapUnits.doo");
	}

	std::string load_and_save_units(const std::string& name, const std::string& bytes, const MapInfo& info) {
		return load_and_save_units(name, bytes, info, [](Units&) {});
	}
}

TEST_CASE("a random type 2 unit keeps its count and entries on save") {
	for (const std::uint32_t version : { 8u, 13u }) {
		CAPTURE(version);
		for (const std::uint32_t entries : { 0u, 1u, 3u }) {
			CAPTURE(entries);
			UnitFileSpec spec;
			spec.version = version;
			spec.random_type = 2;
			spec.random = le32(entries);
			for (std::uint32_t i = 0; i < entries; i++) {
				spec.random += "hfo" + std::string(1, static_cast<char>('a' + i)) + le32(30 + i);
			}
			const std::string bytes = unit_file(spec);
			CHECK(load_and_save_units("units_random_2", bytes, game_version(3, 0), [&](Units& units) {
				REQUIRE(units.items.size() == 1);
				CHECK(units.items[0].random_type == 2);
				CHECK(units.items[0].random.size() == 4 + std::size_t { entries } * 8);
			}) == bytes);
		}
	}
}

TEST_CASE("an id that is not 4 characters long still takes up 4 bytes on save") {
	for (const std::uint32_t version : { 8u, 13u }) {
		CAPTURE(version);
		for (const std::string& id : { std::string(4, '\0'), std::string("ab\0\0", 4) }) {
			UnitFileSpec spec;
			spec.version = version;
			const auto check_same = [&](const std::string_view field) {
				CAPTURE(field);
				const std::string bytes = unit_file(spec);
				CHECK(load_and_save_units("units_short_id", bytes, game_version(3, 0)) == bytes);
			};

			spec.item_set_id = id;
			check_same("item set");
			spec.item_set_id = "ratc";

			spec.inventory_id = id;
			check_same("inventory");
			spec.inventory_id = "rat6";

			spec.ability_id = id;
			check_same("ability");
			spec.ability_id = "Ahbz";

			spec.skin = id;
			check_same("skin");
			spec.skin = "hsk1";

			spec.id = id;
			check_same("id");
		}
	}
}

namespace {
	/// A fresh World Editor 3.0.1 save: subversion 9, with a start location, a unit and a hero.
	constexpr std::string_view fresh_world_editor_units =
		"5733646f0d0000000900000003000000736c6f6300000000000000c50000c84400000000e4cb96400000803f0000803f0000803f736c6f63ffffffff0200"
		"0000000000ffffffffffffffff0000000000000000000000000000000000000000000000000000000001000000ffffffffffffffff000000000000000000"
		"000000000000006867727900000000c02c9cc300c2f8c200000000dfead73f0000803f0000803f0000803f68677279ffffffff02000000000000ffffffff"
		"ffffffff00000000d4300000000080bf0100000000000000000000000000000001000000ffffffffffffffff010000000000000000000000000000004870"
		"616c000000006a800b43d01709c3000000000b41043d0000803f0000803f0000803f4870616cffffffff02000000000000320000001600000000000000d4"
		"300000000080bf0a00000000000000000000000000000001000000ffffffffffffffff02000000000000000000000000000000";

	/// A crafted map whose war3mapUnits.doo is the given file.
	fs::path crafted_map_with_units(const std::string& name, const std::string& units) {
		const fs::path dir = make_crafted_map(name);
		write_all(dir / "war3mapUnits.doo", units);
		return dir;
	}

	/// read_records into units.units at offset 0, and then a real save. This keeps the order and the z, which Units::load does not,
	/// so it says that the writer reproduces what the reader read, and nothing about Units::load.
	std::string read_records_then_save_units(const std::string& bytes, const MapInfo& info, const std::function<void(Units&)>& edit = {}) {
		const fs::path dir = make_scratch_dir("units_read_and_save");
		const ScopedMapDirectory open_map(dir);
		Units units;
		BinaryReader reader = reader_over(bytes);
		std::uint32_t version = 0;
		std::uint32_t subversion = 0;
		Units::read_records(reader, {}, info, version, subversion, [&](Unit& unit) { units.units.push_back(unit); });
		units.format_version = static_cast<int>(version);
		units.format_subversion = static_cast<int>(subversion);
		if (edit) {
			edit(units);
		}
		units.save(terrain_without_gl());
		return read_all(dir / "war3mapUnits.doo");
	}

	void check_units_refused_saying(const fs::path& dir, const std::vector<std::string>& parts) {
		const auto result = check_map_bodies(dir);
		REQUIRE_FALSE(result.has_value());
		CHECK_MESSAGE(contains(result.error(), "war3mapUnits.doo"), result.error());
		for (const std::string& part : parts) {
			CHECK_MESSAGE(contains(result.error(), part), result.error());
		}
	}

	constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 4> units_v12_pairs = { {
		{ 12, 9 },
		{ 12, 11 },
		{ 13, 9 },
		{ 13, 11 },
	} };

	UnitFileSpec unit_spec(const std::uint32_t version, const std::uint32_t subversion) {
		UnitFileSpec spec;
		spec.version = version;
		spec.subversion = subversion;
		return spec;
	}

	std::string units_without_records(const std::uint32_t version, const std::uint32_t subversion) {
		return "W3do" + le32(version) + le32(subversion) + le32(0);
	}

	/// The fields of a record no subversion 9 file can hold, at their defaults.
	UnitFileSpec with_subversion_11_defaults(UnitFileSpec spec) {
		spec.item_table = 0xFFFFFFFF;
		spec.stats = { 0, 0, 0 };
		return spec;
	}
}

TEST_CASE("the fresh World Editor v13 war3mapUnits.doo reads as what it holds and the writer reproduces it from read_records") {
	const std::string bytes = from_hex(fresh_world_editor_units);
	REQUIRE(bytes.size() == 361);

	BinaryReader reader = reader_over(bytes);
	std::vector<Unit> units;
	std::uint32_t version = 0;
	std::uint32_t subversion = 0;
	Units::read_records(reader, {}, game_version(3, 0), version, subversion, [&](Unit& unit) { units.push_back(unit); });
	CHECK(version == 13);
	CHECK(subversion == 9);
	CHECK(reader.remaining() == 0);

	struct Expected {
		std::string id;
		int gold;
		int level;
		int health;
		int mana;
		std::vector<std::uint8_t> random;
	};
	const std::array<Expected, 3> expected = { {
		{ "sloc", 0, 0, -1, -1, { 1, 0, 0, 0 } },
		{ "hgry", 12500, 1, -1, -1, { 1, 0, 0, 0 } },
		{ "Hpal", 12500, 10, 50, 22, { 1, 0, 0, 0 } },
	} };
	REQUIRE(units.size() == expected.size());
	for (std::size_t i = 0; i < expected.size(); i++) {
		CAPTURE(i);
		const Unit& unit = units[i];
		CHECK(unit.id == expected[i].id);
		CHECK(unit.skin_id == expected[i].id);
		CHECK(unit.gold == expected[i].gold);
		CHECK(unit.level == expected[i].level);
		CHECK(unit.health == expected[i].health);
		CHECK(unit.mana == expected[i].mana);
		CHECK(unit.random_type == 0);
		CHECK(unit.random == expected[i].random);
		CHECK(unit.group_id == 0xFFFFFFFF);
		CHECK(unit.trailer == std::array<std::uint32_t, 2> { 0, 0 });
		CHECK(int { unit.flags } == 2);
		CHECK(unit.creation_number == static_cast<int>(i));
		// Not in a subversion 9 file, so these are the defaults, which do not ask for subversion 11.
		CHECK(unit.item_table_pointer == -1);
		CHECK(unit.strength == 0);
		CHECK(unit.agility == 0);
		CHECK(unit.intelligence == 0);
	}

	CHECK(read_records_then_save_units(bytes, game_version(3, 0)) == bytes);
	check_accepted(crafted_map_with_units("units_fresh", bytes));
}

TEST_CASE("a v12 or v13 war3mapUnits.doo of one record at offset 0 with every field set saves as the same bytes through Units::load") {
	for (const auto& [version, subversion] : units_v12_pairs) {
		CAPTURE(version);
		CAPTURE(subversion);
		const std::string bytes = unit_file(unit_spec(version, subversion));
		const std::string saved = load_and_save_units("units_every_field", bytes, game_version(3, 0), [&](Units& units) {
			CHECK(units.format_version == static_cast<int>(version));
			CHECK(units.format_subversion == static_cast<int>(subversion));
			// Without object data nothing is known about hfoo, so Units::load files it as an item.
			REQUIRE(units.items.size() == 1);
			const Unit& unit = units.items[0];
			CHECK(unit.group_id == 0x12345678u);
			CHECK(int { unit.flags } == 0x0A);
			CHECK(unit.item_table_pointer == (subversion >= 11 ? 5 : -1));
			CHECK(unit.strength == (subversion >= 11 ? 4 : 0));
			CHECK(unit.agility == (subversion >= 11 ? 5 : 0));
			CHECK(unit.intelligence == (subversion >= 11 ? 6 : 0));
		});
		// A sub 9 file cannot hold the item table and stats, so unit_file leaves them out and they stay at their defaults.
		CHECK(saved == bytes);
		check_accepted(crafted_map_with_units("units_every_field_accepted", bytes));
	}
}

TEST_CASE("what a war3mapUnits.doo saves as depends on what it was") {
	const auto check_load_saves_as = [](const std::uint32_t version, const std::uint32_t subversion, const int saved_version, const int saved_subversion) {
		CAPTURE(version);
		CAPTURE(subversion);
		const std::string saved = load_and_save_units("units_saves_as", units_without_records(version, subversion), game_version(3, 0), [&](Units& units) {
			CHECK(units.format_version == saved_version);
			CHECK(units.format_subversion == saved_subversion);
		});
		CHECK(saved == units_without_records(saved_version, saved_subversion));
	};

	SUBCASE("every accepted pair") {
		check_load_saves_as(7, 9, 8, 11);
		check_load_saves_as(7, 11, 8, 11);
		check_load_saves_as(8, 9, 8, 11);
		check_load_saves_as(8, 11, 8, 11);
		check_load_saves_as(12, 9, 12, 9);
		check_load_saves_as(12, 11, 12, 11);
		check_load_saves_as(13, 9, 13, 9);
		check_load_saves_as(13, 11, 13, 11);
	}
	SUBCASE("a new map saves as (8, 11)") {
		const fs::path dir = make_scratch_dir("units_saves_new");
		const ScopedMapDirectory open_map(dir);
		Units units;
		CHECK(units.format_version == 8);
		CHECK(units.format_subversion == 11);
		units.save(terrain_without_gl());
		CHECK(read_all(dir / "war3mapUnits.doo") == units_without_records(8, 11));
	}
	SUBCASE("loading again leaves nothing behind of the previous file") {
		const fs::path dir = make_scratch_dir("units_saves_stale");
		const ScopedMapDirectory open_map(dir);
		Units units;
		write_all(dir / "war3mapUnits.doo", units_without_records(13, 9));
		units.load(terrain_without_gl(), game_version(3, 0));
		CHECK(units.format_version == 13);
		CHECK(units.format_subversion == 9);

		write_all(dir / "war3mapUnits.doo", units_without_records(8, 9));
		units.load(terrain_without_gl(), game_version(3, 0));
		CHECK(units.format_version == 8);
		CHECK(units.format_subversion == 11);

		write_all(dir / "war3mapUnits.doo", units_without_records(13, 9));
		units.load(terrain_without_gl(), game_version(3, 0));
		write_all(dir / "war3mapUnits.doo", units_without_records(14, 9));
		CHECK_THROWS(units.load(terrain_without_gl(), game_version(3, 0)));
		CHECK(units.format_version == 8);
		CHECK(units.format_subversion == 11);
	}
}

TEST_CASE("a v7 or v8 war3mapUnits.doo saves with no new fields") {
	for (const std::uint32_t version : { 7u, 8u }) {
		CAPTURE(version);
		UnitFileSpec spec = unit_spec(version, 11);
		const std::string saved = load_and_save_units("units_old_saves", unit_file(spec), game_version(3, 0));
		spec.version = 8;
		CHECK(saved == unit_file(spec));

		UnitFileSpec sub_9 = unit_spec(version, 9);
		const std::string saved_sub_9 = load_and_save_units("units_old_saves_sub_9", unit_file(sub_9), game_version(3, 0));
		sub_9.version = 8;
		sub_9.subversion = 11;
		CHECK(saved_sub_9 == unit_file(with_subversion_11_defaults(sub_9)));
	}
}

TEST_CASE("a subversion 9 v12 or v13 file only saves as subversion 11 when it has to") {
	const std::array<std::function<void(Unit&)>, 4> edits = {
		[](Unit& unit) { unit.item_table_pointer = 0; },
		[](Unit& unit) { unit.strength = 7; },
		[](Unit& unit) { unit.agility = 8; },
		[](Unit& unit) { unit.intelligence = 9; },
	};
	const std::array<std::function<void(UnitFileSpec&)>, 4> expected_edits = {
		[](UnitFileSpec& spec) { spec.item_table = 0; },
		[](UnitFileSpec& spec) { spec.stats[0] = 7; },
		[](UnitFileSpec& spec) { spec.stats[1] = 8; },
		[](UnitFileSpec& spec) { spec.stats[2] = 9; },
	};

	for (const std::uint32_t version : { 12u, 13u }) {
		CAPTURE(version);
		const UnitFileSpec sub_9 = unit_spec(version, 9);
		const std::string bytes = unit_file(sub_9);
		REQUIRE(bytes.size() + 16 == unit_file(unit_spec(version, 11)).size());

		// Untouched, it keeps its shape.
		CHECK(read_records_then_save_units(bytes, game_version(3, 0)) == bytes);

		for (std::size_t i = 0; i < edits.size(); i++) {
			CAPTURE(i);
			UnitFileSpec expected = with_subversion_11_defaults(sub_9);
			expected.subversion = 11;
			expected_edits[i](expected);

			// On a unit, and on an item, which is saved from the other list.
			CHECK(read_records_then_save_units(bytes, game_version(3, 0), [&](Units& units) { edits[i](units.units[0]); }) == unit_file(expected));
			CHECK(read_records_then_save_units(bytes, game_version(3, 0), [&](Units& units) {
				units.items.push_back(units.units[0]);
				units.units.clear();
				edits[i](units.items[0]);
			}) == unit_file(expected));
		}

		// Only for a value that the file could not hold: other edits keep the shape.
		const std::string other_edit = read_records_then_save_units(bytes, game_version(3, 0), [](Units& units) { units.units[0].gold = 1; });
		CHECK(other_edit.size() == bytes.size());
		CHECK(read_u32_at(other_edit, 8) == 9u);

		// An edit that was taken back asks for nothing any more: this is computed at save.
		CHECK(read_records_then_save_units(bytes, game_version(3, 0), [](Units& units) {
			units.units[0].strength = 7;
			units.units[0].strength = 0;
		}) == bytes);

		// One that needs it is enough.
		const std::string one_of_two = read_records_then_save_units(bytes, game_version(3, 0), [](Units& units) {
			units.items.push_back(units.units[0]);
			units.items[0].intelligence = 1;
		});
		CHECK(read_u32_at(one_of_two, 8) == 11u);
		CHECK(read_u32_at(one_of_two, 12) == 2u);

		// A file loaded as 11 stays 11, even with the stats at their defaults.
		UnitFileSpec defaults = with_subversion_11_defaults(unit_spec(version, 11));
		const std::string sub_11 = unit_file(defaults);
		CHECK(read_records_then_save_units(sub_11, game_version(3, 0)) == sub_11);
		CHECK(load_and_save_units("units_never_demoted", sub_11, game_version(3, 0)) == sub_11);
	}
}

TEST_CASE("the third trailer value is refused and the other unknown values are preserved") {
	constexpr std::array<std::array<std::uint32_t, 2>, 3> first_and_second = { {
		{ 0x7FA00001, 0xFFC12345 }, // NaNs, which a float would not keep
		{ 0xFFFFFFFF, 0x80000000 },
		{ 1, 2 },
	} };

	for (const auto& [version, subversion] : units_v12_pairs) {
		CAPTURE(version);
		CAPTURE(subversion);

		for (const std::uint32_t third : { 1u, 0xFFFFFFFFu }) {
			UnitFileSpec spec = unit_spec(version, subversion);
			spec.trailer[2] = third;
			check_units_refused_saying(crafted_map_with_units("units_third_trailer", unit_file(spec)), { "hfoo", std::to_string(third), "trailer" });
		}

		for (const auto& [first, second] : first_and_second) {
			for (const std::uint32_t group_id : { 0u, 0x7FC00001u, 0xFFFFFFFEu }) {
				UnitFileSpec spec = unit_spec(version, subversion);
				spec.trailer = { first, second, 0 };
				spec.group_id = group_id;
				const std::string bytes = unit_file(spec);
				check_accepted(crafted_map_with_units("units_unknown_values", bytes));
				CHECK(load_and_save_units("units_unknown_values_save", bytes, game_version(3, 0), [&](Units& units) {
					REQUIRE(units.items.size() == 1);
					CHECK(units.items[0].group_id == group_id);
					CHECK(units.items[0].trailer[0] == first);
					CHECK(units.items[0].trailer[1] == second);
				}) == bytes);
			}
		}
	}
}

TEST_CASE("every prefix of a v12 or v13 war3mapUnits.doo is refused") {
	for (const auto& [version, subversion] : units_v12_pairs) {
		CAPTURE(version);
		CAPTURE(subversion);
		check_every_prefix_refused(crafted_map_with_units("units_prefix", unit_file(unit_spec(version, subversion))), "war3mapUnits.doo");
		check_every_prefix_refused(crafted_map_with_units("units_prefix_fresh", from_hex(fresh_world_editor_units)), "war3mapUnits.doo");
	}
}

TEST_CASE("a v12 or v13 war3mapUnits.doo is refused for what stock never has") {
	for (const auto& [version, subversion] : units_v12_pairs) {
		CAPTURE(version);
		CAPTURE(subversion);
		const std::string valid = unit_file(unit_spec(version, subversion));
		const fs::path dir = crafted_map_with_units("units_refusals", valid);
		check_accepted(dir);
		CHECK(check_map_for_opening(dir).has_value());

		// Bytes left after the last unit
		write_all(dir / "war3mapUnits.doo", valid + std::string(1, '\0'));
		check_units_refused_saying(dir, { "leftover bytes after its last unit: 1" });
		write_all(dir / "war3mapUnits.doo", valid + std::string(4, '\0'));
		check_units_refused_saying(dir, { "leftover bytes after its last unit: 4" });

		// A subversion that does not exist
		for (const std::uint32_t wrong : { 0u, 10u, 12u }) {
			write_all(dir / "war3mapUnits.doo", with_u32_at(valid, 8, wrong));
			check_units_refused_saying(dir, { "subversion " + std::to_string(wrong) });
			CHECK_FALSE(check_map_format_versions(dir).has_value());
		}

		// A record that is not what the subversion says
		write_all(dir / "war3mapUnits.doo", with_u32_at(valid, 8, subversion == 9 ? 11 : 9));
		check_units_refused_saying(dir, {});

		// A count that cannot fit
		write_all(dir / "war3mapUnits.doo", with_u32_at(valid, 12, 2));
		check_units_refused_saying(dir, { "A count of 2 does not fit" });

		// A w3i game version below 1.32. The doo has no records for that to matter to.
		write_all(dir / "war3mapUnits.doo", valid);
		write_all(dir / "war3map.doo", "W3do" + le32(8) + le32(11) + le32(0) + le32(0) + le32(0));
		const std::string w3i = w3i_file(33).data;
		write_all(dir / "war3map.w3i", with_u32_at(with_u32_at(w3i, 12, 1), 16, 31));
		check_units_refused_saying(dir, { "1.31", "has not been seen" });
		write_all(dir / "war3map.w3i", with_u32_at(with_u32_at(w3i, 12, 1), 16, 32));
		check_accepted(dir);
	}
}

TEST_CASE("a damaged count in a v12 or v13 war3mapUnits.doo is refused") {
	const fs::path dir = make_crafted_map("units_v12_counts");
	check_every_count_refused(dir, "war3mapUnits.doo", units_file(12).bytes);
	check_every_count_refused(dir, "war3mapUnits.doo", units_file(13).bytes);
}

TEST_CASE("the smallest v12 and v13 unit records are exactly what the unit count is measured against") {
	for (const auto& [version, subversion] : units_v12_pairs) {
		CAPTURE(version);
		CAPTURE(subversion);
		const std::size_t smallest = 91 + 4 + (subversion >= 11 ? 16 : 0) + 16;
		const std::string header = "W3do" + le32(version) + le32(subversion);
		const MapInfo info = game_version(3, 0);

		std::uint32_t read_version = 0;
		std::uint32_t read_subversion = 0;
		int records = 0;
		const auto read = [&](const std::string& bytes) {
			records = 0;
			BinaryReader reader = reader_over(bytes);
			Units::read_records(reader, {}, info, read_version, read_subversion, [&](Unit&) { records++; });
		};

		CHECK_THROWS_WITH_AS(read(header + le32(2) + std::string(2 * smallest - 1, '\0')), doctest::Contains("does not fit in the file"), std::out_of_range);
		// One byte more and the count passes. Random type 0 has a payload, for which there is no room then.
		CHECK_THROWS_WITH_AS(read(header + le32(2) + std::string(2 * smallest, '\0')), doctest::Contains("out of range"), std::out_of_range);

		// A record of exactly that size exists: no random payload.
		std::string record(smallest, '\0');
		const std::size_t random_type_offset = smallest - 4 * 7; // Random type, color, waygate, creation number, trailer.
		record.replace(random_type_offset, 4, le32(0xFFFFFFFF));
		read(header + le32(1) + record);
		CHECK(records == 1);
		CHECK(read_version == version);
		CHECK(read_subversion == subversion);
		CHECK_THROWS_WITH_AS(read(header + le32(1) + record.substr(0, smallest - 1)), doctest::Contains("does not fit in the file"), std::out_of_range);
	}
}

TEST_CASE("a mismatch between w3i flag 0x4000 and the subversion stays accepted") {
	constexpr std::size_t w3i_33_flags_offset = 116;
	const std::string with_flag = with_u32_at(w3i_file(33).data, w3i_33_flags_offset, 0x4000);
	{
		BinaryReader reader = reader_over(with_flag);
		MapInfo info {};
		info.load(reader);
		REQUIRE(info.all_flags == 0x4000); // That the offset above is the flags.
	}

	for (const std::uint32_t version : { 12u, 13u }) {
		CAPTURE(version);
		const fs::path dir = crafted_map_with_units("units_flag_mismatch", unit_file(unit_spec(version, 9)));
		write_all(dir / "war3map.w3i", with_flag);
		check_accepted(dir);

		write_all(dir / "war3mapUnits.doo", unit_file(unit_spec(version, 11)));
		write_all(dir / "war3map.w3i", w3i_file(33).data);
		check_accepted(dir);
	}
}

TEST_CASE("v7 and v8 war3mapUnits.doo get none of the v12 and v13 refusals") {
	for (const std::uint32_t version : { 7u, 8u }) {
		CAPTURE(version);
		for (const std::uint32_t subversion : { 9u, 11u }) {
			CAPTURE(subversion);
			const std::string valid = unit_file(unit_spec(version, subversion));
			const fs::path dir = crafted_map_with_units("units_old_lenient", valid);
			check_accepted(dir);

			write_all(dir / "war3mapUnits.doo", valid + std::string(4, '\0'));
			check_accepted(dir);

			// Below 1.32 the record has no skin.
			UnitFileSpec no_skin = unit_spec(version, subversion);
			no_skin.skin = "";
			BinaryReader reader = reader_over(unit_file(no_skin));
			std::uint32_t read_version = 0;
			std::uint32_t read_subversion = 0;
			std::vector<Unit> units;
			Units::read_records(reader, {}, game_version(1, 31), read_version, read_subversion, [&](Unit& unit) { units.push_back(unit); });
			CHECK(read_version == version);
			CHECK(read_subversion == subversion);
			REQUIRE(units.size() == 1);
			CHECK(units[0].skin_id == units[0].id);
			CHECK(reader.remaining() == 0);
		}
	}
}

TEST_CASE("a unit made in the editor has the defaults of a fresh World Editor save") {
	const Unit unit;
	CHECK(unit.group_id == 0xFFFFFFFF);
	CHECK(unit.trailer == std::array<std::uint32_t, 2> { 0, 0 });

	Units units;
	Unit& start_location = units.add_start_location(3, glm::vec3(1.f, 2.f, 0.f));
	CHECK(start_location.group_id == 0xFFFFFFFF);
	CHECK(start_location.trailer == std::array<std::uint32_t, 2> { 0, 0 });

	// And paste, which goes through this, keeps what the copied unit had.
	Unit copied;
	copied.group_id = 7;
	copied.trailer = { 8, 9 };
	Unit& pasted = units.add_unit(copied);
	CHECK(pasted.group_id == 7u);
	CHECK(pasted.trailer == std::array<std::uint32_t, 2> { 8, 9 });

	// A start location placed on a v13 map saves with them, and as what read_records then reads back.
	const fs::path dir = make_scratch_dir("units_placed_v13");
	const ScopedMapDirectory open_map(dir);
	units.units.pop_back();
	units.format_version = 13;
	units.format_subversion = 9;
	units.save(terrain_without_gl());
	const std::string saved = read_all(dir / "war3mapUnits.doo");
	CHECK(saved.size() == 16 + 91 + 4 + 16 + 4);
	BinaryReader reader = reader_over(saved);
	std::uint32_t version = 0;
	std::uint32_t subversion = 0;
	int records = 0;
	Units::read_records(reader, {}, game_version(3, 0), version, subversion, [&](Unit& read) {
		records++;
		CHECK(read.id == "sloc");
		CHECK(read.group_id == 0xFFFFFFFF);
	});
	CHECK(records == 1);
	CHECK(version == 13);
	CHECK(subversion == 9);
}

TEST_CASE("the template a new map starts from has a war3mapUnits.doo that saves as (8, 11)") {
	// HiveWE loads this folder for a new map and only clears the units, so the format fields come from this file.
	const std::string bytes = read_all(fs::path(TEST_MAP_DIR) / "war3mapUnits.doo");
	REQUIRE(bytes.size() >= 12);
	CHECK(bytes.substr(0, 4) == "W3do");
	const std::uint32_t version = read_u32_at(bytes, 4);
	CAPTURE(version);
	CHECK((version == 7 || version == 8));
}

TEST_CASE("the writer reproduces every stock war3mapUnits.doo v12 or v13 below HIVEWE_STOCK_BODY_ROOT from read_records") {
	const std::string root_text = env("HIVEWE_STOCK_BODY_ROOT");
	if (root_text.empty()) {
		MESSAGE("skipped: HIVEWE_STOCK_BODY_ROOT is not set");
		return;
	}
	const fs::path root = root_text;

	std::map<std::pair<std::uint32_t, std::uint32_t>, std::size_t> files;
	std::map<std::pair<std::uint32_t, std::uint32_t>, std::size_t> records;
	std::map<int, std::size_t> random_types;
	std::size_t group_other = 0;
	std::size_t trailer_other = 0;
	std::size_t flag_set = 0;
	std::size_t flag_unset = 0;
	std::vector<std::string> failures;

	const std::map<std::string, std::string> index = read_sweep_index(root / "index.json");
	for (const auto& [key, name] : index) {
		CAPTURE(name);
		const std::string original = read_all(root / key / "war3mapUnits.doo");
		if (original.size() < 12) {
			failures.push_back(std::format("{}: war3mapUnits.doo has {} bytes", name, original.size()));
			continue;
		}
		const std::uint32_t version = read_u32_at(original, 4);
		const std::uint32_t subversion = read_u32_at(original, 8);

		MapInfo info {};
		{
			BinaryReader reader = reader_over(read_all(root / key / "war3map.w3i"));
			info.load(reader);
		}

		const bool flag = (info.all_flags & 0x4000) != 0;
		(flag ? flag_set : flag_unset)++;
		if (flag != (subversion == 11)) {
			failures.push_back(std::format("{}: subversion {} with w3i flags {:#x}", name, subversion, info.all_flags));
		}

		if (version < 12) {
			continue;
		}
		files[{ version, subversion }]++;

		BinaryReader reader = reader_over(original);
		std::uint32_t read_version = 0;
		std::uint32_t read_subversion = 0;
		Units::read_records(reader, {}, info, read_version, read_subversion, [&](Unit& unit) {
			records[{ version, subversion }]++;
			random_types[unit.random_type]++;
			group_other += unit.group_id != 0xFFFFFFFF ? 1 : 0;
			trailer_other += unit.trailer != std::array<std::uint32_t, 2> { 0, 0 } ? 1 : 0;
		});
		CHECK(read_version == version);
		CHECK(read_subversion == subversion);

		const std::string saved = read_records_then_save_units(original, info);
		if (saved != original) {
			const auto [from, to] = std::ranges::mismatch(original, saved);
			failures.push_back(std::format(
				"{}: {} bytes became {}, first difference at {}",
				name,
				original.size(),
				saved.size(),
				std::distance(original.begin(), from)
			));
		}
	}

	std::println(
		"stock units round trip: (12,11) {} files {} records, (12,9) {} files {} records, (13,11) {} files {} records, (13,9) {} files {} records, failures {}",
		files[{ 12, 11 }],
		records[{ 12, 11 }],
		files[{ 12, 9 }],
		records[{ 12, 9 }],
		files[{ 13, 11 }],
		records[{ 13, 11 }],
		files[{ 13, 9 }],
		records[{ 13, 9 }],
		failures.size()
	);
	std::println("stock units round trip: random types 0: {}, -1: {}, 2: {}; w3i flag 0x4000 set {}, unset {}", random_types[0], random_types[-1], random_types[2], flag_set, flag_unset);
	for (const std::string& failure : failures) {
		std::println("stock units round trip failed: {}", failure);
	}

	CHECK(index.size() == 466);
	CHECK(files[{ 12, 11 }] == 79);
	CHECK(records[{ 12, 11 }] == 14381);
	CHECK(files[{ 12, 9 }] == 51);
	CHECK(records[{ 12, 9 }] == 8549);
	CHECK(files[{ 13, 11 }] == 57);
	CHECK(records[{ 13, 11 }] == 10321);
	CHECK(files[{ 13, 9 }] == 4);
	CHECK(records[{ 13, 9 }] == 562);
	CHECK(random_types[0] == 30400);
	CHECK(random_types[-1] == 3412);
	CHECK(random_types[2] == 1);
	CHECK(random_types.size() == 3);
	CHECK(group_other == 0);
	CHECK(trailer_other == 0);
	CHECK(flag_set == 355);
	CHECK(flag_unset == 111);
	CHECK_MESSAGE(failures.empty(), std::format("{} stock files failed", failures.size()));
}

namespace {
	/// Plain SHA-256, for tying a fixture to its source.
	std::string sha256_hex(const std::string& bytes) {
		static constexpr std::array<std::uint32_t, 64> k = {
			0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
			0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
			0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
			0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
			0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
			0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
		};
		std::array<std::uint32_t, 8> h = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };

		std::string message = bytes;
		message += static_cast<char>(0x80);
		message.append((120 - message.size() % 64) % 64, '\0');
		const std::uint64_t bits = std::uint64_t { bytes.size() } * 8;
		for (int i = 7; i >= 0; i--) {
			message += static_cast<char>((bits >> (8 * i)) & 0xFF);
		}

		for (std::size_t chunk = 0; chunk < message.size(); chunk += 64) {
			std::array<std::uint32_t, 64> w {};
			for (std::size_t i = 0; i < 16; i++) {
				for (std::size_t j = 0; j < 4; j++) {
					w[i] = (w[i] << 8) | static_cast<unsigned char>(message[chunk + i * 4 + j]);
				}
			}
			for (std::size_t i = 16; i < 64; i++) {
				const std::uint32_t s0 = std::rotr(w[i - 15], 7) ^ std::rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
				const std::uint32_t s1 = std::rotr(w[i - 2], 17) ^ std::rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
				w[i] = w[i - 16] + s0 + w[i - 7] + s1;
			}

			std::array<std::uint32_t, 8> v = h;
			for (std::size_t i = 0; i < 64; i++) {
				const std::uint32_t s1 = std::rotr(v[4], 6) ^ std::rotr(v[4], 11) ^ std::rotr(v[4], 25);
				const std::uint32_t ch = (v[4] & v[5]) ^ (~v[4] & v[6]);
				const std::uint32_t t1 = v[7] + s1 + ch + k[i] + w[i];
				const std::uint32_t s0 = std::rotr(v[0], 2) ^ std::rotr(v[0], 13) ^ std::rotr(v[0], 22);
				const std::uint32_t maj = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
				const std::uint32_t t2 = s0 + maj;
				v = { t1 + t2, v[0], v[1], v[2], v[3] + t1, v[4], v[5], v[6] };
			}
			for (std::size_t i = 0; i < 8; i++) {
				h[i] += v[i];
			}
		}

		std::string hex;
		for (const std::uint32_t word : h) {
			hex += std::format("{:08x}", word);
		}
		return hex;
	}

	/// One record of a v12 or v13 war3mapUnits.doo, walked here rather than by the reader, which wrote one of the files compared.
	struct RawUnitRecord {
		std::string id;
		std::uint32_t creation_number = 0;
		/// The whole record without its 12 position bytes.
		std::string without_position;
		std::array<std::uint32_t, 3> position_bits {};
	};

	std::vector<RawUnitRecord> raw_unit_records(const std::string& bytes) {
		const bool sub_11 = read_u32_at(bytes, 8) >= 11;
		std::size_t at = 16;
		const auto u32 = [&] {
			const std::uint32_t value = read_u32_at(bytes, at);
			at += 4;
			return value;
		};

		std::vector<RawUnitRecord> records(read_u32_at(bytes, 12));
		for (RawUnitRecord& record : records) {
			const std::size_t start = at;
			at += 4 + 4; // id, variation
			const std::size_t position = at;
			for (std::uint32_t& bits : record.position_bits) {
				bits = u32();
			}
			at += 4 + 12 + 4 + 4 + 1 + 4 + 2 + 4 + 4; // angle, scale, skin, group, flags, player, unknown bytes, health, mana
			at += sub_11 ? 4 : 0; // item table
			const std::uint32_t item_sets = u32();
			for (std::uint32_t i = 0; i < item_sets; i++) {
				const std::uint32_t items = u32();
				at += std::size_t { items } * 8;
			}
			at += 12 + (sub_11 ? 12 : 0); // gold, acquisition, level, stats
			const std::uint32_t inventory = u32();
			at += std::size_t { inventory } * 8;
			const std::uint32_t abilities = u32();
			at += std::size_t { abilities } * 12;
			const std::uint32_t random_type = u32();
			if (random_type == 0) {
				at += 4;
			} else if (random_type == 1) {
				at += 8;
			} else if (random_type == 2) {
				const std::uint32_t entries = u32();
				at += std::size_t { entries } * 8;
			}
			at += 8; // color, waygate
			record.creation_number = u32();
			at += 12; // trailer

			record.id = bytes.substr(start, 4);
			record.without_position = bytes.substr(start, position - start) + bytes.substr(position + 12, at - position - 12);
		}
		REQUIRE(at == bytes.size());
		return records;
	}

	/// Terrain's destructor calls GL, so this is kept for the process. As terrain_without_gl, at another offset.
	Terrain& offset_terrain_without_gl(const glm::vec2 offset) {
		static Terrain* const terrain = [] {
			Terrain* const created = new Terrain();
			created->width = 2;
			created->height = 2;
			created->resize_corner_arrays(4);
			return created;
		}();
		terrain->offset = offset;
		return *terrain;
	}

	/// What Terrain::load reads for positions and heights, which it cannot do here as it creates GPU resources.
	Terrain& terrain_from_w3e(const std::string& bytes, const float water_offset) {
		static Terrain* const terrain = new Terrain();
		BinaryReader reader = reader_over(bytes);
		reader.advance(4);
		const std::uint32_t version = reader.read<std::uint32_t>();
		reader.advance(5);
		reader.advance(std::size_t { reader.read<std::uint32_t>() } * 4);
		reader.advance(std::size_t { reader.read<std::uint32_t>() } * 4);
		terrain->width = reader.read<std::uint32_t>();
		terrain->height = reader.read<std::uint32_t>();
		terrain->offset = reader.read<glm::vec2>();
		terrain->water_offset = water_offset;
		const std::size_t corners = std::size_t { static_cast<std::uint32_t>(terrain->width) } * static_cast<std::uint32_t>(terrain->height);
		terrain->resize_corner_arrays(corners);
		for (std::size_t i = 0; i < corners; i++) {
			terrain->corner_height[i] = (reader.read<std::uint16_t>() - 8192.f) / 512.f;
			terrain->corner_water_height[i] = ((reader.read<std::uint16_t>() & 0x3FFF) - 8192.f) / 512.f;
			if (version >= 12) {
				terrain->corner_water[i] = (reader.read<std::uint16_t>() & 0b01'0000'0000) != 0;
			} else {
				terrain->corner_water[i] = (reader.read<std::uint8_t>() & 0b0100'0000) != 0;
			}
			reader.advance(1);
			terrain->corner_layer_height[i] = reader.read<std::uint8_t>() & 0b0000'1111;
		}
		return *terrain;
	}
}

TEST_CASE("the fresh World Editor war3mapUnits.doo fixture is the file it claims to be") {
	CHECK(sha256_hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
	CHECK(sha256_hex(from_hex(fresh_world_editor_units)) == "3c2fb35cbdf13f042a9a6317d3ca1a0acf6d2f64a756da00713087940cba7a55");
}

TEST_CASE("through Units::load and save at a terrain offset, a v13 file keeps all but its positions, which stay close") {
	const std::string bytes = from_hex(fresh_world_editor_units);
	const std::vector<RawUnitRecord> original = raw_unit_records(bytes);
	REQUIRE(original.size() == 3);
	REQUIRE(original[0].position_bits[0] == 0xC5000000); // sloc at x = -2048, the left edge of a map at this offset.
	REQUIRE(original[2].position_bits[0] == 0x430B806A); // Hpal

	const Terrain& terrain = offset_terrain_without_gl(glm::vec2(-2048.f, -2048.f));
	const std::string saved = load_and_save_units(
		"units_terrain_offset",
		bytes,
		game_version(3, 0),
		[](Units& units) {
			CHECK(units.format_version == 13);
			CHECK(units.format_subversion == 9);
			// hgry and Hpal are items here, as there is no object data. The file happens to have sloc first, so the order holds.
			REQUIRE(units.units.size() == 1);
			REQUIRE(units.items.size() == 2);
			CHECK(units.units[0].position.x == 0.f);
			CHECK(units.items[1].position.x == doctest::Approx((std::bit_cast<float>(0x430B806Au) + 2048.f) / 128.f));
		},
		terrain
	);

	const std::vector<RawUnitRecord> after = raw_unit_records(saved);
	REQUIRE(after.size() == original.size());
	CHECK(saved.size() == bytes.size());
	CHECK(saved.substr(0, 16) == bytes.substr(0, 16));
	for (std::size_t i = 0; i < original.size(); i++) {
		CAPTURE(i);
		CHECK(after[i].without_position == original[i].without_position);
		for (std::size_t axis = 0; axis < 2; axis++) {
			CAPTURE(axis);
			const float was = std::bit_cast<float>(original[i].position_bits[axis]);
			const float is = std::bit_cast<float>(after[i].position_bits[axis]);
			CHECK(std::abs(is - was) < 0.001f);
		}
		CHECK(after[i].position_bits[2] == 0u); // The test terrain is flat at 0.
	}
	CHECK(after[0].position_bits[0] == 0xC5000000); // -2048 is exact at this offset.
}

TEST_CASE("a subversion 9 file that was promoted goes back to 9 once no unit needs 11 any more") {
	const fs::path dir = make_scratch_dir("units_promoted_back");
	const ScopedMapDirectory open_map(dir);
	for (const std::uint32_t version : { 12u, 13u }) {
		CAPTURE(version);
		const std::string bytes = unit_file(unit_spec(version, 9));
		write_all(dir / "war3mapUnits.doo", bytes);

		Units units;
		BinaryReader reader = reader_over(bytes);
		std::uint32_t read_version = 0;
		std::uint32_t read_subversion = 0;
		Units::read_records(reader, {}, game_version(3, 0), read_version, read_subversion, [&](Unit& unit) { units.units.push_back(unit); });
		units.format_version = static_cast<int>(read_version);
		units.format_subversion = static_cast<int>(read_subversion);

		units.units[0].strength = 7;
		units.save(terrain_without_gl());
		CHECK(read_u32_at(read_all(dir / "war3mapUnits.doo"), 8) == 11u);
		CHECK(units.format_subversion == 9); // Still what was loaded: the promotion belongs to that save.

		units.units[0].strength = 0;
		units.save(terrain_without_gl());
		CHECK(read_all(dir / "war3mapUnits.doo") == bytes);
	}
}

TEST_CASE("an id with bytes after its first NUL is refused on v12 and up and read as before on v7 and v8") {
	const std::string odd_id("h\0xy", 4);
	const std::array<std::pair<std::string, std::function<void(UnitFileSpec&)>>, 5> fields = { {
		{ "id", [&](UnitFileSpec& spec) { spec.id = odd_id; } },
		{ "skin", [&](UnitFileSpec& spec) { spec.skin = odd_id; } },
		{ "item set", [&](UnitFileSpec& spec) { spec.item_set_id = odd_id; } },
		{ "inventory", [&](UnitFileSpec& spec) { spec.inventory_id = odd_id; } },
		{ "ability", [&](UnitFileSpec& spec) { spec.ability_id = odd_id; } },
	} };

	for (const auto& [field, apply] : fields) {
		CAPTURE(field);
		for (const auto& [version, subversion] : units_v12_pairs) {
			CAPTURE(version);
			CAPTURE(subversion);
			UnitFileSpec spec = unit_spec(version, subversion);
			apply(spec);
			check_units_refused_saying(crafted_map_with_units("units_odd_id", unit_file(spec)), { "after the first NUL", "the " + field + " id" });
		}
		for (const std::uint32_t version : { 7u, 8u }) {
			CAPTURE(version);
			UnitFileSpec spec = unit_spec(version, 11);
			apply(spec);
			check_accepted(crafted_map_with_units("units_odd_id_old", unit_file(spec)));
		}
	}
}

TEST_CASE("an id over 4 bytes is not cut down to another id on save") {
	const fs::path dir = make_scratch_dir("units_long_id");
	const ScopedMapDirectory open_map(dir);
	write_all(dir / "war3mapUnits.doo", "the file as it was");

	const std::array<std::function<void(Unit&)>, 5> edits = {
		[](Unit& unit) { unit.id = "hfoox"; },
		[](Unit& unit) { unit.skin_id = "hfoox"; },
		[](Unit& unit) { unit.item_sets = { ItemSet {} }; unit.item_sets[0].items = { { 100, "hfoox" } }; },
		[](Unit& unit) { unit.items = { { 0, "hfoox" } }; },
		[](Unit& unit) { unit.abilities = { { "hfoox", 0, 1 } }; },
	};
	for (const auto& edit : edits) {
		Units units;
		Unit unit;
		unit.id = "hfoo";
		unit.skin_id = "hfoo";
		unit.position = glm::vec3(0.f);
		unit.scale = glm::vec3(1.f);
		unit.angle = 0.f;
		edit(unit);
		units.add_unit(unit);
		CHECK_THROWS_WITH_AS(units.save(terrain_without_gl()), doctest::Contains("hfoox"), std::runtime_error);
		CHECK(read_all(dir / "war3mapUnits.doo") == "the file as it was");
	}
}

TEST_CASE("the save check finds every over-long id that save would refuse, and the refused save changes no file") {
	const fs::path dir = make_scratch_dir("units_long_id_check");
	const ScopedMapDirectory open_map(dir);
	write_all(dir / "war3mapUnits.doo", "the file as it was");

	struct Case {
		std::string field;
		std::function<void(Unit&)> edit;
	};
	const std::array<Case, 5> cases = { {
		{ "type", [](Unit& unit) { unit.id = "hfoox"; } },
		{ "skin", [](Unit& unit) { unit.skin_id = "hfoox"; } },
		{ "dropped item", [](Unit& unit) { unit.item_sets = { ItemSet {}, ItemSet {} }; unit.item_sets[1].items = { { 50, "ratc" }, { 50, "hfoox" } }; } },
		{ "inventory item", [](Unit& unit) { unit.items = { { 0, "rat6" }, { 1, "hfoox" } }; } },
		{ "ability", [](Unit& unit) { unit.abilities = { { "Ahbz", 0, 1 }, { "hfoox", 0, 1 } }; } },
	} };
	for (const bool as_item : { false, true }) {
		CAPTURE(as_item);
		for (const auto& [field, edit] : cases) {
			CAPTURE(field);
			Units units;
			Unit fine;
			fine.id = "hpea";
			fine.skin_id = "hpea";
			fine.position = glm::vec3(0.f);
			fine.scale = glm::vec3(1.f);
			fine.angle = 0.f;
			Unit unit = fine;
			unit.id = "hfoo";
			unit.skin_id = "hfoo";
			edit(unit);
			(as_item ? units.items : units.units) = { fine, unit };

			const auto found = units.find_id_over_4_bytes();
			REQUIRE(found.has_value());
			CHECK(found->id == "hfoox");
			CHECK(found->field == field);
			CHECK(found->owner_id == (field == "type" ? "hfoox" : "hfoo"));

			// The check and the writer agree: what the check refuses is what write_id would throw on, after the file was left alone.
			CHECK_THROWS_WITH_AS(units.save(terrain_without_gl()), doctest::Contains("hfoox"), std::runtime_error);
			CHECK(read_all(dir / "war3mapUnits.doo") == "the file as it was");
		}
	}
}

TEST_CASE("the save check passes the test map, whose units then save") {
	const fs::path dir = copy_test_map("units_check_valid");
	const ScopedMapDirectory open_map(dir);

	BinaryReader w3i = reader_over(read_all(dir / "war3map.w3i"));
	MapInfo info {};
	info.load(w3i);

	Units units;
	units.load(terrain_without_gl(), info);
	REQUIRE(units.units.size() + units.items.size() > 0);
	CHECK_FALSE(units.find_id_over_4_bytes().has_value());
	CHECK_NOTHROW(units.save(terrain_without_gl()));
}

TEST_CASE("the save check counts the bytes of an id, not its characters") {
	// U+00F6 is two bytes in UTF-8, which is what QString::toStdString gives the object editor.
	const std::string four_bytes = "hf\xC3\xB6";
	const std::string four_characters_five_bytes = "hfo\xC3\xB6";
	CHECK(Units::id_fits(four_bytes));
	CHECK_FALSE(Units::id_fits(four_characters_five_bytes));
	CHECK(Units::id_fits(""));
	CHECK(Units::id_fits("hfoo"));
	CHECK_FALSE(Units::id_fits("hfoox"));

	Units units;
	Unit unit;
	unit.id = "hfoo";
	unit.skin_id = four_bytes;
	units.add_unit(unit);
	CHECK_FALSE(units.find_id_over_4_bytes().has_value());
	units.units[0].skin_id = four_characters_five_bytes;
	const auto found = units.find_id_over_4_bytes();
	REQUIRE(found.has_value());
	CHECK(found->id == four_characters_five_bytes);
	CHECK(found->field == "skin");
}

TEST_CASE("every stock war3mapUnits.doo v12 or v13 below HIVEWE_STOCK_BODY_ROOT keeps all but positions and order through Units::load and save") {
	const std::string root_text = env("HIVEWE_STOCK_BODY_ROOT");
	if (root_text.empty()) {
		MESSAGE("skipped: HIVEWE_STOCK_BODY_ROOT is not set");
		return;
	}
	const fs::path root = root_text;

	// Units::load splits units from items with units_slk, and heights over water need the object data and the tileset's water level.
	// With the game available these are real, custom units from each map's war3map.w3u included. Without it only sloc and the like
	// are units, so the order numbers mean little, and water is at 0.
	const std::string warcraft = env("HIVEWE_BODY_CHECK_CASC");
	const bool has_object_data = !warcraft.empty();
	slk::SLK stock_units;
	slk::SLK water;
	if (has_object_data) {
		REQUIRE(hierarchy.open_casc(warcraft));
		// The stock tables also read data/overrides, relative to the working directory, which under ctest is not the repository.
		const fs::path previous_directory = fs::current_path();
		fs::current_path(fs::path(TEST_MAP_DIR).parent_path().parent_path());
		world_edit_strings = {};
		world_edit_strings.load("UI/WorldEditStrings.txt");
		world_edit_game_strings = {};
		world_edit_game_strings.load("UI/WorldEditGameStrings.txt");
		stock_object_data::Tables tables = stock_object_data::load();
		stock_units = std::move(tables.units.data);
		units_meta_slk = std::move(tables.units.meta);
		water = slk::SLK("TerrainArt/Water.slk");
		fs::current_path(previous_directory);
	}
	std::size_t stock_rows_not_4_bytes = 0;
	for (const auto& [row, index] : stock_units.row_headers) {
		stock_rows_not_4_bytes += row.size() != 4 ? 1 : 0;
	}

	const fs::path scratch = make_scratch_dir("stock_units_real_load");
	const ScopedMapDirectory open_map(scratch);

	std::size_t files = 0;
	std::size_t records = 0;
	std::size_t units_after_load = 0;
	std::size_t items_after_load = 0;
	std::size_t duplicate_keys = 0;
	std::size_t files_order_changed = 0;
	std::array<std::size_t, 3> files_position_changed {};
	std::array<std::size_t, 3> records_position_changed {};
	std::array<float, 3> largest_change {};
	std::array<std::string, 3> largest_change_map;
	std::vector<std::string> failures;
	std::vector<std::string> unloadable_object_files;

	for (const auto& [key, name] : read_sweep_index(root / "index.json")) {
		const std::string original = read_all(root / key / "war3mapUnits.doo");
		const std::uint32_t version = original.size() >= 12 ? read_u32_at(original, 4) : 0;
		if (version != 12 && version != 13) {
			continue;
		}
		CAPTURE(name);
		files++;

		MapInfo info {};
		{
			BinaryReader reader = reader_over(read_all(root / key / "war3map.w3i"));
			info.load(reader);
		}

		const std::string w3e = read_all(root / key / "war3map.w3e");
		REQUIRE(w3e.size() > 8);
		float water_offset = 0.f;
		if (has_object_data) {
			water_offset = water.data<float>("height", std::string(1, w3e[8]) + "Sha");
			units_slk = stock_units;
			for (const std::string_view file : { "war3map.w3u", "war3mapSkin.w3u" }) {
				fs::remove(scratch / file);
				if (fs::exists(root / key / file)) {
					fs::copy_file(root / key / file, scratch / file);
					// What this throws on, Map::load throws on too. The rows read until then still count, so the sweep goes on.
					try {
						load_modification_file(file, units_slk, units_meta_slk, false);
					} catch (const std::exception& error) {
						unloadable_object_files.push_back(std::format("{} {}: {}", name, file, error.what()));
					}
				}
			}
		}
		const Terrain& terrain = terrain_from_w3e(w3e, water_offset);

		write_all(scratch / "war3mapUnits.doo", original);
		Units units;
		units.load(terrain, info);
		units_after_load += units.units.size();
		items_after_load += units.items.size();
		CHECK(units.format_version == static_cast<int>(version));
		units.save(terrain);
		const std::string saved = read_all(scratch / "war3mapUnits.doo");

		// The header holds the version, the subversion and the record count.
		if (saved.substr(0, 16) != original.substr(0, 16)) {
			failures.push_back(std::format("{}: header changed", name));
			continue;
		}

		const std::vector<RawUnitRecord> before = raw_unit_records(original);
		const std::vector<RawUnitRecord> after = raw_unit_records(saved);
		records += before.size();

		// By creation number and id. Where a pair is not unique the records match in file order, which the split between units and items keeps.
		std::map<std::pair<std::uint32_t, std::string>, std::vector<const RawUnitRecord*>> saved_by_key;
		for (auto it = after.rbegin(); it != after.rend(); ++it) {
			saved_by_key[{ it->creation_number, it->id }].push_back(&*it);
		}
		for (const auto& [record_key, matches] : saved_by_key) {
			duplicate_keys += matches.size() > 1 ? 1 : 0;
		}

		std::array<bool, 3> file_changed {};
		for (const RawUnitRecord& record : before) {
			auto& matches = saved_by_key[{ record.creation_number, record.id }];
			if (matches.empty()) {
				failures.push_back(std::format("{}: record {} {} is gone", name, record.id, record.creation_number));
				continue;
			}
			const RawUnitRecord& match = *matches.back();
			matches.pop_back();
			if (match.without_position != record.without_position) {
				failures.push_back(std::format("{}: record {} {} changed outside its position", name, record.id, record.creation_number));
			}
			for (std::size_t axis = 0; axis < 3; axis++) {
				if (match.position_bits[axis] != record.position_bits[axis]) {
					file_changed[axis] = true;
					records_position_changed[axis]++;
					const float change = std::abs(std::bit_cast<float>(match.position_bits[axis]) - std::bit_cast<float>(record.position_bits[axis]));
					if (change > largest_change[axis]) {
						largest_change[axis] = change;
						largest_change_map[axis] = name;
					}
				}
			}
		}
		for (std::size_t axis = 0; axis < 3; axis++) {
			files_position_changed[axis] += file_changed[axis] ? 1 : 0;
		}

		const auto same_place = [](const RawUnitRecord& a, const RawUnitRecord& b) {
			return a.creation_number == b.creation_number && a.id == b.id;
		};
		files_order_changed += std::ranges::equal(before, after, same_place) ? 0 : 1;
	}
	units_slk = slk::SLK();
	units_meta_slk = slk::SLK();

	std::println("stock units through Units::load: object data {}, stock rows not 4 bytes {}", has_object_data ? "from the game" : "absent", stock_rows_not_4_bytes);
	std::println("stock units through Units::load: {} files, {} records, {} units and {} items after load, {} repeated keys, {} failures", files, records, units_after_load, items_after_load, duplicate_keys, failures.size());
	for (std::size_t axis = 0; axis < 3; axis++) {
		std::println(
			"stock units through Units::load: {} changed in {} files and {} records, at most by {} in {}",
			"xyz"[axis],
			files_position_changed[axis],
			records_position_changed[axis],
			largest_change[axis],
			largest_change_map[axis]
		);
	}
	std::println("stock units through Units::load: {} files change order", files_order_changed);
	std::println("stock units through Units::load: {} object files threw on load", unloadable_object_files.size());
	for (const std::string& unloadable : unloadable_object_files) {
		std::println("stock units through Units::load: threw: {}", unloadable);
	}
	for (const std::string& failure : failures | std::views::take(20)) {
		std::println("stock units through Units::load failed: {}", failure);
	}

	CHECK(files == 191);
	CHECK(records == 33813);
	CHECK_MESSAGE(failures.empty(), std::format("{} stock failures", failures.size()));
}

namespace {
	/// Two v7 regions in which every field is set and differs from its neighbours.
	std::string w3r_v7_every_field() {
		Bytes b;
		b.u32(7);
		b.u32(2);
		for (int i = 0; i < 2; i++) {
			b.raw(le_f32(-1024.f - i) + le_f32(-512.f - i) + le_f32(256.f + i) + le_f32(768.f + i));
			b.str(i == 0 ? "First region" : "Second");
			b.u32(41 + i); // creation number
			b.raw(i == 0 ? "RAhr" : "SNbs");
			b.str(i == 0 ? "LordaeronSummerDay" : "");
			b.raw(std::string { '\x11', '\x22', static_cast<char>(0x33 + i), '\xFF' });
			b.u32(0x01020304u + i); // camera blocker
			b.raw(std::string { '\x40', '\x50', '\x60', static_cast<char>(0x70 + i) }); // alpha tile minimap colour
		}
		return b.data;
	}

	/// One v3 camera whose floats all differ, so a field read from the wrong place shows.
	std::string w3c_v3_one_camera() {
		Bytes b;
		b.u32(3);
		b.u32(1);
		for (int i = 0; i < 16; i++) {
			b.raw(le_f32(1.5f + i));
		}
		b.str("Close Arthas");
		b.u32(2); // camera type
		return b.data;
	}

	std::string load_and_save_w3r(const std::string& name, const std::string& bytes, const std::function<void(Regions&)>& check) {
		const fs::path dir = make_scratch_dir(name);
		write_all(dir / "war3map.w3r", bytes);
		const ScopedMapDirectory open_map(dir);
		Regions regions;
		REQUIRE(regions.load(0.f, 0.f));
		check(regions);
		fs::remove(dir / "war3map.w3r");
		regions.save(0.f, 0.f);
		return read_all(dir / "war3map.w3r");
	}
}

TEST_CASE("a v7 war3map.w3r with every field set reads as what it holds and saves as the same bytes") {
	const std::string bytes = w3r_v7_every_field();
	const std::string saved = load_and_save_w3r("w3r_v7_round_trip", bytes, [](Regions& regions) {
		CHECK(regions.format_version == 7);
		REQUIRE(regions.regions.size() == 2);
		const Region& first = regions.regions[0];
		CHECK(first.left == -8.f);
		CHECK(first.bottom == -4.f);
		CHECK(first.right == 2.f);
		CHECK(first.top == 6.f);
		CHECK(first.name == "First region");
		CHECK(first.creation_number == 41);
		CHECK(first.weather_id == "RAhr");
		CHECK(first.ambient_id == "LordaeronSummerDay");
		CHECK(first.color == glm::vec3(0x11, 0x22, 0x33));
		CHECK(first.camera_blocker == 0x01020304);
		CHECK(first.alpha_tile_minimap_color == std::array<std::uint8_t, 4> { 0x40, 0x50, 0x60, 0x70 });
		const Region& second = regions.regions[1];
		CHECK(second.name == "Second");
		CHECK(second.ambient_id.empty());
		CHECK(second.camera_blocker == 0x01020305);
		CHECK(second.alpha_tile_minimap_color[3] == 0x71);
	});
	CHECK(saved == bytes);
}

TEST_CASE("what a war3map.w3r saves as depends on what it was") {
	for (const std::uint32_t version : { 2u, 3u, 5u }) {
		CAPTURE(version);
		const std::string bytes = w3r_file(version).data;
		const std::string saved = load_and_save_w3r("w3r_old", bytes, [](Regions& regions) { CHECK(regions.format_version == 5); });
		CHECK(saved == w3r_file(5).data);
	}

	// A region made in the editor has the v7 fields at 0, and a new map saves as v5.
	const Region made {};
	CHECK(made.camera_blocker == 0);
	CHECK(made.alpha_tile_minimap_color == std::array<std::uint8_t, 4> {});
	CHECK(Regions().format_version == 5);

	// The same object read as v7 and then as v5 saves the second as v5.
	const fs::path dir = make_scratch_dir("w3r_reused");
	const ScopedMapDirectory open_map(dir);
	Regions regions;
	write_all(dir / "war3map.w3r", w3r_file(7).data);
	REQUIRE(regions.load(0.f, 0.f));
	CHECK(regions.format_version == 7);
	write_all(dir / "war3map.w3r", w3r_file(5).data);
	REQUIRE(regions.load(0.f, 0.f));
	CHECK(regions.format_version == 5);
	regions.save(0.f, 0.f);
	CHECK(read_all(dir / "war3map.w3r") == w3r_file(5).data);
}

TEST_CASE("an empty v7 war3map.w3r and v3 war3map.w3c, as a fresh 3.0.1 World Editor save writes them, load") {
	const fs::path dir = make_scratch_dir("w3r_w3c_empty");
	const ScopedMapDirectory open_map(dir);
	write_all(dir / "war3map.w3r", from_hex("0700000000000000"));
	write_all(dir / "war3map.w3c", from_hex("0300000000000000"));
	Regions regions;
	REQUIRE(regions.load(0.f, 0.f));
	CHECK(regions.regions.empty());
	CHECK(regions.format_version == 7);
	GameCameras cameras;
	cameras.load(3, 0, 0.f, 0.f);
	CHECK(cameras.cameras.empty());
	CHECK(cameras.format_version == 3);
	regions.save(0.f, 0.f);
	CHECK(read_all(dir / "war3map.w3r") == from_hex("0700000000000000"));
}

TEST_CASE("a v3 war3map.w3c reads every field from its place") {
	BinaryReader reader = reader_over(w3c_v3_one_camera());
	GameCameras cameras;
	cameras.load(reader, 3, 0, 0.f, 0.f);
	CHECK(reader.remaining() == 0);
	CHECK(cameras.format_version == 3);
	REQUIRE(cameras.cameras.size() == 1);
	const GameCamera& camera = cameras.cameras[0];
	CHECK(camera.target_x == 1.5f / 128.f);
	CHECK(camera.target_y == 2.5f / 128.f);
	CHECK(camera.z_offset == 3.5f / 128.f);
	CHECK(camera.rotation == 4.5f);
	CHECK(camera.angle_of_attack == 5.5f);
	CHECK(camera.distance == 6.5f);
	CHECK(camera.roll == 7.5f);
	CHECK(camera.fov == 8.5f);
	CHECK(camera.far_z == 9.5f);
	CHECK(camera.near_z == 10.5f);
	CHECK(camera.local_pitch == 11.5f);
	CHECK(camera.local_yaw == 12.5f);
	CHECK(camera.local_roll == 13.5f);
	CHECK(camera.dof_distance == 14.5f);
	CHECK(camera.dof_scale == 15.5f);
	CHECK(camera.pos_absolute_z == 16.5f);
	CHECK(camera.name == "Close Arthas");
	CHECK(camera.camera_type == 2);
}

TEST_CASE("a v3 war3map.w3c with a war3map.w3i game version below 1.31 is refused") {
	for (const int minor : { 30, 0 }) {
		CAPTURE(minor);
		BinaryReader reader = reader_over(w3c_v3_one_camera());
		GameCameras cameras;
		CHECK_THROWS_WITH_AS(cameras.load(reader, 1, minor, 0.f, 0.f), doctest::Contains("has not been seen"), std::runtime_error);
	}
	BinaryReader at_131 = reader_over(w3c_v3_one_camera());
	GameCameras cameras;
	CHECK_NOTHROW(cameras.load(at_131, 1, 31, 0.f, 0.f));
	// v0 still reads without local rotation before 1.31.
	BinaryReader v0 = reader_over(le32(0) + le32(1) + std::string(40, '\0') + "Camera" + '\0');
	CHECK_NOTHROW(cameras.load(v0, 1, 30, 0.f, 0.f));
	CHECK(cameras.cameras.at(0).name == "Camera");
}

TEST_CASE("v7 and v3 refuse leftover bytes while v5 and v0 stay lenient") {
	const fs::path dir = make_crafted_map("w3r_w3c_trailing");
	write_all(dir / "war3map.w3r", w3r_file(7).data + '\0');
	check_refused(dir, "war3map.w3r");
	CHECK(contains(check_map_bodies(dir).error(), "leftover bytes"));
	write_all(dir / "war3map.w3r", w3r_file(5).data + '\0');
	check_accepted(dir);

	write_all(dir / "war3map.w3c", w3c_file(3).data + '\0');
	check_refused(dir, "war3map.w3c");
	CHECK(contains(check_map_bodies(dir).error(), "leftover bytes"));
	write_all(dir / "war3map.w3c", w3c_file(0).data + '\0');
	check_accepted(dir);
}

TEST_CASE("every prefix of a v7 war3map.w3r or a v3 war3map.w3c is refused") {
	const fs::path dir = make_crafted_map("w3r_w3c_prefixes");
	write_all(dir / "war3map.w3r", w3r_v7_every_field());
	write_all(dir / "war3map.w3c", w3c_v3_one_camera());
	check_every_prefix_refused(dir, "war3map.w3r");
	check_every_prefix_refused(dir, "war3map.w3c");
}

TEST_CASE("the smallest v7 region and v3 camera are exactly what their counts are measured against") {
	CHECK(w3r_file(7).data.size() - 8 == 38 + std::string("Region").size());
	CHECK(w3c_file(3).data.size() - 8 == 69 + std::string("Camera").size());
	const auto all_zero = [](const std::uint32_t version, const std::size_t record, const std::uint32_t count) {
		return le32(version) + le32(count) + std::string(record * count, '\0');
	};
	const fs::path dir = make_scratch_dir("w3r_w3c_smallest");
	const ScopedMapDirectory open_map(dir);
	Regions regions;
	write_all(dir / "war3map.w3r", all_zero(7, 38, 2));
	CHECK_NOTHROW(regions.load(0.f, 0.f));
	CHECK(regions.regions.size() == 2);
	write_all(dir / "war3map.w3r", all_zero(7, 38, 2).substr(0, 8 + 75));
	CHECK_THROWS(regions.load(0.f, 0.f));

	GameCameras cameras;
	write_all(dir / "war3map.w3c", all_zero(3, 69, 2));
	CHECK_NOTHROW(cameras.load(3, 0, 0.f, 0.f));
	CHECK(cameras.cameras.size() == 2);
	write_all(dir / "war3map.w3c", all_zero(3, 69, 2).substr(0, 8 + 137));
	CHECK_THROWS(cameras.load(3, 0, 0.f, 0.f));
}

TEST_CASE("every stock war3map.w3r v7 below HIVEWE_STOCK_BODY_ROOT saves byte-exact and every w3c v3 reads to its end unchanged") {
	const std::string root_text = env("HIVEWE_STOCK_BODY_ROOT");
	if (root_text.empty()) {
		MESSAGE("skipped: HIVEWE_STOCK_BODY_ROOT is not set");
		return;
	}
	const fs::path root = root_text;
	const fs::path scratch = make_scratch_dir("stock_w3r_w3c");
	const ScopedMapDirectory open_map(scratch);

	std::map<std::uint32_t, std::size_t> w3r_by_version;
	std::map<std::uint32_t, std::size_t> w3c_by_version;
	std::size_t regions_compared = 0;
	std::size_t cameras_read = 0;
	std::vector<std::string> failures;

	const std::map<std::string, std::string> index = read_sweep_index(root / "index.json");
	for (const auto& [key, name] : index) {
		CAPTURE(name);
		const fs::path directory = root / key;
		fs::remove(scratch / "war3map.w3r");
		fs::remove(scratch / "war3map.w3c");

		if (fs::exists(directory / "war3map.w3r")) {
			const std::string original = read_all(directory / "war3map.w3r");
			const std::uint32_t version = leading_u32(directory / "war3map.w3r");
			w3r_by_version[version]++;
			if (version == 7) {
				write_all(scratch / "war3map.w3r", original);
				Regions regions;
				try {
					regions.load(0.f, 0.f);
					regions_compared += regions.regions.size();
					regions.save(0.f, 0.f);
					if (read_all(scratch / "war3map.w3r") != original) {
						failures.push_back(std::format("{}: war3map.w3r changed on save", name));
					}
				} catch (const std::exception& error) {
					failures.push_back(std::format("{}: war3map.w3r: {}", name, error.what()));
				}
			}
		}

		if (fs::exists(directory / "war3map.w3c")) {
			const std::string original = read_all(directory / "war3map.w3c");
			const std::uint32_t version = leading_u32(directory / "war3map.w3c");
			w3c_by_version[version]++;
			if (version == 3) {
				MapInfo info {};
				{
					BinaryReader reader = reader_over(read_all(directory / "war3map.w3i"));
					info.load(reader);
				}
				write_all(scratch / "war3map.w3c", original);
				GameCameras cameras;
				try {
					// v3 refuses leftover bytes, so passing means it read to the end.
					cameras.load(info.game_version_major, info.game_version_minor, 0.f, 0.f);
					cameras_read += cameras.cameras.size();
					cameras.save();
					if (read_all(scratch / "war3map.w3c") != original) {
						failures.push_back(std::format("{}: war3map.w3c changed on save", name));
					}
				} catch (const std::exception& error) {
					failures.push_back(std::format("{}: war3map.w3c: {}", name, error.what()));
				}
			}
		}
	}

	for (const auto& [version, amount] : w3r_by_version) {
		std::println("stock w3r: version {} files {}", version, amount);
	}
	for (const auto& [version, amount] : w3c_by_version) {
		std::println("stock w3c: version {} files {}", version, amount);
	}
	std::println("stock w3r v7: regions saved byte-exact {}; stock w3c v3: cameras read {}; failures {}", regions_compared, cameras_read, failures.size());
	for (const std::string& failure : failures | std::views::take(20)) {
		std::println("stock w3r/w3c failed: {}", failure);
	}
	CHECK(index.size() == 466);
	CHECK(w3r_by_version[7] == 191);
	CHECK(w3c_by_version[3] == 191);
	CHECK_MESSAGE(failures.empty(), std::format("{} stock failures", failures.size()));
}
