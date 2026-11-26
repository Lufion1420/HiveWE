// Which map files the Asset Manager calls used (src/base/map/asset_usage.ixx). A file wrongly listed as unused can be deleted by
// "Select all unused", so each case below is a way a map references a file. Map::get_file_usage only gathers the inputs: it needs a
// Map and so OpenGL, and is checked with `HiveWE --qa-offscreen --qa-open-dialog asset-manager --qa-script-out <file>`.
// doctest's own forward declaration of std::char_traits breaks std::string here otherwise.
#define DOCTEST_CONFIG_USE_STD_HEADERS
#include <doctest/doctest.h>

import std;
import types;
import AssetUsage;
import Imports;
import SLK;
import MDX;
import BinaryWriter;
import MapInfo;
import Sounds;
import Triggers;
import TriggerStrings;
import UnorderedMap;

namespace fs = std::filesystem;
using namespace std::string_literals;

namespace {
	fs::path utf8_to_path(const std::string_view text) {
		return fs::path(std::u8string(text.begin(), text.end()));
	}

	/// A throwaway map folder under the temp directory.
	struct MapFolder {
		fs::path root;
		AssetUsageRequest request;

		MapFolder() {
			std::random_device random;
			root = fs::temp_directory_path() / std::format("hivewe_asset_usage_{}_{}", random(), random());
			fs::create_directories(root);
			request.map_folder = root / "";
			request.unlisted_file_names = { "war3map.j",   "war3map.lua",    "war3map.imp",     "war3map.w3u",     "war3map.w3d",
											"war3map.w3b", "war3map.wts",    "war3map.wtg",     "war3map.wct",     "war3map.w3i",
											"war3map.w3s", "war3mapSkin.w3u", "war3mapSkin.txt", "war3mapMisc.txt" };
		}
		~MapFolder() {
			std::error_code ignored;
			fs::remove_all(root, ignored);
		}
		MapFolder(const MapFolder&) = delete;
		MapFolder& operator=(const MapFolder&) = delete;

		void write(const std::string_view relative, const std::string_view content = "x") const {
			const fs::path path = root / utf8_to_path(relative);
			fs::create_directories(path.parent_path());
			std::ofstream out(path, std::ios::binary);
			out.write(content.data(), static_cast<std::streamsize>(content.size()));
		}

		[[nodiscard]]
		std::vector<FileUsage> usage() const {
			return find_file_usage(request);
		}
	};

	const FileUsage* find(const std::vector<FileUsage>& files, const std::string_view path) {
		const auto found = std::ranges::find_if(files, [&](const FileUsage& file) { return file.path == path; });
		return found == files.end() ? nullptr : &*found;
	}

	bool used(const std::vector<FileUsage>& files, const std::string_view path) {
		const FileUsage* file = find(files, path);
		REQUIRE_MESSAGE(file != nullptr, "file not listed: ", std::string(path));
		return !file->used_by.empty() || file->is_override;
	}

	bool used_by(const std::vector<FileUsage>& files, const std::string_view path, const std::string_view source) {
		const FileUsage* file = find(files, path);
		REQUIRE_MESSAGE(file != nullptr, "file not listed: ", std::string(path));
		return file->used_by.contains(std::string(source));
	}

	/// An object data table holding only the given edits.
	slk::SLK table_with(const std::vector<std::array<std::string, 3>>& edits) {
		slk::SLK table;
		for (const auto& [id, field, value] : edits) {
			table.shadow_data[id][field] = value;
		}
		return table;
	}
}

TEST_CASE("asset usage: every object data field counts, including comma separated lists") {
	MapFolder map;
	map.write("war3mapImported/BTNScore.blp");
	map.write("war3mapImported/First.mdx");
	map.write("war3mapImported/Second.mdx");
	map.write("war3mapImported/BTNUpgradeTwo.blp");
	map.write("war3mapImported/Unrelated.blp");

	// A field no hand kept list holds, a comma separated art list (spelled .mdl) and an upgrade icon list without extensions.
	add_object_references(map.request.references, table_with({ { "h000", "scorescreenicon", "war3mapImported\\BTNScore.blp" } }), false);
	add_object_references(
		map.request.references,
		table_with({ { "A000", "casterart", "war3mapImported\\First.mdl,war3mapImported\\Second.mdl" } }),
		false
	);
	add_object_references(
		map.request.references,
		table_with({ { "R000", "art", "ReplaceableTextures\\CommandButtons\\BTNPeasant.blp,war3mapImported\\BTNUpgradeTwo" } }),
		false
	);

	const auto files = map.usage();
	CHECK(used_by(files, "war3mapImported/BTNScore.blp", "h000"));
	CHECK(used_by(files, "war3mapImported/First.mdx", "A000"));
	CHECK(used_by(files, "war3mapImported/Second.mdx", "A000"));
	CHECK(used_by(files, "war3mapImported/BTNUpgradeTwo.blp", "R000"));
	CHECK_FALSE(used(files, "war3mapImported/Unrelated.blp"));
}

TEST_CASE("asset usage: the 3.0 map files are not listed, and what they name counts as used") {
	MapFolder map;
	// As Map::get_file_usage sets it.
	map.request.unlisted_file_names = Imports().blacklist;
	map.write("war3map.w3l", "W3L!\x03\0\0\0war3mapImported\\Light.mdl\0"s);
	map.write("war3map.w3grp", std::string(12, '\0'));
	map.write("war3mapPostProcessing.txt", "[ASSAO]\r\nRadius=6.000000\r\n");
	map.write("war3mapImported/Light.mdx");
	map.write("war3mapImported/Unrelated.blp");

	const auto files = map.usage();
	CHECK(find(files, "war3map.w3l") == nullptr);
	CHECK(find(files, "war3map.w3grp") == nullptr);
	CHECK(find(files, "war3mapPostProcessing.txt") == nullptr);
	CHECK(files.size() == 2);
	CHECK(used_by(files, "war3mapImported/Light.mdx", "war3map.w3l"));
	CHECK_FALSE(used(files, "war3mapImported/Unrelated.blp"));
}

TEST_CASE("asset usage: the base object id of a custom object is not a file reference") {
	MapFolder map;
	map.write("hfoo.blp");
	add_object_references(map.request.references, table_with({ { "h000", "oldid", "hfoo" } }), false);

	CHECK_FALSE(used(map.usage(), "hfoo.blp"));
}

TEST_CASE("asset usage: doodad and destructible model variations are used") {
	MapFolder map;
	map.write("war3mapImported/Tree0.mdx");
	map.write("war3mapImported/Tree1.mdx");
	map.write("war3mapImported/Tree12.mdx");
	map.write("war3mapImported/Rock20.mdx");
	map.write("Bush3.mdx");
	map.write("war3mapImported/Hero2.mdx");
	map.write("war3mapImported/Base0.mdx");

	add_object_references(
		map.request.references,
		table_with({ { "D000", "file", "war3mapImported\\Tree.mdl" },
					 { "D001", "file", "war3mapImported\\Rock2" },
					 { "D002", "file", "Bush.mdx" } }),
		true
	);
	// A unit has no variations: Hero2 is another file.
	add_object_references(map.request.references, table_with({ { "h000", "file", "war3mapImported\\Hero.mdl" } }), false);

	// Only numvar is edited: the model path comes from the base object.
	slk::SLK doodads = table_with({ { "D003", "numvar", "4" } });
	doodads.base_data["D003"]["file"] = "war3mapImported\\Base";
	add_object_references(map.request.references, doodads, true);

	const auto files = map.usage();
	CHECK(used_by(files, "war3mapImported/Tree0.mdx", "D000"));
	CHECK(used_by(files, "war3mapImported/Tree1.mdx", "D000"));
	CHECK(used_by(files, "war3mapImported/Tree12.mdx", "D000"));
	CHECK(used_by(files, "war3mapImported/Rock20.mdx", "D001"));
	CHECK(used_by(files, "Bush3.mdx", "D002"));
	CHECK(used_by(files, "war3mapImported/Base0.mdx", "D003"));
	CHECK_FALSE(used(files, "war3mapImported/Hero2.mdx"));
}

TEST_CASE("asset usage: every map script location and other script files are read") {
	MapFolder map;
	map.write("war3mapImported/root.blp");
	map.write("war3mapImported/sub.blp");
	map.write("war3mapImported/lib.blp");
	map.write("war3mapImported/ai.blp");
	map.write("war3mapImported/unrelated.blp");
	map.write("war3map.j", "call Preload(\"war3mapImported/root.blp\")");
	map.write("scripts/war3map.lua", "Preload('war3mapImported/sub.blp')");
	map.write("war3mapImported/library.lua", "x = 'war3mapImported/lib.blp'");
	map.write("war3mapImported/melee.ai", "call Preload(\"war3mapImported/ai.blp\")");

	const auto files = map.usage();
	CHECK(used_by(files, "war3mapImported/root.blp", map_script_source));
	CHECK(used_by(files, "war3mapImported/sub.blp", map_script_source));
	CHECK(used_by(files, "war3mapImported/lib.blp", "war3mapImported/library.lua"));
	CHECK(used_by(files, "war3mapImported/ai.blp", "war3mapImported/melee.ai"));
	CHECK_FALSE(used(files, "war3mapImported/unrelated.blp"));
	// Nothing names the script files themselves.
	CHECK_FALSE(used(files, "war3mapImported/library.lua"));
}

TEST_CASE("asset usage: escaped backslashes and the .mdl spelling match") {
	MapFolder map;
	map.write("war3mapImported/My Hero.mdx");
	map.write("war3mapImported/icons/BTNthing.blp");
	map.write("war3mapImported/other.mdx");
	map.write("war3map.lua", R"lua(AddSpecialEffect("war3mapImported\\My Hero.mdl", 0, 0) x = "War3MapImported\\ICONS\\btnthing.blp")lua");

	const auto files = map.usage();
	CHECK(used(files, "war3mapImported/My Hero.mdx"));
	CHECK(used(files, "war3mapImported/icons/BTNthing.blp"));
	CHECK_FALSE(used(files, "war3mapImported/other.mdx"));
}

TEST_CASE("asset usage: textures match with another or no extension, models without one") {
	MapFolder map;
	map.write("war3mapImported/frame.blp");
	map.write("war3mapImported/glow.dds");
	map.write("war3mapImported/BTNbare.tga");
	map.write("war3mapImported/orb.mdx");
	map.write("war3mapImported/frame2.blp");
	map.write("war3mapImported/sound.wav");
	map.write("war3map.lua", R"lua(
a("war3mapImported\\frame.png")
b("war3mapImported\\glow.tif")
c("war3mapImported\\BTNbare")
d("war3mapImported\\orb")
e("war3mapImported\\soundtrack.mp3")
)lua");

	const auto files = map.usage();
	CHECK(used(files, "war3mapImported/frame.blp"));
	CHECK(used(files, "war3mapImported/glow.dds"));
	CHECK(used(files, "war3mapImported/BTNbare.tga"));
	CHECK(used(files, "war3mapImported/orb.mdx"));
	CHECK_FALSE(used(files, "war3mapImported/frame2.blp"));
	CHECK_FALSE(used(files, "war3mapImported/sound.wav"));
}

TEST_CASE("asset usage: audio files match with another or no extension and override game audio of another type") {
	MapFolder map;
	map.write("Units/Hero/Voice.wav");
	map.write("Sound/Spells/NCOW_SFX_Dash_5.wav");
	map.write("war3mapImported/hit.wav");
	map.write("war3mapImported/theme.ogg");
	map.write("war3mapImported/other.wav");
	map.write("war3mapImported/NCOW_SFX_Dash_50.flac");
	map.write("boom.wav");
	map.write(
		"war3map.lua",
		R"lua(PlaySoundExFast("Sound/Spells/NCOW_SFX_Dash_5", 1) a("war3mapImported\\hit.mp3") b("war3mapImported\\theme.flac") -- boom goes it)lua"
	);
	map.request.is_stock_asset = [](const std::string& path) {
		std::string lowercase = path;
		std::ranges::transform(lowercase, lowercase.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return lowercase == "units/hero/voice.ogg";
	};

	const auto files = map.usage();
	CHECK(find(files, "Units/Hero/Voice.wav")->is_override);
	CHECK(used(files, "Sound/Spells/NCOW_SFX_Dash_5.wav"));
	CHECK(used(files, "war3mapImported/hit.wav"));
	CHECK(used(files, "war3mapImported/theme.ogg"));
	CHECK_FALSE(used(files, "war3mapImported/other.wav"));
	CHECK_FALSE(used(files, "war3mapImported/NCOW_SFX_Dash_50.flac"));
	CHECK_FALSE(used(files, "boom.wav"));
}

TEST_CASE("asset usage: a name that is not valid Unicode is searched but never listed") {
	MapFolder map;
	map.write("war3mapImported/referenced.blp");
	map.write("war3mapImported/unrelated.blp");
	// An unpaired surrogate: NTFS allows it, a UTF-8 conversion refuses it.
	const std::wstring broken = std::wstring(L"broken") + static_cast<wchar_t>(0xD800);
	fs::create_directories(map.root / broken);
	std::ofstream(map.root / broken / L"inside.blp") << "x";
	std::ofstream(map.root / L"war3mapImported" / (broken + L".blp")) << "x";
	std::ofstream(map.root / L"war3mapImported" / (broken + L".txt")) << "war3mapImported/referenced.blp";

	const auto files = map.usage();
	CHECK(files.size() == 2);
	CHECK(used(files, "war3mapImported/referenced.blp"));
	CHECK_FALSE(used(files, "war3mapImported/unrelated.blp"));
}

TEST_CASE("asset usage: a short file name in the map root is not matched by other words or paths") {
	MapFolder map;
	map.write("stand.blp");
	map.write("ui.blp");
	map.write("a.mdx");
	map.write("war3mapImported/ground.blp");
	map.write("quoted.blp");
	map.write("spaced.blp");
	map.write(
		"war3map.lua",
		"-- the units stand in a row\nlocal standing = 'UI\\\\Widgets\\\\x.blp'\nf('war3mapImported\\\\background.blp') g(\"quoted\")"
	);
	map.write("war3mapSkin.txt", "[CustomSkin]\r\nSomeFrame = spaced\r\n");

	const auto files = map.usage();
	CHECK_FALSE(used(files, "stand.blp"));
	CHECK_FALSE(used(files, "ui.blp"));
	CHECK_FALSE(used(files, "a.mdx"));
	CHECK_FALSE(used(files, "war3mapImported/ground.blp"));
	CHECK(used(files, "quoted.blp"));
	CHECK(used(files, "spaced.blp"));
}

TEST_CASE("asset usage: frame definitions, table of contents and other text files are read") {
	MapFolder map;
	map.write("war3mapImported/ui.toc", "war3mapImported\\ui.fdf\r\n");
	map.write("war3mapImported/ui.fdf", "Frame \"BACKDROP\" \"X\" { BackdropBackground \"war3mapImported\\back.blp\", }");
	map.write("war3mapImported/back.blp");
	map.write("war3mapImported/coin.blp");
	map.write("war3mapImported/misc.blp");
	map.write("war3mapImported/note.blp");
	map.write("war3mapImported/wts.blp");
	map.write("war3mapSkin.txt", "[CustomSkin]\r\nGoldIcon=war3mapImported\\coin.tga\r\n");
	map.write("war3mapMisc.txt", "[Misc]\r\nSomething=war3mapImported/misc.blp\r\n");
	map.write("data/list.txt", "war3mapImported/note.blp");
	map.write("war3map.wts", "STRING 1\r\n{\r\n<img war3mapImported\\wts.blp>\r\n}\r\n");
	map.write("war3map.j", "call BlzLoadTOCFile(\"war3mapImported\\\\ui.toc\")");

	const auto files = map.usage();
	CHECK(used_by(files, "war3mapImported/ui.toc", map_script_source));
	CHECK(used_by(files, "war3mapImported/ui.fdf", "war3mapImported/ui.toc"));
	CHECK(used_by(files, "war3mapImported/back.blp", "war3mapImported/ui.fdf"));
	CHECK(used_by(files, "war3mapImported/coin.blp", "war3mapSkin.txt"));
	CHECK(used_by(files, "war3mapImported/misc.blp", "war3mapMisc.txt"));
	CHECK(used_by(files, "war3mapImported/note.blp", "data/list.txt"));
	CHECK(used_by(files, "war3mapImported/wts.blp", "war3map.wts"));
}

TEST_CASE("asset usage: saved object data bytes count when HiveWE's tables do not hold the reference") {
	MapFolder map;
	map.write("war3mapImported/skipped.mdx");
	map.write("war3mapImported/BTNskin.blp");
	map.write("war3mapImported/skippedtree1.mdx");
	map.write("loaded.blp");
	map.write("war3mapImported/shared.blp");
	// Raw records: ids, field id, type 3, NUL terminated value.
	map.write("war3map.w3u", "\x03\0\0\0h001umdl\x03\0\0\0war3mapImported\\skipped.mdl\0h001\0\0\0\0war3mapImported\\shared.blp\0"s);
	map.write("war3mapSkin.w3u", "\x03\0\0\0uico\x03\0\0\0war3mapImported\\BTNskin\0\0\0\0\0"s);
	map.write("war3map.w3d", "\x03\0\0\0dfil\x03\0\0\0war3mapImported\\skippedtree\0\0\0\0\0"s);
	add_object_references(map.request.references, table_with({ { "h000", "art", "war3mapImported\\shared.blp" } }), false);

	auto files = map.usage();
	CHECK(used_by(files, "war3mapImported/skipped.mdx", "war3map.w3u"));
	CHECK(used_by(files, "war3mapImported/BTNskin.blp", "war3mapSkin.w3u"));
	CHECK(used_by(files, "war3mapImported/skippedtree1.mdx", "war3map.w3d"));
	CHECK_FALSE(used(files, "loaded.blp"));
	// The loaded object explains the use, so the saved copy of the same reference is not listed a second time.
	CHECK(find(files, "war3mapImported/shared.blp")->used_by == std::unordered_set<std::string> { "h000" });

	// With skipped custom objects in the file, its references are always listed: deleting h000 must not leave the file "unused".
	map.request.always_listed_files = { "war3map.w3u" };
	files = map.usage();
	CHECK(used_by(files, "war3mapImported/shared.blp", "war3map.w3u"));
	CHECK(used_by(files, "war3mapImported/shared.blp", "h000"));
}

TEST_CASE("asset usage: models name their textures, attachments and emitters") {
	MapFolder map;
	map.write("war3mapImported/skin.blp");
	map.write("war3mapImported/birth.mdx");
	map.write("war3mapImported/spark.mdx");
	map.write("war3mapImported/unrelated.blp");

	mdx::MDX model;
	model.textures.push_back({ 0, "war3mapImported\\skin.blp", 0 });
	const BinaryWriter textures_only = model.to_mdx(800);
	std::string bytes(textures_only.buffer.begin(), textures_only.buffer.end());
	// Attachment and particle paths are fixed size, NUL padded strings like the texture path.
	bytes += "war3mapImported\\birth.mdl"s + std::string(40, '\0') + "war3mapImported\\spark.mdl"s + std::string(40, '\0');
	map.write("war3mapImported/unit.mdx", bytes);

	const auto files = map.usage();
	CHECK(used_by(files, "war3mapImported/skin.blp", "war3mapImported/unit.mdx"));
	CHECK(used_by(files, "war3mapImported/birth.mdx", "war3mapImported/unit.mdx"));
	CHECK(used_by(files, "war3mapImported/spark.mdx", "war3mapImported/unit.mdx"));
	CHECK_FALSE(used(files, "war3mapImported/unrelated.blp"));
	CHECK_FALSE(used(files, "war3mapImported/unit.mdx"));
}

TEST_CASE("asset usage: names inside a model that are not paths do not mark root files") {
	MapFolder map;
	map.write("stand.blp");
	map.write("hero.mdx");
	map.write("war3mapImported/other.mdx", "MDLX\0\0Stand\0\0\0hero\0\0\0"s);

	const auto files = map.usage();
	CHECK_FALSE(used(files, "stand.blp"));
	CHECK_FALSE(used(files, "hero.mdx"));
}

TEST_CASE("asset usage: a portrait model is used with its model") {
	MapFolder map;
	map.write("war3mapImported/Knight.mdx");
	map.write("war3mapImported/Knight_Portrait.mdx");
	map.write("Mage.mdx");
	map.write("Mage_portrait.mdx");
	map.write("war3mapImported/Alone_portrait.mdx");
	add_object_references(
		map.request.references,
		table_with({ { "h000", "file", "war3mapImported\\Knight.mdl" }, { "h001", "file", "Mage" } }),
		false
	);

	const auto files = map.usage();
	CHECK(used_by(files, "war3mapImported/Knight_Portrait.mdx", "h000"));
	CHECK(used_by(files, "Mage_portrait.mdx", "h001"));
	CHECK_FALSE(used(files, "war3mapImported/Alone_portrait.mdx"));
}

TEST_CASE("asset usage: unsaved triggers, map header and variables count") {
	MapFolder map;
	for (const char* name : { "custom", "header", "param", "sub", "array", "nested", "variable", "unrelated" }) {
		map.write(std::format("war3mapImported/{}.mdx", name));
	}

	Trigger script;
	script.name = "Script";
	script.custom_text = "call Preload(\"war3mapImported\\\\custom.mdl\")";

	TriggerParameter sub_value {};
	sub_value.value = "war3mapImported\\sub.mdl";
	TriggerParameter array_index {};
	array_index.value = "war3mapImported\\array.mdl";
	TriggerParameter parameter {};
	parameter.value = "war3mapImported\\param.mdl";
	parameter.has_sub_parameter = true;
	parameter.sub_parameter.parameters.push_back(sub_value);
	parameter.parameters.push_back(array_index);
	TriggerParameter nested_value {};
	nested_value.value = "war3mapImported\\nested.mdl";
	ECA nested {};
	nested.parameters.push_back(nested_value);
	ECA action {};
	action.parameters.push_back(parameter);
	action.ecas.push_back(nested);
	Trigger gui;
	gui.name = "Gui";
	gui.ecas.push_back(action);

	TriggerVariable variable = make_new_trigger_variable("Model", 1, 0);
	variable.initial_value = "war3mapImported\\variable.mdl";

	add_trigger_references(
		map.request.references,
		"",
		"function A() Preload('war3mapImported/header.mdx') end",
		{ script, gui },
		{ variable }
	);

	const auto files = map.usage();
	CHECK(used_by(files, "war3mapImported/custom.mdx", "Trigger: Script"));
	CHECK(used_by(files, "war3mapImported/header.mdx", map_header_source));
	CHECK(used_by(files, "war3mapImported/param.mdx", "Trigger: Gui"));
	CHECK(used_by(files, "war3mapImported/sub.mdx", "Trigger: Gui"));
	CHECK(used_by(files, "war3mapImported/array.mdx", "Trigger: Gui"));
	CHECK(used_by(files, "war3mapImported/nested.mdx", "Trigger: Gui"));
	CHECK(used_by(files, "war3mapImported/variable.mdx", "Variable: Model"));
	CHECK_FALSE(used(files, "war3mapImported/unrelated.mdx"));
}

TEST_CASE("asset usage: text still being typed in an open editor counts") {
	MapFolder map;
	map.write("war3mapImported/typed.mdx");
	map.write("war3mapImported/unrelated.mdx");
	add_editor_text_reference(map.request.references, "Map Header", "call Preload(\"war3mapImported\\\\typed.mdl\")");

	const auto files = map.usage();
	CHECK(used_by(files, "war3mapImported/typed.mdx", "Open editor: Map Header"));
	CHECK_FALSE(used(files, "war3mapImported/unrelated.mdx"));
}

TEST_CASE("asset usage: the game is asked about other extensions only for a file nothing refers to") {
	std::vector<std::string> asked;
	const std::function<bool(const std::string&)> game = [&](const std::string& path) {
		asked.push_back(path);
		return path == "textures/thing.dds";
	};

	CHECK_FALSE(is_picked_up_by_path("Textures/Thing.blp", true, game));
	CHECK(asked == std::vector<std::string> { "Textures/Thing.blp" });
	// What the Asset Manager asks again when the last reference to a file is removed.
	CHECK(is_picked_up_by_path("Textures/Thing.blp", false, game));
	CHECK(std::ranges::count(asked, "textures/thing.blp") == 0);
}

TEST_CASE("asset usage: loading screens, sounds and unsaved trigger strings count") {
	MapFolder map;
	map.write("war3mapImported/loading.mdx");
	map.write("war3mapImported/prologue.mdx");
	map.write("war3mapImported/boom.wav");
	map.write("war3mapImported/boom2.flac");
	map.write("war3mapImported/tooltip.blp");

	MapInfo info;
	info.loading_screen_number = 3; // not the "imported model" setting: the path is still stored
	info.loading_screen_model = "war3mapImported\\loading.mdl";
	info.prologue_screen_model = "war3mapImported\\prologue.mdl";
	add_map_info_references(map.request.references, info);

	Sound sound {};
	sound.name = "gg_snd_boom";
	sound.file = "war3mapImported\\boom.wav";
	sound.file_v2 = "war3mapImported\\boom2.flac";
	add_sound_references(map.request.references, { sound });

	TriggerStrings strings;
	std::string key;
	strings.set_string(key, "|cffff0000war3mapImported\\tooltip.blp|r");
	add_trigger_string_references(map.request.references, strings);

	const auto files = map.usage();
	CHECK(used_by(files, "war3mapImported/loading.mdx", loading_screen_source));
	CHECK(used_by(files, "war3mapImported/prologue.mdx", prologue_screen_source));
	CHECK(used_by(files, "war3mapImported/boom.wav", "gg_snd_boom"));
	CHECK(used_by(files, "war3mapImported/boom2.flac", "gg_snd_boom"));
	CHECK(used_by(files, "war3mapImported/tooltip.blp", trigger_strings_source));
}

TEST_CASE("asset usage: file lists and a file's own name are not references") {
	MapFolder map;
	map.write("war3mapImported/orphan.blp");
	map.write("war3mapImported/self.txt", "war3mapImported/self.txt");
	map.write("war3map.imp", "war3mapImported/orphan.blp\0war3mapImported/self.txt\0"s);
	map.write("(listfile)", "war3mapImported/orphan.blp");
	map.write(".git/index", "war3mapImported/orphan.blp");
	map.write("war3mapImported/orphan.blp.hivewe-tmp", "war3mapImported/orphan.blp");

	const auto files = map.usage();
	CHECK_FALSE(used(files, "war3mapImported/orphan.blp"));
	CHECK_FALSE(used(files, "war3mapImported/self.txt"));
	CHECK(find(files, "war3map.imp") == nullptr);
	CHECK(find(files, "war3mapImported/orphan.blp.hivewe-tmp") == nullptr);
}

TEST_CASE("asset usage: dot files and dot folders are never listed, so they cannot be deleted") {
	MapFolder map;
	map.write(".git/index");
	map.write(".git/objects/ab/cdef");
	map.write(".svn/entries");
	map.write(".vscode/settings.json");
	map.write(".gitignore");
	map.write("war3mapImported/.hidden/note.blp");
	map.write("war3mapImported/.DS_Store");
	map.write("war3mapImported/v1.2/kept.blp");

	const auto files = map.usage();
	CHECK(files.size() == 1);
	CHECK(find(files, "war3mapImported/v1.2/kept.blp") != nullptr);
}

TEST_CASE("asset usage: files the game picks up by their path are overrides") {
	MapFolder map;
	map.write("TerrainArt/tile.blp");
	map.write("Textures/stock.blp");
	map.write("Textures/stockdds.blp");
	map.write("units/stock_portrait.mdx");
	map.write("war3mapPreview.dds");
	map.write("_hd.w3mod/Textures/stock.blp");
	map.write("war3mapImported/own.blp");
	const std::set<std::string> stock = { "textures/stock.blp", "textures/stockdds.dds", "units/stock.mdx" };
	map.request.is_stock_asset = [&](const std::string& path) {
		std::string lowercase = path;
		std::ranges::transform(lowercase, lowercase.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return stock.contains(lowercase);
	};

	const auto files = map.usage();
	CHECK(find(files, "TerrainArt/tile.blp")->is_override);
	CHECK(find(files, "Textures/stock.blp")->is_override);
	CHECK(find(files, "Textures/stockdds.blp")->is_override);
	CHECK(find(files, "units/stock_portrait.mdx")->is_override);
	CHECK(find(files, "war3mapPreview.dds")->is_override);
	CHECK(find(files, "_hd.w3mod/Textures/stock.blp")->is_override);
	CHECK_FALSE(used(files, "war3mapImported/own.blp"));
}

TEST_CASE("asset usage: nothing is called unused when a possible reference cannot be read") {
	MapFolder map;
	map.write("war3mapImported/maybe.blp");
	map.write("war3mapImported/known.blp");
	map.write("war3map.lua", "'war3mapImported/maybe.blp'");
	map.write("notes.txt", "war3mapImported/known.blp");
	map.request.read = [](const fs::path& path) -> std::optional<std::string> {
		if (path.filename() == "war3map.lua") {
			return std::nullopt;
		}
		std::ifstream in(path, std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	};

	const auto files = map.usage();
	CHECK(used_by(files, "war3mapImported/maybe.blp", "Could not read war3map.lua"));
	CHECK(find(files, "war3mapImported/known.blp")->used_by == std::unordered_set<std::string> { "notes.txt" });
}

TEST_CASE("asset usage: non-ASCII file names are listed as UTF-8 and matched") {
	MapFolder map;
	const std::string name = "war3mapImported/\xD0\xBC\xD0\xB5\xD1\x87.blp";
	map.write(name);
	map.write("war3map.lua", "x = '" + name + "'");

	CHECK(used(map.usage(), name));
}

TEST_CASE("asset usage: thousands of imports and megabytes of script") {
	MapFolder map;
	std::string script;
	for (int i = 0; i < 3000; i++) {
		map.write(std::format("war3mapImported/file{}.blp", i));
		if (i % 2 == 0) {
			script += std::format("f(\"war3mapImported\\\\file{}.blp\")\n", i);
		}
	}
	while (script.size() < 4 * 1024 * 1024) {
		script += "local x = GetUnitX(udg_Unit) + 100.0 -- a line without any path in it\n";
	}
	map.write("war3map.lua", script);

	// A real lookup opens the game archive several times, so it is asked as little as possible.
	std::size_t lookups = 0;
	std::set<std::string> asked;
	map.request.is_stock_asset = [&](const std::string& path) {
		lookups++;
		asked.insert(path);
		return false;
	};

	const auto start = std::chrono::steady_clock::now();
	const auto files = map.usage();
	MESSAGE("scan ms: ", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
	MESSAGE("game lookups: ", lookups);

	CHECK(files.size() == 3000);
	CHECK(std::ranges::count_if(files, [](const FileUsage& file) { return !file.used_by.empty(); }) == 1500);
	// Its own path for each file, and the three other texture types for each file nothing refers to.
	CHECK(lookups <= 1500 + 1500 * 4);
	CHECK(lookups == asked.size());
}

TEST_CASE("asset usage: portrait and extra model fields and fields kept raw count") {
	MapFolder map;
	map.write("war3mapImported/Portrait.mdx");
	map.write("war3mapImported/Extra.mdx");
	map.write("war3mapImported/RawOnly.mdx");
	map.write("war3mapImported/RawNextToKnown.blp");
	map.write("war3mapImported/Unrelated.mdx");

	slk::SLK units = table_with({ { "h000", "portrait", "war3mapImported\\Portrait.mdx" }, { "h000", "file1", "war3mapImported\\Extra.mdl" } });
	slk::RawModification raw_only;
	raw_only.field_id = "zzzz";
	raw_only.type = 3;
	raw_only.value = "war3mapImported\\RawOnly.mdx";
	units.raw_modifications["hfoo"].push_back(raw_only); // an object whose only change is a field the meta data does not know
	slk::RawModification next_to_known = raw_only;
	next_to_known.value = "war3mapImported\\RawNextToKnown.blp";
	units.raw_modifications["h000"].push_back(next_to_known);
	add_object_references(map.request.references, units, false);

	const auto files = map.usage();
	CHECK(used_by(files, "war3mapImported/Portrait.mdx", "h000"));
	CHECK(used_by(files, "war3mapImported/Extra.mdx", "h000"));
	CHECK(used_by(files, "war3mapImported/RawOnly.mdx", "hfoo"));
	CHECK(used_by(files, "war3mapImported/RawNextToKnown.blp", "h000"));
	CHECK_FALSE(used(files, "war3mapImported/Unrelated.mdx"));
}
