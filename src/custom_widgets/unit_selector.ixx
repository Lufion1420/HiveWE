module;

#include <QListView>
#include <QComboBox>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QSortFilterProxyModel>

export module UnitSelector;

import std;
import Globals;
import UnitListModel;
import TableModel;

export class UnitSelector : public QWidget {
	Q_OBJECT

  public:
	explicit UnitSelector(QWidget* parent = nullptr)
		: QWidget(parent) {
		list_model = new UnitListModel(this);
		list_model->setSourceModel(units_table);

		filter_model = new UnitListFilter(this);
		filter_model->setFilterCaseSensitivity(Qt::CaseInsensitive);
		filter_model->setSourceModel(list_model);
		filter_model->sort(0, Qt::AscendingOrder);
		filter_model->setFilterRace("human");

		race = new QComboBox(this);
		search = new QLineEdit(this);
		units = new QListView(this);

		race->setObjectName("unitSelectorRace");
		race->setMaxVisibleItems(16);
		search->setObjectName("unitSelectorSearch");
		search->setPlaceholderText("Search units");
		search->setClearButtonEnabled(true);
		units->setObjectName("unitSelectorList");

		units->setUniformItemSizes(true);
		units->setIconSize(QSize(24, 24));
		units->setSpacing(4);
		units->setSelectionMode(QAbstractItemView::SingleSelection);
		units->setAlternatingRowColors(false);
		units->setModel(filter_model);

		QVBoxLayout* layout = new QVBoxLayout;
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(8);
		layout->addWidget(race);
		layout->addWidget(search);
		layout->addWidget(units);
		setLayout(layout);

		for (const auto& [key, value] : unit_editor_data.section("unitRace")) {
			if (key == "Sort" || key == "NumValues") {
				continue;
			}
			race->addItem(QString::fromStdString(value[1]), QString::fromStdString(value[0]));
		}
		race->addItem("Custom", "__custom__");

		connect(race, QOverload<int>::of(&QComboBox::currentIndexChanged), [&]() {
			filter_model->setFilterRace(race->currentData().toString());
		});

		connect(search, &QLineEdit::textEdited, filter_model, &QSortFilterProxyModel::setFilterFixedString);
		connect(search, &QLineEdit::returnPressed, [&]() {
			units->setCurrentIndex(units->model()->index(0, 0));
			units->setFocus();
		});

		auto emit_unit_at_index = [this](const QModelIndex& index) {
			if (!index.isValid()) {
				return;
			}
			const int row = filter_model->mapToSource(index).row();
			if (!units_slk.index_to_row.contains(row)) {
				return;
			}
			emit unitSelected(units_slk.index_to_row.at(row));
		};

		connect(units, &QListView::activated, this, emit_unit_at_index);
		connect(units->selectionModel(), &QItemSelectionModel::currentChanged, this, emit_unit_at_index);
	}

	UnitListModel* list_model;
	UnitListFilter* filter_model;

	QComboBox* race;
	QLineEdit* search;
	QListView* units;

  public slots:
	void forceSelection() {
		if (units->selectionModel()->selectedRows().isEmpty()) {
			return;
		}
		QModelIndex index = units->selectionModel()->selectedRows().front();
		if (!index.isValid()) {
			return;
		}
		const int row = filter_model->mapToSource(index).row();
		emit unitSelected(units_slk.index_to_row.at(row));
	}

  signals:
	void unitSelected(std::string id);
};

#include "unit_selector.moc"
