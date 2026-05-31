module;

#include <QMargins>
#include <QObject>
#include <QModelIndex>

export module UnitTreeModel;

import std;
import BaseTreeModel;
import SLK;
import Globals;
import UnorderedMap;

export class UnitTreeModel : public BaseTreeModel {
	Q_OBJECT

	struct Category {
		std::string name;
		BaseTreeItem* item;
		BaseTreeItem* custom_item;
	};

	hive::unordered_map<std::string, Category> categories;
	BaseTreeItem* custom_root = nullptr;

	std::array<std::string, 4> subCategories = {
		"Units",
		"Buildings",
		"Heroes",
		"Special",
	};

	BaseTreeItem* getFolderParent(const std::string& id) const override {
		const std::string_view race = units_slk.data<std::string_view>("race", id);
		const bool isBuilding = units_slk.data<std::string_view>("isbldg", id) == "1";
		const bool isHero = isupper(id.front());
		const bool isSpecial = units_slk.data<std::string_view>("special", id) == "1";
		const bool is_custom = units_slk.shadow_data.contains(id) && units_slk.shadow_data.at(id).contains("oldid");

		int subIndex = 0;
		if (isSpecial) {
			subIndex = 3;
		} else if (isBuilding) {
			subIndex = 1;
		} else if (isHero) {
			subIndex = 2;
		}

		if (const auto found = categories.find(race); found != categories.end()) {
			return (is_custom ? found->second.custom_item : found->second.item)->children[subIndex];
		} else {
			std::println("Unit with id: {} has no race set. Set a race!", id);
			return (is_custom ? categories.begin()->second.custom_item : categories.begin()->second.item)->children[subIndex];
		}
	}

  public:
	QVariant data(const QModelIndex& index, int role) const override {
		if (!index.isValid()) {
			return {};
		}

		BaseTreeItem* item = static_cast<BaseTreeItem*>(index.internalPointer());

		switch (role) {
			case Qt::EditRole:
				if (item->baseCategory) {
					return QString::fromStdString(item->label);
				} else if (item->subCategory) {
					return QString::fromStdString(item->label);
				} else {
					return source_edit_data(index);
				}
			case Qt::DisplayRole:
				if (item->baseCategory) {
					return QString::fromStdString(item->label);
				} else if (item->subCategory) {
					return QString::fromStdString(item->label + " (" + std::to_string(item->children.size()) + ")");
				} else {
					if (units_slk.data<std::string_view>("campaign", item->id) == "1") {
						const std::string_view properNames = units_slk.data<std::string_view>("propernames", item->id);

						if (!properNames.empty()) {
							return append_id_label(QString::fromUtf8(properNames).split(',').first(), item->id);
						}
					}

					return append_id_label(source_display_data(index, role).toString() + " " + sourceModel()->data(sourceModel()->index(slk->row_headers.at(item->id), slk->column_headers.at("editorsuffix")), role).toString(), item->id);
				}
			default:
				return BaseTreeModel::data(index, role);
		}
	}

	explicit UnitTreeModel(QObject* parent)
		: BaseTreeModel(parent) {
		slk = &units_slk;

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

		for (size_t i = 0; i < units_slk.rows(); i++) {
			const std::string id = units_slk.index_to_row.at(i);
			BaseTreeItem* item = new BaseTreeItem(UnitTreeModel::getFolderParent(id));
			item->id = id;
			items.emplace(id, item);
		}

		categoryChangeFields = { "race", "isbldg", "special" };
	}
};

#include "unit_tree_model.moc"
