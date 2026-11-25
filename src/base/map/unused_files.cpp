
module;

#include <QObject>

module Map;

import std;

namespace fs = std::filesystem;

namespace {
	bool is_game_file(const std::string& path) {
		// Game files have ASCII names, and a path outside the system code page makes game_file_exists throw.
		if (std::ranges::any_of(path, [](const char c) { return static_cast<unsigned char>(c) >= 0x80; })) {
			return false;
		}
		return hierarchy.game_file_exists(path);
	}
}

bool Map::is_override_file(const std::string& path) const {
	return is_picked_up_by_path(path, false, is_game_file);
}

// Gathers what only the open map knows (unsaved object data, triggers, ...) and leaves the matching to AssetUsage.
std::vector<FileUsage> Map::get_file_usage(std::vector<AssetReference> editor_text) const {
	const auto start = std::chrono::steady_clock::now();

	AssetUsageRequest request;
	request.map_folder = filesystem_path;
	request.unlisted_file_names = imports.blacklist;
	request.references = std::move(editor_text);

	add_object_references(request.references, units_slk, false);
	add_object_references(request.references, items_slk, false);
	add_object_references(request.references, destructibles_slk, true);
	add_object_references(request.references, doodads_slk, true);
	add_object_references(request.references, abilities_slk, false);
	add_object_references(request.references, buff_slk, false);
	add_object_references(request.references, upgrade_slk, false);
	add_trigger_references(request.references, triggers.global_jass_comment, triggers.global_jass, triggers.triggers, triggers.variables);
	add_trigger_string_references(request.references, trigger_strings);
	add_map_info_references(request.references, info);
	add_sound_references(request.references, sounds.sounds);

	// Custom objects skipped on load exist only in the saved file, so its references count on their own.
	for (const auto& entry : skipped_objects) {
		request.always_listed_files.emplace(entry.file);
	}

	request.is_stock_asset = is_game_file;

	std::vector<FileUsage> result = find_file_usage(request);

	std::println(
		"Find unused files: {:>5}ms",
		std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()
	);

	return result;
}
