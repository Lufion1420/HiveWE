#include "object_editor.h"

#include <QTableView>
#include <QLineEdit>
#include <QToolBar>
#include <QDialogButtonBox>
#include <QSortFilterProxyModel>
#include <QAbstractProxyModel>
#include <QApplication>
#include <QClipboard>
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
#include <QButtonGroup>
#include <QComboBox>
#include <QFrame>
#include <QStyledItemDelegate>
#include <QStyle>

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

class ObjectTreeDelegate : public QStyledItemDelegate {
  public:
	using QStyledItemDelegate::QStyledItemDelegate;

	QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
		QSize size = QStyledItemDelegate::sizeHint(option, index);
		size.setHeight(std::max(size.height(), 24));
		return size;
	}

	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
		QStyleOptionViewItem opt(option);
		initStyleOption(&opt, index);

		const QModelIndex source_index = [&]() -> QModelIndex {
			if (const auto* proxy = qobject_cast<const QAbstractProxyModel*>(index.model())) {
				return proxy->mapToSource(index);
			}
			return index;
		}();

		const auto* item = source_index.isValid() ? static_cast<const BaseTreeItem*>(source_index.internalPointer()) : nullptr;
		const bool is_folder = item && (item->baseCategory || item->subCategory);
		const bool is_custom = item && !is_folder && index.data(Qt::ForegroundRole).value<QColor>() == QColor("violet");
		const bool selected = opt.state.testFlag(QStyle::State_Selected);

		painter->save();
		if (selected) {
			painter->fillRect(opt.rect, opt.palette.highlight());
		} else if (opt.state.testFlag(QStyle::State_MouseOver)) {
			painter->fillRect(opt.rect, QColor(255, 255, 255, 10));
		}

		const QRect content_rect = opt.rect.adjusted(6, 0, -8, 0);
		QRect icon_rect(content_rect.left(), content_rect.top() + (content_rect.height() - 18) / 2, 18, 18);
		const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
		icon.paint(painter, icon_rect);

		QString full_text = opt.text;
		QString main_text = full_text;
		QString id_text;
		if (!is_folder && full_text.endsWith(')')) {
			const int id_start = full_text.lastIndexOf(" (");
			if (id_start > 0) {
				main_text = full_text.first(id_start);
				id_text = full_text.mid(id_start + 1);
			}
		}

		int text_left = icon_rect.right() + 8;
		QRect text_rect = content_rect.adjusted(text_left - content_rect.left(), 0, 0, 0);
		QFont main_font = opt.font;
		if (is_folder && item->baseCategory) {
			main_font.setBold(true);
		}
		painter->setFont(main_font);
		painter->setPen(selected ? opt.palette.highlightedText().color() : opt.palette.text().color());

		if (id_text.isEmpty()) {
			painter->drawText(text_rect, Qt::AlignVCenter | Qt::AlignLeft, main_text);
		} else {
			QFontMetrics fm(main_font);
			const int main_width = fm.horizontalAdvance(main_text);
			const int gap = 6;
			painter->drawText(text_rect, Qt::AlignVCenter | Qt::AlignLeft, main_text);

			QRect id_rect = text_rect.adjusted(main_width + gap, 0, 0, 0);
			QColor id_color = selected ? opt.palette.highlightedText().color().lighter(115) : QColor(152, 158, 168);
			painter->setPen(id_color);
			painter->drawText(id_rect, Qt::AlignVCenter | Qt::AlignLeft, id_text);
		}

		if (is_custom && !selected) {
			QFont badge_font = opt.font;
			badge_font.setPointSize(std::max(7, badge_font.pointSize() - 1));
			painter->setFont(badge_font);
			const QString badge = "Custom";
			QFontMetrics badge_metrics(badge_font);
			const int badge_width = badge_metrics.horizontalAdvance(badge) + 12;
			QRect badge_rect(opt.rect.right() - badge_width - 8, opt.rect.top() + (opt.rect.height() - 16) / 2, badge_width, 16);
			painter->setPen(Qt::NoPen);
			painter->setBrush(QColor(214, 112, 255, 38));
			painter->drawRoundedRect(badge_rect, 8, 8);
			painter->setPen(QColor(224, 160, 255));
			painter->drawText(badge_rect, Qt::AlignCenter, badge);
		}

		painter->restore();
	}
};

std::string primary_name_field(TableModel* table) {
	if (table->slk->column_headers.contains("name")) {
		return "name";
	} else if (table->slk->column_headers.contains("name1")) {
		return "name1";
	} else if (table->slk->column_headers.contains("editorname")) {
		return "editorname";
	} else if (table->slk->column_headers.contains("bufftip")) {
		return "bufftip";
	}

	return {};
}

QString object_display_name(TableModel* table, const std::string& id) {
	const std::string field = primary_name_field(table);
	if (field.empty() || !table->slk->row_headers.contains(id)) {
		return {};
	}

	QString name = table->data(id, field, Qt::DisplayRole).toString();
	if (name.isEmpty() && field == "editorname") {
		name = table->data(id, "bufftip", Qt::DisplayRole).toString();
	}
	return name;
}

std::string base_object_id(TableModel* table, const std::string& id) {
	if (table->slk->shadow_data.contains(id) && table->slk->shadow_data.at(id).contains("oldid")) {
		return table->slk->shadow_data.at(id).at("oldid");
	}

	return {};
}

int modified_field_count(TableModel* table, const std::string& id) {
	if (!table->slk->shadow_data.contains(id)) {
		return 0;
	}

	int count = 0;
	for (const auto& [field, value] : table->slk->shadow_data.at(id)) {
		if (field != "oldid") {
			++count;
		}
	}
	return count;
}

int custom_object_count(TableModel* table) {
	int count = 0;
	for (const auto& [id, values] : table->slk->shadow_data) {
		if (values.contains("oldid")) {
			++count;
		}
	}
	return count;
}

QString object_count_label(TableModel* table) {
	return QString::number(table->rowCount()) + " total  ·  " + QString::number(custom_object_count(table)) + " custom";
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
	setStyleSheet(
		"#objectEditorSummary { border: 1px solid palette(mid); border-radius: 8px; background: rgba(255,255,255,0.03); }"
		"#objectEditorSummaryTitle { font-size: 16px; font-weight: 600; }"
		"#objectEditorSummaryMeta { color: rgb(182, 188, 198); }"
		"#objectEditorPill { border: 1px solid palette(mid); border-radius: 10px; padding: 2px 8px; background: rgba(255,255,255,0.04); }"
		"#objectEditorFieldSearch { padding: 4px 8px; }"
		"#objectEditorSectionFilter { padding: 4px 8px; min-width: 150px; }"
		"#objectEditorSectionStrip { background: rgba(255,255,255,0.02); border-radius: 8px; }"
		"QLineEdit[class='fieldSearch'] { padding: 4px 8px; }"
		"#objectEditorBrowserBar { background: rgba(255,255,255,0.03); border-bottom: 1px solid palette(mid); }"
		"#objectEditorBrowserTitle { font-size: 14px; font-weight: 600; }"
		"#objectEditorBrowserMeta { color: rgb(168, 174, 184); }"
		"#objectEditorInspectorMeta { color: rgb(168, 174, 184); }"
		"QToolBar { border: 0; spacing: 8px; padding: 0; background: rgba(255,255,255,0.02); }"
		"ads--CDockAreaTitleBar { background: rgba(255,255,255,0.02); border-bottom: 1px solid palette(mid); }"
		"ads--CDockWidgetTab { background: transparent; border: 0; border-radius: 6px; margin: 4px 2px; padding: 4px 10px; }"
		"ads--CDockWidgetTab:hover { background: rgba(255,255,255,0.05); }"
		"ads--CDockWidgetTab[activeTab=\"true\"] { background: palette(highlight); color: palette(highlighted-text); }"
		"QToolButton#objectEditorChip { border: 1px solid palette(mid); border-radius: 11px; padding: 3px 10px; background: rgba(255,255,255,0.03); }"
		"QToolButton#objectEditorChip:checked { background: palette(highlight); color: palette(highlighted-text); border-color: palette(highlight); }"
		"QTreeView { outline: 0; }"
	);

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
	current_category =
		view == unit_explorer		 ? Category::unit
		: view == item_explorer		 ? Category::item
		: view == doodad_explorer	 ? Category::doodad
		: view == destructible_explorer ? Category::destructible
		: view == ability_explorer	 ? Category::ability
		: view == upgrade_explorer	 ? Category::upgrade
									 : Category::buff;
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
	SingleModelFilter* filter_model = new SingleModelFilter(this);
	filter_model->setSourceModel(single_model);
	filter_model->set_field_search(current_field_search);
	filter_model->set_category_filter(current_field_category);
	filter_model->set_modified_only(current_modified_only);
	filter_model->set_core_only(current_core_only);

	TableDelegate* delegate = new TableDelegate(single_model);

	QFrame* summary = new QFrame;
	summary->setObjectName("objectEditorSummary");
	summary->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	QHBoxLayout* summary_layout = new QHBoxLayout(summary);
	summary_layout->setContentsMargins(12, 10, 12, 10);
	summary_layout->setSpacing(10);

	QLabel* summary_icon = new QLabel;
	summary_icon->setPixmap(icon.pixmap(32, 32));
	summary_icon->setFixedSize(36, 36);
	summary_icon->setAlignment(Qt::AlignCenter);

	QVBoxLayout* summary_text_layout = new QVBoxLayout;
	summary_text_layout->setContentsMargins(0, 0, 0, 0);
	summary_text_layout->setSpacing(3);

	QLabel* summary_title = new QLabel(name);
	summary_title->setObjectName("objectEditorSummaryTitle");
	summary_title->setWordWrap(true);

	const std::string base_id = base_object_id(table, id);
	const QString base_name = base_id.empty() ? QString() : object_display_name(table, base_id);
	const int modified_count = modified_field_count(table, id);
	QString summary_meta_text = QString::fromStdString(id);
	if (!base_id.empty()) {
		const QString base_label = base_name.isEmpty() ? QString::fromStdString(base_id) : base_name + " (" + QString::fromStdString(base_id) + ")";
		summary_meta_text += "  |  Base: " + base_label;
	}
	QLabel* summary_meta = new QLabel(summary_meta_text);
	summary_meta->setObjectName("objectEditorSummaryMeta");
	summary_meta->setWordWrap(true);

	QHBoxLayout* summary_badges = new QHBoxLayout;
	summary_badges->setContentsMargins(0, 0, 0, 0);
	summary_badges->setSpacing(6);

	QLabel* modified_badge = new QLabel(QString::number(modified_count) + " modified");
	modified_badge->setObjectName("objectEditorPill");
	summary_badges->addWidget(modified_badge);

	if (!base_id.empty()) {
		QLabel* derived_badge = new QLabel("Derived");
		derived_badge->setObjectName("objectEditorPill");
		summary_badges->addWidget(derived_badge);
	}
	summary_badges->addStretch();

	summary_text_layout->addWidget(summary_title);
	summary_text_layout->addWidget(summary_meta);
	summary_text_layout->addLayout(summary_badges);

	QVBoxLayout* summary_actions = new QVBoxLayout;
	summary_actions->setContentsMargins(0, 0, 0, 0);
	summary_actions->setSpacing(6);

	QToolButton* copy_id = new QToolButton;
	copy_id->setText("Copy ID");
	copy_id->setAutoRaise(true);
	connect(copy_id, &QToolButton::clicked, this, [id]() {
		QApplication::clipboard()->setText(QString::fromStdString(id));
	});

	QToolButton* open_parent = new QToolButton;
	open_parent->setText("Open Parent");
	open_parent->setAutoRaise(true);
	open_parent->setEnabled(!base_id.empty());
	connect(open_parent, &QToolButton::clicked, this, [this, base_id]() {
		if (!base_id.empty()) {
			select_id(current_category, base_id);
		}
	});

	summary_actions->addWidget(copy_id);
	summary_actions->addWidget(open_parent);
	summary_actions->addStretch();

	summary_layout->addWidget(summary_icon);
	summary_layout->addLayout(summary_text_layout, 1);
	summary_layout->addLayout(summary_actions);

	QWidget* inspector_bar = new QWidget;
	inspector_bar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	QHBoxLayout* inspector_bar_layout = new QHBoxLayout(inspector_bar);
	inspector_bar_layout->setContentsMargins(0, 0, 0, 0);
	inspector_bar_layout->setSpacing(8);

	QLineEdit* field_search = new QLineEdit;
	field_search->setObjectName("objectEditorFieldSearch");
	field_search->setPlaceholderText("Search fields, ids, or descriptions");
	field_search->setClearButtonEnabled(true);
	field_search->setText(current_field_search);

	QComboBox* section_filter = new QComboBox;
	section_filter->setObjectName("objectEditorSectionFilter");
	section_filter->addItem("All sections", QString());
	{
		QStringList categories;
		for (int row = 0; row < single_model->rowCount(); ++row) {
			const QString category = single_model->category_label(row);
			if (!category.isEmpty() && !categories.contains(category)) {
				categories.push_back(category);
			}
		}
		for (const auto& category : categories) {
			section_filter->addItem(category, category);
		}
	}
	{
		const int category_index = section_filter->findData(current_field_category);
		section_filter->setCurrentIndex(category_index >= 0 ? category_index : 0);
	}

	QToolButton* modified_chip = new QToolButton;
	modified_chip->setObjectName("objectEditorChip");
	modified_chip->setText("Modified");
	modified_chip->setCheckable(true);
	modified_chip->setChecked(current_modified_only);

	QToolButton* core_chip = new QToolButton;
	core_chip->setObjectName("objectEditorChip");
	core_chip->setText("Core");
	core_chip->setCheckable(true);
	core_chip->setChecked(current_core_only);

	QLabel* inspector_meta = new QLabel;
	inspector_meta->setObjectName("objectEditorInspectorMeta");

	inspector_bar_layout->addWidget(field_search, 1);
	inspector_bar_layout->addWidget(section_filter);
	inspector_bar_layout->addWidget(inspector_meta);
	inspector_bar_layout->addWidget(modified_chip);
	inspector_bar_layout->addWidget(core_chip);

	QWidget* section_strip = new QWidget;
	section_strip->setObjectName("objectEditorSectionStrip");
	section_strip->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	QHBoxLayout* section_strip_layout = new QHBoxLayout(section_strip);
	section_strip_layout->setContentsMargins(8, 6, 8, 6);
	section_strip_layout->setSpacing(6);

	QButtonGroup* section_group = new QButtonGroup(section_strip);
	section_group->setExclusive(true);
	QList<QToolButton*> section_buttons;

	const auto add_section_button = [&](const QString& label, const QString& value) {
		QToolButton* button = new QToolButton;
		button->setObjectName("objectEditorChip");
		button->setText(label);
		button->setCheckable(true);
		button->setProperty("sectionValue", value);
		section_group->addButton(button);
		section_strip_layout->addWidget(button);
		section_buttons.push_back(button);
		return button;
	};

	add_section_button("All", QString());
	for (int i = 1; i < section_filter->count(); ++i) {
		add_section_button(section_filter->itemText(i), section_filter->itemData(i).toString());
	}
	section_strip_layout->addStretch();

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
	view->verticalHeader()->setMinimumSectionSize(46);
	view->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::Stretch);
	view->setIconSize({24, 24});
	view->setWordWrap(true);
	view->setSizeAdjustPolicy(QAbstractScrollArea::SizeAdjustPolicy::AdjustToContents);
	view->setStyleSheet("QTableView::item { padding-left: 8px; padding-right: 8px; }");
	{
		QPalette palette = view->palette();
		palette.setColor(QPalette::Inactive, QPalette::Highlight, palette.color(QPalette::Midlight));
		palette.setColor(QPalette::Inactive, QPalette::HighlightedText, palette.color(QPalette::Text));
		view->setPalette(palette);
		view->verticalHeader()->setPalette(palette);
		view->verticalHeader()->viewport()->setAutoFillBackground(true);
		QPalette header_palette = view->verticalHeader()->viewport()->palette();
		header_palette.setColor(QPalette::Window, palette.color(QPalette::Base));
		view->verticalHeader()->viewport()->setPalette(header_palette);
	}
	view->setModel(filter_model);
	current_details_view = view;
	const auto update_inspector_meta = [filter_model, single_model, inspector_meta]() {
		inspector_meta->setText(QString::number(filter_model->rowCount()) + " of " + QString::number(single_model->rowCount()) + " fields");
	};
	update_inspector_meta();
	connect(field_search, &QLineEdit::textChanged, this, [this, filter_model](const QString& text) {
		current_field_search = text;
		filter_model->set_field_search(text);
	});
	connect(section_filter, &QComboBox::currentIndexChanged, this, [this, filter_model, section_filter](int) {
		current_field_category = section_filter->currentData().toString();
		filter_model->set_category_filter(current_field_category);
	});
	connect(section_filter, &QComboBox::currentIndexChanged, section_strip, [section_buttons, section_filter](int) {
		const QString value = section_filter->currentData().toString();
		for (auto* button : section_buttons) {
			button->setChecked(button->property("sectionValue").toString() == value);
		}
	});
	connect(section_group, &QButtonGroup::buttonToggled, section_strip, [section_filter](QAbstractButton* button, bool checked) {
		if (!checked) {
			return;
		}

		const QString value = button->property("sectionValue").toString();
		const int index = section_filter->findData(value);
		if (index >= 0 && section_filter->currentIndex() != index) {
			section_filter->setCurrentIndex(index);
		}
	});
	connect(modified_chip, &QToolButton::toggled, this, [this, filter_model](const bool checked) {
		current_modified_only = checked;
		filter_model->set_modified_only(checked);
	});
	connect(core_chip, &QToolButton::toggled, this, [this, filter_model](const bool checked) {
		current_core_only = checked;
		filter_model->set_core_only(checked);
	});
	connect(filter_model, &QAbstractItemModel::modelReset, inspector_meta, update_inspector_meta);
	connect(filter_model, &QAbstractItemModel::rowsInserted, inspector_meta, [update_inspector_meta](const QModelIndex&, int, int) {
		update_inspector_meta();
	});
	connect(filter_model, &QAbstractItemModel::rowsRemoved, inspector_meta, [update_inspector_meta](const QModelIndex&, int, int) {
		update_inspector_meta();
	});
	connect(filter_model, &QAbstractItemModel::layoutChanged, inspector_meta, update_inspector_meta);
	connect(field_search, &QLineEdit::textChanged, inspector_meta, [update_inspector_meta](const QString&) {
		update_inspector_meta();
	});
	connect(section_filter, &QComboBox::currentIndexChanged, inspector_meta, [update_inspector_meta](int) {
		update_inspector_meta();
	});
	connect(modified_chip, &QToolButton::toggled, inspector_meta, [update_inspector_meta](bool) {
		update_inspector_meta();
	});
	connect(core_chip, &QToolButton::toggled, inspector_meta, [update_inspector_meta](bool) {
		update_inspector_meta();
	});
	{
		const int category_index = section_filter->currentIndex();
		const QString value = section_filter->itemData(category_index).toString();
		for (auto* button : section_buttons) {
			button->setChecked(button->property("sectionValue").toString() == value);
		}
	}

	connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this, [this, filter_model, single_model](const QModelIndex& current, const QModelIndex&) {
		if (!current.isValid()) {
			current_detail_key.clear();
			current_detail_level = -1;
			return;
		}

		const QModelIndex source_index = filter_model->mapToSource(current);
		if (!source_index.isValid()) {
			return;
		}

		const auto& mapping = single_model->getMapping();
		current_detail_key = mapping[source_index.row()].key;
		current_detail_level = mapping[source_index.row()].level;
	});
	connect(view, &QTableView::pressed, this, [this](const QModelIndex&) {
		refresh_details_focus_visuals();
	});

	if (!current_detail_key.empty()) {
		const int source_row = single_model->find_mapping_row(current_detail_key, current_detail_level);
		if (source_row >= 0) {
			const QModelIndex filtered_index = filter_model->mapFromSource(single_model->index(source_row, 0));
			if (filtered_index.isValid()) {
				view->setCurrentIndex(filtered_index);
				view->selectionModel()->select(filtered_index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
			}
		}
	} else if (filter_model->rowCount() > 0) {
		const QModelIndex first_index = filter_model->index(0, 0);
		view->setCurrentIndex(first_index);
		view->selectionModel()->select(first_index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
	}

	connect(view->verticalHeader(), &QHeaderView::sectionPressed, view, [view, filter_model](int section) {
		const QModelIndex index = filter_model->index(section, 0);
		view->setCurrentIndex(index);
		view->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		view->setFocus(Qt::FocusReason::MouseFocusReason);
	});

	QWidget* column_header = new QWidget;
	column_header->setObjectName("objectEditorColumnHeader");
	column_header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	column_header->setStyleSheet(
		"#objectEditorColumnHeader {"
		"border-bottom: 1px solid palette(mid);"
		"}"
	);

	QHBoxLayout* column_header_layout = new QHBoxLayout(column_header);
	column_header_layout->setContentsMargins(0, 0, 0, 0);
	column_header_layout->setSpacing(0);

	QLabel* name_header = new QLabel("Field");
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

	layout->addWidget(summary);
	layout->addWidget(inspector_bar);
	layout->addWidget(section_strip);
	layout->addWidget(column_header);
	layout->addWidget(view);
	layout->setStretch(0, 0);
	layout->setStretch(1, 0);
	layout->setStretch(2, 0);
	layout->setStretch(3, 0);
	layout->setStretch(4, 1);

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
	view->setItemDelegate(new ObjectTreeDelegate(view));
	view->setSelectionBehavior(QAbstractItemView::SelectRows);
	view->setSelectionMode(QAbstractItemView::ExtendedSelection);
	view->setUniformRowHeights(true);
	view->setIndentation(18);
	view->setIconSize({18, 18});
	view->setAnimated(true);
	view->setAllColumnsShowFocus(true);
	view->setStyleSheet(
		"QTreeView { border: 0; padding: 4px 0; }"
		"QTreeView::item { padding: 2px 4px; }"
	);
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
	search->setProperty("class", "fieldSearch");
	search->setClearButtonEnabled(true);
	search->setPlaceholderText("Search " + name);
	connect(search, &QLineEdit::textChanged, [=](const QString& string) {
		filter->setFilterFixedString(string);
		view->expandAll();
	});

	QToolButton* all_objects = new QToolButton;
	all_objects->setObjectName("objectEditorChip");
	all_objects->setText("All");
	all_objects->setCheckable(true);
	all_objects->setChecked(true);

	QToolButton* custom_objects = new QToolButton;
	custom_objects->setObjectName("objectEditorChip");
	custom_objects->setText("Custom");
	custom_objects->setToolTip("Show only custom " + name.toLower());
	custom_objects->setCheckable(true);

	QButtonGroup* mode_group = new QButtonGroup(view);
	mode_group->setExclusive(true);
	mode_group->addButton(all_objects);
	mode_group->addButton(custom_objects);
	connect(custom_objects, &QToolButton::toggled, [=](bool checked) {
		filter->setFilterCustom(checked);
		if (!checked) {
			view->expandAll();
		}
	});

	QLabel* browser_meta = new QLabel(object_count_label(table));
	browser_meta->setObjectName("objectEditorBrowserMeta");
	const auto update_browser_meta = [table, browser_meta]() {
		browser_meta->setText(object_count_label(table));
	};
	connect(table, &QAbstractItemModel::modelReset, browser_meta, update_browser_meta);
	connect(table, &QAbstractItemModel::rowsInserted, browser_meta, [update_browser_meta](const QModelIndex&, int, int) {
		update_browser_meta();
	});
	connect(table, &QAbstractItemModel::rowsRemoved, browser_meta, [update_browser_meta](const QModelIndex&, int, int) {
		update_browser_meta();
	});

	QWidget* browser_bar = new QWidget;
	browser_bar->setObjectName("objectEditorBrowserBar");
	QVBoxLayout* browser_layout = new QVBoxLayout(browser_bar);
	browser_layout->setContentsMargins(10, 10, 10, 8);
	browser_layout->setSpacing(8);

	QHBoxLayout* browser_title_row = new QHBoxLayout;
	browser_title_row->setContentsMargins(0, 0, 0, 0);
	browser_title_row->setSpacing(8);

	QLabel* browser_icon = new QLabel;
	browser_icon->setPixmap(icon.pixmap(18, 18));
	browser_icon->setFixedSize(20, 20);
	browser_icon->setAlignment(Qt::AlignCenter);

	QLabel* browser_title = new QLabel(name);
	browser_title->setObjectName("objectEditorBrowserTitle");

	browser_title_row->addWidget(browser_icon);
	browser_title_row->addWidget(browser_title);
	browser_title_row->addStretch();
	browser_title_row->addWidget(browser_meta);

	QHBoxLayout* browser_controls_row = new QHBoxLayout;
	browser_controls_row->setContentsMargins(0, 0, 0, 0);
	browser_controls_row->setSpacing(8);
	browser_controls_row->addWidget(search, 1);
	browser_controls_row->addWidget(all_objects);
	browser_controls_row->addWidget(custom_objects);

	browser_layout->addLayout(browser_title_row);
	browser_layout->addLayout(browser_controls_row);

	QToolBar* bar = new QToolBar;
	bar->setMovable(false);
	bar->addWidget(browser_bar);

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
	const_cast<ObjectEditor*>(this)->current_category = category;
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

