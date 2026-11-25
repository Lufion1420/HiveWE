export module MapBackup;

import std;

namespace fs = std::filesystem;

// Versioned backups of a map folder's own files, made before each editor save. Qt-free: the backup root, the clock, the editor session
// and the keep count are parameters, so tests run it on scratch folders.
//
// Layout: <root>\<map folder name, cut to 32 characters>-<16 hex digits>\<yyyyMMdd-HHmmss-mmm[-n]>\ with the copied files, source.txt
// (the map folder's full path, UTF-8) and session.txt (the editor session id). The hex digits are FNV-1a 64 of the lowercased
// weakly_canonical path as UTF-8, so two folders with the same name get separate backups and the name is the same in every run.
// A set is first written as <name>.partial and renamed when every copy has been checked, so a set without that suffix is complete.

namespace {
	constexpr std::wstring_view partial_suffix = L".partial";
	/// A set rotation is removing. Never a backup; a later run removes what is left of it.
	constexpr std::wstring_view deleting_suffix = L".deleting";
	constexpr std::wstring_view source_file_name = L"source.txt";
	constexpr std::wstring_view session_file_name = L"session.txt";
	constexpr std::wstring_view temp_file_suffix = L".hivewe-tmp";
	constexpr size_t folder_name_limit = 32;

	std::wstring lower(std::wstring_view text) {
		std::wstring out(text);
		for (wchar_t& c : out) {
			c = static_cast<wchar_t>(std::towlower(static_cast<std::wint_t>(c)));
		}
		return out;
	}

	std::string utf8(const fs::path& path) {
		const std::u8string text = path.u8string();
		return std::string(text.begin(), text.end());
	}

	/// Error text naming a path (UTF-8) and the system reason.
	std::string describe(const std::string& what, const fs::path& path, const std::error_code& ec) {
		std::string text = what + " " + utf8(path);
		if (ec) {
			text += " (" + ec.message() + ")";
		}
		return text;
	}

	bool all_digits(std::wstring_view text) {
		return !text.empty() && std::ranges::all_of(text, [](wchar_t c) { return c >= L'0' && c <= L'9'; });
	}

	int number(std::wstring_view digits) {
		int value = 0;
		for (const wchar_t c : digits) {
			value = value * 10 + (c - L'0');
		}
		return value;
	}

	/// Clears read-only attributes under `path` (and on it), then removes it. Errors are ignored; the caller checks what is left.
	void remove_tree(const fs::path& path) {
		std::error_code ec;
		fs::permissions(path, fs::perms::owner_write, fs::perm_options::add, ec);
		if (fs::is_directory(fs::symlink_status(path, ec))) {
			for (auto it = fs::recursive_directory_iterator(path, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
				std::error_code ignored;
				fs::permissions(it->path(), fs::perms::owner_write, fs::perm_options::add, ignored);
			}
		}
		fs::remove_all(path, ec);
	}

	bool same_bytes(const fs::path& a, const fs::path& b) {
		std::error_code ec;
		const auto size_a = fs::file_size(a, ec);
		if (ec) {
			return false;
		}
		const auto size_b = fs::file_size(b, ec);
		if (ec || size_a != size_b) {
			return false;
		}
		std::ifstream in_a(a, std::ios::binary);
		std::ifstream in_b(b, std::ios::binary);
		if (!in_a || !in_b) {
			return false;
		}
		std::array<char, 65536> buffer_a;
		std::array<char, 65536> buffer_b;
		while (in_a && in_b) {
			in_a.read(buffer_a.data(), buffer_a.size());
			in_b.read(buffer_b.data(), buffer_b.size());
			if (in_a.gcount() != in_b.gcount() || !std::equal(buffer_a.begin(), buffer_a.begin() + in_a.gcount(), buffer_b.begin())) {
				return false;
			}
		}
		return in_a.eof() && in_b.eof();
	}

	bool write_text(const fs::path& path, std::string_view text) {
		{
			std::ofstream out(path, std::ios::binary | std::ios::trunc);
			if (!out) {
				return false;
			}
			out.write(text.data(), static_cast<std::streamsize>(text.size()));
			out.close();
			if (!out) {
				return false;
			}
		}
		std::error_code ec;
		return fs::file_size(path, ec) == text.size() && !ec;
	}

	std::string read_first_line(const fs::path& path) {
		std::ifstream in(path, std::ios::binary);
		std::string line;
		std::getline(in, line);
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		return line;
	}
} // namespace

export namespace map_backup {
	/// Times in set names and retention are local wall-clock time; the caller converts the current time.
	using LocalTime = std::chrono::local_time<std::chrono::milliseconds>;

	struct Options {
		/// The backup root, for example %LOCALAPPDATA%\HiveWE\HiveWE\backups.
		fs::path root;
		LocalTime now;
		/// The editor session. The first backup of each session is kept for pin_days.
		std::string session_id;
		/// How many of the newest backups are always kept (at least 1).
		size_t keep = 10;
		/// How long the first backup of each session and of each day is kept on top of the newest ones.
		std::chrono::days pin_days { 14 };
	};

	enum class Outcome {
		/// A new backup set was written.
		created,
		/// The files equal the newest complete backup, so none was written. `set` names that backup.
		identical,
		/// The folder does not exist or holds none of the files that are backed up (a new map): nothing to do.
		nothing_to_back_up,
	};

	struct Result {
		Outcome outcome = Outcome::nothing_to_back_up;
		/// The map's folder under the root (empty for nothing_to_back_up).
		fs::path map_backups;
		/// The new set, or the newest one for `identical`.
		fs::path set;
		/// Sets rotation removed after the new one was written.
		std::vector<fs::path> removed;
	};

	struct Error {
		/// UTF-8.
		std::string message;
		/// The backup folder the attempt used (the map's folder under the root, or the root when that is not known yet).
		fs::path folder;
	};

	/// A complete backup set as rotation sees it.
	struct BackupSet {
		fs::path path;
		LocalTime time;
		int index = 0;
		std::string session;
	};

	/// True for a file the backup copies, by its path relative to the map folder: a root file whose name starts with "war3map" (any
	/// case) or conversation.json, or scripts\war3map.j / scripts\war3map.lua. Never a *.hivewe-tmp file, never other subfolders.
	bool is_backed_up_name(const fs::path& relative) {
		std::vector<std::wstring> parts;
		for (const auto& part : relative) {
			parts.push_back(lower(part.native()));
		}
		if (parts.empty() || parts.back().ends_with(temp_file_suffix)) {
			return false;
		}
		if (parts.size() == 1) {
			return parts[0].starts_with(L"war3map") || parts[0] == L"conversation.json";
		}
		if (parts.size() == 2) {
			return parts[0] == L"scripts" && (parts[1] == L"war3map.j" || parts[1] == L"war3map.lua");
		}
		return false;
	}

	/// The files of `map_folder` that a backup copies, relative to it, sorted. Empty when the folder does not exist. Throws when the
	/// folder cannot be listed.
	std::vector<fs::path> select_files(const fs::path& map_folder) {
		std::vector<fs::path> files;
		std::error_code ec;
		if (!fs::is_directory(map_folder, ec)) {
			return files;
		}
		for (const auto& entry : fs::directory_iterator(map_folder)) {
			const fs::path name = entry.path().filename();
			std::error_code type_ec;
			if (entry.is_regular_file(type_ec) && is_backed_up_name(name)) {
				files.push_back(name);
			} else if (entry.is_directory(type_ec) && lower(name.native()) == L"scripts") {
				for (const auto& script : fs::directory_iterator(entry.path())) {
					const fs::path relative = name / script.path().filename();
					if (script.is_regular_file(type_ec) && is_backed_up_name(relative)) {
						files.push_back(relative);
					}
				}
			}
		}
		std::ranges::sort(files);
		return files;
	}

	/// FNV-1a 64 of `bytes` as 16 lowercase hex digits. Stable across runs and builds, unlike std::hash.
	std::string fnv1a_64_hex(std::string_view bytes) {
		std::uint64_t hash = 0xcbf29ce484222325ULL;
		for (const char c : bytes) {
			hash ^= static_cast<std::uint8_t>(c);
			hash *= 0x100000001b3ULL;
		}
		return std::format("{:016x}", hash);
	}

	/// The absolute, weakly canonical form of `map_folder` without a trailing separator, as written into source.txt.
	fs::path normalized_folder(const fs::path& map_folder) {
		std::error_code ec;
		fs::path path = fs::weakly_canonical(map_folder, ec);
		if (ec) {
			path = fs::absolute(map_folder, ec);
			if (ec) {
				path = map_folder;
			}
		}
		path.make_preferred();
		std::wstring text = path.native();
		while (text.size() > 3 && (text.back() == L'\\' || text.back() == L'/')) {
			text.pop_back();
		}
		return fs::path(text);
	}

	/// <root>\<folder name cut to 32 characters>-<FNV-1a 64 hex of the lowercased normalized path as UTF-8>.
	fs::path backups_folder_for(const fs::path& root, const fs::path& map_folder) {
		const fs::path folder = normalized_folder(map_folder);
		const std::string key = utf8(fs::path(lower(folder.native())));

		std::wstring name = folder.filename().native();
		if (name.empty()) {
			name = L"map";
		}
		// Cut by characters, never inside a UTF-16 surrogate pair.
		size_t characters = 0;
		size_t cut = 0;
		while (cut < name.size() && characters < folder_name_limit) {
			const bool pair = name[cut] >= 0xD800 && name[cut] <= 0xDBFF && cut + 1 < name.size();
			cut += pair ? 2 : 1;
			characters++;
		}
		name.resize(cut);

		const std::string hex = fnv1a_64_hex(key);
		return root / (name + L"-" + std::wstring(hex.begin(), hex.end()));
	}

	/// yyyyMMdd-HHmmss-mmm, with -n appended for n > 0.
	std::wstring set_name(LocalTime time, int n = 0) {
		const auto day = std::chrono::floor<std::chrono::days>(time);
		const std::chrono::year_month_day date { day };
		const std::chrono::hh_mm_ss clock { time - day };
		std::string text = std::format("{:04}{:02}{:02}-{:02}{:02}{:02}-{:03}", static_cast<int>(date.year()), static_cast<unsigned>(date.month()),
									   static_cast<unsigned>(date.day()), clock.hours().count(), clock.minutes().count(), clock.seconds().count(),
									   clock.subseconds().count());
		if (n > 0) {
			text += std::format("-{}", n);
		}
		return std::wstring(text.begin(), text.end());
	}

	/// The time and index of a set name made by set_name, or nothing for any other name.
	std::optional<std::pair<LocalTime, int>> parse_set_name(std::wstring_view name) {
		if (name.size() < 19 || name[8] != L'-' || name[15] != L'-') {
			return std::nullopt;
		}
		if (!all_digits(name.substr(0, 8)) || !all_digits(name.substr(9, 6)) || !all_digits(name.substr(16, 3))) {
			return std::nullopt;
		}
		int index = 0;
		if (name.size() > 19) {
			if (name[19] != L'-' || name.size() > 29 || !all_digits(name.substr(20)) || name[20] == L'0') {
				return std::nullopt;
			}
			index = number(name.substr(20));
		}
		const std::chrono::year_month_day date { std::chrono::year(number(name.substr(0, 4))), std::chrono::month(number(name.substr(4, 2))),
												 std::chrono::day(number(name.substr(6, 2))) };
		const int hours = number(name.substr(9, 2));
		const int minutes = number(name.substr(11, 2));
		const int seconds = number(name.substr(13, 2));
		if (!date.ok() || hours > 23 || minutes > 59 || seconds > 59) {
			return std::nullopt;
		}
		const LocalTime time = std::chrono::local_days(date) + std::chrono::hours(hours) + std::chrono::minutes(minutes)
			+ std::chrono::seconds(seconds) + std::chrono::milliseconds(number(name.substr(16, 3)));
		return std::pair { time, index };
	}

	/// The complete sets in a map's backup folder, newest first. Only folders named by set_name that hold source.txt count, so nothing
	/// else in the folder is ever touched.
	std::vector<BackupSet> list_sets(const fs::path& map_backups) {
		std::vector<BackupSet> sets;
		std::error_code ec;
		for (auto it = fs::directory_iterator(map_backups, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
			std::error_code type_ec;
			if (!it->is_directory(type_ec)) {
				continue;
			}
			const auto parsed = parse_set_name(it->path().filename().native());
			if (!parsed || !fs::is_regular_file(it->path() / source_file_name, type_ec)) {
				continue;
			}
			sets.push_back({ it->path(), parsed->first, parsed->second, read_first_line(it->path() / session_file_name) });
		}
		std::ranges::sort(sets, [](const BackupSet& a, const BackupSet& b) {
			return std::tie(a.time, a.index) > std::tie(b.time, b.index);
		});
		return sets;
	}

	/// Keeps the newest `keep` sets, plus for pin_days the first set of each editor session and the first of each day; removes the rest
	/// (read-only attributes cleared first). Only complete sets from list_sets are candidates, and `protect` (the set just made) is never
	/// removed: after the clock went back it can sort below older-stamped sets. A set is first renamed to <name>.deleting, so a removal
	/// that stops partway never leaves something that counts as a backup; one that cannot be renamed stays as it is and is not reported.
	/// Returns the removed sets.
	std::vector<fs::path> rotate(const fs::path& map_backups, const Options& options, const fs::path& protect = {}) {
		const auto sets = list_sets(map_backups);
		std::vector<bool> kept(sets.size(), false);
		const size_t keep = std::max<size_t>(options.keep, 1);
		for (size_t i = 0; i < sets.size() && i < keep; i++) {
			kept[i] = true;
		}

		std::set<std::string> sessions;
		std::set<std::chrono::local_days> days;
		for (size_t i = sets.size(); i-- > 0;) { // oldest first
			const BackupSet& set = sets[i];
			const bool first_of_session = !set.session.empty() && sessions.insert(set.session).second;
			const bool first_of_day = days.insert(std::chrono::floor<std::chrono::days>(set.time)).second;
			if ((first_of_session || first_of_day) && options.now - set.time <= options.pin_days) {
				kept[i] = true;
			}
		}

		std::vector<fs::path> removed;
		for (size_t i = 0; i < sets.size(); i++) {
			if (kept[i] || (!protect.empty() && sets[i].path == protect)) {
				continue;
			}
			fs::path aside = sets[i].path;
			aside += deleting_suffix;
			std::error_code ec;
			fs::rename(sets[i].path, aside, ec);
			if (ec) {
				continue; // locked: try again on a later run
			}
			remove_tree(aside);
			removed.push_back(sets[i].path);
		}
		return removed;
	}

	/// Copies the backed-up files of `map_folder` into a new set under options.root, unless they equal the newest complete set. Stale
	/// <name>.partial folders of this map are removed first; rotation runs only after a new set was completed. Reads the map folder,
	/// never writes it. Never throws.
	std::expected<Result, Error> back_up(const fs::path& map_folder, const Options& options) {
		fs::path map_backups;
		try {
			const std::vector<fs::path> files = select_files(map_folder);
			if (files.empty()) {
				return Result {};
			}

			map_backups = backups_folder_for(options.root, map_folder);
			std::error_code ec;
			fs::create_directories(map_backups, ec);
			if (ec || !fs::is_directory(map_backups, ec)) {
				return std::unexpected(Error { describe("Could not create the backup folder", map_backups, ec), map_backups });
			}

			// A crash or a full disk can leave a partial set, and a locked file a half-removed one; neither is a backup, so they go.
			// Only names made by set_name plus one of the two suffixes are touched.
			std::vector<fs::path> partials;
			for (auto it = fs::directory_iterator(map_backups, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
				const std::wstring name = it->path().filename().native();
				for (const std::wstring_view suffix : { partial_suffix, deleting_suffix }) {
					if (name.ends_with(suffix) && parse_set_name(std::wstring_view(name).substr(0, name.size() - suffix.size()))) {
						partials.push_back(it->path());
					}
				}
			}
			for (const fs::path& partial : partials) {
				remove_tree(partial);
			}

			const auto sets = list_sets(map_backups);
			if (!sets.empty()) {
				std::vector<fs::path> newest_files;
				for (auto it = fs::recursive_directory_iterator(sets.front().path, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
					std::error_code type_ec;
					if (!it->is_regular_file(type_ec)) {
						continue;
					}
					const fs::path relative = it->path().lexically_relative(sets.front().path);
					if (relative != fs::path(source_file_name) && relative != fs::path(session_file_name)) {
						newest_files.push_back(relative);
					}
				}
				std::ranges::sort(newest_files);
				if (!ec && newest_files == files && std::ranges::all_of(files, [&](const fs::path& file) {
						return same_bytes(map_folder / file, sets.front().path / file);
					})) {
					return Result { Outcome::identical, map_backups, sets.front().path, {} };
				}
			}

			std::wstring name;
			for (int n = 0;; n++) {
				name = set_name(options.now, n);
				if (!fs::exists(map_backups / name, ec) && !fs::exists(map_backups / (name + std::wstring(partial_suffix)), ec)
					&& !fs::exists(map_backups / (name + std::wstring(deleting_suffix)), ec)) {
					break;
				}
			}
			const fs::path partial = map_backups / (name + std::wstring(partial_suffix));
			const fs::path complete = map_backups / name;

			const auto fail = [&](const std::string& message) {
				remove_tree(partial);
				return std::unexpected(Error { message, map_backups });
			};

			if (!fs::create_directory(partial, ec) || ec) {
				return fail(describe("Could not create the backup", partial, ec));
			}
			for (const fs::path& file : files) {
				const fs::path source = map_folder / file;
				const fs::path target = partial / file;
				if (file.has_parent_path()) {
					fs::create_directories(target.parent_path(), ec);
					if (ec) {
						return fail(describe("Could not create the backup folder", target.parent_path(), ec));
					}
				}
				if (!fs::copy_file(source, target, ec) || ec) {
					return fail(describe("Could not copy", source, ec));
				}
				// A read-only map file gives a read-only copy; the backup itself need not be.
				fs::permissions(target, fs::perms::owner_write, fs::perm_options::add, ec);
				const auto source_size = fs::file_size(source, ec);
				const auto target_size = ec ? 0 : fs::file_size(target, ec);
				if (ec || source_size != target_size) {
					return fail(describe("The backup copy has the wrong size:", target, ec));
				}
			}
			if (!write_text(partial / source_file_name, utf8(normalized_folder(map_folder)) + "\n")
				|| !write_text(partial / session_file_name, options.session_id + "\n")) {
				return fail(describe("Could not write", partial / source_file_name, {}));
			}

			// A virus scanner or indexer can hold the new files for a moment.
			for (int attempt = 0; attempt < 5; attempt++) {
				fs::rename(partial, complete, ec);
				if (!ec) {
					break;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
			if (ec) {
				return fail(describe("Could not finish the backup", complete, ec));
			}

			Result result { Outcome::created, map_backups, complete, {} };
			result.removed = rotate(map_backups, options, complete);
			return result;
		} catch (const std::exception& e) {
			return std::unexpected(Error { std::string("The backup failed: ") + e.what(), map_backups.empty() ? options.root : map_backups });
		} catch (...) {
			return std::unexpected(Error { "The backup failed (unknown error)", map_backups.empty() ? options.root : map_backups });
		}
	}
} // namespace map_backup
