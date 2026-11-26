export module AssetUsage;

import std;
import Hierarchy;
import ScriptPreservation;
import SLK;
import MapInfo;
import Sounds;
import Triggers;
import TriggerStrings;
import UnorderedMap;

namespace fs = std::filesystem;

// Which files of a map folder the map refers to, for the Asset Manager. A file wrongly called used costs nothing, a file wrongly
// called unused gets deleted, so everything that can hold a path is searched as plain text instead of through lists of known fields.

export struct FileUsage {
	std::string path; // relative to the map folder, UTF-8, forward slashes
	std::unordered_set<std::string> used_by; // empty and not an override = unused
	bool is_override = false; // the game picks the file up by its path; counts as used
	std::uintmax_t size = 0; // file size in bytes
};

/// Names in FileUsage::used_by besides object ids, sound names and map file paths.
export constexpr std::string_view map_script_source = "map script";
export constexpr std::string_view loading_screen_source = "loadingscreen";
export constexpr std::string_view prologue_screen_source = "Prologue Screen";
export constexpr std::string_view map_header_source = "Map Header";
export constexpr std::string_view trigger_strings_source = "Trigger Strings";
export constexpr std::string_view trigger_source_prefix = "Trigger: ";
/// Text typed into an open editor that the map may not hold yet.
export constexpr std::string_view open_editor_source_prefix = "Open editor: ";
export constexpr std::string_view variable_source_prefix = "Variable: ";
/// A file that might hold references could not be read, so every file nothing else refers to is listed as used by this.
export constexpr std::string_view unreadable_source_prefix = "Could not read ";

/// Text held in memory that may name files, for example one object's field values.
export struct AssetReference {
	std::string source;
	std::string text;
	bool variation_models = false; // from doodads or destructibles: "tree" also means tree0.mdx, tree1.mdx, ...
};

export struct AssetUsageRequest {
	fs::path map_folder;
	/// File names (any folder) that are part of the map itself: searched for references but not listed.
	std::unordered_set<std::string> unlisted_file_names;
	/// Saved map files whose references are always listed. The other files HiveWE holds a newer copy of (object data, triggers, ...)
	/// are only listed for a file nothing else refers to.
	std::unordered_set<std::string> always_listed_files;
	std::vector<AssetReference> references;
	/// Whether the game itself has a file at this relative path (UTF-8). Optional.
	std::function<bool(const std::string&)> is_stock_asset;
	/// Reads a file, or nothing when it cannot be read. Empty reads from disk. Called from several threads.
	std::function<std::optional<std::string>(const fs::path&)> read;
};

/// The lowercase extension with its dot, or nothing.
export std::string asset_extension(const std::string_view path) {
	const size_t dot = path.rfind('.');
	const size_t slash = path.find_last_of("/\\");
	if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) {
		return {};
	}
	std::string result(path.substr(dot));
	std::ranges::transform(result, result.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return result;
}

namespace {
	/// Extensions the game accepts for one another when it resolves a path.
	enum class Family {
		none,
		texture,
		model,
		audio
	};

	constexpr std::array<std::string_view, 7> texture_extensions = { ".blp", ".dds", ".tga", ".tif", ".png", ".jpg", ".jpeg" };
	constexpr std::array<std::string_view, 2> model_extensions = { ".mdx", ".mdl" };
	constexpr std::array<std::string_view, 4> audio_extensions = { ".wav", ".mp3", ".flac", ".ogg" };
	/// The texture types game data holds, so the only ones worth asking it for under another extension.
	constexpr std::array<std::string_view, 4> stock_texture_extensions = { ".blp", ".dds", ".tga", ".tif" };
	/// Together with the textures and audio: files that cannot name another file, so they are not searched.
	constexpr std::array<std::string_view, 5> other_media_extensions = { ".mp4", ".webm", ".avi", ".ttf", ".otf" };

	Family family_of(const std::string_view extension) {
		if (std::ranges::contains(texture_extensions, extension)) {
			return Family::texture;
		}
		if (std::ranges::contains(model_extensions, extension)) {
			return Family::model;
		}
		if (std::ranges::contains(audio_extensions, extension)) {
			return Family::audio;
		}
		return Family::none;
	}

	/// Brings paths and searched text to one spelling: lowercase ASCII, single forward slashes (scripts write "a\\b") and .mdx for
	/// .mdl. In place, so a large file is not copied.
	void normalize_asset_text(std::string& text) {
		size_t length = 0;
		for (char c : text) {
			if (c >= 'A' && c <= 'Z') {
				c = static_cast<char>(c - 'A' + 'a');
			} else if (c == '\\') {
				c = '/';
			}
			if (c == '/' && length > 0 && text[length - 1] == '/') {
				continue;
			}
			text[length++] = c;
		}
		text.resize(length);

		for (size_t at = text.find(".mdl"); at != std::string::npos; at = text.find(".mdl", at + 4)) {
			text[at + 3] = 'x';
		}
	}

	std::string normalized(std::string text) {
		normalize_asset_text(text);
		return text;
	}

	/// Without leading "_hd.w3mod/" style folders: a file there stands in for the same path outside it.
	std::string_view without_mod_folders(std::string_view path) {
		while (true) {
			const size_t slash = path.find('/');
			if (slash == std::string_view::npos || !path.substr(0, slash).ends_with(".w3mod")) {
				return path;
			}
			path.remove_prefix(slash + 1);
		}
	}

	bool is_name_char(const char c) {
		const unsigned char u = static_cast<unsigned char>(c);
		return std::isalnum(u) || c == '_' || c == '-' || u >= 0x80;
	}

	/// What can stand around a whole value in a script, an INI line or a binary string.
	bool is_value_delimiter(const char c) {
		return std::string_view("\0\"'=,;\n\r\t[]", 11).contains(c);
	}

	struct SearchOptions {
		bool variation_models = false;
		/// A model file. Its sequence and model names are not paths, so a root file's name without extension does not count in it.
		bool model_file = false;
	};

	/// Finds every wanted file name in a text in one pass (Aho-Corasick), however many files the map has.
	class FileNameSearch {
	  public:
		enum class Kind {
			path, // the full relative path, found anywhere
			stem, // without extension, or with another one of its family
			variation // like stem, for a model path without its variation number
		};

		void add(const std::string_view name, const std::uint32_t file, const Kind kind, const Family family) {
			if (name.empty()) {
				return;
			}
			int node = 0;
			for (const char c : name) {
				int next = child(node, c);
				if (next < 0) {
					next = static_cast<int>(nodes.size());
					nodes.emplace_back();
					auto& children = nodes[node].children;
					children.insert(std::ranges::lower_bound(children, c, {}, &std::pair<char, int>::first), { c, next });
				}
				node = next;
			}
			nodes[node].patterns.push_back(static_cast<std::uint32_t>(patterns.size()));
			patterns.push_back({ file, name.size(), kind, family, name.contains('/') });
			file_count = std::max<size_t>(file_count, file + 1);
		}

		/// Call once after the last add().
		void build() {
			std::vector<int> queue = { 0 };
			for (size_t head = 0; head < queue.size(); head++) {
				const int node = queue[head];
				for (const auto& [c, next] : nodes[node].children) {
					int fail = nodes[node].fail;
					while (fail != 0 && child(fail, c) < 0) {
						fail = nodes[fail].fail;
					}
					const int target = node == 0 ? 0 : std::max(child(fail, c), 0);
					nodes[next].fail = target;
					nodes[next].output = nodes[target].patterns.empty() ? nodes[target].output : target;
					queue.push_back(next);
				}
			}
		}

		/// The files named in `text`, which must be normalized.
		[[nodiscard]]
		std::vector<std::uint32_t> find(const std::string_view text, const SearchOptions options) const {
			std::vector<std::uint32_t> result;
			std::vector<bool> found(file_count);
			int state = 0;
			for (size_t i = 0; i < text.size(); i++) {
				int next = child(state, text[i]);
				while (next < 0 && state != 0) {
					state = nodes[state].fail;
					next = child(state, text[i]);
				}
				state = std::max(next, 0);

				for (int node = nodes[state].patterns.empty() ? nodes[state].output : state; node >= 0; node = nodes[node].output) {
					for (const std::uint32_t index : nodes[node].patterns) {
						const Pattern& pattern = patterns[index];
						if (!found[pattern.file] && accepts(pattern, text, i + 1 - pattern.length, i + 1, options)) {
							found[pattern.file] = true;
							result.push_back(pattern.file);
						}
					}
				}
			}
			return result;
		}

	  private:
		struct Pattern {
			std::uint32_t file;
			size_t length;
			Kind kind;
			Family family;
			bool has_folder;
		};

		struct Node {
			std::vector<std::pair<char, int>> children; // sorted by character
			int fail = 0;
			int output = -1; // the nearest node along the fail links that ends a pattern
			std::vector<std::uint32_t> patterns;
		};

		std::vector<Node> nodes = std::vector<Node>(1);
		std::vector<Pattern> patterns;
		size_t file_count = 0;

		[[nodiscard]]
		int child(const int node, const char c) const {
			const auto& children = nodes[node].children;
			const auto found = std::ranges::lower_bound(children, c, {}, &std::pair<char, int>::first);
			return found != children.end() && found->first == c ? found->second : -1;
		}

		static bool accepts(const Pattern& pattern, const std::string_view text, const size_t begin, const size_t end, const SearchOptions options) {
			if (pattern.kind == Kind::path) {
				return true;
			}
			if (pattern.kind == Kind::variation && !options.variation_models) {
				return false;
			}

			// A name in a folder only has to start a word. A root name must not be the end of a longer path either.
			const bool starts_word = begin == 0 || (!is_name_char(text[begin - 1]) && (pattern.has_folder || text[begin - 1] != '/'));

			if (end < text.size() && text[end] == '.') {
				size_t last = end + 1;
				while (last < text.size() && std::isalnum(static_cast<unsigned char>(text[last]))) {
					last++;
				}
				if (last > end + 1) {
					return starts_word && family_of(text.substr(end, last - end)) == pattern.family;
				}
			}

			if (pattern.has_folder) {
				return starts_word && (end == text.size() || (!is_name_char(text[end]) && text[end] != '/'));
			}

			// A root name without extension is an ordinary word, so it must be a whole value.
			if (options.model_file) {
				return false;
			}
			size_t before = begin;
			while (before > 0 && text[before - 1] == ' ') {
				before--;
			}
			size_t after = end;
			while (after < text.size() && text[after] == ' ') {
				after++;
			}
			return (before == 0 || is_value_delimiter(text[before - 1])) && (after == text.size() || is_value_delimiter(text[after]));
		}
	};

	/// Every spelling under which a map file can be referred to.
	void add_file_names(FileNameSearch& search, const std::string_view key, const std::uint32_t file) {
		using Kind = FileNameSearch::Kind;

		std::vector<std::string_view> names = { key };
		if (const std::string_view stripped = without_mod_folders(key); stripped != key) {
			names.push_back(stripped);
		}

		for (const std::string_view name : names) {
			search.add(name, file, Kind::path, Family::none);

			const std::string extension = asset_extension(name);
			const Family family = family_of(extension);
			if (family == Family::none) {
				continue;
			}
			const std::string_view stem = name.substr(0, name.size() - extension.size());
			search.add(stem, file, Kind::stem, family);

			if (family != Family::model) {
				continue;
			}
			std::vector<std::string_view> model_stems = { stem };
			// The game loads x_portrait.mdx for x.mdx without anything naming it.
			if (constexpr std::string_view portrait = "_portrait"; stem.ends_with(portrait)) {
				const std::string_view base = stem.substr(0, stem.size() - portrait.size());
				if (!base.empty() && !base.ends_with('/')) {
					search.add(base, file, Kind::stem, family);
					model_stems.push_back(base);
				}
			}
			// Variation n of the doodad model x is xn.mdx. x may end in digits itself, so every split of the trailing digits counts.
			for (std::string_view base : model_stems) {
				while (!base.empty() && std::isdigit(static_cast<unsigned char>(base.back()))) {
					base.remove_suffix(1);
					if (!base.empty() && !base.ends_with('/')) {
						search.add(base, file, Kind::variation, family);
					}
				}
			}
		}
	}

	/// A file name or path as UTF-8. NTFS names may hold an unpaired UTF-16 surrogate, on which u8string() throws: it becomes U+FFFD
	/// here and clears `exact`.
	std::string utf8(const std::wstring_view text, bool& exact) {
		std::string result;
		result.reserve(text.size());
		for (size_t i = 0; i < text.size(); i++) {
			std::uint32_t code = static_cast<std::uint16_t>(text[i]);
			if (code >= 0xD800 && code <= 0xDFFF) {
				const std::uint32_t low = i + 1 < text.size() ? static_cast<std::uint16_t>(text[i + 1]) : 0;
				if (code < 0xDC00 && low >= 0xDC00 && low <= 0xDFFF) {
					code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
					i++;
				} else {
					code = 0xFFFD;
					exact = false;
				}
			}
			if (code < 0x80) {
				result += static_cast<char>(code);
				continue;
			}
			const int continuation_bytes = code < 0x800 ? 1 : code < 0x10000 ? 2 : 3;
			const std::uint32_t lead_marker = (0xFF00u >> (continuation_bytes + 1)) & 0xFFu;
			result += static_cast<char>(lead_marker | (code >> (6 * continuation_bytes)));
			for (int byte = continuation_bytes - 1; byte >= 0; byte--) {
				result += static_cast<char>(0x80u | ((code >> (6 * byte)) & 0x3Fu));
			}
		}
		return result;
	}

	std::optional<std::string> read_from_disk(const fs::path& path) {
		std::ifstream stream(path, std::ios::binary | std::ios::ate);
		if (!stream) {
			return std::nullopt;
		}
		const std::streamoff size = stream.tellg();
		if (size < 0) {
			return std::nullopt;
		}
		std::string content(static_cast<size_t>(size), '\0');
		stream.seekg(0);
		stream.read(content.data(), size);
		if (!stream) {
			return std::nullopt;
		}
		return content;
	}

	/// A map file to search for references.
	struct SourceFile {
		fs::path path;
		std::string relative; // as displayed
		std::string label; // the name it gets in used_by
		SearchOptions options;
		std::optional<std::uint32_t> listed_as; // its own entry, which its content does not count for
		bool memory_is_newer = false; // HiveWE holds this file's content in memory, so it only counts as a last resort
	};

	/// File lists name every file whether the map uses it or not (a version control index is one).
	bool is_searchable(const std::string_view key) {
		if (key.starts_with(".git/") || key.starts_with(".svn/")) {
			return false;
		}
		const std::string_view name = key.substr(key.find_last_of('/') + 1);
		if (name == "(listfile)" || name == "(attributes)" || name == "(signature)") {
			return false;
		}
		const std::string extension = asset_extension(name);
		return extension != ".imp" && !std::ranges::contains(texture_extensions, extension)
			&& !std::ranges::contains(audio_extensions, extension) && !std::ranges::contains(other_media_extensions, extension);
	}

	bool has_newer_copy_in_memory(const std::string_view key) {
		static constexpr std::array<std::string_view, 19> names = {
			"war3map.w3u",     "war3map.w3t",     "war3map.w3a",     "war3map.w3b",     "war3map.w3d",     "war3map.w3h",     "war3map.w3q",
			"war3mapskin.w3u", "war3mapskin.w3t", "war3mapskin.w3a", "war3mapskin.w3b", "war3mapskin.w3d", "war3mapskin.w3h", "war3mapskin.w3q",
			"war3map.wtg",     "war3map.wct",     "war3map.wts",     "war3map.w3i",     "war3map.w3s",
		};
		return std::ranges::contains(names, key);
	}

	void append_eca(std::string& text, const ECA& eca);

	void append_parameter(std::string& text, const TriggerParameter& parameter) {
		text += parameter.value;
		text += '\n';
		if (parameter.has_sub_parameter) {
			append_eca(text, parameter.sub_parameter);
		}
		for (const TriggerParameter& child : parameter.parameters) {
			append_parameter(text, child);
		}
	}

	void append_eca(std::string& text, const ECA& eca) {
		for (const TriggerParameter& parameter : eca.parameters) {
			append_parameter(text, parameter);
		}
		for (const ECA& child : eca.ecas) {
			append_eca(text, child);
		}
	}
}

/// Whether the game picks up the map file at `path` by its path alone, so nothing in the map has to name it. `referenced` says that
/// something names it anyway: then the game is not also asked for the same name with other extensions, which only matters for a file
/// that would otherwise be unused. Ask again when a file loses its last reference.
export bool is_picked_up_by_path(
	const std::string& path,
	const bool referenced,
	const std::function<bool(const std::string&)>& is_stock_asset
) {
	const std::string key = normalized(path);
	const std::string_view stripped_key = without_mod_folders(key);

	// Files under these stock roots override game content by path (terrain tiles, cliffs, team colours, button icons, ...).
	if (stripped_key.starts_with("terrainart/") || stripped_key.starts_with("replaceabletextures/")) {
		return true;
	}

	// The minimap and preview images the game looks up by name.
	if (!key.contains('/')) {
		const std::string_view stem = std::string_view(key).substr(0, key.rfind('.'));
		if (stem == "war3map" || stem == "war3mapmap" || stem == "war3mappreview" || stem == "war3mappath") {
			return true;
		}
	}

	if (!is_stock_asset) {
		return false;
	}
	const std::string stripped(stripped_key);
	if (is_stock_asset(path) || (stripped != key && is_stock_asset(stripped))) {
		return true;
	}
	if (referenced) {
		return false;
	}

	// The game has the file under another extension, or it is the portrait of a game model.
	const std::string extension = asset_extension(stripped);
	const Family family = family_of(extension);
	std::span<const std::string_view> extensions;
	switch (family) {
		case Family::texture:
			extensions = stock_texture_extensions;
			break;
		case Family::model:
			extensions = model_extensions;
			break;
		case Family::audio:
			extensions = audio_extensions;
			break;
		case Family::none:
			return false;
	}
	std::vector<std::string> stems = { stripped.substr(0, stripped.size() - extension.size()) };
	if (family == Family::model && stems.front().ends_with("_portrait")) {
		stems.push_back(stems.front().substr(0, stems.front().size() - std::string_view("_portrait").size()));
	}
	return std::ranges::any_of(stems, [&](const std::string& stem) {
		return std::ranges::any_of(extensions, [&](const std::string_view other) {
			const std::string candidate = stem + std::string(other);
			// The file's own name was asked above.
			return candidate != stripped && is_stock_asset(candidate);
		});
	});
}

/// Text typed into an open editor, under the title of the window or tab holding it.
export void add_editor_text_reference(std::vector<AssetReference>& references, const std::string_view title, std::string text) {
	references.push_back({ std::string(open_editor_source_prefix) + std::string(title), std::move(text) });
}

/// Every edited field of every object in an object data table, whatever the field, including fields kept raw because the meta data
/// does not know their id.
export void add_object_references(std::vector<AssetReference>& references, const slk::SLK& slk, const bool has_model_variations) {
	const auto append_raw = [&](std::string& text, const std::string& id) {
		if (const auto found = slk.raw_modifications.find(id); found != slk.raw_modifications.end()) {
			for (const slk::RawModification& record : found->second) {
				text += record.value;
				text += '\n';
			}
		}
	};

	for (const auto& [id, fields] : slk.shadow_data) {
		std::string text;
		for (const auto& [field, value] : fields) {
			// The id of the base object, not a file.
			if (field == "oldid") {
				continue;
			}
			text += value;
			text += '\n';
		}
		append_raw(text, id);
		// The model of the base object counts too: editing only the variation count adds files next to it.
		if (has_model_variations) {
			text += slk.data<std::string>("file", id);
		}
		references.push_back({ std::string(id), std::move(text), has_model_variations });
	}

	for (const auto& [id, records] : slk.raw_modifications) {
		if (slk.shadow_data.contains(id) || records.empty()) {
			continue;
		}
		std::string text;
		append_raw(text, id);
		references.push_back({ std::string(id), std::move(text), false });
	}
}

/// The map header, custom text, GUI parameters and variable defaults as they are now, saved or not.
export void add_trigger_references(
	std::vector<AssetReference>& references,
	const std::string_view header_comment,
	const std::string_view header,
	const std::vector<Trigger>& triggers,
	const std::vector<TriggerVariable>& variables
) {
	references.push_back({ std::string(map_header_source), std::format("{}\n{}", header_comment, header) });
	for (const Trigger& trigger : triggers) {
		std::string text = trigger.description + '\n' + trigger.custom_text + '\n';
		for (const ECA& eca : trigger.ecas) {
			append_eca(text, eca);
		}
		references.push_back({ std::string(trigger_source_prefix) + trigger.name, std::move(text) });
	}
	for (const TriggerVariable& variable : variables) {
		references.push_back({ std::string(variable_source_prefix) + variable.name, variable.initial_value });
	}
}

export void add_map_info_references(std::vector<AssetReference>& references, const MapInfo& info) {
	references.push_back({ std::string(loading_screen_source), info.loading_screen_model });
	references.push_back({ std::string(prologue_screen_source), info.prologue_screen_model });
}

export void add_sound_references(std::vector<AssetReference>& references, const std::vector<Sound>& sounds) {
	for (const Sound& sound : sounds) {
		references.push_back({ sound.name, std::format("{}\n{}\n{}", sound.file, sound.file_v2, sound.facial_animation_set_path) });
	}
}

export void add_trigger_string_references(std::vector<AssetReference>& references, const TriggerStrings& strings) {
	std::string text;
	for (const auto& [key, value] : strings.entries()) {
		text += value;
		text += '\n';
	}
	references.push_back({ std::string(trigger_strings_source), std::move(text) });
}

/// Every file in the map folder (except unlisted ones and dot files or folders) with everything that refers to it: the given
/// references and the content of every file in the folder that can hold a path.
export std::vector<FileUsage> find_file_usage(const AssetUsageRequest& request) {
	std::vector<FileUsage> files;
	std::vector<SourceFile> sources;
	std::vector<std::string> unreadable;

	std::unordered_set<std::string> script_keys;
	for (const std::string_view name : map_script_file_names) {
		script_keys.insert(normalized(std::string(name)));
	}
	std::unordered_set<std::string> always_listed;
	for (const std::string& name : request.always_listed_files) {
		always_listed.insert(normalized(name));
	}

	std::error_code ec;
	for (auto it = fs::recursive_directory_iterator(request.map_folder, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
		std::error_code entry_error;
		const bool regular = it->is_regular_file(entry_error);
		// A name that has no exact UTF-8 form cannot be shown or matched safely: its content counts, but it is not listed.
		bool exact_name = true;
		const std::string relative = utf8(it->path().lexically_relative(request.map_folder).generic_wstring(), exact_name);
		if (entry_error) {
			unreadable.push_back(relative);
		}
		if (!regular || is_map_write_temp_file(it->path())) {
			continue;
		}
		const std::string key = normalized(relative);

		std::optional<std::uint32_t> listed_as;
		// Tool files (.git, .gitignore, .vscode, ...) are not map assets. Unlisted, the Asset Manager cannot delete them.
		const bool dot_path = key.starts_with('.') || key.contains("/.");
		bool exact_file_name = true;
		const std::string file_name = utf8(it->path().filename().wstring(), exact_file_name);
		if (exact_name && !dot_path && !request.unlisted_file_names.contains(file_name)) {
			listed_as = static_cast<std::uint32_t>(files.size());
			const std::uintmax_t size = it->file_size(entry_error);
			files.push_back({ relative, {}, false, entry_error ? 0 : size });
		}

		// The key of a .mdl file ends in .mdx.
		if (is_searchable(key)) {
			SourceFile source;
			source.path = it->path();
			source.relative = relative;
			source.label = script_keys.contains(key) ? std::string(map_script_source) : relative;
			source.options.model_file = family_of(asset_extension(key)) == Family::model;
			source.options.variation_models = key == "war3map.w3d" || key == "war3map.w3b" || key == "war3mapskin.w3d" || key == "war3mapskin.w3b";
			source.listed_as = listed_as;
			source.memory_is_newer = has_newer_copy_in_memory(key) && !always_listed.contains(key);
			sources.push_back(std::move(source));
		}
	}
	if (ec) {
		unreadable.push_back("the whole map folder");
	}

	FileNameSearch search;
	for (std::uint32_t i = 0; i < files.size(); i++) {
		add_file_names(search, normalized(files[i].path), i);
	}
	search.build();

	for (const AssetReference& reference : request.references) {
		const std::string text = normalized(reference.text);
		for (const std::uint32_t file : search.find(text, { .variation_models = reference.variation_models })) {
			files[file].used_by.insert(reference.source);
		}
	}

	std::mutex mutex;
	// Files that nothing referred to before the second pass; empty during the first.
	std::vector<bool> only_for;
	const auto search_file = [&](const SourceFile& source) {
		std::optional<std::vector<std::uint32_t>> found;
		try {
			if (std::optional<std::string> content = request.read ? request.read(source.path) : read_from_disk(source.path)) {
				normalize_asset_text(*content);
				found = search.find(*content, source.options);
			}
		} catch (const std::exception& e) {
			std::println("Error searching {} for file references: {}", source.relative, e.what());
		}

		const auto guard = std::scoped_lock(mutex);
		if (!found) {
			unreadable.push_back(source.relative);
			return;
		}
		for (const std::uint32_t file : *found) {
			if (source.listed_as != file && (only_for.empty() || only_for[file])) {
				files[file].used_by.insert(source.label);
			}
		}
	};

	const auto newer_in_memory = std::ranges::stable_partition(sources, [](const SourceFile& source) { return !source.memory_is_newer; });
	std::for_each(std::execution::par, sources.begin(), newer_in_memory.begin(), search_file);
	for (const FileUsage& file : files) {
		only_for.push_back(file.used_by.empty());
	}
	std::for_each(std::execution::par, newer_in_memory.begin(), sources.end(), search_file);

	// Every question to the game is asked once: files that differ only in their extension ask for the same names.
	std::unordered_map<std::string, bool> answers;
	std::function<bool(const std::string&)> ask_game;
	if (request.is_stock_asset) {
		ask_game = [&](const std::string& path) {
			std::string lowercase = path;
			std::ranges::transform(lowercase, lowercase.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
			const auto [answer, is_new] = answers.try_emplace(std::move(lowercase), false);
			if (is_new) {
				answer->second = request.is_stock_asset(path);
			}
			return answer->second;
		};
	}

	for (FileUsage& file : files) {
		file.is_override = is_picked_up_by_path(file.path, !file.used_by.empty(), ask_game);
		if (file.used_by.empty() && !file.is_override) {
			for (const std::string& name : unreadable) {
				file.used_by.insert(std::string(unreadable_source_prefix) + name);
			}
		}
	}

	return files;
}
