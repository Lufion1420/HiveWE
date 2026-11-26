module;

// Headers, not import std: .cpp files that include std headers import this module (C2374 in mocs_compilation otherwise).
#include <vector>
#include <filesystem>
#include <stdexcept>
#include <cstdlib>
#include <cstdint>

export module Texture;

import Hierarchy;
import BLP;
import BinaryReader;
import ResourceManager;
import <soil2/SOIL2.h>;

namespace fs = std::filesystem;
using std::size_t;
using std::uint8_t;

/// Decodes a BLP1 (picked by content: an alias can answer a .blp request with .dds bytes) or anything SOIL reads into `pixels`.
/// Returns false, with `pixels` empty, when the bytes cannot be decoded.
export bool decode_image(BinaryReader& reader, int& width, int& height, int& channels, std::vector<uint8_t>& pixels) {
	width = 0;
	height = 0;
	channels = 0;
	pixels.clear();

	if (blp::has_blp1_magic(reader)) {
		uint8_t* image_data = nullptr;
		try {
			image_data = blp::load(reader, width, height, channels);
		} catch (const std::exception&) {
			// A cut or corrupt header: out_of_range from the reader, bad_alloc for an absurd size.
			return false;
		}
		const bool valid = image_data != nullptr && width > 0 && height > 0 && channels > 0;
		if (valid) {
			pixels.assign(image_data, image_data + static_cast<size_t>(width) * height * channels);
		}
		delete[] image_data;
		return valid;
	}

	uint8_t* image_data =
		SOIL_load_image_from_memory(reader.buffer.data(), static_cast<int>(reader.buffer.size()), &width, &height, &channels, SOIL_LOAD_AUTO);
	const bool valid = image_data != nullptr && width > 0 && height > 0 && channels > 0;
	if (valid) {
		pixels.assign(image_data, image_data + static_cast<size_t>(width) * height * channels);
	}
	std::free(image_data); // stb_image allocates with malloc
	return valid;
}

/// The image used for a file that exists but cannot be decoded: the same Textures/btntempw.dds a missing texture gets, or, if that
/// cannot be read either, a 64x64 opaque magenta RGBA image. 64x64 is also a valid ground tile atlas (4x4 tiles of 16 pixels).
export void placeholder_image(int& width, int& height, int& channels, std::vector<uint8_t>& pixels) {
	if (auto reader = hierarchy.open_file("Textures/btntempw.dds"); reader && decode_image(*reader, width, height, channels, pixels)) {
		return;
	}
	width = 64;
	height = 64;
	channels = 4;
	pixels.assign(static_cast<size_t>(width) * height * channels, 255);
	for (size_t i = 0; i < pixels.size(); i += 4) {
		pixels[i + 1] = 0; // green off: magenta
	}
}

export class Texture : public Resource {
  public:
	int width;
	int height;
	int channels;
	std::vector<uint8_t> data;

	static constexpr const char* name = "Texture";

	explicit Texture() = default;
	explicit Texture(const fs::path& path) {
		fs::path new_path = path;

		if (hierarchy.hd) {
			new_path.replace_filename(path.stem().string() + "_diffuse");
		}

		new_path.replace_extension(".tga");
		BinaryReader reader = hierarchy.open_file(new_path)
			.or_else([&](const std::string&) {
				new_path.replace_extension(".blp");
				return hierarchy.open_file(new_path);
			})
			.or_else([&](const std::string&) {
				new_path.replace_extension(".dds");
				return hierarchy.open_file(new_path);
			})
			.or_else([&](const std::string&) {
				std::println("Error loading texture {}", new_path.string());
				new_path = "Textures/btntempw.dds";
				return hierarchy.open_file(new_path);
			})
			.value();

		// Many callers take .value(), so an undecodable file must not become an error: it gets a placeholder, like a missing one.
		if (!decode_image(reader, width, height, channels, data)) {
			std::println("Could not decode texture {}, using a placeholder", new_path.string());
			placeholder_image(width, height, channels, data);
		}
	}
};
