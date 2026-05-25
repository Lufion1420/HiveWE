module;

#include <QMap>
#include <QObject>
#include <QModelIndex>
#include <QIcon>

export module ItemTreeModel;

import std;
import BaseTreeModel;
import SLK;
import Globals;
import UnorderedMap;

export class ItemTreeModel : public BaseTreeModel {
	struct Category {
		std::string name;
		BaseTreeItem* item;
		BaseTreeItem* custom_item;
	};

	hive::unordered_map<std::string, Category> categories;
	BaseTreeItem* custom_root = nullptr;

	BaseTreeItem* getFolderParent(const std::string& id) const override {
		const std::string_view itemClass = items_slk.data<std::string_view>("class", id);
		if (itemClass.empty()) {
			std::cout << "Empty class for " << id << " in items\n";
			return nullptr;
		}
		const bool is_custom = items_slk.shadow_data.contains(id) && items_slk.shadow_data.at(id).contains("oldid");

		return is_custom ? categories.at(itemClass).custom_item : categories.at(itemClass).item;
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
				} else {
					return QAbstractProxyModel::data(index, role);
				}
			default:
				return BaseTreeModel::data(index, role);
		}
	}

	explicit ItemTreeModel(QObject* parent)
		: BaseTreeModel(parent) {
		slk = &items_slk;

		custom_root = new BaseTreeItem(rootItem);
		custom_root->baseCategory = true;
		custom_root->customFolder = true;
		custom_root->label = "Custom";

		for (const auto& [key, value] : unit_editor_data.section("itemClass")) {
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

		for (int i = 0; i < items_slk.rows(); i++) {
			const std::string& id = items_slk.index_to_row.at(i);

			BaseTreeItem* parent_item = getFolderParent(id);
			if (!parent_item) {
				continue;
			}
			BaseTreeItem* item = new BaseTreeItem(parent_item);
			item->id = id;
			items.emplace(id, item);
		}

		categoryChangeFields = { "class" };
	}
};
