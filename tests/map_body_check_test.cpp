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
	/// dropped, so that the check can reach the bodies of everything else in it. A war3map.doo of v12 or v13 has
	/// a parser now, so it stays, and these passes read its real body.
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
		std::println("{}: of those, passes that parsed a real war3map.doo v12 or v13: {}", label, readable_parts_with_real_doo);
		for (const auto& [kind, amount] : refusals_by_kind) {
			std::println("{}: refused as {}: {}", label, kind, amount);
		}
		std::println("{}: deepest trigger nesting {} in {}", label, deepest, deepest_map);
		CHECK_MESSAGE(unexpected.empty(), std::format("{} unexpected refusals", unexpected.size()));
		CHECK(deepest <= Triggers::max_parse_depth);

		// The stock maps of 3.0.1, so that an extraction that lost maps or a stubbed doo cannot pass unnoticed.
		CHECK(index.size() == 466);
		CHECK(accepted == 275);
		CHECK(refused == 191);
		CHECK(refusals_by_kind["version"] == 191);
		CHECK(readable_parts_passed == 190);
		CHECK(readable_parts_with_real_doo == 190);
		CHECK(refusals_by_kind["undead02 wtg, behind a version refusal"] == 1);
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
		check_every_count_refused(dir, "war3map.doo", doo_file(12).bytes);
		check_every_count_refused(dir, "war3map.doo", doo_file(13).bytes);
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
