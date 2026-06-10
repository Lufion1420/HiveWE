module;

#include <QSortFilterProxyModel>

export module DoodadListModel;

import std;
import QIconResource;
import ResourceManager;
import Globals;
import BaseListModel;

export class DoodadListModel: public BaseListModel {
	Q_OBJECT

  public:
	explicit DoodadListModel(QObject* parent) : BaseListModel(doodads_slk, parent) {
		for (auto&& [key, value] : world_edit_data.section("DoodadCategories")) {
			const std::string tileset_key = value.front();
			icons[key.front()] = resource_manager.load<QIconResource>(value[1]).value();
		}
	}

	QModelIndex mapToSource(const QModelIndex& proxyIndex) const override {
		if (!proxyIndex.isValid()) {
			return {};
		}

		return sourceModel()->index(proxyIndex.row(), doodads_slk.column_headers.at("name"));
	}

	QVariant data(const QModelIndex& index, int role) const override {
		if (!index.isValid()) {
			return {};
		}

		switch (role) {
			case Qt::DisplayRole:
				return sourceModel()->data(mapToSource(index), role).toString();
			case Qt::UserRole:
				return QString::fromStdString("doodads/" + doodads_slk.data("category", index.row()) + "/" + doodads_slk.index_to_row.at(index.row()));
			case Qt::DecorationRole: {
				const std::string_view category = doodads_slk.data<std::string_view>("category", index.row());
				if (!category.empty()) {
					if (const auto found = icons.find(category.front()); found != icons.end()) {
						return found->second->icon;
					}
				}

				return {};
			}
			default:
				return BaseListModel::data(index, role);
		}
	}

  private:
	std::unordered_map<char, std::shared_ptr<QIconResource>> icons;
};

export class DoodadListFilter : public QSortFilterProxyModel {
	static constexpr std::string_view custom_category = "__custom__";

	[[nodiscard]] bool filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const override {
		if (!filterRegularExpression().pattern().isEmpty()) {
			if (QString::fromStdString(doodads_slk.index_to_row.at(sourceRow)).contains(filterRegularExpression())) {
				return true;
			}

			const QModelIndex source_index = sourceModel()->index(sourceRow, 0);
			return source_index.data().toString().contains(filterRegularExpression());
		}

		if (filterCategory) {
			const std::string_view id = doodads_slk.index_to_row.at(sourceRow);
			if (*filterCategory == custom_category) {
				if (!doodads_slk.shadow_data.contains(id) || !doodads_slk.shadow_data.at(id).contains("oldid")) {
					return false;
				}
			} else if (doodads_slk.data<std::string_view>("category", sourceRow) != *filterCategory) {
				return false;
			}
		}

		if (filterTileset) {
			const std::string_view tilesets = doodads_slk.data<std::string_view>("tilesets", sourceRow);
			if (tilesets.find('*') == std::string::npos && tilesets.find(*filterTileset) == std::string::npos && filterTileset != '*') {
				return false;
			}
		}

		return true;
	}

	bool lessThan(const QModelIndex& left, const QModelIndex& right) const override {
		// Sort on the resolved display name (the same text shown in the list, with TRIGSTR/etc.
		// references expanded) rather than the raw SLK field, and do so case-insensitively so the
		// ordering matches the World Editor.
		const QString left_name = sourceModel()->data(left, Qt::DisplayRole).toString();
		const QString right_name = sourceModel()->data(right, Qt::DisplayRole).toString();
		return left_name.localeAwareCompare(right_name) < 0;
	}

	std::optional<std::string> filterCategory;
	std::optional<char> filterTileset;

public:
	void setFilterCategory(const QString& category) {
		beginFilterChange();
		filterCategory = category.toStdString();
		endFilterChange(Direction::Rows);
	}

	void setFilterTileset(const char tileset) {
		beginFilterChange();
		filterTileset = tileset;
		endFilterChange(Direction::Rows);
	}

	using QSortFilterProxyModel::QSortFilterProxyModel;
};

#include "doodad_list_model.moc"
