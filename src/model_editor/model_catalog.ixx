module;

#include <QString> // Unicode-aware lower casing and UTF-8 conversion; no widgets, no application object needed

export module ModelCatalog;

import std;
import Utilities;

namespace fs = std::filesystem;

/// Paths cross this module as UTF-8 `std::string`s. `fs::path::string()` and `generic_string()` use the ANSI code page on MSVC, which
/// garbles (or throws on) a map model with a non-ANSI name, so these two helpers go through the wide path instead.
export std::string utf8_from_path(const fs::path& path) {
	return QString::fromStdWString(path.generic_wstring()).toUtf8().toStdString();
}

export fs::path path_from_utf8(const std::string_view text) {
	return fs::path(QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())).toStdWString());
}

/// Non-GL list of the models the Model Editor and the model browser offer. Enumerating the files stays with the callers (they go
/// through Hierarchy); this module only decides what counts as a model, which category it belongs to and which duplicate wins.

export enum class ModelCategory {
	Abilities,
	Buildings,
	Doodads,
	Environment,
	Objects,
	SharedModels,
	Units,
	Map,
	Count
};

export struct ModelEntry {
	fs::path path;
	ModelCategory category;
};

/// The browser's "Models" and "VFX/SFX" filters. Every entry is exactly one of the two, so their counts add up to the total.
export enum class ModelKind {
	Model,
	Effect
};

export struct GameCategorySource {
	ModelCategory category;
	std::string_view glob;
	std::string_view label;
};

/// The game folders that are listed, with the Hierarchy glob that enumerates each one.
export inline constexpr std::array<GameCategorySource, 7> game_category_sources { {
	{ ModelCategory::Abilities, "war3.w3mod:abilities/*.mdx", "Abilities" },
	{ ModelCategory::Buildings, "war3.w3mod:buildings/*.mdx", "Buildings" },
	{ ModelCategory::Doodads, "war3.w3mod:doodads/*.mdx", "Doodads" },
	{ ModelCategory::Environment, "war3.w3mod:environment/*.mdx", "Environment" },
	{ ModelCategory::Objects, "war3.w3mod:objects/*.mdx", "Objects" },
	{ ModelCategory::SharedModels, "war3.w3mod:sharedmodels/*.mdx", "Shared Models" },
	{ ModelCategory::Units, "war3.w3mod:units/*.mdx", "Units" },
} };

export inline constexpr std::string_view war3_mod_prefix = "war3.w3mod:";

export std::string_view model_category_label(const ModelCategory category) {
	switch (category) {
		case ModelCategory::Abilities: return "Abilities";
		case ModelCategory::Buildings: return "Buildings";
		case ModelCategory::Doodads: return "Doodads";
		case ModelCategory::Environment: return "Environment";
		case ModelCategory::Objects: return "Objects";
		case ModelCategory::SharedModels: return "Shared Models";
		case ModelCategory::Units: return "Units";
		case ModelCategory::Map: return "Map (imported)";
		case ModelCategory::Count: break;
	}
	return "";
}

/// True for a `.mdx` file (any letter case) that is not a portrait model. A leftover `<name>.mdx.hivewe-tmp` has the extension
/// `.hivewe-tmp`, so it is refused here too.
export bool is_model_file(const fs::path& path) {
	if (QString::fromStdWString(path.extension().wstring()).toLower() != ".mdx") {
		return false;
	}
	return !QString::fromStdWString(path.stem().wstring()).toLower().endsWith("_portrait");
}

/// UTF-8, lower case (Unicode aware), forward slashes, without the `war3.w3mod:` prefix: the key that decides whether two paths are the
/// same model. `path` is UTF-8.
export std::string model_catalog_key(std::string_view path) {
	QString folded = QString::fromUtf8(path.data(), static_cast<qsizetype>(path.size())).toLower();
	folded.replace('\\', '/');
	std::string key = folded.toUtf8().toStdString();
	if (key.starts_with(war3_mod_prefix)) {
		key.erase(0, war3_mod_prefix.size());
	}
	return key;
}

/// The game category of a path such as `units/human/footman/footman.mdx` (optionally with the `war3.w3mod:` prefix), from its first folder.
export std::optional<ModelCategory> game_model_category(std::string_view path) {
	const std::string key = model_catalog_key(path);
	const std::string_view first_folder = std::string_view(key).substr(0, key.find('/'));
	if (first_folder.size() == key.size()) {
		return std::nullopt; // no folder at all
	}
	for (const auto& source : game_category_sources) {
		// The glob is "war3.w3mod:<folder>/*.mdx".
		const std::string_view folder = source.glob.substr(war3_mod_prefix.size(), source.glob.find('/') - war3_mod_prefix.size());
		if (first_folder == folder) {
			return source.category;
		}
	}
	return std::nullopt;
}

/// Deterministic Models vs VFX/SFX rule ("SFX" here means special effects, not sound). An entry is an Effect when
///  - it is in the game's `abilities/` folder (spell effects, missiles, buffs), or
///  - its lower-cased path contains "vfx", "sfx" or "effect" anywhere (covers imported map effects and e.g. `objects/.../specialeffects`).
/// Everything else (units, buildings, doodads, environment, other objects and shared models, map models without those words) is a Model.
export ModelKind model_kind(const ModelEntry& entry) {
	if (entry.category == ModelCategory::Abilities) {
		return ModelKind::Effect;
	}
	const std::string key = model_catalog_key(utf8_from_path(entry.path));
	if (key.contains("vfx") || key.contains("sfx") || key.contains("effect")) {
		return ModelKind::Effect;
	}
	return ModelKind::Model;
}

/// What the Object Editor stores for a typed or picked model path: surrounding whitespace removed and a leading `war3.w3mod:` dropped.
/// Anything else is kept as typed (UI paths, ReplaceableTextures, .mdl, portraits, imported files); an empty text clears the field.
export std::string normalize_typed_model_path(const std::string_view text) {
	const auto first = text.find_first_not_of(" \t\r\n");
	if (first == std::string_view::npos) {
		return {};
	}
	std::string result(text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1));
	if (result.starts_with(war3_mod_prefix)) {
		result.erase(0, war3_mod_prefix.size());
	}
	return result;
}

/// "1.25 MB" (two decimals, binary megabytes, always MB, even below 1 MB: 0 bytes is "0.00 MB").
export std::string format_file_size_mb(const std::uint64_t bytes) {
	return std::format("{:.2f} MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
}

export struct ModelCatalog {
	std::vector<ModelEntry> entries;
	std::array<std::size_t, static_cast<std::size_t>(ModelCategory::Count)> counts {};

	std::size_t count(const ModelCategory category) const {
		return counts[static_cast<std::size_t>(category)];
	}

	std::size_t total() const {
		return entries.size();
	}
};

/// Indices into `entries` of the models the browser shows: those in `category` (all when empty) whose path contains every
/// whitespace-separated term of `query` (letter case and slash direction do not matter) and are of `kind` (all when empty).
/// An empty query keeps everything.
export std::vector<std::size_t> filter_model_entries(
	const std::span<const ModelEntry> entries,
	const std::optional<ModelCategory> category,
	const std::string_view query,
	const std::optional<ModelKind> kind = std::nullopt
) {
	std::vector<std::string> terms;
	{
		const std::string lowered = model_catalog_key(query);
		std::size_t start = 0;
		while (start < lowered.size()) {
			const std::size_t end = lowered.find_first_of(" \t", start);
			if (end == std::string::npos) {
				terms.push_back(lowered.substr(start));
				break;
			}
			if (end > start) {
				terms.push_back(lowered.substr(start, end - start));
			}
			start = end + 1;
		}
	}

	std::vector<std::size_t> result;
	for (std::size_t i = 0; i < entries.size(); ++i) {
		if (category && entries[i].category != *category) {
			continue;
		}
		if (kind && model_kind(entries[i]) != *kind) {
			continue;
		}
		if (!terms.empty()) {
			const std::string key = model_catalog_key(utf8_from_path(entries[i].path));
			if (!std::ranges::all_of(terms, [&](const std::string& term) { return key.contains(term); })) {
				continue;
			}
		}
		result.push_back(i);
	}
	return result;
}

/// Bumped whenever the thumbnail rendering changes, so old files on disk are not reused.
export inline constexpr int model_thumbnail_version = 1;

/// What decides which file Hierarchy returns for a path besides the path itself.
export struct ThumbnailContext {
	bool hd = false;
	bool teen = false;
	char tileset = 'L';
	std::string_view locale;
};

/// File name (without extension) of a model's thumbnail on disk and in the memory cache. It changes with the model path, the Hierarchy
/// context (HD, teen, tileset, locale) and `stamp` (the source file's modification time for a map model, the game build for a stock one),
/// never with the letter case or slash direction of the path. FNV-1a over a canonical text, so it is the same on every run.
export std::string model_thumbnail_key(const std::string_view path, const ThumbnailContext& context, const std::string_view stamp) {
	const std::string canonical = std::format(
		"v{}|{}|hd{}|teen{}|ts{}|loc{}|{}",
		model_thumbnail_version,
		model_catalog_key(path),
		context.hd ? 1 : 0,
		context.teen ? 1 : 0,
		context.tileset,
		context.locale,
		stamp
	);
	std::uint64_t hash = 14695981039346656037ull;
	for (const char c : canonical) {
		hash ^= static_cast<std::uint8_t>(c);
		hash *= 1099511628211ull;
	}
	return std::format("{:016x}", hash);
}

/// `map_files` are UTF-8 paths relative to the map folder, `game_files` are Hierarchy names (with or without `war3.w3mod:`). Map models come
/// first and win over a game model at the same path. Files that are not models, game files outside the known folders and repeated
/// paths are dropped.
export ModelCatalog build_model_catalog(const std::span<const std::string> map_files, const std::span<const std::string> game_files) {
	ModelCatalog catalog;
	std::unordered_set<std::string> seen;

	const auto add = [&](fs::path path, const ModelCategory category) {
		++catalog.counts[static_cast<std::size_t>(category)];
		catalog.entries.push_back(ModelEntry { std::move(path), category });
	};

	for (const std::string& file : map_files) {
		if (!is_model_file(path_from_utf8(file))) {
			continue;
		}
		std::string key = model_catalog_key(file);
		if (!seen.insert(key).second) {
			continue;
		}
		std::string path = file;
		normalize_path_to_forward_slash(path);
		add(path_from_utf8(path), ModelCategory::Map);
	}

	for (const std::string& file : game_files) {
		if (!is_model_file(path_from_utf8(file))) {
			continue;
		}
		const auto category = game_model_category(file);
		if (!category) {
			continue;
		}
		if (!seen.insert(model_catalog_key(file)).second) {
			continue;
		}
		std::string name = file;
		if (name.starts_with(war3_mod_prefix)) {
			name.erase(0, war3_mod_prefix.size());
		}
		add(path_from_utf8(name), *category);
	}

	return catalog;
}
