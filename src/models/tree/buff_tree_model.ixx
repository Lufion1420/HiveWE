module;

#include <QMap>
#include <QModelIndex>

export module BuffTreeModel;

import std;
import BaseTreeModel;
import SLK;
import Globals;
import UnorderedMap;

export class BuffTreeModel : public BaseTreeModel {
	struct Category {
		std::string name;
		BaseTreeItem* item;
		BaseTreeItem* custom_item;
	};

	hive::unordered_map<std::string, Category> categories;
	BaseTreeItem* custom_root = nullptr;

	std::array<std::string, 2> subCategories = {
		"Buffs",
		"Effects",
	};

	BaseTreeItem* getFolderParent(const std::string& id) const override {
		const std::string_view race = buff_slk.data<std::string_view>("race", id);
		if (race.empty()) {
			std::cout << "Empty race for " << id << " in buffs\n";
			return nullptr;
		}
		const bool isEffect = buff_slk.data("iseffect", id) == "1";
		const bool is_custom = buff_slk.shadow_data.contains(id) && buff_slk.shadow_data.at(id).contains("oldid");
		const int subIndex = isEffect ? 1 : 0;

		return (is_custom ? categories.at(race).custom_item : categories.at(race).item)->children[subIndex];
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
					const QString editorname = sourceModel()->data(sourceModel()->index(slk->row_headers.at(item->id), slk->column_headers.at("editorname")), role).toString();
					if (editorname.isEmpty()) {
						return QAbstractProxyModel::data(index, role).toString() + " " + sourceModel()->data(sourceModel()->index(slk->row_headers.at(item->id), slk->column_headers.at("editorsuffix")), role).toString();
					} else {
						return editorname + " " + sourceModel()->data(sourceModel()->index(slk->row_headers.at(item->id), slk->column_headers.at("editorsuffix")), role).toString();
					}
				}
			default:
				return BaseTreeModel::data(index, role);
		}
	}

	explicit BuffTreeModel(QObject* parent)
		: BaseTreeModel(parent) {
		slk = &buff_slk;

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

		for (int i = 0; i < buff_slk.rows(); i++) {
			const std::string& id = buff_slk.index_to_row.at(i);

			BaseTreeItem* parent_item = getFolderParent(id);
			if (!parent_item) {
				continue;
			}

			BaseTreeItem* item = new BaseTreeItem(parent_item);
			item->id = id;
			items.emplace(id, item);
		}

		categoryChangeFields = { "race", "iseffect" };
	}
};
