// doctest's own forward declaration of std::char_traits breaks std::string here otherwise.
#define DOCTEST_CONFIG_USE_STD_HEADERS
#include <doctest/doctest.h>
#define __STORMLIB_NO_STATIC_LINK__
#include "StormLib.h"

import std;
import MPQ;
import Doodad;
import Doodads;
import Globals;
import Hierarchy;
import MapArchiveCheck;
import MapBodyCheck;
import MapFormatCheck;
import MapInfo;
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
		b.raw(std::string(2, '\0')); // flags, life
		if (version >= 8) {
			b.u32(); // item table
			b.count(1); // item sets
			b.count(1); // items
			b.raw("ratc");
			b.u32();
		}
		b.u32(7); // creation number
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
		return file;
	}

	Bytes w3r_file() {
		Bytes b;
		b.u32(5);
		b.count(1);
		b.u32s(4);
		b.str("Region");
		b.u32();
		b.raw("RLlr");
		b.str("");
		b.raw(std::string(4, '\0'));
		return b;
	}

	Bytes w3c_file() {
		Bytes b;
		b.u32(0);
		b.count(1);
		b.u32s(10 + 3);
		b.str("Camera");
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

	/// A copy of a map the version check refuses, with the files in a format no parser exists for replaced or
	/// dropped, so that the check can reach the bodies of everything else in it.
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
		int deepest = 0;
		std::string deepest_map;
		const int trigger_next_id = Trigger::next_id;

		for (const auto& [key, name] : index) {
			const fs::path directory = root / key;
			if (const auto result = check_map_for_opening(directory); result) {
				accepted++;
			} else {
				refused++;
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
					if (const auto readable = check_map_bodies(copy_with_readable_formats_only(directory)); readable) {
						readable_parts_passed++;
					} else {
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
		for (const auto& [kind, amount] : refusals_by_kind) {
			std::println("{}: refused as {}: {}", label, kind, amount);
		}
		std::println("{}: deepest trigger nesting {} in {}", label, deepest, deepest_map);
		CHECK_MESSAGE(unexpected.empty(), std::format("{} unexpected refusals", unexpected.size()));
		CHECK(deepest <= Triggers::max_parse_depth);
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
	Units::read_records(reader, {}, info, [&](Unit&) { records++; });
	CHECK(records == 1);
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
	}
	SUBCASE("war3mapUnits.doo") {
		check_every_count_refused(dir, "war3mapUnits.doo", units_file(7).bytes);
		check_every_count_refused(dir, "war3mapUnits.doo", units_file(8).bytes);
	}
	SUBCASE("war3map.w3r") {
		check_every_count_refused(dir, "war3map.w3r", w3r_file());
	}
	SUBCASE("war3map.w3c") {
		check_every_count_refused(dir, "war3map.w3c", w3c_file());
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
