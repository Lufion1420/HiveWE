export module Sounds;

import std;
import types;
import BinaryReader;
import BinaryWriter;
import Hierarchy;
import MapFormatCheck;

/// The World Editor stores 2^32 (bit pattern 0x4F800000) in a w3s float field that is left at "use default". It writes no setter for it.
export constexpr float sound_default_float = 4294967296.f;

export bool is_default_sound_float(const float value) {
	return std::bit_cast<u32>(value) == std::bit_cast<u32>(sound_default_float);
}

export struct Sound {
	std::string name;
	std::string file;
	std::string eax_effect;
	u32 flags = 0; // Raw flag word, kept so unknown bits survive a write
	bool looping;
	bool is_3d;
	bool stop_out_of_range;
	bool music;

	int fade_in_rate;
	int fade_out_rate;
	int volume;
	float pitch;
	float pitch_variance;
	int priority;
	int channel;
	float min_distance;
	float max_distance;
	float distance_cutoff;
	float cone_inside;
	float cone_outside;
	int cone_outside_volume;
	float cone_orientation_x;
	float cone_orientation_y;
	float cone_orientation_z;

	// Version 2 fields, in file order. The unknown fields are opaque and only preserved.
	std::string script_name;
	std::string label; // SLK sound label (SetSoundParamsFromLabel); empty when the sound does not use one
	std::string file_v2;
	int text_key = -1; // TRIGSTR number of the dialogue text, -1 for none
	std::string unknown_string_1;
	int speaker_key = -1; // TRIGSTR number of the dialogue speaker name, -1 for none
	std::string unknown_string_2;
	int unknown_int_1 = 0;
	std::string speaker_unit_id;
	std::string facial_animation_label;
	std::string facial_animation_group_label;
	std::string facial_animation_set_path;

	// Version 3 field. 0 on stock dialogue, 1 on stock music; meaning unknown.
	int unknown_int_v3 = 0;
};

/// Play length in milliseconds of an audio file as the World Editor computes it for SetSoundDuration, or nullopt when the format is not
/// supported or the data is malformed. Ogg Vorbis: last granule position * 1000 / sample rate (matches the 3.0.1 World Editor on stock
/// files). WAV (PCM data size / byte rate) and FLAC (STREAMINFO samples / rate) use the same floor rounding (Needs verification against
/// the World Editor). MP3 and Opus are not supported.
export std::optional<int> audio_duration_ms(const std::span<const u8> data) {
	const auto u32_at = [&](const size_t offset) {
		u32 value;
		std::memcpy(&value, data.data() + offset, sizeof(value));
		return value;
	};
	const auto matches = [&](const size_t offset, const std::string_view tag) {
		return offset + tag.size() <= data.size() && std::memcmp(data.data() + offset, tag.data(), tag.size()) == 0;
	};
	const auto to_ms = [](const u64 samples, const u64 rate) -> std::optional<int> {
		if (rate == 0 || samples > std::numeric_limits<u64>::max() / 1000) {
			return std::nullopt;
		}
		const u64 ms = samples * 1000 / rate;
		if (ms > static_cast<u64>(std::numeric_limits<int>::max())) {
			return std::nullopt;
		}
		return static_cast<int>(ms);
	};

	if (matches(0, "OggS")) {
		// The identification header packet starts on the first page; search its start for the Vorbis tag.
		const size_t search_end = std::min<size_t>(data.size(), 512);
		size_t header = std::string_view::npos;
		for (size_t i = 0; i + 16 <= search_end; i++) {
			if (matches(i, "\x01vorbis")) {
				header = i;
				break;
			}
		}
		if (header == std::string_view::npos) {
			return std::nullopt;
		}
		const u32 rate = u32_at(header + 12);

		// Last page: the final "OggS" capture pattern with stream structure version 0 and room for a full page header.
		for (size_t i = data.size() >= 27 ? data.size() - 27 : 0; data.size() >= 27; i--) {
			if (matches(i, "OggS") && data[i + 4] == 0) {
				i64 granule;
				std::memcpy(&granule, data.data() + i + 6, sizeof(granule));
				if (granule < 0) {
					return std::nullopt;
				}
				return to_ms(static_cast<u64>(granule), rate);
			}
			if (i == 0) {
				break;
			}
		}
		return std::nullopt;
	}

	if (matches(0, "RIFF") && matches(8, "WAVE")) {
		u32 byte_rate = 0;
		for (size_t offset = 12; offset + 8 <= data.size();) {
			const u32 chunk_size = u32_at(offset + 4);
			if (matches(offset, "fmt ") && chunk_size >= 16 && offset + 8 + 16 <= data.size()) {
				byte_rate = u32_at(offset + 8 + 8);
			} else if (matches(offset, "data")) {
				return to_ms(chunk_size, byte_rate);
			}
			offset += 8 + static_cast<size_t>(chunk_size) + (chunk_size & 1);
		}
		return std::nullopt;
	}

	if (matches(0, "fLaC")) {
		// The first metadata block is STREAMINFO (type 0, 34 bytes) starting at offset 8.
		if (data.size() < 8 + 18 || (data[4] & 0x7F) != 0) {
			return std::nullopt;
		}
		const u8* info = data.data() + 8;
		const u64 rate = (static_cast<u64>(info[10]) << 12) | (static_cast<u64>(info[11]) << 4) | (info[12] >> 4);
		const u64 samples = (static_cast<u64>(info[13] & 0x0F) << 32) | (static_cast<u64>(info[14]) << 24)
			| (static_cast<u64>(info[15]) << 16) | (static_cast<u64>(info[16]) << 8) | info[17];
		return to_ms(samples, rate);
	}

	return std::nullopt;
}

export class Sounds {
  public:
	std::vector<Sound> sounds;
	u32 version = 3;

	void load() {
		BinaryReader reader = hierarchy.map_file_read("war3map.w3s").value();
		load(reader);
	}

	void load(BinaryReader& reader) {
		version = reader.read<u32>();
		if (!map_format::is_accepted(map_format::w3s_versions, version)) {
			std::cout << "Unknown war3map.w3s version: " << version << " Attempting to load but may crash\n";
		}

		sounds.resize(reader.read_count(71 + (version >= 2 ? 21 : 0) + (version >= 3 ? 4 : 0)));
		for (auto& i : sounds) {
			i.name = reader.read_c_string();
			i.file = reader.read_c_string();
			i.eax_effect = reader.read_c_string();
			i.flags = reader.read<u32>();
			i.looping = i.flags & 0b00000001;
			i.is_3d = i.flags & 0b00000010;
			i.stop_out_of_range = i.flags & 0b00000100;
			i.music = i.flags & 0b00001000;

			i.fade_in_rate = reader.read<u32>();
			i.fade_out_rate = reader.read<u32>();
			i.volume = reader.read<u32>();
			i.pitch = reader.read<float>();
			i.pitch_variance = reader.read<float>();
			i.priority = reader.read<u32>();
			i.channel = reader.read<u32>();
			i.min_distance = reader.read<float>();
			i.max_distance = reader.read<float>();
			i.distance_cutoff = reader.read<float>();
			i.cone_inside = reader.read<float>();
			i.cone_outside = reader.read<float>();
			i.cone_outside_volume = reader.read<u32>();
			i.cone_orientation_x = reader.read<float>();
			i.cone_orientation_y = reader.read<float>();
			i.cone_orientation_z = reader.read<float>();

			if (version >= 2) {
				i.script_name = reader.read_c_string();
				i.label = reader.read_c_string();
				i.file_v2 = reader.read_c_string();
				i.text_key = reader.read<i32>();
				i.unknown_string_1 = reader.read_c_string();
				i.speaker_key = reader.read<i32>();
				i.unknown_string_2 = reader.read_c_string();
				i.unknown_int_1 = reader.read<i32>();
				i.speaker_unit_id = reader.read_c_string();
				i.facial_animation_label = reader.read_c_string();
				i.facial_animation_group_label = reader.read_c_string();
				i.facial_animation_set_path = reader.read_c_string();
				if (version >= 3) {
					i.unknown_int_v3 = reader.read<i32>();
				}
			}
		}
	}

	/// Serializes in the loaded version's layout. Not called by save(): HiveWE does not edit sounds, so the map's w3s stays as it is.
	void write(BinaryWriter& writer) const {
		writer.write<u32>(version);
		writer.write<u32>(static_cast<u32>(sounds.size()));
		for (const auto& i : sounds) {
			writer.write_c_string(i.name);
			writer.write_c_string(i.file);
			writer.write_c_string(i.eax_effect);
			writer.write<u32>(i.flags);
			writer.write<i32>(i.fade_in_rate);
			writer.write<i32>(i.fade_out_rate);
			writer.write<i32>(i.volume);
			writer.write<float>(i.pitch);
			writer.write<float>(i.pitch_variance);
			writer.write<i32>(i.priority);
			writer.write<i32>(i.channel);
			writer.write<float>(i.min_distance);
			writer.write<float>(i.max_distance);
			writer.write<float>(i.distance_cutoff);
			writer.write<float>(i.cone_inside);
			writer.write<float>(i.cone_outside);
			writer.write<i32>(i.cone_outside_volume);
			writer.write<float>(i.cone_orientation_x);
			writer.write<float>(i.cone_orientation_y);
			writer.write<float>(i.cone_orientation_z);

			if (version >= 2) {
				writer.write_c_string(i.script_name);
				writer.write_c_string(i.label);
				writer.write_c_string(i.file_v2);
				writer.write<i32>(i.text_key);
				writer.write_c_string(i.unknown_string_1);
				writer.write<i32>(i.speaker_key);
				writer.write_c_string(i.unknown_string_2);
				writer.write<i32>(i.unknown_int_1);
				writer.write_c_string(i.speaker_unit_id);
				writer.write_c_string(i.facial_animation_label);
				writer.write_c_string(i.facial_animation_group_label);
				writer.write_c_string(i.facial_animation_set_path);
				if (version >= 3) {
					writer.write<i32>(i.unknown_int_v3);
				}
			}
		}
	}

	void save() const {}
};
