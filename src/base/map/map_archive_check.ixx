export module MapArchiveCheck;

import std;
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
