module;

#define __STORMLIB_NO_STATIC_LINK__
#include "StormLib.h"
#include <filesystem>

export module ProtectionPipeline;

import std;
import Map;
import MapGlobal;
import Hierarchy;

namespace fs = std::filesystem;

export struct ProtectionOptions {
	// MPQ archive hardening
	bool remove_listfile = true;
	bool remove_attributes = true;
	bool encrypt_files = true;
	bool inject_junk_files = false;
	int junk_file_count = 50;

	// Trigger hardening
	bool remove_gui_triggers = true;

	// Metadata sanitization
	bool clear_author = false;
	bool clear_description = false;
	bool clear_loading_text = false;
	bool normalize_name = false;
};

export struct SyncSaveResult {
	bool success = false;
	std::string error;
};

export struct PackResult {
	bool success = false;
	std::string error;
};

/// Saves the currently loaded map into temp_dir with the requested metadata fields cleared.
///
/// This is the only function in the pipeline allowed to touch the global `map`/`hierarchy`
/// singletons. Map::save() mutates map->filesystem_path/map->name when the target path differs
/// from the map's own folder, and its sub-steps (info.save(), triggers.save(), etc.) resolve
/// their write locations through hierarchy.map_directory rather than the path passed to save() -
/// see HiveWE::save_current_map_as() for the existing precedent of redirecting map_directory
/// before a save to a different location. Both are redirected here and restored immediately and
/// unconditionally once save() returns, so no other code can observe the redirected state.
/// Must be called on the UI thread, synchronously, before any background work starts.
export SyncSaveResult run_sync_save_and_restore(const fs::path& temp_dir, const ProtectionOptions& options) {
	if (!map || !map->loaded) {
		return { false, "No map is loaded." };
	}

	const fs::path original_filesystem_path = map->filesystem_path;
	const std::string original_name = map->name;
	const fs::path original_map_directory = hierarchy.map_directory;

	const std::string original_author = map->info.author;
	const std::string original_description = map->info.description;
	const std::string original_loading_screen_text = map->info.loading_screen_text;
	const std::string original_loading_screen_title = map->info.loading_screen_title;
	const std::string original_loading_screen_subtitle = map->info.loading_screen_subtitle;
	const std::string original_info_name = map->info.name;

	if (options.clear_author) {
		map->info.author.clear();
	}
	if (options.clear_description) {
		map->info.description.clear();
	}
	if (options.clear_loading_text) {
		map->info.loading_screen_text.clear();
		map->info.loading_screen_title.clear();
		map->info.loading_screen_subtitle.clear();
	}
	if (options.normalize_name) {
		map->info.name = "Warcraft III Map";
	}

	hierarchy.map_directory = temp_dir;
	const bool save_ok = map->save(temp_dir);

	// Restore immediately and unconditionally - no early return above this point.
	map->filesystem_path = original_filesystem_path;
	map->name = original_name;
	hierarchy.map_directory = original_map_directory;
	map->info.author = original_author;
	map->info.description = original_description;
	map->info.loading_screen_text = original_loading_screen_text;
	map->info.loading_screen_title = original_loading_screen_title;
	map->info.loading_screen_subtitle = original_loading_screen_subtitle;
	map->info.name = original_info_name;

	if (!save_ok) {
		return { false, "Failed to save map data to a temporary folder." };
	}
	return { true, "" };
}

/// Adds `count` files with random names, extensions, and content to temp_dir so they get
/// packed into the archive alongside the real map files. Purely cosmetic noise for anyone
/// enumerating archive contents - WC3 only loads files that are actually referenced by name,
/// so unreferenced junk is silently ignored by the game.
void inject_junk_files(const fs::path& temp_dir, int count) {
	static constexpr std::array<std::string_view, 5> junk_extensions = { ".blp", ".mdx", ".wav", ".dat", ".txt" };

	std::mt19937 rng{ std::random_device{}() };
	std::uniform_int_distribution<int> hex_digit(0, 15);
	std::uniform_int_distribution<int> extension_index(0, static_cast<int>(junk_extensions.size()) - 1);
	std::uniform_int_distribution<int> content_size(64, 2048);
	std::uniform_int_distribution<int> content_byte(0, 255);

	for (int i = 0; i < count; ++i) {
		std::string name = "_junk_";
		for (int digit = 0; digit < 8; ++digit) {
			name += "0123456789abcdef"[hex_digit(rng)];
		}
		name += junk_extensions[extension_index(rng)];

		std::vector<char> content(content_size(rng));
		for (char& byte : content) {
			byte = static_cast<char>(content_byte(rng));
		}

		std::ofstream file(temp_dir / name, std::ios::binary);
		file.write(content.data(), static_cast<std::streamsize>(content.size()));
	}
}

/// Packs temp_dir into a protected MPQ at output_path. Operates only on plain files under
/// temp_dir/output_path - never touches map/hierarchy - so it is safe to run on a background
/// thread once run_sync_save_and_restore() has returned. Mirrors HiveWE::export_mpq()'s raw
/// StormLib usage; the MPQ wrapper in mpq.ixx has no archive-creation support.
export PackResult run_async_pack(const fs::path& temp_dir, const fs::path& output_path, const ProtectionOptions& options) {
	if (options.remove_gui_triggers) {
		std::error_code ec;
		fs::remove(temp_dir / "war3map.wtg", ec);
	}

	if (options.inject_junk_files && options.junk_file_count > 0) {
		inject_junk_files(temp_dir, options.junk_file_count);
	}

	std::error_code remove_ec;
	fs::remove(output_path, remove_ec);

	std::error_code dir_ec;
	fs::create_directories(output_path.parent_path(), dir_ec);

	const uint64_t file_count = std::distance(fs::recursive_directory_iterator{ temp_dir }, {});

	const unsigned long create_flags = (options.remove_listfile ? 0ul : static_cast<unsigned long>(MPQ_CREATE_LISTFILE))
		| (options.remove_attributes ? 0ul : static_cast<unsigned long>(MPQ_CREATE_ATTRIBUTES));

	HANDLE handle;
	if (!SFileCreateArchive(output_path.c_str(), create_flags, file_count, &handle)) {
		return { false, std::format("There was an error creating the protected archive (error code {}).", GetLastError()) };
	}

	const unsigned long file_flags = static_cast<unsigned long>(MPQ_FILE_COMPRESS)
		| (options.encrypt_files ? static_cast<unsigned long>(MPQ_FILE_ENCRYPTED) : 0ul);

	for (const auto& entry : fs::recursive_directory_iterator(temp_dir)) {
		if (entry.is_regular_file()) {
			if (!SFileAddFileEx(handle, entry.path().c_str(), entry.path().lexically_relative(temp_dir).string().c_str(), file_flags, MPQ_COMPRESSION_ZLIB, MPQ_COMPRESSION_NEXT_SAME)) {
				const DWORD add_file_error = GetLastError();
				SFileCloseArchive(handle);
				return { false, std::format("There was an error adding '{}' to the protected archive (error code {}).", entry.path().filename().string(), add_file_error) };
			}
		}
	}

	SFileCompactArchive(handle, nullptr, false);
	SFileCloseArchive(handle);

	return { true, "" };
}
