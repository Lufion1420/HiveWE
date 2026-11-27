export module BLP;

import std;
import types;
import BinaryReader;
import <turbojpeg.h>;

namespace blp {
	/// True if the bytes are a BLP1 file, the only BLP version load() reads. Decoders pick by content, not by the requested
	/// extension: a filealiases.json target can resolve to a .dds for a .blp request (Hierarchy alias_candidates).
	export bool has_blp1_magic(const BinaryReader& reader) {
		const auto& buffer = reader.buffer;
		return buffer.size() >= 4 && buffer[0] == 'B' && buffer[1] == 'L' && buffer[2] == 'P' && buffer[3] == '1';
	}

	/// A header claiming a longer side is damaged: no renderer holds such a texture.
	constexpr u32 max_side = 16384;

	/// The first mipmap as width * height RGBA pixels, which the caller frees with delete[]. Nullptr for a file that cannot be decoded:
	/// the file is untrusted, so this never throws and never reads or writes out of bounds.
	export u8* load(BinaryReader& reader, int& width, int& height, int& channels) try {
		const std::string magic_number = reader.read_string(4);
		if (magic_number != "BLP1") {
			std::print("Wrong magic number, should be BLP1, is {}\n", magic_number);
			return nullptr;
		}

		int content_type = reader.read<u32>();
		int alpha_bits = reader.read<u32>();

		const u32 header_width = reader.read<u32>();
		const u32 header_height = reader.read<u32>();
		if (header_width == 0 || header_height == 0 || header_width > max_side || header_height > max_side) {
			std::print("Error loading BLP: impossible size {}x{}\n", header_width, header_height);
			return nullptr;
		}
		width = header_width;
		height = header_height;
		channels = 4;
		const std::size_t pixel_count = static_cast<std::size_t>(width) * height;

		// extra and has_mipmaps
		reader.advance(8);

		auto mipmap_offsets = reader.read_vector<u32>(16);
		auto mipmap_sizes = reader.read_vector<u32>(16);
		const std::size_t offset = mipmap_offsets[0];

		auto data = std::make_unique<u8[]>(pixel_count * 4);

		if (content_type == 0) { // jpeg
			const u32 header_size = reader.read<u32>();
			if (header_size > reader.buffer.size() - reader.position || offset > reader.buffer.size()) {
				std::print("Error loading BLP: the JPEG data lies outside the file\n");
				return nullptr;
			}
			// Some of a declared size may lie past the end. That is fine only if what is there decodes without complaint.
			const std::size_t available = reader.buffer.size() - offset;
			const bool cut = mipmap_sizes[0] > available;
			const std::size_t content_size = std::min<std::size_t>(mipmap_sizes[0], available);

			// The shared header followed by this mipmap's content
			const auto header_position = reader.buffer.begin() + reader.position;
			std::vector<u8> jpeg(header_position, header_position + header_size);
			jpeg.insert(jpeg.end(), reader.buffer.begin() + offset, reader.buffer.begin() + offset + content_size);

			tjhandle handle = tjInitDecompress();
			const int success = tjDecompress2(handle, jpeg.data(), static_cast<unsigned long>(jpeg.size()), data.get(), width, 0, height, TJPF_CMYK, 0); // Actually BGRA

			bool failed = false;
			if (success == -1) {
				std::print("Error loading JPEG data from BLP {}\n", tjGetErrorStr());
				// After a mere warning (for example stray bytes) the picture is there, unless the file was cut short.
				failed = cut || tjGetErrorCode(handle) != TJERR_WARNING;
			}
			tjDestroy(handle);
			if (failed) {
				return nullptr;
			}
		} else if (content_type == 1) { // direct
			auto header = reader.read_vector<u32>(256);

			// There might be fake mipmaps or the first mipmap could start within the 256 bytes of the colour header
			// Thus we cannot rely purely on advancing the position by mipmap sizes alone
			if (offset > reader.buffer.size()) {
				std::print("Error loading BLP: the pixel data lies outside the file\n");
				return nullptr;
			}
			reader.position = offset;
			auto rgb = reader.read_vector<u8>(pixel_count);

			if (alpha_bits == 0) {
				for (size_t j = 0; j < rgb.size(); j++) {
					// + (255 << 24) because the header alpha value is always 0 so we add 255
					reinterpret_cast<u32*>(data.get())[j] = header[rgb[j]] + (255 << 24);
				}
			} else {
				auto alpha = reader.read_vector<u8>((pixel_count * static_cast<u32>(alpha_bits) + 7) / 8);

				for (size_t j = 0; j < rgb.size(); j++) {
					reinterpret_cast<u32*>(data.get())[j] = header[rgb[j]];
					switch (alpha_bits) {
						case 8:
							data[j * 4 + 3] = alpha[j];
							break;
						case 4: {
							u8 byte = alpha[j / 2];
							data[j * 4 + 3] = j % 2 ? byte >> 4 : byte & 0b00001111;
							break;
						}
						case 1:
							data[j * 4 + 3] = alpha[j / 8] & (1 << (j % 8));
							break;
					}
				}
			}
		}

		// Data is BGR(A) instead of RGB(A). While GPUs can natively load BGRA some of the code would have to deal with both RGBA and BGRA which is a pita
		for (std::size_t i = 0; i < pixel_count; i++) {
			std::swap(data[i * channels], data[i * channels + 2]);
		}

		return data.release();
	} catch (const std::exception& e) {
		// A short file (out_of_range from the reader) or too large an image (bad_alloc).
		std::print("Error loading BLP: {}\n", e.what());
		return nullptr;
	}
} // namespace blp