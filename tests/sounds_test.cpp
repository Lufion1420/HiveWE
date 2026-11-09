#include <doctest/doctest.h>

import std;
import types;
import BinaryReader;
import BinaryWriter;
import no_init_allocator;
import Sounds;
import Triggers;

namespace fs = std::filesystem;

namespace {

	BinaryReader read_file(const fs::path& path) {
		std::ifstream file(path, std::ios::binary);
		REQUIRE(file.good());
		std::vector<u8, default_init_allocator<u8>> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		return BinaryReader(std::move(buffer));
	}

	/// (1)ForgottenHollow's war3map.w3s (map 0239) below HIVEWE_STOCK_BODY_ROOT, or empty when the variable is unset.
	fs::path stock_w3s_path() {
#pragma warning(suppress : 4996) // getenv: read on a single-threaded test.
		const char* const root = std::getenv("HIVEWE_STOCK_BODY_ROOT");
		if (root == nullptr || *root == '\0') {
			return {};
		}
		const fs::path path = fs::path(root) / "0239" / "war3map.w3s";
		REQUIRE(fs::exists(path));
		return path;
	}

	Sounds load_sounds(BinaryReader& reader) {
		Sounds sounds;
		sounds.load(reader);
		CHECK(reader.remaining() == 0);
		return sounds;
	}

	// The fields up to the cone orientation, as in every w3s version.
	void write_base(
		BinaryWriter& w,
		const std::string_view name,
		const std::string_view file,
		const u32 flags,
		const int fade,
		const int volume,
		const int channel,
		const float min_distance,
		const float max_distance,
		const float cutoff
	) {
		w.write_c_string(name);
		w.write_c_string(file);
		w.write_c_string("DefaultEAXON");
		w.write<u32>(flags);
		w.write<i32>(fade);
		w.write<i32>(fade);
		w.write<i32>(volume);
		w.write<float>(sound_default_float); // pitch
		w.write<float>(1.f); // pitch variance
		w.write<i32>(1000); // priority
		w.write<i32>(channel);
		w.write<float>(min_distance);
		w.write<float>(max_distance);
		w.write<float>(cutoff);
		w.write<float>(sound_default_float); // cone inside
		w.write<float>(sound_default_float); // cone outside
		w.write<i32>(-1); // cone outside volume
		w.write<float>(sound_default_float);
		w.write<float>(sound_default_float);
		w.write<float>(sound_default_float);
	}

	/// A war3map.w3s written field by field in the stock layout: a dialogue sound and a music-category sound, both with a label from
	/// version 2 on. Version 1 has only the base fields, version 2 lacks the final int of version 3.
	BinaryWriter synthetic_w3s(const u32 version = 3) {
		BinaryWriter w;
		w.write<u32>(version);
		w.write<u32>(2);

		write_base(w, "gg_snd_Line", "Sound/Dialogue/Test/Line.ogg", 0, 0, 120, -1, 3000.f, sound_default_float, 100000.f);
		if (version >= 2) {
			w.write_c_string("gg_snd_Line");
			w.write_c_string("Line-Label");
			w.write_c_string("Sound/Dialogue/Test/Line.ogg");
			w.write<i32>(12); // text key
			w.write_c_string("");
			w.write<i32>(11); // speaker key
			w.write_c_string("");
			w.write<i32>(0);
			w.write_c_string("Hfoo");
			w.write_c_string("Line-Face");
			w.write_c_string("Map");
			w.write_c_string("Sound/Dialogue/FaceAnimation/Test/Hero.animset");
			if (version >= 3) {
				w.write<i32>(0);
			}
		}

		write_base(w, "gg_snd_Tune", "Sound/Music/Test/Tune.ogg", 0x4, 1, 90, 7, 100000.f, 100000.f, sound_default_float);
		if (version >= 2) {
			w.write_c_string("gg_snd_Tune");
			w.write_c_string("Tune");
			w.write_c_string("Sound/Music/Test/Tune.ogg");
			w.write<i32>(-1);
			w.write_c_string("");
			w.write<i32>(-1);
			w.write_c_string("");
			w.write<i32>(0);
			w.write_c_string("");
			w.write_c_string("");
			w.write_c_string("");
			w.write_c_string("");
			if (version >= 3) {
				w.write<i32>(1);
			}
		}
		return w;
	}

	Sounds synthetic_sounds() {
		BinaryReader reader(synthetic_w3s().buffer);
		return load_sounds(reader);
	}

	// SetSoundDuration values the 3.0.1 World Editor wrote for (1)ForgottenHollow, by audio file.
	std::optional<int> stock_duration(const Sound& sound) {
		static const std::unordered_map<std::string, int> durations = {
			{"Sound/Dialogue/UndeadRECamp/Undeadre02m1/A2MU-T592-ANYA.ogg", 4786},
			{"Sound/Dialogue/UndeadRECamp/Undeadre02m1/A2MU-T591-GAREK.ogg", 5271},
			{"Sound/Dialogue/UndeadRECamp/Undeadre02m1/A2MU-T590-GAREK.ogg", 7452},
			{"Sound/Dialogue/UndeadRECamp/Undeadre02m1/A2MU-T593-LEONID.ogg", 3590},
			{"Sound/Music/mp3Music/CrimsonForest.ogg", 354240},
			{"Sound/Music/mp3Music/Forsaken1.ogg", 221974},
			{"Sound/Music/mp3Music/undercity.ogg", 148800},
			{"Sound/Music/mp3Music/Battle_Intense_C.ogg", 71228},
			{"Sound/Music/mp3Music/Battle_Intense_A.ogg", 61669},
			{"Sound/Music/mp3Music/Battle_Intense_B.ogg", 66321},
		};
		if (const auto found = durations.find(sound.file); found != durations.end()) {
			return found->second;
		}
		return std::nullopt;
	}

} // namespace

TEST_CASE("InitSounds for the stock Forgotten Hollow w3s matches the World Editor's war3map.lua") {
	const fs::path path = stock_w3s_path();
	if (path.empty()) {
		MESSAGE("skipped: HIVEWE_STOCK_BODY_ROOT is not set");
		return;
	}
	BinaryReader reader = read_file(path);
	const Sounds stock = load_sounds(reader);
	REQUIRE(stock.sounds.size() == 10);
	MESSAGE("stock Forgotten Hollow w3s checked: " << stock.sounds.size() << " sounds");

	// Lines 32920-32979 of the stock war3map.lua, with HiveWE's tab indentation.
	const std::string expected = "function InitSounds()\n"
								 "\tgg_snd_A2MU_T592_ANYA = CreateSound(\"Sound/Dialogue/UndeadRECamp/Undeadre02m1/A2MU-T592-ANYA.ogg\", false, false, false, 0, 0, \"DefaultEAXON\")\n"
								 "\tSetSoundParamsFromLabel(gg_snd_A2MU_T592_ANYA, \"A2MU-T592-ANYA\")\n"
								 "\tSetSoundFacialAnimationLabel(gg_snd_A2MU_T592_ANYA, \"A2MU-T592-ANYA\")\n"
								 "\tSetSoundFacialAnimationGroupLabel(gg_snd_A2MU_T592_ANYA, \"Map\")\n"
								 "\tSetSoundFacialAnimationSetFilepath(gg_snd_A2MU_T592_ANYA, \"Sound/Dialogue/FaceAnimation/UndeadRE02_01/FacialAnimation/HeroAnya.animset\")\n"
								 "\tSetDialogueSpeakerNameKey(gg_snd_A2MU_T592_ANYA, \"TRIGSTR_309\")\n"
								 "\tSetDialogueTextKey(gg_snd_A2MU_T592_ANYA, \"TRIGSTR_310\")\n"
								 "\tSetSoundDuration(gg_snd_A2MU_T592_ANYA, 4786)\n"
								 "\tSetSoundVolume(gg_snd_A2MU_T592_ANYA, 120)\n"
								 "\tgg_snd_A2MU_T591_GAREK = CreateSound(\"Sound/Dialogue/UndeadRECamp/Undeadre02m1/A2MU-T591-GAREK.ogg\", false, false, false, 0, 0, \"DefaultEAXON\")\n"
								 "\tSetSoundParamsFromLabel(gg_snd_A2MU_T591_GAREK, \"A2MU-T591-GAREK\")\n"
								 "\tSetSoundFacialAnimationLabel(gg_snd_A2MU_T591_GAREK, \"A2MU-T591-GAREK\")\n"
								 "\tSetSoundFacialAnimationGroupLabel(gg_snd_A2MU_T591_GAREK, \"Map\")\n"
								 "\tSetSoundFacialAnimationSetFilepath(gg_snd_A2MU_T591_GAREK, \"Sound/Dialogue/FaceAnimation/UndeadRE02_01/FacialAnimation/heroforsakengarekupgrade.animset\")\n"
								 "\tSetDialogueSpeakerNameKey(gg_snd_A2MU_T591_GAREK, \"TRIGSTR_308\")\n"
								 "\tSetDialogueTextKey(gg_snd_A2MU_T591_GAREK, \"TRIGSTR_307\")\n"
								 "\tSetSoundDuration(gg_snd_A2MU_T591_GAREK, 5271)\n"
								 "\tSetSoundVolume(gg_snd_A2MU_T591_GAREK, 120)\n"
								 "\tgg_snd_A2MU_T590_GAREK = CreateSound(\"Sound/Dialogue/UndeadRECamp/Undeadre02m1/A2MU-T590-GAREK.ogg\", false, false, false, 0, 0, \"DefaultEAXON\")\n"
								 "\tSetSoundParamsFromLabel(gg_snd_A2MU_T590_GAREK, \"A2MU-T590-GAREK\")\n"
								 "\tSetSoundFacialAnimationLabel(gg_snd_A2MU_T590_GAREK, \"A2MU-T590-GAREK\")\n"
								 "\tSetSoundFacialAnimationGroupLabel(gg_snd_A2MU_T590_GAREK, \"Map\")\n"
								 "\tSetSoundFacialAnimationSetFilepath(gg_snd_A2MU_T590_GAREK, \"Sound/Dialogue/FaceAnimation/UndeadRE02_01/FacialAnimation/heroforsakengarekupgrade.animset\")\n"
								 "\tSetDialogueSpeakerNameKey(gg_snd_A2MU_T590_GAREK, \"TRIGSTR_305\")\n"
								 "\tSetDialogueTextKey(gg_snd_A2MU_T590_GAREK, \"TRIGSTR_304\")\n"
								 "\tSetSoundDuration(gg_snd_A2MU_T590_GAREK, 7452)\n"
								 "\tSetSoundVolume(gg_snd_A2MU_T590_GAREK, 120)\n"
								 "\tgg_snd_A2MU_T593_LEONID = CreateSound(\"Sound/Dialogue/UndeadRECamp/Undeadre02m1/A2MU-T593-LEONID.ogg\", false, false, false, 0, 0, \"DefaultEAXON\")\n"
								 "\tSetSoundParamsFromLabel(gg_snd_A2MU_T593_LEONID, \"A2MU-T593-LEONID\")\n"
								 "\tSetSoundFacialAnimationLabel(gg_snd_A2MU_T593_LEONID, \"A2MU-T593-LEONID\")\n"
								 "\tSetSoundFacialAnimationGroupLabel(gg_snd_A2MU_T593_LEONID, \"Map\")\n"
								 "\tSetSoundFacialAnimationSetFilepath(gg_snd_A2MU_T593_LEONID, \"Sound/Dialogue/FaceAnimation/UndeadRE02_01/FacialAnimation/HeroLeonid.animset\")\n"
								 "\tSetDialogueSpeakerNameKey(gg_snd_A2MU_T593_LEONID, \"TRIGSTR_311\")\n"
								 "\tSetDialogueTextKey(gg_snd_A2MU_T593_LEONID, \"TRIGSTR_312\")\n"
								 "\tSetSoundDuration(gg_snd_A2MU_T593_LEONID, 3590)\n"
								 "\tSetSoundVolume(gg_snd_A2MU_T593_LEONID, 120)\n"
								 "\tgg_snd_CrimsonForest = CreateSound(\"Sound/Music/mp3Music/CrimsonForest.ogg\", false, false, true, 1, 1, \"DefaultEAXON\")\n"
								 "\tSetSoundParamsFromLabel(gg_snd_CrimsonForest, \"CrimsonForest\")\n"
								 "\tSetSoundDuration(gg_snd_CrimsonForest, 354240)\n"
								 "\tSetSoundVolume(gg_snd_CrimsonForest, 90)\n"
								 "\tgg_snd_Forsaken1 = CreateSound(\"Sound/Music/mp3Music/Forsaken1.ogg\", false, false, true, 1, 1, \"DefaultEAXON\")\n"
								 "\tSetSoundParamsFromLabel(gg_snd_Forsaken1, \"Forsaken1\")\n"
								 "\tSetSoundDuration(gg_snd_Forsaken1, 221974)\n"
								 "\tSetSoundVolume(gg_snd_Forsaken1, 127)\n"
								 "\tgg_snd_undercity = CreateSound(\"Sound/Music/mp3Music/undercity.ogg\", false, false, true, 1, 1, \"DefaultEAXON\")\n"
								 "\tSetSoundParamsFromLabel(gg_snd_undercity, \"Undercity\")\n"
								 "\tSetSoundDuration(gg_snd_undercity, 148800)\n"
								 "\tSetSoundVolume(gg_snd_undercity, 127)\n"
								 "\tgg_snd_Battle_Intense_C = CreateSound(\"Sound/Music/mp3Music/Battle_Intense_C.ogg\", false, false, true, 1, 1, \"DefaultEAXON\")\n"
								 "\tSetSoundParamsFromLabel(gg_snd_Battle_Intense_C, \"Battle_Intense_C\")\n"
								 "\tSetSoundDuration(gg_snd_Battle_Intense_C, 71228)\n"
								 "\tSetSoundVolume(gg_snd_Battle_Intense_C, 127)\n"
								 "\tgg_snd_Battle_Intense_A = CreateSound(\"Sound/Music/mp3Music/Battle_Intense_A.ogg\", false, false, true, 1, 1, \"DefaultEAXON\")\n"
								 "\tSetSoundParamsFromLabel(gg_snd_Battle_Intense_A, \"Battle_Intense_A\")\n"
								 "\tSetSoundDuration(gg_snd_Battle_Intense_A, 61669)\n"
								 "\tSetSoundVolume(gg_snd_Battle_Intense_A, 127)\n"
								 "\tgg_snd_Battle_Intense_B = CreateSound(\"Sound/Music/mp3Music/Battle_Intense_B.ogg\", false, false, true, 1, 1, \"DefaultEAXON\")\n"
								 "\tSetSoundParamsFromLabel(gg_snd_Battle_Intense_B, \"Battle_Intense_B\")\n"
								 "\tSetSoundDuration(gg_snd_Battle_Intense_B, 66321)\n"
								 "\tSetSoundVolume(gg_snd_Battle_Intense_B, 127)\n"
								 "end\n\n";

	CHECK(generate_sound_script(stock, ScriptMode::lua, stock_duration) == expected);

	const std::string jass = generate_sound_script(stock, ScriptMode::jass, stock_duration);
	CHECK(jass.starts_with("function InitSounds takes nothing returns nothing\n"));
	CHECK(jass.contains("\tset gg_snd_A2MU_T592_ANYA = CreateSound(\"Sound/Dialogue/UndeadRECamp/Undeadre02m1/A2MU-T592-ANYA.ogg\", "
						"false, false, false, 0, 0, \"DefaultEAXON\")\n"
						"\tcall SetSoundParamsFromLabel(gg_snd_A2MU_T592_ANYA, \"A2MU-T592-ANYA\")\n"));
	CHECK(jass.contains("\tcall SetSoundDuration(gg_snd_CrimsonForest, 354240)\n\tcall SetSoundVolume(gg_snd_CrimsonForest, 90)\n"));
	CHECK_FALSE(jass.contains("SetSoundChannel"));
	CHECK_FALSE(jass.contains("SetSoundPitch"));

	// The v2/v3 fields hold what the stock dump shows, and the file is written back byte for byte.
	const Sound& anya = stock.sounds[0];
	CHECK(anya.text_key == 310);
	CHECK(anya.speaker_key == 309);
	CHECK(anya.speaker_unit_id == "Uany");
	CHECK(anya.unknown_int_v3 == 0);
	CHECK(stock.sounds[4].unknown_int_v3 == 1);

	BinaryWriter writer;
	stock.write(writer);
	const BinaryReader original = read_file(path);
	CHECK(std::equal(writer.buffer.begin(), writer.buffer.end(), original.buffer.begin(), original.buffer.end()));
}

TEST_CASE("label sounds get the World Editor's 3.0 call order and no channel or pitch setters") {
	const auto duration = [](const Sound& sound) {
		return std::optional<int>(sound.name == "gg_snd_Line" ? 4786 : 354240);
	};

	const std::string lua = generate_sound_script(synthetic_sounds(), ScriptMode::lua, duration);
	CHECK(lua == "function InitSounds()\n"
				 "\tgg_snd_Line = CreateSound(\"Sound/Dialogue/Test/Line.ogg\", false, false, false, 0, 0, \"DefaultEAXON\")\n"
				 "\tSetSoundParamsFromLabel(gg_snd_Line, \"Line-Label\")\n"
				 "\tSetSoundFacialAnimationLabel(gg_snd_Line, \"Line-Face\")\n"
				 "\tSetSoundFacialAnimationGroupLabel(gg_snd_Line, \"Map\")\n"
				 "\tSetSoundFacialAnimationSetFilepath(gg_snd_Line, \"Sound/Dialogue/FaceAnimation/Test/Hero.animset\")\n"
				 "\tSetDialogueSpeakerNameKey(gg_snd_Line, \"TRIGSTR_11\")\n"
				 "\tSetDialogueTextKey(gg_snd_Line, \"TRIGSTR_12\")\n"
				 "\tSetSoundDuration(gg_snd_Line, 4786)\n"
				 "\tSetSoundVolume(gg_snd_Line, 120)\n"
				 "\tgg_snd_Tune = CreateSound(\"Sound/Music/Test/Tune.ogg\", false, false, true, 1, 1, \"DefaultEAXON\")\n"
				 "\tSetSoundParamsFromLabel(gg_snd_Tune, \"Tune\")\n"
				 "\tSetSoundDuration(gg_snd_Tune, 354240)\n"
				 "\tSetSoundVolume(gg_snd_Tune, 90)\n"
				 "end\n\n");

	const std::string jass = generate_sound_script(synthetic_sounds(), ScriptMode::jass, duration);
	CHECK(jass.starts_with("function InitSounds takes nothing returns nothing\n"
						   "\tset gg_snd_Line = CreateSound(\"Sound/Dialogue/Test/Line.ogg\", false, false, false, 0, 0, \"DefaultEAXON\")\n"
						   "\tcall SetSoundParamsFromLabel(gg_snd_Line, \"Line-Label\")\n"));
	CHECK(jass.contains("\tcall SetDialogueSpeakerNameKey(gg_snd_Line, \"TRIGSTR_11\")\n"
						"\tcall SetDialogueTextKey(gg_snd_Line, \"TRIGSTR_12\")\n"
						"\tcall SetSoundDuration(gg_snd_Line, 4786)\n"
						"\tcall SetSoundVolume(gg_snd_Line, 120)\n"));
	CHECK_FALSE(jass.contains("SetSoundChannel"));
	CHECK_FALSE(jass.contains("SetSoundPitch"));
	CHECK(jass.ends_with("endfunction\n\n"));
}

TEST_CASE("a sound without a label gets its own setters, and default values get none") {
	Sounds sounds;
	Sound sound = synthetic_sounds().sounds.at(0);
	sound.name = "gg_snd_Custom";
	sound.file = "war3mapImported\\custom.wav";
	sound.label.clear();
	sound.facial_animation_label.clear();
	sound.facial_animation_group_label.clear();
	sound.facial_animation_set_path.clear();
	sound.speaker_key = -1;
	sound.text_key = -1;
	sound.is_3d = true;
	sound.channel = 0;
	sound.pitch = 1.f;
	sound.min_distance = 600.f;
	sound.max_distance = 10000.f;
	sound.distance_cutoff = 3000.f;
	sounds.sounds.push_back(sound); // Cone fields stay at the default (2^32 / -1).

	const std::string lua = generate_sound_script(sounds, ScriptMode::lua, [](const Sound&) { return std::optional<int>(1234); });
	CHECK(lua == "function InitSounds()\n"
				 "\tgg_snd_Custom = CreateSound(\"war3mapImported\\\\custom.wav\", false, true, false, 0, 0, \"DefaultEAXON\")\n"
				 "\tSetSoundDuration(gg_snd_Custom, 1234)\n"
				 "\tSetSoundChannel(gg_snd_Custom, 0)\n"
				 "\tSetSoundVolume(gg_snd_Custom, 120)\n"
				 "\tSetSoundPitch(gg_snd_Custom, 1.0)\n"
				 "\tSetSoundDistances(gg_snd_Custom, 600.0, 10000.0)\n"
				 "\tSetSoundDistanceCutoff(gg_snd_Custom, 3000.0)\n"
				 "end\n\n");

	// Reals are written with one decimal in JASS too, never in exponent form. Values from stock scripts: pitch 1.04 is written as
	// 1.0 and 0.85 as 0.9 (map 0044 and 0454); distances 100000.0 appear 242 times.
	sounds.sounds[0].pitch = 0.85f;
	sounds.sounds[0].min_distance = 100000.f;
	sounds.sounds[0].max_distance = 100000.f;
	sounds.sounds[0].distance_cutoff = 0.0001f;
	sounds.sounds[0].cone_inside = 0.f;
	sounds.sounds[0].cone_outside = 0.f;
	sounds.sounds[0].cone_outside_volume = 127;
	sounds.sounds[0].cone_orientation_x = 0.f;
	sounds.sounds[0].cone_orientation_y = 0.f;
	sounds.sounds[0].cone_orientation_z = 0.f;
	const std::string jass = generate_sound_script(sounds, ScriptMode::jass, [](const Sound&) { return std::optional<int>(1234); });
	CHECK(jass.contains("\tcall SetSoundPitch(gg_snd_Custom, 0.9)\n"
						"\tcall SetSoundDistances(gg_snd_Custom, 100000.0, 100000.0)\n"
						"\tcall SetSoundDistanceCutoff(gg_snd_Custom, 0.0)\n"
						"\tcall SetSoundConeAngles(gg_snd_Custom, 0.0, 0.0, 127)\n"
						"\tcall SetSoundConeOrientation(gg_snd_Custom, 0.0, 0.0, 0.0)\n"));
	CHECK_FALSE(jass.contains("e+"));
	CHECK_FALSE(jass.contains("e-"));
	sounds.sounds[0].pitch = 1.04f;
	CHECK(generate_sound_script(sounds, ScriptMode::jass, [](const Sound&) { return std::optional<int>(1234); })
			  .contains("\tcall SetSoundPitch(gg_snd_Custom, 1.0)\n"));

	// An unknown duration omits the setter instead of writing a wrong value.
	sounds.sounds[0].channel = -1;
	sounds.sounds[0].pitch = sound_default_float;
	sounds.sounds[0].is_3d = false;
	const std::string unknown = generate_sound_script(sounds, ScriptMode::lua, [](const Sound&) { return std::optional<int>(); });
	CHECK_FALSE(unknown.contains("SetSoundDuration"));
	CHECK_FALSE(unknown.contains("SetSoundChannel"));
	CHECK_FALSE(unknown.contains("SetSoundPitch"));
	CHECK_FALSE(unknown.contains("SetSoundDistances"));
	CHECK(unknown.contains("\tSetSoundVolume(gg_snd_Custom, 120)\n"));

	// Non-finite floats from a damaged w3s are treated as default and get no setter (nan/inf would not compile in JASS).
	{
		Sounds damaged = sounds;
		damaged.sounds[0].is_3d = true;
		damaged.sounds[0].pitch = std::numeric_limits<float>::quiet_NaN();
		damaged.sounds[0].min_distance = std::numeric_limits<float>::infinity();
		damaged.sounds[0].max_distance = 10000.f;
		damaged.sounds[0].distance_cutoff = -std::numeric_limits<float>::infinity();
		damaged.sounds[0].cone_inside = 0.f;
		damaged.sounds[0].cone_outside = 0.f;
		damaged.sounds[0].cone_outside_volume = 127;
		damaged.sounds[0].cone_orientation_x = 0.f;
		damaged.sounds[0].cone_orientation_y = std::numeric_limits<float>::quiet_NaN();
		damaged.sounds[0].cone_orientation_z = 0.f;
		const std::string jass = generate_sound_script(damaged, ScriptMode::jass, [](const Sound&) { return std::optional<int>(1234); });
		CHECK_FALSE(jass.contains("SetSoundPitch"));
		CHECK_FALSE(jass.contains("SetSoundDistances"));
		CHECK_FALSE(jass.contains("SetSoundDistanceCutoff"));
		CHECK_FALSE(jass.contains("SetSoundConeOrientation"));
		CHECK(jass.contains("\tcall SetSoundConeAngles(gg_snd_Custom, 0.0, 0.0, 127)\n"));
		CHECK_FALSE(jass.contains("nan"));
		CHECK_FALSE(jass.contains("inf"));
	}

	// Stock scripts write a -1 volume as is (for example map 0000, MassTeleportTarget).
	sounds.sounds[0].volume = -1;
	CHECK(generate_sound_script(sounds, ScriptMode::lua, [](const Sound&) { return std::optional<int>(); })
			  .contains("\tSetSoundVolume(gg_snd_Custom, -1)\n"));
}

TEST_CASE("w3s v1 and v2 are read to the end and written back byte for byte") {
	for (const u32 version : {1u, 2u}) {
		CAPTURE(version);
		const BinaryWriter original = synthetic_w3s(version);
		BinaryReader reader(original.buffer);
		const Sounds sounds = load_sounds(reader); // Checks remaining() == 0
		REQUIRE(sounds.sounds.size() == 2);
		CHECK(sounds.version == version);
		CHECK(sounds.sounds[0].name == "gg_snd_Line");
		CHECK(sounds.sounds[1].name == "gg_snd_Tune");
		CHECK(sounds.sounds[1].channel == 7);
		CHECK(sounds.sounds[0].label == (version >= 2 ? "Line-Label" : ""));
		CHECK(sounds.sounds[0].text_key == (version >= 2 ? 12 : -1));

		BinaryWriter writer;
		sounds.write(writer);
		CHECK(writer.buffer.size() == original.buffer.size());
		CHECK(std::equal(writer.buffer.begin(), writer.buffer.end(), original.buffer.begin(), original.buffer.end()));
	}
}

TEST_CASE("w3s v3 fields are read and written back byte for byte") {
	const BinaryWriter original = synthetic_w3s();
	BinaryReader reader(original.buffer);
	const Sounds sounds = load_sounds(reader);
	REQUIRE(sounds.sounds.size() == 2);
	CHECK(sounds.version == 3);

	const Sound& line = sounds.sounds[0];
	CHECK(line.script_name == "gg_snd_Line");
	CHECK(line.label == "Line-Label");
	CHECK(line.file_v2 == "Sound/Dialogue/Test/Line.ogg");
	CHECK(line.text_key == 12);
	CHECK(line.speaker_key == 11);
	CHECK(line.speaker_unit_id == "Hfoo");
	CHECK(line.facial_animation_label == "Line-Face");
	CHECK(line.facial_animation_group_label == "Map");
	CHECK(line.facial_animation_set_path == "Sound/Dialogue/FaceAnimation/Test/Hero.animset");
	CHECK(line.unknown_int_v3 == 0);
	CHECK(line.channel == -1);
	CHECK(is_default_sound_float(line.pitch));

	const Sound& tune = sounds.sounds[1];
	CHECK(tune.label == "Tune");
	CHECK(tune.text_key == -1);
	CHECK(tune.speaker_key == -1);
	CHECK(tune.speaker_unit_id.empty());
	CHECK(tune.unknown_int_v3 == 1);
	CHECK(tune.channel == 7);
	CHECK(tune.stop_out_of_range);

	BinaryWriter writer;
	sounds.write(writer);
	CHECK(writer.buffer.size() == original.buffer.size());
	CHECK(std::equal(writer.buffer.begin(), writer.buffer.end(), original.buffer.begin(), original.buffer.end()));
}

namespace {

	void append(std::vector<u8>& data, const std::string_view bytes) {
		data.insert(data.end(), bytes.begin(), bytes.end());
	}

	template <typename T>
	void append_value(std::vector<u8>& data, const T value) {
		const auto* bytes = reinterpret_cast<const u8*>(&value);
		data.insert(data.end(), bytes, bytes + sizeof(T));
	}

	std::string_view as_text(const std::vector<u8>& bytes) {
		return std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	}

	// An Ogg page header (27 bytes, one segment) followed by `payload`.
	void append_ogg_page(std::vector<u8>& data, const i64 granule, const std::string_view payload) {
		append(data, "OggS");
		data.push_back(0); // Stream structure version
		data.push_back(0); // Header type
		append_value(data, granule);
		append_value<u32>(data, 1); // Serial
		append_value<u32>(data, 0); // Sequence
		append_value<u32>(data, 0); // Checksum (not checked)
		data.push_back(1);
		data.push_back(static_cast<u8>(payload.size()));
		append(data, payload);
	}

	std::vector<u8> vorbis_identification(const u32 rate) {
		std::vector<u8> packet;
		append(packet, std::string_view("\x01vorbis", 7));
		append_value<u32>(packet, 0); // Vorbis version
		packet.push_back(2); // Channels
		append_value(packet, rate);
		packet.resize(30, 0);
		return packet;
	}

} // namespace

TEST_CASE("Ogg Vorbis duration is the last granule position in milliseconds, rounded down") {
	const auto header = vorbis_identification(44100);

	std::vector<u8> dialogue;
	append_ogg_page(dialogue, 0, as_text(header));
	append_ogg_page(dialogue, 100000, "audio");
	append_ogg_page(dialogue, 211067, "audio"); // A2MU-T592-ANYA.ogg: 211067 samples at 44100 Hz; the World Editor writes 4786
	CHECK(audio_duration_ms(dialogue) == 4786);

	std::vector<u8> music;
	append_ogg_page(music, 0, as_text(header));
	append_ogg_page(music, 15621984, "audio"); // CrimsonForest.ogg; the World Editor writes 354240
	CHECK(audio_duration_ms(music) == 354240);
}

TEST_CASE("unsupported or malformed audio has no duration") {
	CHECK_FALSE(audio_duration_ms({}).has_value());

	std::vector<u8> truncated;
	append(truncated, "OggS");
	CHECK_FALSE(audio_duration_ms(truncated).has_value());

	std::vector<u8> opus;
	append_ogg_page(opus, 1000, "OpusHead");
	CHECK_FALSE(audio_duration_ms(opus).has_value());

	std::vector<u8> zero_rate;
	append_ogg_page(zero_rate, 0, as_text(vorbis_identification(0)));
	append_ogg_page(zero_rate, 1000, "audio");
	CHECK_FALSE(audio_duration_ms(zero_rate).has_value());

	std::vector<u8> negative_granule;
	append_ogg_page(negative_granule, 0, as_text(vorbis_identification(44100)));
	append_ogg_page(negative_granule, -1, "audio");
	CHECK_FALSE(audio_duration_ms(negative_granule).has_value());

	// granule * 1000 would overflow 64 bits.
	std::vector<u8> huge_granule;
	append_ogg_page(huge_granule, 0, as_text(vorbis_identification(44100)));
	append_ogg_page(huge_granule, std::numeric_limits<i64>::max(), "audio");
	CHECK_FALSE(audio_duration_ms(huge_granule).has_value());

	// Fits in 64 bits but not in an int of milliseconds.
	std::vector<u8> too_long;
	append_ogg_page(too_long, 0, as_text(vorbis_identification(1)));
	append_ogg_page(too_long, 1'000'000'000'000, "audio");
	CHECK_FALSE(audio_duration_ms(too_long).has_value());

	std::vector<u8> mp3;
	append(mp3, "ID3");
	mp3.resize(64, 0);
	CHECK_FALSE(audio_duration_ms(mp3).has_value());
}

TEST_CASE("WAV and FLAC durations come from the header") {
	std::vector<u8> wav;
	append(wav, "RIFF");
	append_value<u32>(wav, 0);
	append(wav, "WAVE");
	append(wav, "fmt ");
	append_value<u32>(wav, 16);
	append_value<u16>(wav, 1);
	append_value<u16>(wav, 1);
	append_value<u32>(wav, 22050);
	append_value<u32>(wav, 44100); // Byte rate
	append_value<u16>(wav, 2);
	append_value<u16>(wav, 16);
	append(wav, "data");
	append_value<u32>(wav, 66150); // 1.5 s
	CHECK(audio_duration_ms(wav) == 1500);

	std::vector<u8> flac;
	append(flac, "fLaC");
	flac.push_back(0x80); // Last metadata block, type 0 (STREAMINFO)
	flac.push_back(0);
	flac.push_back(0);
	flac.push_back(34);
	std::array<u8, 34> info{};
	// 48000 Hz (0x0BB80 in 20 bits), 2 channels, 16 bits per sample, 96000 samples (36 bits).
	info[10] = 0x0B;
	info[11] = 0xB8;
	info[12] = 0x02;
	info[13] = 0xF0;
	info[14] = 0x00;
	info[15] = 0x01;
	info[16] = 0x77;
	info[17] = 0x00;
	flac.insert(flac.end(), info.begin(), info.end());
	CHECK(audio_duration_ms(flac) == 2000);
}
