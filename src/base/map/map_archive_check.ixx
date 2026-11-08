export module MapArchiveCheck;

import std;
import MapBodyCheck;
import MapFormatCheck;
import MPQ;

namespace fs = std::filesystem;

/// check_map_format_versions for a map still inside its archive, so a refused map never reaches a user's
/// folder. Kept apart from MapFormatCheck so the readers importing that do not depend on StormLib.
/// Extracts only the checked files, by name so a missing (listfile) does not matter, into
/// scratch_directory, which must be empty and is the caller's to clean up.
export auto check_archive_format_versions(const mpq::MPQ& archive, const fs::path& scratch_directory)
	-> std::expected<void, std::string> {
	for (const std::string_view file : checked_map_files()) {
		if (archive.file_exists(file) && !archive.extract_file(file, scratch_directory / file)) {
			return std::unexpected(std::format("This map cannot be opened because {} could not be read from the archive.", file));
		}
	}
	return check_map_format_versions(scratch_directory);
}

/// check_archive_format_versions, then the bodies of everything that can be judged without what a map imports,
/// which a scratch folder lacks. Triggers and w3e texture indices are checked once the map is in a folder.
/// A file that fails to extract is left out rather than refused: the unpack that follows may not need it,
/// and the check after that sees what really reached the folder.
export auto check_archive_for_opening(const mpq::MPQ& archive, const fs::path& scratch_directory) -> std::expected<void, std::string> {
	if (auto versions = check_archive_format_versions(archive, scratch_directory); !versions) {
		return versions;
	}
	for (const std::string_view file : body_only_map_files()) {
		if (archive.file_exists(file) && !archive.extract_file(file, scratch_directory / file)) {
			std::error_code error;
			fs::remove(scratch_directory / file, error);
		}
	}
	return check_map_bodies(scratch_directory, BodyScope::without_triggers);
}
