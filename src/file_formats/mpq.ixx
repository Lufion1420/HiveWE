module;

#define __STORMLIB_NO_STATIC_LINK__
#include "StormLib.h";

export module MPQ;

import std;

namespace fs = std::filesystem;

// A thin wrapper around StormLib https://github.com/ladislav-zezula/StormLib
namespace mpq {
	/// True when an archive entry name is safe to join onto a target folder: non-empty, relative, with no
	/// drive or root, no ':' (drive letters, NTFS streams), and every part non-empty, not only dots and spaces
	/// ("." and ".." included) and not a Windows device name (CON, PRN, AUX, NUL, COM1-9, LPT1-9, CONIN$, CONOUT$,
	/// with or without an extension). MPQ names use backslashes, so both separators count. Works on the raw
	/// bytes without fs::path, so it cannot throw on a code page that cannot hold the name. unpack() and
	/// extract_file() refuse every other name before anything is written.
	export bool is_safe_archive_name(std::string_view name) {
		if (name.empty() || name.find(':') != std::string_view::npos) {
			return false;
		}
		auto part_is_safe = [](std::string_view part) {
			if (part.find_first_not_of(". ") == std::string_view::npos) { // empty, or only dots and spaces
				return false;
			}
			std::string stem(part.substr(0, part.find('.')));
			while (!stem.empty() && stem.back() == ' ') {
				stem.pop_back();
			}
			for (char& c : stem) {
				c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
			}
			if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL" || stem == "CONIN$" || stem == "CONOUT$") {
				return false;
			}
			if (stem.starts_with("COM") || stem.starts_with("LPT")) {
				const std::string_view digit = std::string_view(stem).substr(3);
				if (digit.size() == 1 && ((digit[0] >= '1' && digit[0] <= '9') || digit[0] == '\xB9' || digit[0] == '\xB2' || digit[0] == '\xB3')) {
					return false; // COM1-9, LPT1-9 and the cp1252 superscripts
				}
				if (digit == "\xC2\xB9" || digit == "\xC2\xB2" || digit == "\xC2\xB3") {
					return false; // the same superscripts in UTF-8
				}
			}
			return true;
		};
		std::size_t start = 0;
		while (true) {
			const std::size_t end = name.find_first_of("\\/", start);
			if (!part_is_safe(name.substr(start, end == std::string_view::npos ? end : end - start))) {
				return false;
			}
			if (end == std::string_view::npos) {
				return true;
			}
			start = end + 1;
		}
	}

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
		///
		/// Returns false when any enumerated entry could not be extracted (the other entries are still
		/// extracted, and the temporary-file cleanup still runs). That includes entries under a
		/// StormLib-fabricated name in a listfile-less archive: StormLib resolves those by block index,
		/// so a failure there is a real read or write failure, not an expected miss. If failed_names is
		/// given it receives the archive names of the entries that failed.
		bool unpack(const fs::path& path, std::vector<std::string>* failed_names = nullptr) {
			bool all_extracted = true;
			SFILE_FIND_DATA file_data;
			HANDLE find_handle = SFileFindFirstFile(handle, "*", &file_data, nullptr);
			if (find_handle) {
				do {
					const fs::path target = path / file_data.cFileName;
					// Refuse names that could land outside `path` (see is_safe_archive_name), then check the
					// joined path really stays below it.
					const fs::path relative = target.lexically_normal().lexically_relative(path.lexically_normal());
					const bool inside = is_safe_archive_name(file_data.cFileName) && !relative.empty() && *relative.begin() != "..";
					std::error_code ec;
					if (inside) {
						fs::create_directories(target.parent_path(), ec);
					}
					if (!inside || ec || !SFileExtractFile(handle, file_data.cFileName, target.c_str(), SFILE_OPEN_FROM_MPQ)) {
						all_extracted = false;
						if (failed_names) {
							failed_names->push_back(file_data.cFileName);
						}
					}
				} while (SFileFindNextFile(find_handle, &file_data));
				SFileFindClose(find_handle);
			} else if (GetLastError() != ERROR_NO_MORE_FILES) {
				// The search itself could not start: that is not the same as an empty archive.
				all_extracted = false;
			}

			// Delete unneeded files
			std::error_code ignored;
			fs::remove(path / "(listfile)", ignored);
			fs::remove(path / "(attributes)", ignored);
			fs::remove(path / "(war3map.imp)", ignored);
			return all_extracted;
		}

		enum class ExtractResult { extracted, absent, failed };

		/// Extracts a single file by its exact archive-internal name, bypassing wildcard
		/// enumeration entirely. MPQ files are looked up by name hash, so this works even
		/// when the archive has no (listfile). `absent` means the archive does not hold the
		/// file (also for an unsafe name it does not hold, so a stale import-list entry is skipped);
		/// `failed` means it does but it could not be read or written to disk, or its name is
		/// unsafe (see is_safe_archive_name) and was refused before anything touched the disk.
		ExtractResult extract_file(const fs::path& archived_name, const fs::path& disk_path) const {
			if (!file_exists(archived_name)) {
				return ExtractResult::absent;
			}
			if (!is_safe_archive_name(archived_name.string())) {
				return ExtractResult::failed;
			}
			std::error_code ec;
			fs::create_directories(disk_path.parent_path(), ec);
			if (ec) {
				return ExtractResult::failed;
			}
			return SFileExtractFile(handle, archived_name.string().c_str(), disk_path.c_str(), SFILE_OPEN_FROM_MPQ) ? ExtractResult::extracted
																													: ExtractResult::failed;
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