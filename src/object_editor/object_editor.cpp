#include "object_editor.h"

#include <QTableView>
#include <QLineEdit>
#include <QToolBar>
#include <QDialogButtonBox>
#include <QSortFilterProxyModel>
#include <QAbstractProxyModel>
#include <QApplication>
#include <QPushButton>
#include <QItemSelection>
#include <QTimer>
#include <QLabel>
#include <QShortcut>
#include <QDialog>
#include <QToolButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QSet>

import std;
import UnitSelector;
import MapGlobal;
import Globals;
import ResourceManager;
import SlkConversions;
import "single_model.h";

namespace {
const BaseTreeItem* tree_item_from_index(const QModelIndex& index) {
	if (!index.isValid()) {
		return nullptr;
	}

	const QModelIndex source_index = [&]() -> QModelIndex {
		if (const auto* proxy = qobject_cast<const QAbstractProxyModel*>(index.model())) {
			return proxy->mapToSource(index);
		}
		return index;
	}();

	if (!source_index.isValid()) {
		return nullptr;
	}

	return static_cast<const BaseTreeItem*>(source_index.internalPointer());
}

QString tree_item_path(const QModelIndex& index) {
	QStringList parts;
	const auto* item = tree_item_from_index(index);
	while (item && item->parent) {
		if (item->baseCategory || item->subCategory) {
			parts.prepend(QString::fromStdString(item->label));
		}
		item = item->parent;
	}
	return parts.join('/');
}

void collect_expanded_paths(const QTreeView* view, const QModelIndex& parent, QStringList& paths) {
	const auto* model = view->model();
	for (int row = 0; row < model->rowCount(parent); ++row) {
		const QModelIndex child = model->index(row, 0, parent);
		if (!child.isValid()) {
			continue;
		}

		const auto* item = tree_item_from_index(child);
		if (!item || !(item->baseCategory || item->subCategory)) {
			continue;
		}

		if (view->isExpanded(child)) {
			paths.push_back(tree_item_path(child));
		}

		collect_expanded_paths(view, child, paths);
	}
}

void expand_saved_paths(QTreeView* view, const QModelIndex& parent, const QSet<QString>& paths) {
	const auto* model = view->model();
	for (int row = 0; row < model->rowCount(parent); ++row) {
		const QModelIndex child = model->index(row, 0, parent);
		if (!child.isValid()) {
			continue;
		}

		const auto* item = tree_item_from_index(child);
		if (!item || !(item->baseCategory || item->subCategory)) {
			continue;
		}

		const QString path = tree_item_path(child);
		if (paths.contains(path)) {
			view->expand(child);
		}

		expand_saved_paths(view, child, paths);
	}
}

bool is_focus_within(QWidget* focus_widget, QWidget* container) {
	return focus_widget && container && (focus_widget == container || container->isAncestorOf(focus_widget));
}
}

ObjectEditor::ObjectEditor(QWidget* parent) : QMainWindow(parent) {
	ui.setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose);

	std::ifstream f("data/warcraft/ability_insights.json");
	ability_insights = nlohmann::json::parse(f);

	custom_unit_icon = resource_manager.load<QIconResource>(world_edit_data.data("WorldEditArt", "ToolBarIcon_OE_NewUnit")).value();
	custom_item_icon = resource_manager.load<QIconResource>(world_edit_data.data("WorldEditArt", "ToolBarIcon_OE_NewItem")).value();
	custom_doodad_icon = resource_manager.load<QIconResource>(world_edit_data.data("WorldEditArt", "ToolBarIcon_OE_NewDood")).value();
	custom_destructible_icon = resource_manager.load<QIconResource>(world_edit_data.data("WorldEditArt", "ToolBarIcon_OE_NewDest")).value();
	custom_ability_icon = resource_manager.load<QIconResource>(world_edit_data.data("WorldEditArt", "ToolBarIcon_OE_NewAbil")).value();
	custom_buff_icon = resource_manager.load<QIconResource>(world_edit_data.data("WorldEditArt", "ToolBarIcon_OE_NewBuff")).value();
	custom_upgrade_icon = resource_manager.load<QIconResource>(world_edit_data.data("WorldEditArt", "ToolBarIcon_OE_NewUpgr")).value();

	dock_manager = new ads::CDockManager;
	dock_manager->setStyleSheet("");
	setCentralWidget(dock_manager);

	details_dock = new ads::CDockWidget(dock_manager, "Object Data");
	details_dock->setFeature(ads::CDockWidget::NoTab, true);
	dock_area = dock_manager->setCentralWidget(details_dock);
	reset_details_panel();

	unitTreeModel = new UnitTreeModel(this);
	itemTreeModel = new ItemTreeModel(this);
	doodadTreeModel = new DoodadTreeModel(this);
	destructibleTreeModel = new DestructibleTreeModel(this);
	abilityTreeModel = new AbilityTreeModel(this);
	upgradeTreeModel = new UpgradeTreeModel(this);
	buffTreeModel = new BuffTreeModel(this);

	addTypeTreeView(unitTreeModel, unitTreeFilter, units_table, unit_explorer, custom_unit_icon->icon, "Units", Category::unit);
	addTypeTreeView(itemTreeModel, itemTreeFilter, items_table, item_explorer, custom_item_icon->icon, "Items", Category::item);
	addTypeTreeView(
		doodadTreeModel,
		doodadTreeFilter,
		doodads_table,
		doodad_explorer,
		custom_doodad_icon->icon,
		"Doodads",
		Category::doodad
	);
	addTypeTreeView(
		destructibleTreeModel,
		destructibleTreeFilter,
		destructibles_table,
		destructible_explorer,
		custom_destructible_icon->icon,
		"Destructibles",
		Category::destructible
	);
	addTypeTreeView(
		abilityTreeModel,
		abilityTreeFilter,
		abilities_table,
		ability_explorer,
		custom_ability_icon->icon,
		"Abilities",
		Category::ability
	);
	addTypeTreeView(
		upgradeTreeModel,
		upgradeTreeFilter,
		upgrade_table,
		upgrade_explorer,
		custom_upgrade_icon->icon,
		"Upgrades",
		Category::upgrade
	);
	addTypeTreeView(buffTreeModel, buffTreeFilter, buff_table, buff_explorer, custom_buff_icon->icon, "Buffs", Category::buff);

	QSettings settings;
	explorer_area->setCurrentIndex(settings.value("ObjectEditor/currentTab", 0).toInt());
	// Set initial sizes, the second size doesn't really matter with only 2 dock areas
	dock_manager->setSplitterSizes(explorer_area, {645, 9999});

	connect(unit_explorer, &QTreeView::clicked, [&](const QModelIndex& index) {
		itemClicked(unit_explorer, unitTreeFilter, units_table, index);
	});
	connect(item_explorer, &QTreeView::clicked, [&](const QModelIndex& index) {
		itemClicked(item_explorer, itemTreeFilter, items_table, index);
	});
	connect(doodad_explorer, &QTreeView::clicked, [&](const QModelIndex& index) {
		itemClicked(doodad_explorer, doodadTreeFilter, doodads_table, index);
	});
	connect(destructible_explorer, &QTreeView::clicked, [&](const QModelIndex& index) {
		itemClicked(destructible_explorer, destructibleTreeFilter, destructibles_table, index);
	});
	connect(ability_explorer, &QTreeView::clicked, [&](const QModelIndex& index) {
		itemClicked(ability_explorer, abilityTreeFilter, abilities_table, index);
	});
	connect(upgrade_explorer, &QTreeView::clicked, [&](const QModelIndex& index) {
		itemClicked(upgrade_explorer, upgradeTreeFilter, upgrade_table, index);
	});
	connect(buff_explorer, &QTreeView::clicked, [&](const QModelIndex& index) {
		itemClicked(buff_explorer, buffTreeFilter, buff_table, index);
	});

	connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F), this), &QShortcut::activated, [&]() {
		auto edit = explorer_area->currentDockWidget()->findChild<QLineEdit*>("search");
		edit->setFocus();
		edit->selectAll();
	});
	connect(new QShortcut(QKeySequence(Qt::Key_O), this), &QShortcut::activated, this, &QWidget::close);
	connect(qApp, &QApplication::focusChanged, this, [this](QWidget*, QWidget*) {
		refresh_details_focus_visuals();
	});

	show();
}

bool ObjectEditor::restore_tree_state(const QString& key, QTreeView* view) const {
	QSettings settings;
	const QStringList expanded_paths = settings.value("ObjectEditor/" + key + "/expandedPaths").toStringList();
	if (expanded_paths.isEmpty()) {
		return false;
	}

	QSet<QString> expanded_path_set;
	for (const auto& path : expanded_paths) {
		expanded_path_set.insert(path);
	}

	view->collapseAll();
	expand_saved_paths(view, QModelIndex(), expanded_path_set);
	return true;
}

void ObjectEditor::save_tree_state(const QString& key, const QTreeView* view) const {
	QStringList expanded_paths;
	collect_expanded_paths(view, QModelIndex(), expanded_paths);

	QSettings settings;
	settings.setValue("ObjectEditor/" + key + "/expandedPaths", expanded_paths);
}

void ObjectEditor::closeEvent(QCloseEvent* event) {
	QSettings settings;
	settings.setValue("ObjectEditor/currentTab", explorer_area ? explorer_area->currentIndex() : 0);
	save_tree_state("units", unit_explorer);
	save_tree_state("items", item_explorer);
	save_tree_state("doodads", doodad_explorer);
	save_tree_state("destructibles", destructible_explorer);
	save_tree_state("abilities", ability_explorer);
	save_tree_state("upgrades", upgrade_explorer);
	save_tree_state("buffs", buff_explorer);

	QMainWindow::closeEvent(event);
}

void ObjectEditor::itemClicked(QTreeView* view, const QSortFilterProxyModel* model, TableModel* table, const QModelIndex& index) {
	const BaseTreeItem* item = static_cast<BaseTreeItem*>(model->mapToSource(index).internalPointer());
	if (item->baseCategory || item->subCategory) {
		return;
	}

	if (auto* area = qobject_cast<QScrollArea*>(details_dock->widget())) {
		current_details_scroll_y = area->verticalScrollBar()->value();
	}

	current_explorer_view = view;
	open_by_id(table, item->id, index.data(Qt::DisplayRole).toString(), index.data(Qt::DecorationRole).value<QIcon>());
	view->setFocus(Qt::FocusReason::OtherFocusReason);
}

void ObjectEditor::reset_details_panel() {
	QLabel* image = new QLabel();
	image->setPixmap(QPixmap("data/icons/object_editor/background.png"));
	image->setAlignment(Qt::AlignCenter);
	details_dock->setWidget(image);
	details_dock->setWindowTitle("Object Data");
	details_dock->setIcon(QIcon());
	current_details_id.clear();
	current_details_view = nullptr;
	current_details_scroll_y = 0;
}

void ObjectEditor::refresh_details_focus_visuals() const {
	if (!current_details_view) {
		return;
	}

	current_details_view->viewport()->update();
	if (auto* header = current_details_view->verticalHeader()) {
		header->viewport()->update();
	}
}

void ObjectEditor::open_by_id(TableModel* table, const std::string& id, const QString& name, QIcon icon) {
	if (current_details_id == id) {
		details_dock->setFocus();
		details_dock->raise();
		return;
	}

	QVBoxLayout* layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);

	const auto found = ability_insights.find(id);
	if (found != ability_insights.end()) {
		QLabel* title = new QLabel(QString::fromUtf8((*found)["name"].get<std::string_view>()));
		QFont font1 = title->font();
		font1.setBold(true);
		font1.setPointSize(15);
		title->setFont(font1);

		std::string all_tags = "Tags: ";
		for (const auto& tag : (*found)["tags"]) {
			all_tags += tag.get<std::string_view>() + " ";
		}

		QLabel* tags = new QLabel(QString::fromStdString(all_tags));
		QFont font2 = tags->font();
		font2.setBold(true);
		tags->setFont(font2);

		QLabel* label = new QLabel(QString::fromUtf8((*found)["raw_text"].get<std::string_view>()));
		QLabel* latest_tested_version =
			new QLabel("Latest tested version: " + QString::fromUtf8((*found)["latest_tested_version"].get<std::string_view>()));
		latest_tested_version->setFont(font2);

		QLabel* link = new QLabel(
			"Fix mistakes or add info directly in <a href=\"https://docs.google.com/document/d/1z17FTnhyfVL87tJgLmwWks3Low6TuQ0tjfKHXBELWpo/edit\">the Google Docs</a>!"
		);
		link->setOpenExternalLinks(true);

		layout->addWidget(title);
		layout->addWidget(tags);
		layout->addWidget(label);
		layout->addWidget(latest_tested_version);
		layout->addWidget(link);
		label->setWordWrap(true);
	}

	SingleModel* single_model = new SingleModel(table, this);
	single_model->setID(id);

	TableDelegate* delegate = new TableDelegate(single_model);

	QTableView* view = new QTableView;
	view->setItemDelegate(delegate);
	view->horizontalHeader()->hide();
	view->setAlternatingRowColors(false);
	view->setVerticalHeader(new AlterHeader(Qt::Vertical, view));
	view->setSelectionBehavior(QAbstractItemView::SelectRows);
	view->setSelectionMode(QAbstractItemView::SingleSelection);
	view->setTabKeyNavigation(false);
	view->verticalHeader()->setSectionsClickable(true);
	view->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::ResizeToContents);
	view->verticalHeader()->setMinimumSectionSize(28);
	view->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::Stretch);
	view->setIconSize({24, 24});
	view->setWordWrap(true);
	view->setSizeAdjustPolicy(QAbstractScrollArea::SizeAdjustPolicy::AdjustToContents);
	{
		QPalette palette = view->palette();
		palette.setColor(QPalette::Inactive, QPalette::Highlight, palette.color(QPalette::Midlight));
		palette.setColor(QPalette::Inactive, QPalette::HighlightedText, palette.color(QPalette::Text));
		view->setPalette(palette);
		view->verticalHeader()->setPalette(palette);
	}
	view->setModel(single_model);
	current_details_view = view;
	connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this, [this, single_model](const QModelIndex& current, const QModelIndex&) {
		if (!current.isValid()) {
			current_detail_key.clear();
			current_detail_level = -1;
			return;
		}

		const auto& mapping = single_model->getMapping();
		current_detail_key = mapping[current.row()].key;
		current_detail_level = mapping[current.row()].level;
	});
	connect(view, &QTableView::pressed, this, [this](const QModelIndex&) {
		refresh_details_focus_visuals();
	});

	if (!current_detail_key.empty()) {
		const int row = single_model->find_mapping_row(current_detail_key, current_detail_level);
		if (row >= 0) {
			const QModelIndex detail_index = single_model->index(row, 0);
			view->setCurrentIndex(detail_index);
			view->selectionModel()->select(detail_index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		}
	}

	connect(view->verticalHeader(), &QHeaderView::sectionPressed, view, [view, single_model](int section) {
		const QModelIndex index = single_model->index(section, 0);
		view->setCurrentIndex(index);
		view->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		view->setFocus(Qt::FocusReason::MouseFocusReason);
	});

	QWidget* column_header = new QWidget;
	column_header->setObjectName("objectEditorColumnHeader");
	column_header->setStyleSheet(
		"#objectEditorColumnHeader {"
		"border-bottom: 1px solid palette(mid);"
		"}"
	);

	QHBoxLayout* column_header_layout = new QHBoxLayout(column_header);
	column_header_layout->setContentsMargins(0, 0, 0, 0);
	column_header_layout->setSpacing(0);

	QLabel* name_header = new QLabel("Name");
	name_header->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	name_header->setContentsMargins(8, 4, 8, 4);

	QFrame* header_separator = new QFrame;
	header_separator->setFrameShape(QFrame::VLine);
	header_separator->setFrameShadow(QFrame::Sunken);

	QLabel* value_header = new QLabel("Value");
	value_header->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	value_header->setContentsMargins(8, 4, 8, 4);

	column_header_layout->addWidget(name_header);
	column_header_layout->addWidget(header_separator);
	column_header_layout->addWidget(value_header, 1);

	const auto sync_column_header = [view, name_header]() {
		name_header->setFixedWidth(view->verticalHeader()->width());
	};
	connect(view->verticalHeader(), &QHeaderView::geometriesChanged, column_header, sync_column_header);
	sync_column_header();

	layout->addWidget(column_header);
	layout->addWidget(view);

	QWidget* container = new QWidget;
	container->setLayout(layout);

	QScrollArea* area = new QScrollArea;
	area->setWidget(container);
	area->setWidgetResizable(true);

	details_dock->setWidget(area);
	details_dock->setWindowTitle(name);
	details_dock->setIcon(icon);
	current_details_id = id;

	// Keep the user's current details scroll position when switching objects.
	// On first open, scroll past the ability insights section.
	if (current_details_scroll_y > 0) {
		area->verticalScrollBar()->setValue(current_details_scroll_y);
	} else {
		const int y = view->mapTo(area->widget(), QPoint(0, 0)).y();
		area->verticalScrollBar()->setValue(y);
	}
}

void ObjectEditor::addTypeTreeView(
	BaseTreeModel* treeModel,
	BaseFilter*& filter,
	TableModel* table,
	QTreeView* view,
	QIcon icon,
	QString name,
	Category category
) {
	treeModel->setSourceModel(table);
	filter = new BaseFilter;
	filter->slk = table->slk;
	filter->setRecursiveFilteringEnabled(true);
	filter->setFilterCaseSensitivity(Qt::CaseSensitivity::CaseInsensitive);
	filter->setDynamicSortFilter(true);
	filter->setSourceModel(treeModel);
	filter->sort(0, Qt::AscendingOrder);
	view->setModel(filter);
	view->header()->hide();
	view->setContextMenuPolicy(Qt::CustomContextMenu);
	view->setSelectionBehavior(QAbstractItemView::SelectRows);
	view->setSelectionMode(QAbstractItemView::ExtendedSelection);
	view->setUniformRowHeights(true);
	view->collapseAll();
	connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, [=, this](const QItemSelection&, const QItemSelection&) {
		const QModelIndex current = view->selectionModel()->currentIndex();
		if (!current.isValid()) {
			return;
		}

		itemClicked(view, filter, table, current);
	});

	const std::function<bool(const QModelIndex&)> expand_custom_branch = [&](const QModelIndex& parent) -> bool {
		bool subtree_has_custom = false;

		for (int row = 0; row < filter->rowCount(parent); ++row) {
			const QModelIndex child = filter->index(row, 0, parent);
			if (!child.isValid()) {
				continue;
			}

			const auto* tree_item = static_cast<const BaseTreeItem*>(filter->mapToSource(child).internalPointer());
			if (!tree_item) {
				continue;
			}

			const bool child_has_custom = (tree_item->baseCategory || tree_item->subCategory) ? expand_custom_branch(child)
																								  : table->slk->shadow_data.contains(tree_item->id)
																										&& table->slk->shadow_data.at(tree_item->id).contains("oldid");

			if (child_has_custom) {
				subtree_has_custom = true;
				if (tree_item->customFolder) {
					view->expand(child);
				}
			}
		}

		return subtree_has_custom;
	};
	if (!restore_tree_state(name.toLower(), view)) {
		expand_custom_branch(QModelIndex());
	}

	connect(view, &QTreeView::customContextMenuRequested, [=, this](const QPoint& pos) {
		QMenu menu;
		QAction* addAction = menu.addAction("Add " + name);
		QAction* removeAction = menu.addAction("Remove " + name);

		if (category == Category::doodad) {
			QAction* convert_to_destructible = menu.addAction("Convert to destructible");

			{
				const auto selection = view->selectionModel()->selectedIndexes();
				if (selection.isEmpty()) {
					convert_to_destructible->setDisabled(true);
				} else {
					const auto treeItem = static_cast<BaseTreeItem*>(filter->mapToSource(selection.front()).internalPointer());
					if (treeItem->id.empty()) {
						convert_to_destructible->setDisabled(true);
					}
				}
			}

			connect(convert_to_destructible, &QAction::triggered, [=, this]() {
				QModelIndexList selection = view->selectionModel()->selectedIndexes();

				for (const auto& i : selection) {
					BaseTreeItem* treeItem = static_cast<BaseTreeItem*>(filter->mapToSource(i).internalPointer());
					if (treeItem->id.empty()) {
						continue;
					}
					std::string new_id;
					destructibles_table->addRow([&] {
						// We pick a barrel (LTbr) for no good reason
						new_id = convert_doodad_to_destructible(treeItem->id, "LTbr");
					});

					const auto index = destructibles_table->rowIDToIndex(new_id);
					const auto index2 = destructibleTreeModel->mapFromSource(index);
					const auto index3 = destructibleTreeFilter->mapFromSource(index2);
					itemClicked(destructible_explorer, destructibleTreeFilter, destructibles_table, index3);
				}
			});
		}

		QModelIndexList selection = view->selectionModel()->selectedIndexes();
		if (selection.empty()) {
			removeAction->setDisabled(true);
		} else {
			BaseTreeItem* treeItem = static_cast<BaseTreeItem*>(filter->mapToSource(selection.front()).internalPointer());
			if (!table->slk->shadow_data.contains(treeItem->id) || !table->slk->shadow_data.at(treeItem->id).contains("oldid")) {
				removeAction->setDisabled(true);
			}
		}

		// add new object
		connect(addAction, &QAction::triggered, [=, this]() {
			QDialog* selectdialog = new QDialog(this, Qt::WindowTitleHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);
			selectdialog->resize(300, 560);
			selectdialog->setWindowModality(Qt::WindowModality::WindowModal);

			QLineEdit* nameEdit = new QLineEdit;
			nameEdit->setPlaceholderText("New name");
			nameEdit->setReadOnly(true);

			QLineEdit* id = new QLineEdit;
			id->setPlaceholderText("Free ID");
			id->setText(QString::fromStdString(map->get_unique_id(false)));
			id->setFont(QFont("consolas"));

			// error icon inside the id field, hidden by default
			QAction* idErrorIcon =
				id->addAction(selectdialog->style()->standardIcon(QStyle::SP_MessageBoxCritical), QLineEdit::TrailingPosition);
			idErrorIcon->setVisible(false);
			idErrorIcon->setToolTip("");

			QHBoxLayout* nameLayout = new QHBoxLayout;
			nameLayout->addWidget(nameEdit, 3);
			nameLayout->addWidget(id, 1);

			BaseFilter* sub_filter = new BaseFilter;
			sub_filter->slk = table->slk;
			sub_filter->setRecursiveFilteringEnabled(true);
			sub_filter->setFilterCaseSensitivity(Qt::CaseSensitivity::CaseInsensitive);
			sub_filter->setSourceModel(treeModel);

			QLineEdit* search = new QLineEdit;
			search->setPlaceholderText("Search " + name);

			QTreeView* sub_view = new QTreeView;
			sub_view->setModel(sub_filter);
			sub_view->setUniformRowHeights(true);
			sub_view->header()->hide();
			sub_view->expandAll();

			QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
			connect(buttonBox, &QDialogButtonBox::accepted, selectdialog, &QDialog::accept);
			connect(buttonBox, &QDialogButtonBox::rejected, selectdialog, &QDialog::reject);
			buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

			QVBoxLayout* selectlayout = new QVBoxLayout(selectdialog);
			selectlayout->addLayout(nameLayout);
			selectlayout->addWidget(search);
			selectlayout->addWidget(sub_view);
			selectlayout->addWidget(buttonBox);

			// validation lambda to check if input is valid
			auto is_valid = [sub_view, sub_filter, table, id, buttonBox, idErrorIcon]() {
				std::string new_id = id->text().toStdString();
				bool wrong_size = new_id.size() != 4;
				bool is_duplicate = table->slk->row_headers.contains(new_id);

				// check if an actual object (not a folder) is selected
				bool object_selected = false;
				QModelIndex current = sub_view->selectionModel()->currentIndex();
				if (current.isValid()) {
					const BaseTreeItem* treeItem = static_cast<BaseTreeItem*>(sub_filter->mapToSource(current).internalPointer());
					object_selected = !treeItem->baseCategory && !treeItem->subCategory;
				}

				// update UI based on validation
				if (wrong_size) {
					idErrorIcon->setVisible(true);
					idErrorIcon->setToolTip("ID must be exactly 4 characters long.");
					id->setStyleSheet("QLineEdit { border: 1px solid #e74c3c; }");
					buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
				} else if (is_duplicate) {
					idErrorIcon->setVisible(true);
					idErrorIcon->setToolTip(QString::fromStdString(std::format("ID '{}' is already in use.", new_id)));
					id->setStyleSheet("QLineEdit { border: 1px solid #e74c3c; }");
					buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
				} else if (!object_selected) {
					idErrorIcon->setVisible(false);
					id->setStyleSheet("");
					buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
				} else {
					// all valid
					idErrorIcon->setVisible(false);
					id->setStyleSheet("");
					buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
				}
			};

			connect(search, &QLineEdit::textChanged, [=](const QString& string) {
				sub_filter->setFilterFixedString(string);
				sub_view->expandAll();
			});

			connect(
				sub_view->selectionModel(),
				&QItemSelectionModel::currentChanged,
				[table, sub_filter, filter, id, nameEdit, is_valid](const QModelIndex& current, const QModelIndex& previous) {
					if (!current.isValid()) {
						return;
					}
					nameEdit->setText(sub_filter->data(current).toString());
					const BaseTreeItem* treeItem = static_cast<BaseTreeItem*>(sub_filter->mapToSource(current).internalPointer());
					if (!(treeItem->baseCategory || treeItem->subCategory)) {
						id->setText(QString::fromStdString(map->get_unique_id(!islower(treeItem->id.front()))));
					}
					is_valid();
				}
			);

			// id input field
			connect(id, &QLineEdit::textChanged, [is_valid](const QString& text) {
				is_valid();
			});

			auto select = [view, table, sub_filter, filter, selectdialog, id, treeModel](const QModelIndex& index) {
				if (id->text().size() != 4) {
					return;
				}

				// check if another object with selected ID already exists
				std::string new_id = id->text().toStdString();
				if (table->slk->row_headers.contains(new_id)) {
					return;
				}

				const BaseTreeItem* treeItem = static_cast<BaseTreeItem*>(sub_filter->mapToSource(index).internalPointer());
				if (treeItem->baseCategory || treeItem->subCategory) {
					return;
				}

				selectdialog->close();
				table->copyRow(treeItem->id, id->text().toStdString());

				QModelIndex new_index = filter->mapFromSource(treeModel->mapFromSource(table->index(table->rowCount() - 1, 0)));
				view->setCurrentIndex(new_index);
				view->scrollTo(new_index, QAbstractItemView::ScrollHint::PositionAtCenter);
			};

			connect(sub_view, &QTreeView::activated, [select](const QModelIndex& index) {
				select(index);
			});

			connect(selectdialog, &QDialog::accepted, [sub_view, select]() {
				auto indices = sub_view->selectionModel()->selectedIndexes();
				if (indices.empty()) {
					return;
				}

				select(indices.front());
			});

			connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F), selectdialog), &QShortcut::activated, [=]() {
				search->setFocus();
				search->selectAll();
			});

			selectdialog->show();
			search->setFocus();
		});

		connect(removeAction, &QAction::triggered, [=, this]() {
			std::vector<std::string> ids_to_delete;
			for (const auto& i : selection) {
				BaseTreeItem* treeItem = static_cast<BaseTreeItem*>(filter->mapToSource(i).internalPointer());

				// skip folders/categories
				if (treeItem->baseCategory || treeItem->subCategory) {
					continue;
				}

				// skip base game objects (only delete custom objects)
				if (!table->slk->shadow_data.contains(treeItem->id) || !table->slk->shadow_data.at(treeItem->id).contains("oldid")) {
					continue;
				}

				ids_to_delete.push_back(treeItem->id);

				if (current_details_id == treeItem->id) {
					reset_details_panel();
				}
			}
			for (const auto& i : ids_to_delete) {
				table->deleteRow(i);
			}
		});

		menu.exec(view->mapToGlobal(pos));
	});

	QLineEdit* search = new QLineEdit;
	search->setObjectName("search");
	search->setClearButtonEnabled(true);
	search->setPlaceholderText("Search " + name);
	connect(search, &QLineEdit::textChanged, [=](const QString& string) {
		filter->setFilterFixedString(string);
		view->expandAll();
	});

	QToolButton* hideDefault = new QToolButton;
	hideDefault->setIcon(icon);
	hideDefault->setToolTip("Hide default " + name);
	hideDefault->setCheckable(true);
	connect(hideDefault, &QToolButton::toggled, [=](bool checked) {
		filter->setFilterCustom(checked);
		if (!checked) {
			view->expandAll();
		}
	});

	QToolBar* bar = new QToolBar;
	bar->addWidget(search);
	bar->addWidget(hideDefault);

	ads::CDockWidget* tab = new ads::CDockWidget(dock_manager, name);
	tab->setToolBar(bar);
	tab->setWidget(view);
	tab->setFeature(ads::CDockWidget::DockWidgetClosable, false);
	tab->setFeature(ads::CDockWidget::DockWidgetAlwaysCloseAndDelete, false);
	tab->setIcon(icon);
	if (explorer_area == nullptr) {
		explorer_area = dock_manager->addDockWidget(ads::LeftDockWidgetArea, tab);
	} else {
		dock_manager->addDockWidget(ads::CenterDockWidgetArea, tab, explorer_area);
	}
}

void ObjectEditor::select_id(Category category, const std::string& id) const {
	explorer_area->setCurrentIndex(static_cast<int>(category));
	const auto edit = explorer_area->currentDockWidget()->findChild<QLineEdit*>("search");
	edit->clear();

	switch (category) {
		case Category::unit: {
			const auto index = unitTreeFilter->mapFromSource(unitTreeModel->getIdIndex(id));
			unit_explorer->setCurrentIndex(index);
			unit_explorer->scrollTo(index, QAbstractItemView::ScrollHint::PositionAtCenter);
			emit unit_explorer->clicked(index);
			break;
		}
		case Category::item: {
			const auto index = itemTreeFilter->mapFromSource(itemTreeModel->getIdIndex(id));
			item_explorer->setCurrentIndex(index);
			item_explorer->scrollTo(index, QAbstractItemView::ScrollHint::PositionAtCenter);
			emit item_explorer->clicked(index);
			break;
		}
		case Category::doodad: {
			const auto index = doodadTreeFilter->mapFromSource(doodadTreeModel->getIdIndex(id));
			doodad_explorer->setCurrentIndex(index);
			doodad_explorer->scrollTo(index, QAbstractItemView::ScrollHint::PositionAtCenter);
			emit doodad_explorer->clicked(index);
			break;
		}
		case Category::destructible: {
			const auto index = destructibleTreeFilter->mapFromSource(destructibleTreeModel->getIdIndex(id));
			destructible_explorer->setCurrentIndex(index);
			destructible_explorer->scrollTo(index, QAbstractItemView::PositionAtCenter);
			emit destructible_explorer->clicked(index);
			break;
		}
		case Category::ability: {
			const auto index = abilityTreeFilter->mapFromSource(abilityTreeModel->getIdIndex(id));
			ability_explorer->setCurrentIndex(index);
			ability_explorer->scrollTo(index, QAbstractItemView::ScrollHint::PositionAtCenter);
			emit ability_explorer->clicked(index);
			break;
		}
		case Category::upgrade: {
			const auto index = upgradeTreeFilter->mapFromSource(upgradeTreeModel->getIdIndex(id));
			upgrade_explorer->setCurrentIndex(index);
			upgrade_explorer->scrollTo(index, QAbstractItemView::ScrollHint::PositionAtCenter);
			emit upgrade_explorer->clicked(index);
			break;
		}
		case Category::buff: {
			const auto index = buffTreeFilter->mapFromSource(buffTreeModel->getIdIndex(id));
			buff_explorer->setCurrentIndex(index);
			buff_explorer->scrollTo(index, QAbstractItemView::ScrollHint::PositionAtCenter);
			emit buff_explorer->clicked(index);
			break;
		}
	}
}

void ObjectEditor::keyPressEvent(QKeyEvent* event) {
	if (event->key() == Qt::Key_Shift && !event->isAutoRepeat()) {
		if (double_shift_timer.isValid() && double_shift_timer.elapsed() < 400) {
			new GlobalSearchWidget(this);
			double_shift_timer.invalidate();
		} else {
			double_shift_timer.start();
		}
	}

	QMainWindow::keyPressEvent(event);
}

bool ObjectEditor::focusNextPrevChild(const bool next) {
	QWidget* focus_widget = QApplication::focusWidget();
	if (current_explorer_view && current_details_view) {
		if (is_focus_within(focus_widget, current_explorer_view)) {
			current_details_view->setFocus(next ? Qt::TabFocusReason : Qt::BacktabFocusReason);
			return true;
		}

		if (is_focus_within(focus_widget, current_details_view)) {
			current_explorer_view->setFocus(next ? Qt::TabFocusReason : Qt::BacktabFocusReason);
			return true;
		}
	}

	return QMainWindow::focusNextPrevChild(next);
}
