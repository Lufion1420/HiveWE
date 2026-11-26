export module PathingTexture;

import std;
import types;
import ResourceManager;
import Hierarchy;
import BLP;
import BinaryReader;
import Utilities;
import <soil2/SOIL2.h>;
import <glm/glm.hpp>;

namespace fs = std::filesystem;

export class PathingTexture : public Resource {
  public:
	int width;
	int height;
	int channels;
	std::vector<u8> data;

	bool homogeneous;

	static constexpr const char* name = "PathingTexture";

	/// Opens `path`, or else the same stem as .dds, .blp or .tga (3.0 maps name Unpathable32x32.tga but ship only the .dds).
	/// The decoder is picked from the bytes, not from the extension that opened.
	static BinaryReader open_image(const fs::path& path) {
		auto exact = hierarchy.open_file(path);
		if (exact) {
			return std::move(*exact);
		}
		const std::string requested_extension = to_lowercase_copy(path.extension().string());
		for (const std::string extension : { ".dds", ".blp", ".tga" }) {
			if (extension == requested_extension) {
				continue;
			}
			fs::path candidate = path;
			candidate.replace_extension(extension);
			if (auto file = hierarchy.open_file(candidate)) {
				return std::move(*file);
			}
		}
		throw std::runtime_error(exact.error());
	}

	explicit PathingTexture(const fs::path& path) {
		BinaryReader reader = open_image(path);
		width = 0;
		height = 0;
		channels = 0;
		uint8_t* image_data;

		// By content: the name can say .tga while the bytes are .dds, and an alias can answer a .blp request with .dds bytes.
		const bool is_blp = blp::has_blp1_magic(reader);
		if (is_blp) {
			image_data = blp::load(reader, width, height, channels);
		} else {
			image_data = SOIL_load_image_from_memory(reader.buffer.data(), static_cast<int>(reader.buffer.size()), &width, &height, &channels, SOIL_LOAD_AUTO);
		}
		const auto free_image = [&] {
			if (is_blp) {
				delete[] image_data;
			} else {
				std::free(image_data); // stb_image allocates with malloc
			}
		};
		if (image_data == nullptr || width <= 0 || height <= 0 || channels <= 0) {
			free_image();
			throw std::runtime_error("Could not decode pathing texture " + path.string());
		}
		data = std::vector<u8>(image_data, image_data + width * height * channels);
		free_image();

		homogeneous = true;
		for (size_t i = 0; i < data.size(); i += channels) {
			if (channels == 3) {
				homogeneous = homogeneous && *reinterpret_cast<glm::u8vec3*>(data.data() + i) == *reinterpret_cast<glm::u8vec3*>(data.data());
			} else if (channels == 4) {
				homogeneous = homogeneous && *reinterpret_cast<glm::u8vec4*>(data.data() + i) == *reinterpret_cast<glm::u8vec4*>(data.data());
			}
		}
	}
};