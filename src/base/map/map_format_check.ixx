export module MapFormatCheck;

import std;

namespace fs = std::filesystem;

namespace {
	auto join_versions(const std::span<const std::uint32_t> versions) -> std::string {
		std::string joined;
		for (const std::uint32_t version : versions) {
			joined += (joined.empty() ? "" : ", ") + std::to_string(version);
		}
		return joined;
	}

	/// Only a version above everything HiveWE reads comes from a newer game. Below that it is an old or
	/// odd format (w3i 27, doo 6, units subversion 10), which a newer HiveWE would not necessarily read.
	auto is_newer_than_accepted(const std::span<const std::uint32_t> accepted, const std::uint32_t found) -> bool {
		return found > std::ranges::max(accepted);
	}

	auto describe_unsupported(
		const std::string_view file,
		const std::string_view field,
		const std::span<const std::uint32_t> accepted,
		const std::uint32_t found
	) -> std::string {
		const std::string_view verdict = is_newer_than_accepted(accepted, found) ? "needs a newer version of HiveWE" : "HiveWE does not support";
		return std::format("{} is {} {}, which {} (this version reads {}).", file, field, found, verdict, join_versions(accepted));
	}

	auto describe_unknown_w3i_editor(const std::uint32_t version, const std::uint32_t editor) -> std::string {
		return std::format("war3map.w3i is version {} from editor build {}, whose layout HiveWE does not support.", version, editor);
	}
}

/// The one definition of the map file versions HiveWE can read. The readers and the preflight below both
/// use these, so they cannot disagree. Only versions the reader actually parses correctly belong here.
export namespace map_format {
	inline constexpr std::array<std::uint32_t, 9> w3i_versions = { 15, 18, 25, 26, 28, 31, 32, 33, 39 };
	inline constexpr std::array<std::uint32_t, 2> doo_versions = { 7, 8 };
	inline constexpr std::array<std::uint32_t, 2> units_versions = { 7, 8 };
	inline constexpr std::array<std::uint32_t, 2> units_subversions = { 9, 11 };
	inline constexpr std::array<std::uint32_t, 3> w3r_versions = { 2, 3, 5 };
	inline constexpr std::array<std::uint32_t, 1> w3c_versions = { 0 };
	inline constexpr std::array<std::uint32_t, 2> w3e_versions = { 11, 12 };
	inline constexpr std::array<std::uint32_t, 3> w3s_versions = { 1, 2, 3 };
	inline constexpr std::array<std::uint32_t, 1> wpm_versions = { 0 };

	auto is_accepted(const std::span<const std::uint32_t> accepted, const std::uint32_t version) -> bool {
		return std::ranges::contains(accepted, version);
	}

	/// For readers reached without the preflight (MapProtector): a misread file must not be parsed on.
	void require_accepted(
		const std::string_view file,
		const std::span<const std::uint32_t> accepted,
		const std::uint32_t version,
		const std::string_view field = "version"
	) {
		if (!is_accepted(accepted, version)) {
			throw std::runtime_error(describe_unsupported(file, field, accepted, version));
		}
	}

	/// Editors 6128-6139 have no v39 samples, so whether their players hold a hud_skin is unknown (6127 and
	/// below do not, 6140 and up do). Guessing would misread the player names, so they are refused.
	auto is_known_w3i_editor(const std::uint32_t version, const std::uint32_t editor) -> bool {
		return version != 39 || editor < 6128 || editor > 6139;
	}

	void require_known_w3i_editor(const std::uint32_t version, const std::uint32_t editor) {
		if (!is_known_w3i_editor(version, editor)) {
			throw std::runtime_error(describe_unknown_w3i_editor(version, editor));
		}
	}
}

namespace {
	/// What a wrong magic number means for the rest of Map::load, which ignores the readers' return values.
	enum class WrongMagic {
		/// The reader returns early and the result does harm once the open map is gone. Terrain: Map::load's
		/// interpolated_height indexes the empty corner arrays. PathingMap: the map stays 0 by 0, so the
		/// next save would overwrite the real war3map.wpm with an empty pathing map.
		refuse,
		/// The reader returns early with empty vectors, which later code copes with.
		skip_file,
		/// The reader never returns early and parses the version anyway.
		check_version,
	};

	struct HeaderFormat {
		std::string_view file;
		/// Map::load reads the file unconditionally, as opposed to only when it exists.
		bool required;
		/// Empty when the file has no magic number.
		std::string_view magic;
		WrongMagic wrong_magic;
		/// Bytes the reader needs for everything up to and including the (sub)version.
		std::size_t header_size;
		std::span<const std::uint32_t> versions;
		std::span<const std::uint32_t> subversions;
	};

	constexpr std::array<HeaderFormat, 8> header_formats = { {
		{ "war3map.w3i", true, "", WrongMagic::check_version, 4, map_format::w3i_versions, {} },
		{ "war3map.w3e", true, "W3E!", WrongMagic::refuse, 8, map_format::w3e_versions, {} },
		{ "war3map.doo", true, "W3do", WrongMagic::skip_file, 12, map_format::doo_versions, {} },
		{ "war3mapUnits.doo", false, "W3do", WrongMagic::check_version, 12, map_format::units_versions, map_format::units_subversions },
		{ "war3map.w3r", false, "", WrongMagic::check_version, 4, map_format::w3r_versions, {} },
		{ "war3map.w3c", false, "", WrongMagic::check_version, 4, map_format::w3c_versions, {} },
		{ "war3map.w3s", false, "", WrongMagic::check_version, 4, map_format::w3s_versions, {} },
		{ "war3map.wpm", false, "MP3W", WrongMagic::refuse, 8, map_format::wpm_versions, {} },
	} };

	constexpr std::size_t largest_header_size = 12;

	struct Problems {
		std::vector<std::string> unsupported;
		std::vector<std::string> damaged;
		bool any_newer = false;
	};

	void check_file(const fs::path& directory, const HeaderFormat& format, Problems& problems) {
		const fs::path path = directory / format.file;

		std::error_code error;
		const bool exists = fs::exists(path, error);
		if (error) {
			problems.damaged.push_back(std::format("{} could not be read.", format.file));
			return;
		}
		if (!exists) {
			if (format.required) {
				problems.damaged.push_back(std::format("{} is missing.", format.file));
			}
			return;
		}

		std::ifstream stream(path, std::ios::binary);
		if (!stream.is_open()) {
			problems.damaged.push_back(std::format("{} could not be read.", format.file));
			return;
		}

		std::array<char, largest_header_size> header {};
		stream.read(header.data(), header.size());
		const std::size_t size = static_cast<std::size_t>(stream.gcount());

		const auto report_short = [&] {
			problems.damaged.push_back(std::format("{} is too short to be a valid file ({} bytes).", format.file, size));
		};

		if (size < format.magic.size()) {
			report_short();
			return;
		}
		if (std::string_view(header.data(), format.magic.size()) != format.magic) {
			if (format.wrong_magic == WrongMagic::refuse) {
				problems.damaged.push_back(std::format("{} is damaged: its magic number is not {}.", format.file, format.magic));
				return;
			}
			if (format.wrong_magic == WrongMagic::skip_file) {
				return;
			}
		}
		if (size < format.header_size) {
			report_short();
			return;
		}

		const auto read_u32 = [&](const std::size_t offset) {
			std::uint32_t value = 0;
			for (std::size_t i = 0; i < 4; i++) {
				value |= static_cast<std::uint32_t>(static_cast<unsigned char>(header[offset + i])) << (8 * i);
			}
			return value;
		};

		const std::size_t version_offset = format.magic.size();
		const std::uint32_t version = read_u32(version_offset);
		if (!map_format::is_accepted(format.versions, version)) {
			problems.unsupported.push_back(describe_unsupported(format.file, "version", format.versions, version));
			problems.any_newer |= is_newer_than_accepted(format.versions, version);
			return;
		}

		// MapInfo::load refuses these too, but nothing around it in Map::load catches that.
		if (format.file == "war3map.w3i" && version == 39) {
			if (size < 12) {
				report_short();
				return;
			}
			const std::uint32_t editor = read_u32(8);
			if (!map_format::is_known_w3i_editor(version, editor)) {
				problems.unsupported.push_back(describe_unknown_w3i_editor(version, editor));
			}
			return;
		}

		if (!format.subversions.empty()) {
			const std::uint32_t subversion = read_u32(version_offset + 4);
			if (!map_format::is_accepted(format.subversions, subversion)) {
				problems.unsupported.push_back(describe_unsupported(format.file, "subversion", format.subversions, subversion));
				problems.any_newer |= is_newer_than_accepted(format.subversions, subversion);
			}
		}
	}
}

/// Decides whether a map directory can be opened at all, without touching the map that is open now: it
/// reads only the first bytes of each file by the directory it is given, never hierarchy.map_directory,
/// and writes nothing. Call it before anything that tears down the open map.
export auto check_map_format_versions(const fs::path& directory) -> std::expected<void, std::string> {
	Problems problems;
	for (const HeaderFormat& format : header_formats) {
		check_file(directory, format, problems);
	}

	if (problems.unsupported.empty() && problems.damaged.empty()) {
		return {};
	}

	std::string message;
	if (problems.any_newer) {
		message = "This map cannot be opened because it needs a newer version of HiveWE. Maps saved by Warcraft III 3.0 are not supported yet.\n";
	} else if (!problems.unsupported.empty()) {
		message = "This map cannot be opened because HiveWE does not support the format of some of its files.\n";
	} else {
		message = "This map cannot be opened because some of its files are missing or damaged.\n";
	}
	for (const std::vector<std::string>* lines : { &problems.unsupported, &problems.damaged }) {
		for (const std::string& line : *lines) {
			message += "\n" + line;
		}
	}
	return std::unexpected(std::move(message));
}

/// The files check_map_format_versions looks at, for checking a map that is not in a folder yet.
export auto checked_map_files() -> std::vector<std::string_view> {
	std::vector<std::string_view> files;
	for (const HeaderFormat& format : header_formats) {
		files.push_back(format.file);
	}
	return files;
}

/// Whether target is the folder of the open map, however it is spelled. open_map is empty for a map that
/// was never saved, and a target that does not exist yet cannot be it.
export auto is_open_map_folder(const fs::path& target, const fs::path& open_map) -> bool {
	if (open_map.empty()) {
		return false;
	}
	std::error_code error;
	const bool same = fs::equivalent(target, open_map, error);
	return !error && same;
}
