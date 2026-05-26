#include "region_palette.h"

#include <QColorDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QSignalBlocker>

import std;
import Camera;
import MapGlobal;

RegionPalette::RegionPalette(QWidget* parent) : Palette(parent) {
	monitor_activation();

	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(6);

	region_list->setSelectionMode(QAbstractItemView::SingleSelection);

	auto* button_row = new QHBoxLayout;
	button_row->setContentsMargins(0, 0, 0, 0);
	button_row->addWidget(new_region);
	button_row->addWidget(delete_region);

	auto* form = new QFormLayout;
	form->setContentsMargins(0, 0, 0, 0);
	form->addRow("Name", name);
	form->addRow("Weather ID", weather_id);
	form->addRow("Ambient ID", ambient_id);
	form->addRow("Color", color);

	layout->addWidget(region_list, 1);
	layout->addLayout(button_row);
	layout->addLayout(form);

	change_mode_parent = new QShortcut(Qt::Key_Space, window(), nullptr, nullptr, Qt::ShortcutContext::WindowShortcut);

	selection_mode->setText("Selection\nMode");
	selection_mode->setIcon(QIcon("data/icons/ribbon/select.png"));
	selection_mode->setCheckable(true);
	selection_mode->setChecked(true);

	auto* selection_section = new QRibbonSection;
	selection_section->setText("Selection");
	selection_section->addWidget(selection_mode);

	auto* region_section = new QRibbonSection;
	region_section->setText("Regions");

	auto* new_region_button = new QRibbonButton;
	new_region_button->setText("New\nRegion");
	new_region_button->setIcon(QIcon("data/icons/ribbon/new.ico"));
	region_section->addWidget(new_region_button);

	auto* delete_region_button = new QRibbonButton;
	delete_region_button->setText("Delete\nRegion");
	delete_region_button->setIcon(QIcon("data/icons/ribbon/exit.ico"));
	region_section->addWidget(delete_region_button);

	ribbon_tab->addSection(selection_section);
	ribbon_tab->addSection(region_section);

	connect(selection_mode, &QRibbonButton::toggled, [&]() { brush.switch_mode(); });
	connect(change_mode_parent, &QShortcut::activated, selection_mode, &QToolButton::click);

	connect(new_region_button, &QRibbonButton::clicked, &brush, &RegionBrush::create_region_at_cursor);
	connect(delete_region_button, &QRibbonButton::clicked, &brush, &RegionBrush::delete_selection);
	connect(new_region, &QPushButton::clicked, &brush, &RegionBrush::create_region_at_cursor);
	connect(delete_region, &QPushButton::clicked, &brush, &RegionBrush::delete_selection);

	connect(region_list, &QListWidget::currentRowChanged, &brush, &RegionBrush::select_region);
	connect(region_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
		if (!item) {
			return;
		}

		const int row = region_list->row(item);
		if (row < 0 || row >= static_cast<int>(map->regions.regions.size())) {
			return;
		}

		if (brush.selected_index() != row) {
			brush.select_region(row);
		}

		const Region& region = map->regions.regions[row];
		camera.position.x = (region.left + region.right) * 0.5f;
		camera.position.y = (region.bottom + region.top) * 0.5f;
	});
	connect(&brush, &Brush::selection_changed, this, [this]() {
		refresh_region_list();
		sync_region_fields();
	});

	connect(name, &QLineEdit::editingFinished, this, [this]() { brush.set_selected_name(name->text()); });
	connect(weather_id, &QLineEdit::editingFinished, this, [this]() { brush.set_selected_weather_id(weather_id->text()); });
	connect(ambient_id, &QLineEdit::editingFinished, this, [this]() { brush.set_selected_ambient_id(ambient_id->text()); });
	connect(color, &QPushButton::clicked, this, [this]() {
		const auto* selected = brush.selected_region();
		const QColor current = selected ? QColor(static_cast<int>(selected->color.r), static_cast<int>(selected->color.g), static_cast<int>(selected->color.b))
										: QColor(255, 120, 210);
		const QColor chosen = QColorDialog::getColor(current, this);
		if (!chosen.isValid()) {
			return;
		}
		brush.set_selected_color(chosen);
		update_color_button(chosen);
	});

	name->setPlaceholderText("Region name");
	weather_id->setPlaceholderText("FourCC or blank");
	ambient_id->setPlaceholderText("Ambient sound or blank");

	refresh_region_list();
	sync_region_fields();
}

RegionPalette::~RegionPalette() {
	if (map->brush == &brush) {
		map->brush = nullptr;
	}
	delete change_mode_parent;
	delete ribbon_tab;
}

void RegionPalette::activate_palette() {
	change_mode_parent->setEnabled(true);
	map->brush = &brush;
	const QSignalBlocker blocker(selection_mode);
	selection_mode->setChecked(brush.get_mode() == Brush::Mode::selection);
	emit ribbon_tab_requested(ribbon_tab, "Region Palette");
}

void RegionPalette::deactivate(QRibbonTab* tab) {
	if (tab != ribbon_tab) {
		change_mode_parent->setEnabled(false);
	}
}

void RegionPalette::refresh_region_list() {
	const QSignalBlocker blocker(region_list);
	const int selected = brush.selected_index();

	if (region_list->count() != static_cast<int>(map->regions.regions.size())) {
		region_list->clear();
		for (const auto& region : map->regions.regions) {
			region_list->addItem(QString::fromStdString(region.name));
		}
	} else {
		for (int i = 0; i < region_list->count(); ++i) {
			const QString label = QString::fromStdString(map->regions.regions[i].name);
			if (region_list->item(i)->text() != label) {
				region_list->item(i)->setText(label);
			}
		}
	}

	if (selected >= 0 && selected < region_list->count()) {
		region_list->setCurrentRow(selected);
	} else {
		region_list->setCurrentRow(-1);
	}
}

void RegionPalette::sync_region_fields() {
	const auto* selected = brush.selected_region();
	const QSignalBlocker name_blocker(name);
	const QSignalBlocker weather_blocker(weather_id);
	const QSignalBlocker ambient_blocker(ambient_id);

	const bool has_selection = selected != nullptr;
	name->setEnabled(has_selection);
	weather_id->setEnabled(has_selection);
	ambient_id->setEnabled(has_selection);
	color->setEnabled(has_selection);
	delete_region->setEnabled(has_selection);

	if (!selected) {
		name->clear();
		weather_id->clear();
		ambient_id->clear();
		update_color_button(QColor(80, 80, 80));
		return;
	}

	name->setText(QString::fromStdString(selected->name));
	weather_id->setText(QString::fromStdString(selected->weather_id));
	ambient_id->setText(QString::fromStdString(selected->ambient_id));
	update_color_button(QColor(static_cast<int>(selected->color.r), static_cast<int>(selected->color.g), static_cast<int>(selected->color.b)));
}

void RegionPalette::update_color_button(const QColor& new_color) {
	const int brightness = static_cast<int>(new_color.red() * 0.299 + new_color.green() * 0.587 + new_color.blue() * 0.114);
	const QColor text_color = brightness > 160 ? QColor(Qt::black) : QColor(Qt::white);
	color->setStyleSheet(
		QString("QPushButton { background-color: %1; color: %2; }").arg(new_color.name(), text_color.name())
	);
}
