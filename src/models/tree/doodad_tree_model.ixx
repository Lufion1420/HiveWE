module;

#include <QMap>
#include <QObject>
#include <QModelIndex>
#include <QIcon>

export module DoodadTreeModel;

import std;
import BaseTreeModel;
import QIconResource;
import SLK;
import Globals;
import ResourceManager;

export class DoodadTreeModel : public BaseTreeModel {
	struct Category {
		std::string name;
		std::shared_ptr<QIconResource> icon;
		BaseTreeItem* item;
		BaseTreeItem* custom_item;
	};

	std::unordered_map<char, Category> categories;
	BaseTreeItem* custom_root = nullptr;

	BaseTreeItem* fallbackFolderParent(const bool is_custom) const {
		return is_custom ? categories.begin()->second.custom_item : categories.begin()->second.item;
	}

	QVariant categoryIcon(const std::string_view category) const {
		if (category.empty()) {
			return {};
		}

		if (const auto found = categories.find(category.front()); found != categories.end()) {
			return found->second.icon->icon;
		}

		return {};
	}

	BaseTreeItem* getFolderParent(const std::string& id) const override {
		const std::string_view category = doodads_slk.data<std::string_view>("category", id);
		const bool is_custom = doodads_slk.shadow_data.contains(id) && doodads_slk.shadow_data.at(id).contains("oldid");

		if (category.empty()) {
			std::println("Doodad with id: {} has no category set. Set a category!", id);
			return fallbackFolderParent(is_custom);
		}

		if (const auto found = categories.find(category.front()); found != categories.end()) {
			return is_custom ? found->second.custom_item : found->second.item;
		}

		std::println("Doodad with id: {} has unknown category `{}`. Set a valid category!", id, category);
		return fallbackFolderParent(is_custom);
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
				} else {
					return source_edit_data(index);
				}
			case Qt::DisplayRole:
				if (item->baseCategory) {
					return QString::fromStdString(item->label);
				} else {
					return append_id_label(source_display_data(index, role).toString(), item->id);
				}
			case Qt::DecorationRole:
				if (item->baseCategory || item->subCategory) {
					return folderIcon;
				}
				return categoryIcon(doodads_slk.data<std::string_view>("category", item->id));
			default:
				return BaseTreeModel::data(index, role);
		}
	}

	explicit DoodadTreeModel(QObject* parent)
		: BaseTreeModel(parent) {
		slk = &doodads_slk;

		custom_root = new BaseTreeItem(rootItem);
		custom_root->baseCategory = true;
		custom_root->customFolder = true;
		custom_root->label = "Custom";

		for (const auto& [key, value] : world_edit_data.section("DoodadCategories")) {
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

		for (size_t i = 0; i < doodads_slk.rows(); i++) {
			const std::string& id = doodads_slk.index_to_row.at(i);
			BaseTreeItem* item = new BaseTreeItem(getFolderParent(id));
			item->id = id;
			items.emplace(id, item);
		}

		categoryChangeFields = { "category" };
	}
};
