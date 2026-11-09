export module Regions;

import std;
import BinaryReader;
import BinaryWriter;
import Hierarchy;
import MapFormatCheck;
import <glm/glm.hpp>;

export struct Region {
	float left;
	float right;
	float top;
	float bottom;
	std::string name;
	int creation_number;
	std::string weather_id;
	std::string ambient_id;
	glm::vec3 color;
	// v7 only. Preserved as read; what the game does with camera_blocker is not interpreted here.
	int camera_blocker = 0;
	std::array<uint8_t, 4> alpha_tile_minimap_color {};
};

export class Regions {
	static constexpr int write_version = 5;

  public:
	std::vector<Region> regions;
	/// A v7 file saves as v7. Everything else, and a new map, saves as v5.
	int format_version = write_version;

	bool load(float terrain_offset_x, float terrain_offset_y) {
		BinaryReader reader = hierarchy.map_file_read("war3map.w3r").value();
		return load(reader, terrain_offset_x, terrain_offset_y);
	}

	bool load(BinaryReader& reader, float terrain_offset_x, float terrain_offset_y) {
		const int version = reader.read<uint32_t>();
		map_format::require_accepted("war3map.w3r", map_format::w3r_versions, version);
		const bool has_v7_fields = version >= 7;

		regions.resize(reader.read_count(has_v7_fields ? 38 : 30));
		for (auto& i : regions) {
			// change coordinate system to [0, terrain_width] x [0, terrain_height]
			// as used by other objects in HiveWE
			i.left = (reader.read<float>() - terrain_offset_x) / 128.f;
			i.bottom = (reader.read<float>() - terrain_offset_y) / 128.f;
			i.right = (reader.read<float>() - terrain_offset_x) / 128.f;
			i.top = (reader.read<float>() - terrain_offset_y) / 128.f;

			i.name = reader.read_c_string();
			i.creation_number = reader.read<int>();
			i.weather_id = reader.read_string(4);
			i.ambient_id = reader.read_c_string();
			i.color = reader.read<glm::u8vec3>();
			reader.advance(1); // alpha, always 255
			if (has_v7_fields) {
				i.camera_blocker = reader.read<int>();
				i.alpha_tile_minimap_color = reader.read<std::array<uint8_t, 4>>();
			}
		}

		if (has_v7_fields && reader.remaining() != 0) {
			throw std::runtime_error(
				std::format("war3map.w3r version {} has leftover bytes after its last region: {}", version, reader.remaining())
			);
		}
		format_version = has_v7_fields ? 7 : write_version;

		return true;
	}

	void save(float terrain_offset_x, float terrain_offset_y) const {
		BinaryWriter writer;
		const bool has_v7_fields = format_version >= 7;
		writer.write<uint32_t>(format_version);
		writer.write<uint32_t>(static_cast<uint32_t>(regions.size()));

		for (const auto& region : regions) {
			writer.write<float>(region.left * 128.f + terrain_offset_x);
			writer.write<float>(region.bottom * 128.f + terrain_offset_y);
			writer.write<float>(region.right * 128.f + terrain_offset_x);
			writer.write<float>(region.top * 128.f + terrain_offset_y);

			writer.write_c_string(region.name);
			writer.write<int>(region.creation_number);
			writer.write_c_string_padded(region.weather_id, 4);
			writer.write_c_string(region.ambient_id);
			writer.write<glm::u8vec3>(glm::u8vec3(region.color));
			writer.write<uint8_t>(255);
			if (has_v7_fields) {
				writer.write<int>(region.camera_blocker);
				writer.write<std::array<uint8_t, 4>>(region.alpha_tile_minimap_color);
			}
		}

		hierarchy.map_file_write("war3map.w3r", writer.buffer);
	}

	void remove_region(Region* region) {
		const auto iterator = regions.begin() + std::distance(regions.data(), region);
		regions.erase(iterator);
	}

	void remove_regions(const std::unordered_set<Region*>& list) {
		std::erase_if(regions, [&](Region& region) {
			return list.contains(&region);
		});
	}
};
