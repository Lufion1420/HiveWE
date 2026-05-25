module;

#include <QSortFilterProxyModel>
#include <QSize>

export module UnitListModel;

import BaseListModel;
import Globals;

export class UnitListModel: public BaseListModel {
	Q_OBJECT

  public:
	explicit UnitListModel(QObject* parent = nullptr) : BaseListModel(units_slk, parent) {}

	[[nodiscard]]
	QModelIndex mapToSource(const QModelIndex& proxyIndex) const override {
		if (!proxyIndex.isValid()) {
			return {};
		}

		return sourceModel()->index(proxyIndex.row(), units_slk.column_headers.at("name"));
	}

	[[nodiscard]]
	QVariant data(const QModelIndex& index, int role) const override {
		if (!index.isValid()) {
			return {};
		}

		switch (role) {
			case Qt::DisplayRole: {
				QString label = mapToSource(index).data(role).toString() + " " + QString::fromUtf8(units_slk.data<std::string_view>("editorsuffix", index.row()));
				const std::string_view id = units_slk.index_to_row.at(index.row());
				if (units_slk.shadow_data.contains(id) && units_slk.shadow_data.at(id).contains("oldid")) {
					const bool isBuilding = units_slk.data<std::string_view>("isbldg", index.row()) == "1";
					const bool isHero = isupper(id.front());
					if (isHero) {
						label += " [Hero]";
					} else if (isBuilding) {
						label += " [Building]";
					} else {
						label += " [Unit]";
					}
				}
				return label;
			}
			case Qt::UserRole:
				return QString::fromStdString("units/" + units_slk.data("race", index.row()) + "/" + units_slk.index_to_row.at(index.row()));
			case Qt::DecorationRole:
				return sourceModel()->index(index.row(), units_slk.column_headers.at("art")).data(role);
			default:
				return BaseListModel::data(index, role);
		}
	}
};

export class UnitListFilter: public QSortFilterProxyModel {
	Q_OBJECT
	static constexpr std::string_view custom_race = "__custom__";

	[[nodiscard]]
	bool filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const override {
		if (!filterRegularExpression().pattern().isEmpty()) {
			if (QString::fromStdString(units_slk.index_to_row.at(sourceRow)).contains(filterRegularExpression())) {
				return true;
			}

			const QModelIndex source_index = sourceModel()->index(sourceRow, 0);
			return source_index.data().toString().contains(filterRegularExpression());
		}

		if (filterRace) {
			const std::string_view id = units_slk.index_to_row.at(sourceRow);
			if (*filterRace == custom_race) {
				if (!units_slk.shadow_data.contains(id) || !units_slk.shadow_data.at(id).contains("oldid")) {
					return false;
				}
			} else if (units_slk.data<std::string_view>("race", sourceRow) != *filterRace) {
				return false;
			}
		}

		return true;
	}

	[[nodiscard]]
	bool lessThan(const QModelIndex& left, const QModelIndex& right) const override {
		if (filterRace && *filterRace == custom_race) {
			auto sort_key = [&](const QModelIndex& index) {
				const bool isBuilding = units_slk.data<std::string_view>("isbldg", index.row()) == "1";
				const bool isHero = isupper(units_slk.index_to_row.at(index.row()).front());

				QString prefix = "1";
				if (isHero) {
					prefix = "0";
				} else if (isBuilding) {
					prefix = "2";
				}

				return prefix + QString::fromUtf8(units_slk.data<std::string_view>("name", index.row()));
			};

			return sort_key(left) < sort_key(right);
		}

		QString leftIndex = "0";
		{
			const bool isBuilding = units_slk.data<std::string_view>("isbldg", left.row()) == "1";
			const bool isHero = isupper(units_slk.index_to_row.at(left.row()).front());
			const bool isSpecial = units_slk.data<std::string_view>("special", left.row()) == "1";

			if (isSpecial) {
				leftIndex = "3";
			} else if (isBuilding) {
				leftIndex = "1";
			} else if (isHero) {
				leftIndex = "2";
			}
			leftIndex += QString::fromUtf8(units_slk.data<std::string_view>("name", left.row()));
		}

		QString rightIndex = "0";
		{
			const bool isBuilding = units_slk.data<std::string_view>("isbldg", right.row()) == "1";
			const bool isHero = isupper(units_slk.index_to_row.at(right.row()).front());
			const bool isSpecial = units_slk.data<std::string_view>("special", right.row()) == "1";

			if (isSpecial) {
				rightIndex = "3";
			} else if (isBuilding) {
				rightIndex = "1";
			} else if (isHero) {
				rightIndex = "2";
			}
			rightIndex += QString::fromUtf8(units_slk.data<std::string_view>("name", right.row()));
		}
		
		return leftIndex < rightIndex;
	}

	std::optional<std::string> filterRace;

  public:
	using QSortFilterProxyModel::QSortFilterProxyModel;

  public slots:

	void setFilterRace(const QString& race) {
  		beginFilterChange();
		filterRace = race.toStdString();
  		endFilterChange(Direction::Rows);
	}
};

#include "unit_list_model.moc"
