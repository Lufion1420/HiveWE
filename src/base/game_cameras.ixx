export module GameCameras;

import std;
import types;
import BinaryReader;
import Hierarchy;
import MapFormatCheck;

export struct GameCamera {
	float target_x;
	float target_y;
	float z_offset;
	float rotation;
	float angle_of_attack;
	float distance;
	float roll;
	float fov;
	float far_z;
	float near_z;
	float local_roll;
	float local_pitch;
	float local_yaw;
	// v3 only. Stored as read; the script generator does not emit them yet.
	float dof_distance = 0;
	float dof_scale = 0;
	float pos_absolute_z = 0;
	std::string name;
	int camera_type = 0;
};

export class GameCameras {
  public:
	std::vector<GameCamera> cameras;
	/// What the file was. save() writes nothing, so war3map.w3c stays the file that was loaded.
	int format_version = 0;

	void load(int game_version_major, int game_version_minor, float terrain_offset_x, float terrain_offset_y) {
		BinaryReader reader = hierarchy.map_file_read("war3map.w3c").value();
		load(reader, game_version_major, game_version_minor, terrain_offset_x, terrain_offset_y);
	}

	void load(BinaryReader& reader, int game_version_major, int game_version_minor, float terrain_offset_x, float terrain_offset_y) {
		int version = reader.read<u32>();
		map_format::require_accepted("war3map.w3c", map_format::w3c_versions, version);

		const bool has_local_rotation = game_version_major * 100 + game_version_minor >= 131;
		const bool has_v3_fields = version >= 3;
		if (has_v3_fields && !has_local_rotation) {
			throw std::runtime_error(std::format(
				"war3map.w3c version {} is paired with a war3map.w3i game version of {}.{}, which has not been seen",
				version,
				game_version_major,
				game_version_minor
			));
		}

		cameras.resize(reader.read_count((has_local_rotation ? 53 : 41) + (has_v3_fields ? 16 : 0)));
		for (auto& i : cameras) {
			// change coordinate system to [0, terrain_width] x [0, terrain_height]
			// as used by other objects in HiveWE
			i.target_x = (reader.read<float>() - terrain_offset_x) / 128.f;
			i.target_y = (reader.read<float>() - terrain_offset_y) / 128.f;
			i.z_offset = reader.read<float>() / 128.f;

			i.rotation = reader.read<float>();
			i.angle_of_attack = reader.read<float>();
			i.distance = reader.read<float>();
			i.roll = reader.read<float>();
			i.fov = reader.read<float>();
			i.far_z = reader.read<float>();
			i.near_z = reader.read<float>();

			if (has_local_rotation) {
				i.local_pitch = reader.read<float>();
				i.local_yaw = reader.read<float>();
				i.local_roll = reader.read<float>();
			}
			if (has_v3_fields) {
				i.dof_distance = reader.read<float>();
				i.dof_scale = reader.read<float>();
				i.pos_absolute_z = reader.read<float>();
			}
			i.name = reader.read_c_string();
			if (has_v3_fields) {
				i.camera_type = reader.read<int>();
			}
		}

		if (has_v3_fields && reader.remaining() != 0) {
			throw std::runtime_error(
				std::format("war3map.w3c version {} has leftover bytes after its last camera: {}", version, reader.remaining())
			);
		}
		format_version = version;
	}

	void save() {}

	void remove_camera(GameCamera* camera) {
		const auto iterator = cameras.begin() + std::distance(cameras.data(), camera);
		cameras.erase(iterator);
	}

	void remove_cameras(const std::unordered_set<GameCamera*>& list) {
		std::erase_if(cameras, [&](GameCamera& camera) {
			return list.contains(&camera);
		});
	}
};
