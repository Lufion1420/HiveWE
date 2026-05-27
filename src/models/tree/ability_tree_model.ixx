module;

#include <QMap>
#include <QModelIndex>

export module AbilityTreeModel;

import std;
import BaseTreeModel;
import SLK;
import Globals;
import UnorderedMap;

export class AbilityTreeModel : public BaseTreeModel {
	struct Category {
		std::string name;
		BaseTreeItem* item;
		BaseTreeItem* custom_item;
	};

	hive::unordered_map<std::string, Category> categories;
	BaseTreeItem* custom_root = nullptr;

	std::array<std::string, 3> subCategories = {
		"Units",
		"Heroes",
		"Items"
	};

	BaseTreeItem* getFolderParent(const std::string& id) const override {
		const std::string_view race = abilities_slk.data<std::string_view>("race", id);
		auto found_race = categories.find(race);
		if (found_race == categories.end()) {
			std::println("Warning: Empty or invalid race for ability ID `{}`, race `{}`", id, race);
			return nullptr;
		}
		const bool isHero = abilities_slk.data<bool>("hero", id);
		const bool isItem = abilities_slk.data<bool>("item", id);
		const bool is_custom = abilities_slk.shadow_data.contains(id) && abilities_slk.shadow_data.at(id).contains("oldid");

		int subIndex = 0;
		if (isHero) {
			subIndex = 1;
		} else if (isItem) {
			subIndex = 2;
		}

		return (is_custom ? found_race->second.custom_item : found_race->second.item)->children[subIndex];
	}

  public:
	QVariant data(const QModelIndex& index, int role) const override {
		if (!index.isValid()) {
			return {};
		}

		BaseTreeItem* item = static_cast<BaseTreeItem*>(index.internalPointer());

		switch (role) {
			case Qt::EditRole:
			case Qt::DisplayRole:
				if (item->baseCategory) {
					return QString::fromStdString(item->label);
				} else if (item->subCategory) {
					return QString::fromStdString(item->label);
				} else {
					return append_id_label(QAbstractProxyModel::data(index, role).toString() + " " + sourceModel()->data(sourceModel()->index(slk->row_headers.at(item->id), slk->column_headers.at("editorsuffix")), role).toString(), item->id);
				}
			default:
				return BaseTreeModel::data(index, role);
		}
	}

	explicit AbilityTreeModel(QObject* parent) : BaseTreeModel(parent) {
		slk = &abilities_slk;

		custom_root = new BaseTreeItem(rootItem);
		custom_root->baseCategory = true;
		custom_root->customFolder = true;
		custom_root->label = "Custom";

		for (const auto& [key, value] : unit_editor_data.section("unitRace")) {
			if (key == "Sort" || key == "NumValues") {
				continue;
			}

			categories[value[0]].name = value[1];
			categories[value[0]].item = new BaseTreeItem(rootItem);
			categories[value[0]].item->baseCategory = true;
			categories[value[0]].item->label = value[1];
			categories[value[0]].custom_item = new BaseTreeItem(custom_root);
			categories[value[0]].custom_item->baseCategory = true;
			categories[value[0]].custom_item->customFolder = true;
			categories[value[0]].custom_item->label = value[1];
		}

		for (const auto& [key, category] : categories) {
			for (const auto& subCategory : subCategories) {
				BaseTreeItem* item = new BaseTreeItem(category.item);
				item->subCategory = true;
				item->label = subCategory;

				BaseTreeItem* custom_item = new BaseTreeItem(category.custom_item);
				custom_item->subCategory = true;
				custom_item->customFolder = true;
				custom_item->label = subCategory;
			}
		}

		for (int i = 0; i < abilities_slk.rows(); i++) {
			const std::string& id = abilities_slk.index_to_row.at(i);

			BaseTreeItem* parent_item = getFolderParent(id);
			if (!parent_item) {
				continue;
			}

			BaseTreeItem* item = new BaseTreeItem(parent_item);
			item->id = id;
			items.emplace(id, item);
		}

		categoryChangeFields = { "race", "hero", "item" };
	}
};
