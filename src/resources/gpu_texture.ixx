export module GPUTexture;

import std;
import types;
import BinaryReader;
import ResourceManager;
import Hierarchy;
import BLP;
import Timer;
import Utilities;
import ModelTextureSource;
import <soil2/SOIL2.h>;
import <glad/glad.h>;

namespace fs = std::filesystem;

export class GPUTexture : public Resource {
  public:
	GLuint id = 0;
	GLuint64 bindless_handle = 0;
	bool handle_resident = false; // Singals the render context to set residency as it is per-context

	/// What was loaded, for diagnostics. `missing`: the referenced texture was not found or not decodable, so something else shows.
	std::string source;
	bool missing = false;

	static constexpr const char* name = "GPUTexture";

	/// Must be called from the rendering context
	void make_resident() {
		if (!handle_resident && bindless_handle != 0) {
			glMakeTextureHandleResidentARB(bindless_handle);
			handle_resident = true;
		}
	}

	explicit GPUTexture(const fs::path& path, int flags = 0) {
		fs::path new_path = path;

		BinaryReader reader = [&] {
			ScopedTimer t(profile_casc_ns);
			new_path.replace_extension(".tga");
			return hierarchy.open_file(new_path)
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
					missing = true;
					new_path = "Textures/btntempw.dds";
					return hierarchy.open_file(new_path);
				})
				.value();
		}();

		source = "map or game data: " + new_path.string();
		upload(reader, path.string(), flags);
	}

	/// A texture from one model's own source: exactly that file, with no Hierarchy lookup and no other extension.
	GPUTexture(const ModelTexture& texture, int flags) {
		const std::u8string utf8 = texture.file.u8string();
		const std::string label(utf8.begin(), utf8.end());
		source = "model folder: " + label;

		auto file = [&] {
			ScopedTimer t(profile_casc_ns);
			return read_file(texture.file);
		}();
		if (!file) {
			std::println("Error loading texture {}", label);
			missing = true;
			file = hierarchy.open_file("Textures/btntempw.dds");
		}
		BinaryReader reader = std::move(file).value();
		upload(reader, label, flags);
	}

	virtual ~GPUTexture() {
		if (handle_resident) {
			glMakeTextureHandleNonResidentARB(bindless_handle);
		}
		glDeleteTextures(1, &id);
	}

  private:
	/// Creates the texture `id` from the bytes. False, with nothing created, when they cannot be decoded.
	bool create_texture(BinaryReader& reader) {
		// By content: an alias can answer a .blp request with .dds bytes.
		if (blp::has_blp1_magic(reader)) {
			int width = 0;
			int height = 0;
			int channels = 0;
			const u8* data = [&] {
				ScopedTimer t(profile_parse_ns);
				return blp::load(reader, width, height, channels);
			}();
			if (data == nullptr) {
				return false;
			}

			{
				ScopedTimer t(profile_gl_ns);
				glCreateTextures(GL_TEXTURE_2D, 1, &id);
				glTextureStorage2D(id, std::log2(std::max(width, height)) + 1, GL_RGBA8, width, height);
				glTextureSubImage2D(id, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
				glGenerateTextureMipmap(id);
			}
			delete[] data;
		} else {
			ScopedTimer t(profile_parse_ns); // SOIL does both decode and upload internally
			id = SOIL_load_OGL_texture_from_memory(
				reader.buffer.data(),
				static_cast<int>(reader.buffer.size()),
				SOIL_LOAD_AUTO,
				SOIL_LOAD_AUTO,
				SOIL_FLAG_DDS_LOAD_DIRECT | SOIL_FLAG_SRGB_COLOR_SPACE
			);
			if (id == 0) {
				return false;
			}
			if (!(reader.buffer.size() >= 4 && std::memcmp(reader.buffer.data(), "DDS ", 4) == 0)) {
				// SOIL only loads a mip chain for DDS (via SOIL_FLAG_DDS_LOAD_DIRECT). For other formats
				// (TGA, PNG, ...) it uploads only the base level, but the mipmapped min filter set below
				// then leaves the texture incomplete, so it samples as solid black. Generate the mips.
				glGenerateTextureMipmap(id);
			}
		}
		return true;
	}

	void upload(BinaryReader& reader, const std::string& label, const int flags) {
		if (!create_texture(reader)) {
			// Callers take .value(), so an undecodable file must not become an error: it gets the placeholder, like a missing one.
			std::println("Error loading texture: {}", label);
			missing = true;
			auto placeholder = hierarchy.open_file("Textures/btntempw.dds");
			if (!placeholder || !create_texture(*placeholder)) {
				glCreateTextures(GL_TEXTURE_2D, 1, &id); // an empty texture
			}
		}

		{
			ScopedTimer t(profile_gl_ns);
			glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_WRAP_S, (flags & 1) ? GL_REPEAT : GL_CLAMP_TO_EDGE);
			glTextureParameteri(id, GL_TEXTURE_WRAP_T, (flags & 2) ? GL_REPEAT : GL_CLAMP_TO_EDGE);

			// Make resident later on the rendering context
			bindless_handle = glGetTextureHandleARB(id);
		}
	}
};