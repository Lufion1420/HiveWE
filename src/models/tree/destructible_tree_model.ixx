module;

#include <QMap>
#include <QMargins>
#include <QObject>
#include <QModelIndex>
#include <QSize>
#include <QIcon>

export module DestructibleTreeModel;

import std;
import BaseTreeModel;
import QIconResource;
import SLK;
import Globals;
import ResourceManager;

export class DestructibleTreeModel : public BaseTreeModel {
	struct Category {
		std::string name;
		std::shared_ptr<QIconResource> icon;
		BaseTreeItem* item;
		BaseTreeItem* custom_item;
	};

	std::unordered_map<char, Category> categories;
	BaseTreeItem* custom_root = nullptr;

	BaseTreeItem* getFolderParent(const std::string& id) const override {
		const std::string_view category = destructibles_slk.data<std::string_view>("category", id);
		const bool is_custom = destructibles_slk.shadow_data.contains(id) && destructibles_slk.shadow_data.at(id).contains("oldid");

		return is_custom ? categories.at(category.front()).custom_item : categories.at(category.front()).item;
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
					return append_id_label(QAbstractProxyModel::data(index, role).toString() + " " + sourceModel()->data(sourceModel()->index(slk->row_headers.at(item->id), slk->column_headers.at("editorsuffix")), role).toString(), item->id);
				}
			case Qt::DecorationRole:
				if (item->baseCategory || item->subCategory) {
					return folderIcon;
				}
				return categories.at(destructibles_slk.data<std::string_view>("category", item->id).front()).icon->icon;
			default:
				return BaseTreeModel::data(index, role);
		}
	}

	explicit DestructibleTreeModel(QObject* parent)
		: BaseTreeModel(parent) {
		slk = &destructibles_slk;

		custom_root = new BaseTreeItem(rootItem);
		custom_root->baseCategory = true;
		custom_root->customFolder = true;
		custom_root->label = "Custom";

		for (const auto& [key, value] : world_edit_data.section("DestructibleCategories")) {
			categories[key.front()].name = value[0];
			categories[key.front()].icon = resource_manager.load<QIconResource>(value[1]).value();
			categories[key.front()].item = new BaseTreeItem(rootItem);
			categories[key.front()].item->baseCategory = true;
			categories[key.front()].item->label = value[0];
			categories[key.front()].custom_item = new BaseTreeItem(custom_root);
			categories[key.front()].custom_item->baseCategory = true;
			categories[key.front()].custom_item->customFolder = true;
			categories[key.front()].custom_item->label = value[0];
		}

		for (const auto& [id, index] : destructibles_slk.row_headers) {
			BaseTreeItem* item = new BaseTreeItem(getFolderParent(id));
			item->id = id;
			items.emplace(id, item);
		}

		categoryChangeFields = { "category" };
	}
};
