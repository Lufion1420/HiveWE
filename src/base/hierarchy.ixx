module;

#include <QSettings>

export module Hierarchy;

import std;
import types;
import JSON;
import BinaryReader;
import CASC;
import no_init_allocator;
import Utilities;

using namespace std::literals::string_literals;
namespace fs = std::filesystem;

/// Suffix of the temporary file Hierarchy::map_file_write writes before renaming it over the target. Unusual on purpose, so it
/// never matches a real import. A crash can leave one behind, so folder loops that list, copy or pack map files skip it.
export constexpr std::string_view map_write_temp_suffix = ".hivewe-tmp";

export bool is_map_write_temp_file(const fs::path& path) {
	return path.filename().native().ends_with(L".hivewe-tmp");
}

/// Deletes every leftover map_file_write temporary file under `directory`, for example after copying a map folder.
export void remove_map_write_temp_files(const fs::path& directory) {
	std::error_code ec;
	std::vector<fs::path> leftovers;
	for (auto it = fs::recursive_directory_iterator(directory, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
		if (is_map_write_temp_file(it->path())) {
			leftovers.push_back(it->path());
		}
	}
	for (const fs::path& path : leftovers) {
		fs::remove(path, ec);
	}
}

/// Map file contents keyed by file name relative to the map folder.
export using MapFiles = std::map<fs::path, std::vector<u8>>;

export class Hierarchy {
  public:
	/// While set (see MapWriteCapture), map_file_write on this thread stores the bytes here and touches no file.
	static inline thread_local MapFiles* map_write_capture = nullptr;

	char tileset = 'L';
	std::string locale = "enus";
	casc::CASC game_data;
	json::JSON aliases;

	fs::path map_directory;
	fs::path warcraft_directory;
	fs::path root_directory;

	/// While set, map_file_read and map_file_exists on this thread resolve against this folder instead of map_directory, for
	/// reading a map that is not open. map_directory itself is off limits for that: the Map Protector's pack
	/// thread redirects it, so saving and restoring it on the UI thread could restore the wrong value.
	static inline thread_local const fs::path* map_read_override = nullptr;

	bool ptr = false;
	bool hd = true;
	bool teen = false;
	bool local_files = true;

	Hierarchy() {
		QSettings war3reg("HKEY_CURRENT_USER\\Software\\Blizzard Entertainment\\Warcraft III", QSettings::NativeFormat);
		local_files = war3reg.value("Allow Local Files", 0).toInt() != 0;
	}

	bool open_casc(const fs::path& directory) {
		QSettings settings;
		ptr = settings.value("flavour", "Retail").toString() == "PTR";
		hd = settings.value("hd", "False").toString() == "True";
		teen = settings.value("teen", "False").toString() == "True";

		warcraft_directory = directory;

		bool open = game_data.open(warcraft_directory / (ptr ? ":w3t" : ":w3"));
		root_directory = warcraft_directory / (ptr ? "_ptr_" : "_retail_");

		if (open) {
			aliases.load(open_file("filealiases.json").value());
		}
		return open;
	}

	[[nodiscard]]
	auto open_file(const fs::path& path) const -> std::expected<BinaryReader, std::string> {
		const std::string path_str = path.string();

		#define TRY_OPEN(expr)                \
				if (auto file = (expr); file) {   \
				return file;                   \
				}

		TRY_OPEN(read_file("data/overrides" / path));

		if (local_files) {
			TRY_OPEN(read_file(root_directory / path));
		}

		if (hd && teen) {
			TRY_OPEN(map_file_read("_hd.w3mod:_teen.w3mod:" + path_str));
		}

		if (hd) {
			TRY_OPEN(map_file_read("_hd.w3mod:" + path_str));
		}

		TRY_OPEN(map_file_read(path));

		if (hd) {
			TRY_OPEN(game_data.open_file(std::format("war3.w3mod:_hd.w3mod:_tilesets/{}.w3mod:{}", tileset, path_str)));
		}

		if (hd && teen) {
			TRY_OPEN(game_data.open_file("war3.w3mod:_hd.w3mod:_teen.w3mod:"s + path_str));
		}

		if (hd) {
			TRY_OPEN(game_data.open_file("war3.w3mod:_hd.w3mod:"s + path_str));
		}

		TRY_OPEN(game_data.open_file(std::format("war3.w3mod:_tilesets/{}.w3mod:{}", tileset, path_str)));
		TRY_OPEN(game_data.open_file(std::format("war3.w3mod:_locales/{}.w3mod:{}", locale, path_str)));

		if (teen) {
			TRY_OPEN(game_data.open_file("war3.w3mod:_teen.w3mod:"s + path_str));
		}

		TRY_OPEN(game_data.open_file("war3.w3mod:"s + path_str));
		TRY_OPEN(game_data.open_file("war3.w3mod:_deprecated.w3mod:"s + path_str));

		#undef TRY_OPEN

		if (aliases.exists(path_str)) {
			return open_file(aliases.alias(path_str));
		}

		return std::unexpected(path_str + " could not be found in the hierarchy");
	}

	/// True if `path` exists as a stock asset in the base game data, ignoring the map's own files and
	/// local overrides. Used to detect map files that override a game asset by path (e.g. TerrainArt/*),
	/// which are "used" implicitly even though nothing references them.
	bool game_file_exists(const fs::path& path) const {
		if (path.empty()) {
			return false;
		}

		const auto path_str = path.string();

		// Same lookup as file_exists() but without the map's own files, so a map file at the same path
		// still counts as overriding the underlying game asset (HiveWE overrides / local install / CASC).
		return fs::exists("data/overrides" / path)
			|| (local_files && fs::exists(root_directory / path))
			|| (hd && game_data.file_exists("war3.w3mod:_hd.w3mod:_tilesets/"s + tileset + ".w3mod:"s + path_str))
			|| (hd && teen && game_data.file_exists("war3.w3mod:_hd.w3mod:_teen.w3mod:"s + path_str))
			|| (hd && game_data.file_exists("war3.w3mod:_hd.w3mod:"s + path_str))
			|| game_data.file_exists("war3.w3mod:_tilesets/"s + tileset + ".w3mod:"s + path_str)
			|| game_data.file_exists(std::format("war3.w3mod:_locales/{}.w3mod:{}", locale, path_str))
			|| (teen && game_data.file_exists("war3.w3mod:_teen.w3mod:"s + path_str))
			|| game_data.file_exists("war3.w3mod:"s + path_str)
			|| game_data.file_exists("war3.w3mod:_deprecated.w3mod:"s + path_str)
			|| (aliases.exists(path_str) ? game_file_exists(aliases.alias(path_str)) : false);
	}

	bool file_exists(const fs::path& path) const {
		if (path.empty()) {
			return false;
		}

		const auto path_str = path.string();

		return fs::exists("data/overrides" / path) || (local_files && fs::exists(root_directory / path))
			|| (hd && teen && map_file_exists("_hd.w3mod:_teen.w3mod:" + path_str))
			|| (hd && map_file_exists("_hd.w3mod:" + path_str)) || map_file_exists(path)
			|| (hd && game_data.file_exists("war3.w3mod:_hd.w3mod:_tilesets/"s + tileset + ".w3mod:"s + path_str))
			|| (hd && teen && game_data.file_exists("war3.w3mod:_hd.w3mod:_teen.w3mod:"s + path_str))
			|| (hd && game_data.file_exists("war3.w3mod:_hd.w3mod:"s + path_str))
			|| game_data.file_exists("war3.w3mod:_tilesets/"s + tileset + ".w3mod:"s + path_str)
			|| game_data.file_exists(std::format("war3.w3mod:_locales/{}.w3mod:{}", locale, path_str))
			|| (teen && game_data.file_exists("war3.w3mod:_teen.w3mod:"s + path_str))
			|| game_data.file_exists("war3.w3mod:"s + path_str)
			|| game_data.file_exists("war3.w3mod:_deprecated.w3mod:"s + path_str)
			|| (aliases.exists(path_str) ? file_exists(aliases.alias(path_str)) : false);
	}

	[[nodiscard]]
	auto map_file_read(const fs::path& path) const -> std::expected<BinaryReader, std::string> {
		return read_file((map_read_override != nullptr ? *map_read_override : map_directory) / path);
	}

	/// source somewhere on disk, destination relative to the map. Copies to `<destination>.hivewe-tmp`, checks its size and renames it
	/// over the target, as map_file_write does, so a failed copy or rename leaves the old file intact. A read-only attribute the copy took
	/// from the source is cleared, or the next save could not replace the file. Throws on any failure and removes the temporary file first.
	void map_file_add(const fs::path& source, const fs::path& destination) const {
		const fs::path target = map_directory / destination;
		fs::path temporary = target;
		temporary += map_write_temp_suffix;

		const auto fail = [&](const std::string& reason) {
			std::error_code ignored;
			fs::permissions(temporary, fs::perms::owner_write, fs::perm_options::add, ignored);
			fs::remove(temporary, ignored);
			const std::u8string name = destination.u8string();
			throw std::runtime_error("Error writing file " + std::string(name.begin(), name.end()) + ": " + reason);
		};

		std::error_code ec;
		// A read-only leftover of a crashed save would refuse the copy.
		fs::permissions(temporary, fs::perms::owner_write, fs::perm_options::add, ec);
		fs::remove(temporary, ec);
		const auto source_size = fs::file_size(source, ec);
		if (ec) {
			fail("the source file cannot be read (" + ec.message() + ")");
		}
		if (!fs::copy_file(source, temporary, fs::copy_options::overwrite_existing, ec) || ec) {
			fail("could not copy to the temporary file (" + ec.message() + ")");
		}
		fs::permissions(temporary, fs::perms::owner_write, fs::perm_options::add, ec);
		if (ec) {
			fail("could not make the temporary file writable (" + ec.message() + ")");
		}
		const auto written = fs::file_size(temporary, ec);
		if (ec || written != source_size) {
			fail("the copied file has the wrong size");
		}

		// A virus scanner or indexer can hold the target for a moment, so retry briefly before giving up.
		for (int attempt = 0; attempt < 5; attempt++) {
			fs::rename(temporary, target, ec);
			if (!ec) {
				return;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		fail("could not replace the file (" + ec.message() + ")");
	}

	/// Writes `<path>.hivewe-tmp`, checks its size and renames it over the target, so a failed write leaves the old file intact.
	/// Throws on any failure and removes the temporary file first.
	void map_file_write(const fs::path& path, const std::span<const u8> data) const {
		if (map_write_capture != nullptr) {
			(*map_write_capture)[path].assign(data.begin(), data.end());
			return;
		}

		const fs::path target = map_directory / path;
		fs::path temporary = target;
		temporary += map_write_temp_suffix;

		const auto fail = [&](const std::string& reason) {
			std::error_code ignored;
			fs::remove(temporary, ignored);
			throw std::runtime_error("Error writing file " + path.string() + ": " + reason);
		};

		{
			std::ofstream outfile(temporary, std::ios::binary | std::ios::trunc);
			if (!outfile) {
				fail("could not create " + temporary.filename().string());
			}
			outfile.write(reinterpret_cast<char const*>(data.data()), static_cast<std::streamsize>(data.size()));
			outfile.close();
			// A short write or a failed flush must not let the save report success.
			if (!outfile) {
				fail("the write or close failed");
			}
		}

		std::error_code ec;
		const auto written = fs::file_size(temporary, ec);
		if (ec || written != data.size()) {
			fail("the written file has the wrong size");
		}

		// A virus scanner or indexer can hold the target for a moment, so retry briefly before giving up.
		for (int attempt = 0; attempt < 5; attempt++) {
			fs::rename(temporary, target, ec);
			if (!ec) {
				return;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		fail("could not replace the file (" + ec.message() + ")");
	}

	void map_file_remove(const fs::path& path) const {
		fs::remove(map_directory / path);
	}

	bool map_file_exists(const fs::path& path) const {
		return fs::exists((map_read_override != nullptr ? *map_read_override : map_directory) / path);
	}

	void map_file_rename(const fs::path& original, const fs::path& renamed) const {
		fs::rename(map_directory / original, map_directory / renamed);
	}
};

export inline Hierarchy hierarchy;

/// While one lives, what Hierarchy::map_file_write would write on this thread goes into `files` instead of the map folder. This
/// is how the unsaved changes check runs the real writers without writing.
export class MapWriteCapture {
	MapFiles* previous;

  public:
	explicit MapWriteCapture(MapFiles& files) : previous(Hierarchy::map_write_capture) {
		Hierarchy::map_write_capture = &files;
	}
	~MapWriteCapture() {
		Hierarchy::map_write_capture = previous;
	}
	MapWriteCapture(const MapWriteCapture&) = delete;
	MapWriteCapture& operator=(const MapWriteCapture&) = delete;
};
