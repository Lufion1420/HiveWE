export module ModelCatalog;

import std;
import Utilities;

namespace fs = std::filesystem;

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
	if (to_lowercase_copy(path.extension().string()) != ".mdx") {
		return false;
	}
	return !to_lowercase_copy(path.stem().string()).ends_with("_portrait");
}

/// Lower case, forward slashes, without the `war3.w3mod:` prefix: the key that decides whether two paths are the same model.
export std::string model_catalog_key(std::string_view path) {
	std::string key = to_lowercase_copy(path);
	normalize_path_to_forward_slash(key);
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

/// `map_files` are paths relative to the map folder, `game_files` are Hierarchy names (with or without `war3.w3mod:`). Map models come
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
		if (!is_model_file(fs::path(file))) {
			continue;
		}
		std::string key = model_catalog_key(file);
		if (!seen.insert(key).second) {
			continue;
		}
		std::string path = file;
		normalize_path_to_forward_slash(path);
		add(fs::path(std::move(path)), ModelCategory::Map);
	}

	for (const std::string& file : game_files) {
		if (!is_model_file(fs::path(file))) {
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
		add(fs::path(std::move(name)), *category);
	}

	return catalog;
}
