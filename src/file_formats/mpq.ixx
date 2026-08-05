module;

#define __STORMLIB_NO_STATIC_LINK__
#include "StormLib.h";

export module MPQ;

import std;

namespace fs = std::filesystem;

// A thin wrapper around StormLib https://github.com/ladislav-zezula/StormLib
namespace mpq {
	export class File {
	  public:
		HANDLE handle = nullptr;

		File() = default;
		~File() {
			close();
		}
		File(File&& move) noexcept {
			handle = move.handle;
			move.handle = nullptr;
		}
		File(const File&) = default;
		File& operator=(const File&) = default;
		File& operator=(File&& move) noexcept {
			handle = move.handle;
			move.handle = nullptr;
			return *this;
		}

		std::vector<std::uint8_t> read() const {
			const std::uint32_t size = SFileGetFileSize(handle, nullptr);
			if (size == 0) {
				return {};
			}

			std::vector<std::uint8_t> buffer(size);

#ifdef _MSC_VER
			unsigned long bytes_read;
#else
			unsigned int bytes_read;
#endif
			const bool success = SFileReadFile(handle, buffer.data(), size, &bytes_read, nullptr);
			if (!success) {
				throw std::runtime_error("Failed to read file: " + std::to_string(GetLastError()));
			}
			return buffer;
		}

		/// An implementation using optional. Use this for all reads?
		std::optional<std::vector<std::uint8_t>> read2() const {
			const std::uint32_t size = SFileGetFileSize(handle, nullptr);
			if (size == 0) {
				return {};
			}

			std::vector<std::uint8_t> buffer(size);

#ifdef _MSC_VER
			unsigned long bytes_read;
#else
			unsigned int bytes_read;
#endif
			const bool success = SFileReadFile(handle, buffer.data(), size, &bytes_read, nullptr);
			if (!success) {
				std::cout << "Failed to read file: " << GetLastError() << std::endl;
			}
			return buffer;
		}

		size_t size() const {
			return SFileGetFileSize(handle, nullptr);
		}

		void close() const {
			SFileCloseFile(handle);
		}
	};

	export class MPQ {
	  public:
		HANDLE handle = nullptr;

		MPQ() = default;

		explicit MPQ(const fs::path& path, const unsigned long flags = 0) {
			open(path, flags);
		}

		~MPQ() {
			close();
		}
		MPQ(MPQ&& move)
		noexcept {
			handle = move.handle;
			move.handle = nullptr;
		}
		MPQ(const MPQ&) = default;
		MPQ& operator=(const MPQ&) = delete;
		MPQ& operator=(MPQ&& move) noexcept {
			handle = move.handle;
			move.handle = nullptr;
			return *this;
		}

		bool open(const fs::path& path, const unsigned long flags = 0) {
			return SFileOpenArchive(path.c_str(), 0, flags, &handle);
		}

		void close() {
			SFileCloseArchive(handle);
			handle = nullptr;
		}

		bool compact() {
			return SFileCompactArchive(handle, nullptr, false);
		}

		/// Extracts every file the archive's wildcard search can find. This depends on the
		/// archive's (listfile): without one, StormLib can still enumerate every entry, but
		/// unnamed ones come back under fabricated names like "File00001234.blp" instead of
		/// their real name. Callers that need specific files by name regardless of listfile
		/// presence should use extract_file() instead.
		bool unpack(const fs::path& path) {
			SFILE_FIND_DATA file_data;
			HANDLE find_handle = SFileFindFirstFile(handle, "*", &file_data, nullptr);
			if (find_handle) {
				do {
					fs::create_directories((path / file_data.cFileName).parent_path());
					SFileExtractFile(handle, file_data.cFileName, (path / file_data.cFileName).c_str(), SFILE_OPEN_FROM_MPQ);
				} while (SFileFindNextFile(find_handle, &file_data));
				SFileFindClose(find_handle);
			}

			// Delete unneeded files
			fs::remove(path / "(listfile)");
			fs::remove(path / "(attributes)");
			fs::remove(path / "(war3map.imp)");
			return true;
		}

		/// Extracts a single file by its exact archive-internal name, bypassing wildcard
		/// enumeration entirely. MPQ files are looked up by name hash, so this works even
		/// when the archive has no (listfile). Returns false if the file isn't present.
		bool extract_file(const fs::path& archived_name, const fs::path& disk_path) const {
			if (!file_exists(archived_name)) {
				return false;
			}
			fs::create_directories(disk_path.parent_path());
			return SFileExtractFile(handle, archived_name.string().c_str(), disk_path.c_str(), SFILE_OPEN_FROM_MPQ);
		}

		File file_open(const fs::path& path) const {
			File file;
			const bool opened = SFileOpenFileEx(handle, path.string().c_str(), 0, &file.handle);
			if (!opened) {
				throw std::runtime_error("Failed to read file " + path.string() + " with error: " + std::to_string(GetLastError()));
			}
			return file;
		}

		void file_write(const fs::path& path, const std::vector<std::uint8_t>& data) const {
			HANDLE out_handle;
			bool success = SFileCreateFile(handle, path.string().c_str(), 0, static_cast<DWORD>(data.size()), 0, MPQ_FILE_COMPRESS | MPQ_FILE_REPLACEEXISTING, &out_handle);
			if (!success) {
				std::cout << GetLastError() << " " << path << "\n";
			}

			success = SFileWriteFile(out_handle, data.data(), static_cast<DWORD>(data.size()), MPQ_COMPRESSION_ZLIB);
			if (!success) {
				std::cout << "Writing to file failed: " << GetLastError() << " " << path << "\n";
			}

			success = SFileFinishFile(out_handle);
			if (!success) {
				std::cout << "Finishing write failed: " << GetLastError() << " " << path << "\n";
			}
		}

		void file_remove(const fs::path& path) const {
			SFileRemoveFile(handle, path.string().c_str(), 0);
		}

		bool file_exists(const fs::path& path) const {
			return SFileHasFile(handle, path.string().c_str());
		}

		void file_add(const fs::path& path, const fs::path& new_path) const {
#ifdef _MSC_VER
			bool success = SFileAddFileEx(handle, path.wstring().c_str(), new_path.string().c_str(), MPQ_FILE_COMPRESS | MPQ_FILE_REPLACEEXISTING, MPQ_COMPRESSION_ZLIB, MPQ_COMPRESSION_ZLIB);
#else
			bool success = SFileAddFileEx(handle, path.string().c_str(), new_path.string().c_str(), MPQ_FILE_COMPRESS | MPQ_FILE_REPLACEEXISTING, MPQ_COMPRESSION_ZLIB, MPQ_COMPRESSION_ZLIB);
#endif
			if (!success) {
				std::cout << "Error adding file: " << GetLastError() << "\n";
			}
		}
	};
} // namespace mpq