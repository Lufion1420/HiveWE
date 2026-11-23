#include "gameplay_constants_editor.h"

import std;
import MapGlobal;
import GameplayConstants;
import TableModel;

#include "single_model.h"

#include <QHeaderView>
#include <QTableView>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QVBoxLayout>

namespace {
// Gives the two columns titles; the model's own horizontal header says "Value" twice.
class ConstantsProxy : public QSortFilterProxyModel {
  public:
	using QSortFilterProxyModel::QSortFilterProxyModel;

	QVariant headerData(const int section, const Qt::Orientation orientation, const int role) const override {
		if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
			return section == 0 ? "Value" : "Description";
		}
		return QSortFilterProxyModel::headerData(section, orientation, role);
	}
};
}

GameplayConstantsEditor::GameplayConstantsEditor(QWidget* parent) : QDialog(parent) {
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowTitle("Gameplay Constants");
	resize(1000, 600);

	auto* layout = new QVBoxLayout(this);
	search = new QLineEdit(this);
	search->setPlaceholderText("Search...");
	layout->addWidget(search);

	TableModel* table = new TableModel(&map->gameplay_constants.data, &map->gameplay_constants.metadata, &map->trigger_strings, this);
	SingleModel* single_model = new SingleModel(table, this);
	single_model->setID(constants_row_key);

	QSortFilterProxyModel* proxy_model = new ConstantsProxy(this);
	proxy_model->setSourceModel(single_model);
	proxy_model->setFilterKeyColumn(-1); // All columns
	proxy_model->setFilterCaseSensitivity(Qt::CaseInsensitive);

	TableDelegate* delegate = new TableDelegate(single_model);

	QTableView* view = new QTableView;
	layout->addWidget(view);
	view->setVerticalHeader(new AlterHeader(Qt::Vertical, view));
	view->setModel(proxy_model);
	view->setItemDelegate(delegate);
	view->setAlternatingRowColors(true);
	view->setIconSize({24, 24});
	layout->addWidget(view);

	connect(search, &QLineEdit::textChanged, proxy_model, &QSortFilterProxyModel::setFilterFixedString);

	show();

	// Category starts are taller than the other rows, so each row gets the height its delegate asks for; redone when a search changes the rows.
	const auto fit_rows = [view, proxy_model, delegate]() {
		QStyleOptionViewItem option;
		option.initFrom(view);
		option.widget = view;
		option.decorationSize = view->iconSize();
		for (int row = 0; row < proxy_model->rowCount(); ++row) {
			int height = 0;
			for (int column = 0; column < proxy_model->columnCount(); ++column) {
				height = std::max(height, delegate->sizeHint(option, proxy_model->index(row, column)).height());
			}
			view->setRowHeight(row, height);
		}
	};
	// One refit per event loop pass, however many row signals a search change emits.
	const auto fit_pending = std::make_shared<bool>(false);
	const auto schedule_fit_rows = [view, fit_rows, fit_pending]() {
		if (*fit_pending) {
			return;
		}
		*fit_pending = true;
		QTimer::singleShot(0, view, [fit_rows, fit_pending]() {
			*fit_pending = false;
			fit_rows();
		});
	};
	connect(proxy_model, &QAbstractItemModel::rowsInserted, this, schedule_fit_rows);
	connect(proxy_model, &QAbstractItemModel::rowsRemoved, this, schedule_fit_rows);
	connect(proxy_model, &QAbstractItemModel::layoutChanged, this, schedule_fit_rows);
	connect(proxy_model, &QAbstractItemModel::modelReset, this, schedule_fit_rows);

	// So the columns don't resize on search
	QTimer::singleShot(0, [view, proxy_model, fit_rows]() {
		view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
		fit_rows();
		const int vertical_width = view->verticalHeader()->width();
		view->verticalHeader()->setFixedWidth(vertical_width);

		view->resizeColumnsToContents();
		for (int i = 0; i < proxy_model->columnCount(); ++i) {
			const int horizontal_width = view->columnWidth(i);

			view->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Fixed);
			view->setColumnWidth(i, horizontal_width);
		}
	});
}
