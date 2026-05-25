module;

#include <QSortFilterProxyModel>

export module DestructibleListModel;

import std;
import QIconResource;
import ResourceManager;
import Globals;
import BaseListModel;

export class DestructableListModel: public BaseListModel {
	Q_OBJECT

  public:
	explicit DestructableListModel(QObject* parent = nullptr) : BaseListModel(destructibles_slk, parent) {
		for (auto&& [key, value] : world_edit_data.section("DestructibleCategories")) {
			const std::string tileset_key = value.front();
			icons[key.front()] = resource_manager.load<QIconResource>(value[1]).value();
		}
	}

	QModelIndex mapToSource(const QModelIndex& proxyIndex) const override {
		if (!proxyIndex.isValid()) {
			return {};
		}

		return sourceModel()->index(proxyIndex.row(), destructibles_slk.column_headers.at("name"));
	}

	QVariant data(const QModelIndex& index, int role) const override {
		if (!index.isValid()) {
			return {};
		}

		switch (role) {
			case Qt::DisplayRole:
				return sourceModel()->data(mapToSource(index), role).toString() + " " + QString::fromUtf8(destructibles_slk.data<std::string_view>("editorsuffix", index.row()));
			case Qt::UserRole:
				return QString::fromStdString("destructibles/" + destructibles_slk.data("category", index.row()) + "/" + destructibles_slk.index_to_row.at(index.row()));
			case Qt::DecorationRole: {
				const char category = destructibles_slk.data<std::string_view>("category", index.row()).front();
				if (icons.contains(category)) {
					return icons.at(category)->icon;
				} else {
					return {};
				}
			}
			default:
				return BaseListModel::data(index, role);
		}
	}

  private:
	std::unordered_map<char, std::shared_ptr<QIconResource>> icons;
};

export class DestructableListFilter: public QSortFilterProxyModel {
	static constexpr std::string_view custom_category = "__custom__";

	[[nodiscard]] bool filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const override {
		if (!filterRegularExpression().pattern().isEmpty()) {
			if (QString::fromStdString(destructibles_slk.index_to_row.at(sourceRow)).contains(filterRegularExpression())) {
				return true;
			}

			const QModelIndex source_index = sourceModel()->index(sourceRow, 0);
			return source_index.data().toString().contains(filterRegularExpression());
		}

		if (filterCategory) {
			const std::string_view id = destructibles_slk.index_to_row.at(sourceRow);
			if (*filterCategory == custom_category) {
				if (!destructibles_slk.shadow_data.contains(id) || !destructibles_slk.shadow_data.at(id).contains("oldid")) {
					return false;
				}
			} else if (destructibles_slk.data<std::string_view>("category", sourceRow) != *filterCategory) {
				return false;
			}
		}

		if (filterTileset) {
			const std::string_view tilesets = destructibles_slk.data<std::string_view>("tilesets", sourceRow);
			if (tilesets.find('*') == std::string::npos && tilesets.find(*filterTileset) == std::string::npos && filterTileset != '*') {
				return false;
			}
		}

		return true;
	}

	[[nodiscard]] bool lessThan(const QModelIndex& left, const QModelIndex& right) const override {
		return destructibles_slk.data<std::string_view>("name", left.row()) < destructibles_slk.data<std::string_view>("name", right.row());
	}

	std::optional<std::string> filterCategory;
	std::optional<char> filterTileset;

  public:
	void setFilterCategory(const QString& category) {
		beginFilterChange();
		filterCategory = category.toStdString();
		endFilterChange(Direction::Rows);
	}

	void setFilterTileset(char tileset) {
		beginFilterChange();
		filterTileset = tileset;
		endFilterChange(Direction::Rows);
	}

	using QSortFilterProxyModel::QSortFilterProxyModel;
};

#include "destructible_list_model.moc"
