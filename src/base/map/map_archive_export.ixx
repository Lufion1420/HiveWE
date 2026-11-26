module;

#define __STORMLIB_NO_STATIC_LINK__
#include "StormLib.h"

export module MapArchiveExport;

import std;
import Hierarchy;

namespace fs = std::filesystem;

// Export MPQ's archive step, Qt-free so tests drive every failure. The archive is built as `<target>.hivewe-tmp`, checked, and only
// then renamed over the target, so a failed export leaves an existing file as it was. No call here throws.

/// True when `path` is `folder` or lies inside it, compared without letter case after resolving both.
export bool path_is_within(const fs::path& path, const fs::path& folder) {
	const auto normalized = [](const fs::path& p) {
		std::error_code ec;
		fs::path result = fs::weakly_canonical(p, ec);
		if (ec) {
			result = fs::absolute(p, ec);
		}
		std::wstring text = result.make_preferred().native();
		while (text.size() > 3 && text.back() == L'\\') {
			text.pop_back();
		}
		for (wchar_t& c : text) {
			c = static_cast<wchar_t>(std::towlower(static_cast<std::wint_t>(c)));
		}
		return fs::path(text);
	};
	const fs::path relative = normalized(path).lexically_relative(normalized(folder));
	return !relative.empty() && *relative.begin() != "..";
}

export namespace map_archive {
	/// Where an export stopped. The four after none are refusals: nothing was written.
	enum class Step {
		none,
		inside_map_folder,
		target_is_folder,
		path_too_long,
		target_read_only,
		check_target,
		list_folder,
		stale_temp,
		create,
		add,
		nothing_added,
		flush,
		compact,
		close,
		verify,
		replace,
	};

	struct FailedFile {
		std::string name; // archive name, UTF-8
		std::uint32_t error = 0;
	};

	struct Result {
		bool ok = false;
		Step step = Step::none;
		/// Windows error code of the failed step, 0 when there is none (refusals, nothing to add).
		std::uint32_t error = 0;
		std::size_t added = 0;
		/// Files that could not be added (Step::add) or were missing from the finished archive (Step::verify).
		std::vector<FailedFile> failed;
		/// A file was at the target before the export. On failure it is still there unchanged.
		bool target_existed = false;
		/// A temporary file that could not be deleted after a failure. Empty normally.
		fs::path leftover;
	};

	/// Test seam for the StormLib calls whose failure cannot be caused from outside. Empty members call StormLib. They get the archive
	/// handle and report failure like StormLib: false with the error in GetLastError. `close` must close the handle either way.
	struct Steps {
		std::function<bool(void*)> flush;
		std::function<bool(void*)> compact;
		std::function<bool(void*)> close;
	};

	/// Longest path StormLib handles: SFileCompactArchive cuts the name of its `<archive>.tmp` file to this silently.
	constexpr std::size_t max_path_length = 259;

	constexpr std::wstring_view temp_suffix = L".hivewe-tmp";

	bool is_refusal(const Step step) {
		return step == Step::inside_map_folder || step == Step::target_is_folder || step == Step::path_too_long
			|| step == Step::target_read_only;
	}

	Result write(const fs::path& folder, const fs::path& target, const Steps& steps = {});
}

namespace {
	std::string utf8(const fs::path& path) {
		const std::u8string text = path.u8string();
		return std::string(text.begin(), text.end());
	}

	std::uint32_t last_error() {
		return static_cast<std::uint32_t>(GetLastError());
	}

	/// (listfile), (attributes) and (signature) at the folder root belong to StormLib, never to the map.
	bool is_archive_internal_name(const fs::path& relative) {
		auto it = relative.begin();
		if (it == relative.end() || std::next(it) != relative.end()) {
			return false;
		}
		std::wstring name = relative.native();
		for (wchar_t& c : name) {
			c = static_cast<wchar_t>(std::towlower(static_cast<std::wint_t>(c)));
		}
		return name == L"(listfile)" || name == L"(attributes)" || name == L"(signature)";
	}

	/// True when something is at `path`. A failed check counts as present.
	bool is_present(const fs::path& path) {
		std::error_code ec;
		const fs::file_status status = fs::symlink_status(path, ec);
		if (status.type() == fs::file_type::not_found) {
			return false;
		}
		return ec || fs::exists(status);
	}

	/// Deletes a file, read-only or not, as Hierarchy::map_file_add does. Returns the delete's error.
	std::error_code remove_file(const fs::path& path) {
		std::error_code ec;
		fs::permissions(path, fs::perms::owner_write, fs::perm_options::add, ec);
		ec.clear();
		fs::remove(path, ec);
		return ec;
	}
}

namespace map_archive {
	Result write(const fs::path& folder, const fs::path& target, const Steps& steps) {
		Result result;
		const auto fail = [&](const Step step, const std::uint32_t error) {
			result.ok = false;
			result.step = step;
			result.error = error;
			return result;
		};

		// --- Refusals and checks, before anything is written ---
		std::error_code ec;
		const fs::path absolute_target = fs::absolute(target, ec);
		if (ec) {
			return fail(Step::check_target, static_cast<std::uint32_t>(ec.value()));
		}
		if (path_is_within(absolute_target, folder)) {
			return fail(Step::inside_map_folder, 0);
		}
		const fs::file_status target_status = fs::status(absolute_target, ec);
		if (target_status.type() == fs::file_type::not_found) {
			ec.clear(); // status reports a missing path as an error too
		}
		if (ec) {
			return fail(Step::check_target, static_cast<std::uint32_t>(ec.value()));
		}
		if (fs::is_directory(target_status)) {
			return fail(Step::target_is_folder, 0);
		}
		fs::path temp = absolute_target;
		temp += temp_suffix;
		fs::path compact_temp = temp;
		compact_temp += L".tmp";
		if (compact_temp.native().size() > max_path_length) {
			return fail(Step::path_too_long, 0);
		}
		result.target_existed = fs::exists(target_status);
		// The rename cannot replace a read-only file. Its attribute is the user's, so it is refused here and never cleared.
		if (result.target_existed && (target_status.permissions() & fs::perms::owner_write) == fs::perms::none) {
			return fail(Step::target_read_only, 0);
		}

		// One listing for the count and the files, in the order the old code added them (the archive layout follows it). The count
		// includes folders and skipped files, as before, so the hash table size and with it the bytes stay the same.
		std::vector<fs::path> files;
		std::uint64_t entry_count = 0;
		for (auto it = fs::recursive_directory_iterator(folder, ec);; it.increment(ec)) {
			if (ec) {
				return fail(Step::list_folder, static_cast<std::uint32_t>(ec.value()));
			}
			if (it == fs::recursive_directory_iterator()) {
				break;
			}
			entry_count++;
			std::error_code type_ec;
			const fs::file_status status = it->status(type_ec);
			if (status.type() == fs::file_type::not_found) {
				type_ec.clear(); // a dangling link is not a file, as before
			}
			if (type_ec) {
				result.failed.push_back({ utf8(it->path().lexically_relative(folder)), static_cast<std::uint32_t>(type_ec.value()) });
				return fail(Step::list_folder, static_cast<std::uint32_t>(type_ec.value()));
			}
			if (fs::is_regular_file(status) && !is_map_write_temp_file(it->path())) {
				files.push_back(it->path());
			}
		}

		const auto clean_up = [&]() {
			for (const fs::path& path : { temp, compact_temp }) {
				remove_file(path);
				if (is_present(path) && result.leftover.empty()) {
					result.leftover = path;
				}
			}
		};

		// A leftover of a crashed export: StormLib would append to it or refuse it.
		const std::error_code stale_ec = remove_file(temp);
		remove_file(compact_temp);
		if (is_present(temp)) {
			return fail(Step::stale_temp, stale_ec ? static_cast<std::uint32_t>(stale_ec.value()) : ERROR_ALREADY_EXISTS);
		}

		// --- Build the temporary archive ---
		HANDLE handle = nullptr;
		if (!SFileCreateArchive(temp.c_str(), MPQ_CREATE_LISTFILE | MPQ_CREATE_ATTRIBUTES, static_cast<DWORD>(entry_count), &handle)) {
			const std::uint32_t error = last_error();
			clean_up();
			return fail(Step::create, error);
		}

		std::vector<std::string> added_names;
		for (const fs::path& path : files) {
			const fs::path relative = path.lexically_relative(folder);
			if (is_archive_internal_name(relative)) {
				continue;
			}
			const std::string name = utf8(relative);
			if (SFileAddFileEx(handle, path.c_str(), name.c_str(), MPQ_FILE_COMPRESS, MPQ_COMPRESSION_ZLIB, MPQ_COMPRESSION_NEXT_SAME)) {
				added_names.push_back(name);
			} else {
				result.failed.push_back({ name, last_error() });
			}
		}
		result.added = added_names.size();

		Step failed_step = Step::none;
		std::uint32_t failed_error = 0;
		if (!result.failed.empty()) {
			failed_step = Step::add;
			failed_error = result.failed.front().error;
		} else if (added_names.empty()) {
			failed_step = Step::nothing_added;
		} else if (!(steps.flush ? steps.flush(handle) : SFileFlushArchive(handle))) {
			// Before compacting: SFileCompactArchive flushes too but ignores the result, so a failed listfile write would pass.
			failed_step = Step::flush;
			failed_error = last_error();
		} else if (!(steps.compact ? steps.compact(handle) : SFileCompactArchive(handle, nullptr, false))) {
			failed_step = Step::compact;
			failed_error = last_error();
		}
		const bool closed = steps.close ? steps.close(handle) : SFileCloseArchive(handle);
		const std::uint32_t close_error = closed ? 0 : last_error();
		if (failed_step == Step::none && !closed) {
			failed_step = Step::close;
			failed_error = close_error;
		}
		if (failed_step != Step::none) {
			clean_up();
			return fail(failed_step, failed_error);
		}

		// --- Read the finished archive back: every added file and the listfile must be in it ---
		HANDLE check = nullptr;
		if (!SFileOpenArchive(temp.c_str(), 0, MPQ_OPEN_READ_ONLY, &check)) {
			const std::uint32_t error = last_error();
			clean_up();
			return fail(Step::verify, error);
		}
		added_names.push_back("(listfile)");
		for (const std::string& name : added_names) {
			if (!SFileHasFile(check, name.c_str())) {
				const std::uint32_t error = last_error();
				result.failed.push_back({ name, error != 0 ? error : ERROR_FILE_NOT_FOUND });
			}
		}
		SFileCloseArchive(check);
		if (!result.failed.empty()) {
			clean_up();
			return fail(Step::verify, result.failed.front().error);
		}

		// --- Replace the target. A virus scanner or indexer can hold it for a moment, so retry briefly, as map_file_write does ---
		for (int attempt = 0; attempt < 5; attempt++) {
			fs::rename(temp, absolute_target, ec);
			if (!ec) {
				result.ok = true;
				result.step = Step::none;
				result.error = 0;
				return result;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		const std::uint32_t error = static_cast<std::uint32_t>(ec.value());
		clean_up();
		return fail(Step::replace, error);
	}
}
