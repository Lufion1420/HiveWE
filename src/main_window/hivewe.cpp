#include "HiveWE.h"
#include "ui_HiveWE.h"
#include "shortcut_config_dialog.h"
#define __STORMLIB_NO_STATIC_LINK__
#include "StormLib.h"

import std;
import Hierarchy;
import MPQ;
import Camera;
import Globals;
import Map;
import <soil2/SOIL2.h>;
import MapGlobal;
import WorldUndoManager;
import SkinnedMeshGlobals;
import ResourceManager;
import WindowHandler;
import "pathing_palette.h";
import "object_editor/object_editor.h";
import "model_editor/model_editor.h";
import "tile_setter.h";
import "map_info_editor.h";
import "terrain_palette.h";
import "settings_editor.h";
import "tile_pather.h";
import "palette.h";
import "terrain_palette.h";
import "doodad_palette.h";
import "unit_palette.h";
import "region_palette.h";
import "object_editor/icon_view.h";
import "trigger_editor.h";
#include "QMessageBox"
#include "QProcess"
#include "QKeySequence"
#include "QString"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QStyle>
#include <QFileInfo>
#include <QStringList>
import "menus/gameplay_constants_editor.h";
import "asset_manager/asset_manager.h";
import "tooltip_editor/tooltip_editor.h";

namespace fs = std::filesystem;

HiveWE::HiveWE(QWidget* parent)
	: QMainWindow(parent), ui(std::make_unique<Ui::HiveWEClass>()) {
	setAutoFillBackground(true);

	// Buggy as of Qt 6.9.1. Likely requires 6.9.2 or later
	// setWindowFlag(Qt::ExpandedClientAreaHint, true);
	// setWindowFlag(Qt::NoTitleBarBackgroundHint, true);
	// setAttribute(Qt::WA_LayoutOnEntireRect, true);
	ui->setupUi(this);
	context = ui->widget;
	setup_palette_sidebar();

	transient_notice = new QLabel(this);
	transient_notice->hide();
	transient_notice->setAttribute(Qt::WA_TransparentForMouseEvents);
	transient_notice->setAlignment(Qt::AlignCenter);
	transient_notice->setStyleSheet(
		"QLabel {"
		"background-color: rgba(20, 24, 30, 228);"
		"color: rgb(244, 247, 250);"
		"border: 1px solid rgba(255, 255, 255, 52);"
		"border-radius: 10px;"
		"padding: 18px 32px;"
		"font-size: 24px;"
		"font-weight: 700;"
		"}"
	);
	transient_notice_timer.setSingleShot(true);
	connect(&transient_notice_timer, &QTimer::timeout, transient_notice, &QLabel::hide);

	connect(ui->ribbon->undo, &QPushButton::clicked, [&]() {
		// ToDo: temporary, undoing should still allow a selection to persist
		if (map->brush) {
			map->brush->clear_selection();
		}

		auto context = WorldEditContext {
			.terrain = map->terrain,
			.units = map->units,
			.doodads = map->doodads,
			.regions = map->regions,
			.brush = map->brush,
			.pathing_map = map->pathing_map,
		};

		map->world_undo.undo(context);
	});
	connect(ui->ribbon->redo, &QPushButton::clicked, [&]() {
		// ToDo: temporary, undoing should still allow a selection to persist
		if (map->brush) {
			map->brush->clear_selection();
		}

		auto context = WorldEditContext {
			.terrain = map->terrain,
			.units = map->units,
			.doodads = map->doodads,
			.regions = map->regions,
			.brush = map->brush,
			.pathing_map = map->pathing_map,
		};

		map->world_undo.redo(context);
	});

	connect(new QShortcut(Qt::CTRL | Qt::Key_Z, this), &QShortcut::activated, ui->ribbon->undo, &QPushButton::click);
	connect(new QShortcut(Qt::CTRL | Qt::Key_Y, this), &QShortcut::activated, ui->ribbon->redo, &QPushButton::click);

	connect(ui->ribbon->units_visible, &QPushButton::toggled, [](bool checked) { map->render_units = checked; });
	connect(ui->ribbon->doodads_visible, &QPushButton::toggled, [](bool checked) { map->render_doodads = checked; });
	connect(ui->ribbon->pathing_visible, &QPushButton::toggled, [](bool checked) { map->render_pathing = checked; });
	connect(ui->ribbon->brush_visible, &QPushButton::toggled, [](bool checked) { map->render_brush = checked; });
	connect(ui->ribbon->lighting_visible, &QPushButton::toggled, [](bool checked) { map->render_lighting = checked; });
	connect(ui->ribbon->water_visible, &QPushButton::toggled, [](bool checked) { map->render_water = checked; });
	connect(ui->ribbon->click_helpers_visible, &QPushButton::toggled, [](bool checked) { map->render_click_helpers = checked; });
	connect(ui->ribbon->wireframe_visible, &QPushButton::toggled, [](bool checked) { map->render_wireframe = checked; });
	connect(ui->ribbon->debug_visible, &QPushButton::toggled, [](bool checked) { map->render_debug = checked; });
	connect(ui->ribbon->minimap_visible, &QPushButton::toggled, [&](bool checked) { (checked) ? minimap->show() : minimap->hide(); });

	connect(new QShortcut(Qt::CTRL | Qt::Key_U, this), &QShortcut::activated, ui->ribbon->units_visible, &QPushButton::click);
	connect(new QShortcut(Qt::CTRL | Qt::Key_D, this), &QShortcut::activated, ui->ribbon->doodads_visible, &QPushButton::click);
	connect(new QShortcut(Qt::CTRL | Qt::Key_P, this), &QShortcut::activated, ui->ribbon->pathing_visible, &QPushButton::click);
	connect(new QShortcut(Qt::CTRL | Qt::Key_L, this), &QShortcut::activated, ui->ribbon->lighting_visible, &QPushButton::click);
	connect(new QShortcut(Qt::CTRL | Qt::Key_W, this), &QShortcut::activated, ui->ribbon->water_visible, &QPushButton::click);
	connect(new QShortcut(Qt::CTRL | Qt::Key_I, this), &QShortcut::activated, ui->ribbon->click_helpers_visible, &QPushButton::click);
	connect(new QShortcut(Qt::CTRL | Qt::Key_T, this), &QShortcut::activated, ui->ribbon->wireframe_visible, &QPushButton::click);
	connect(new QShortcut(Qt::Key_F3, this), &QShortcut::activated, ui->ribbon->debug_visible, &QPushButton::click);

	// Reload theme
	connect(new QShortcut(Qt::Key_F5, this), &QShortcut::activated, [&]() {
		QSettings settings;
		QFile file("data/themes/" + settings.value("theme").toString() + ".qss");
		const auto success = file.open(QFile::ReadOnly);
		if (!success) {
			std::println("Failed to open theme file");
			return;
		}
		QString StyleSheet = QLatin1String(file.readAll());

		qApp->setStyleSheet(StyleSheet);
	});

	connect(ui->ribbon->reset_camera, &QPushButton::clicked, [&]() { camera.reset(); });
	connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C), this), &QShortcut::activated, ui->ribbon->reset_camera, &QPushButton::click);

	connect(ui->ribbon->import_heightmap, &QPushButton::clicked, this, &HiveWE::import_heightmap);

	connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_O), this, nullptr, nullptr, Qt::ApplicationShortcut), &QShortcut::activated, ui->ribbon->open_map_folder, &QPushButton::click);
	// connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_I), this, nullptr, nullptr, Qt::ApplicationShortcut), &QShortcut::activated, ui->ribbon->open_map_mpq, &QPushButton::click);
	connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_S), this, nullptr, nullptr, Qt::ApplicationShortcut), &QShortcut::activated, ui->ribbon->save_map, &QPushButton::click);
	connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), this, nullptr, nullptr, Qt::ApplicationShortcut), &QShortcut::activated, ui->ribbon->save_map_as, &QPushButton::click);

	// connect(ui.ribbon->new_map, &QAction::triggered, this, &HiveWE::load);
	connect(ui->ribbon->open_map_folder, &QPushButton::clicked, this, &HiveWE::load_folder);
	connect(ui->ribbon->open_map_mpq, &QPushButton::clicked, this, &HiveWE::load_mpq);
	for (int i = 0; i < static_cast<int>(ui->ribbon->recent_maps.size()); ++i) {
		connect(ui->ribbon->recent_maps[i], &QPushButton::clicked, this, [this, i]() { open_recent_map(i); });
	}
	connect(ui->ribbon->save_map, &QPushButton::clicked, this, &HiveWE::save);
	connect(ui->ribbon->save_map_as, &QPushButton::clicked, this, &HiveWE::save_as);
	connect(ui->ribbon->export_mpq, &QPushButton::clicked, this, &HiveWE::export_mpq);
	connect(ui->ribbon->test_map, &QPushButton::clicked, this, &HiveWE::play_test);
	connect(ui->ribbon->settings, &QPushButton::clicked, [&]() { new SettingsEditor(this); });
	connect(ui->ribbon->config, &QPushButton::clicked, [&]() { new ShortcutConfigDialog(this); });
	connect(ui->ribbon->switch_warcraft, &QPushButton::clicked, this, &HiveWE::switch_warcraft);
	connect(ui->ribbon->exit, &QPushButton::clicked, [&]() { QApplication::exit(); });

	connect(ui->ribbon->change_tileset, &QRibbonButton::clicked, [this]() { new TileSetter(this); });
	connect(ui->ribbon->change_tile_pathing, &QRibbonButton::clicked, [this]() { new TilePather(this); });

	connect(ui->ribbon->map_description, &QRibbonButton::clicked, [&]() { (new MapInfoEditor(this))->ui.tabs->setCurrentIndex(0); });
	connect(ui->ribbon->map_loading_screen, &QRibbonButton::clicked, [&]() { (new MapInfoEditor(this))->ui.tabs->setCurrentIndex(1); });
	connect(ui->ribbon->map_options, &QRibbonButton::clicked, [&]() { (new MapInfoEditor(this))->ui.tabs->setCurrentIndex(2); });
	// connect(ui, &QAction::triggered, [&]() { (new MapInfoEditor(this))->ui.tabs->setCurrentIndex(3); });

	ui->ribbon->terrain_palette->setCheckable(true);
	ui->ribbon->doodad_palette->setCheckable(true);
	ui->ribbon->unit_palette->setCheckable(true);
	ui->ribbon->region_palette->setCheckable(true);
	ui->ribbon->pathing_palette->setCheckable(true);
	connect(ui->ribbon->pathing_palette, &QRibbonButton::clicked, this, &HiveWE::toggle_pathing_sidebar);
	connect(ui->ribbon->region_palette, &QRibbonButton::clicked, this, &HiveWE::toggle_region_palette);

	connect(new QShortcut(QKeySequence(Qt::Key_T), this, nullptr, nullptr, Qt::WindowShortcut), &QShortcut::activated, this, &HiveWE::toggle_terrain_sidebar);
	connect(ui->ribbon->terrain_palette, &QRibbonButton::clicked, this, &HiveWE::toggle_terrain_sidebar);

	connect(new QShortcut(QKeySequence(Qt::Key_D), this, nullptr, nullptr, Qt::WindowShortcut), &QShortcut::activated, this, &HiveWE::toggle_doodad_palette);
	connect(ui->ribbon->doodad_palette, &QRibbonButton::clicked, this, &HiveWE::toggle_doodad_palette);

	connect(new QShortcut(QKeySequence(Qt::Key_U), this, nullptr, nullptr, Qt::WindowShortcut), &QShortcut::activated, this, &HiveWE::toggle_unit_palette);
	connect(ui->ribbon->unit_palette, &QRibbonButton::clicked, this, &HiveWE::toggle_unit_palette);

	connect(new QShortcut(QKeySequence(Qt::Key_P), this, nullptr, nullptr, Qt::WindowShortcut), &QShortcut::activated, this, &HiveWE::toggle_pathing_sidebar);
	connect(new QShortcut(QKeySequence(Qt::Key_R), this, nullptr, nullptr, Qt::WindowShortcut), &QShortcut::activated, this, &HiveWE::toggle_region_palette);

	connect(ui->ribbon->trigger_editor, &QRibbonButton::clicked, [this]() {
		bool created = false;
		const auto editor = window_handler.create_or_raise<TriggerEditor>(nullptr, created);
		connect(this, &HiveWE::saving_initiated, editor, &TriggerEditor::save_changes, Qt::UniqueConnection);
	});

	connect(ui->ribbon->object_editor, &QRibbonButton::clicked, [this]() {
		bool created = false;
		window_handler.create_or_raise<ObjectEditor>(nullptr, created);
	});
	connect(new QShortcut(QKeySequence(Qt::Key_O), this, nullptr, nullptr, Qt::ApplicationShortcut), &QShortcut::activated, [this]() {
		if (const auto open_editor = window_handler.get_open<ObjectEditor>(); open_editor.has_value() && *open_editor) {
			(*open_editor)->close();
			return;
		}

		bool created = false;
		window_handler.create_or_raise<ObjectEditor>(nullptr, created);
	});

	connect(ui->ribbon->model_editor, &QRibbonButton::clicked, [this]() {
		bool created = false;
		window_handler.create_or_raise<ModelEditor>(nullptr, created);
	});

	connect(ui->ribbon->gameplay_constants, &QRibbonButton::clicked, [this]() {
		bool created = false;
		window_handler.create_or_raise<GameplayConstantsEditor>(nullptr, created);
	});

	connect(ui->ribbon->asset_manager, &QRibbonButton::clicked, [this]() {
		bool created = false;
		window_handler.create_or_raise<AssetManager>(nullptr, created);
	});

	connect(ui->ribbon->tooltip_editor, &QRibbonButton::clicked, [this]() {
		bool created = false;
		window_handler.create_or_raise<TooltipEditor>(nullptr, created);
	});

	restore_window_state();
	update_recent_maps_menu();

	minimap->setParent(ui->widget);
	minimap->move(10, 10);
	minimap->show();

	connect(minimap, &Minimap::clicked, [](QPointF location) { camera.position = { location.x() * map->terrain.width, (1.0 - location.y()) * map->terrain.height, camera.position.z }; });
	ui->widget->makeCurrent();

	map = new Map();
	connect(&map->terrain, &Terrain::minimap_changed, minimap, &Minimap::set_minimap);
	connect(&map->terrain, &Terrain::tileset_changed, [&]() {
		if (terrain_palette) {
			terrain_palette->refresh();
		}
	});

	map->render_manager.resize_framebuffers(ui->widget->width(), ui->widget->height());
}

HiveWE::~HiveWE() = default;

void HiveWE::setup_palette_sidebar() {
	auto* main_layout = qobject_cast<QVBoxLayout*>(centralWidget()->layout());
	main_layout->removeWidget(ui->widget);

	auto* content_layout = new QHBoxLayout;
	content_layout->setContentsMargins(0, 0, 0, 0);
	content_layout->setSpacing(0);
	content_layout->addWidget(ui->widget, 1);
	main_layout->addLayout(content_layout);

	sidebar_root = new QWidget(centralWidget());
	sidebar_root->setObjectName("paletteSidebar");
	sidebar_root->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
	sidebar_root->setMinimumWidth(360);
	sidebar_root->setMaximumWidth(440);
	sidebar_root->setStyleSheet(
		"#paletteSidebar { background: rgba(20, 23, 27, 236); border-left: 1px solid rgba(255, 255, 255, 18); }"
		"QWidget#paletteSidebarHeader { background: transparent; }"
		"QWidget#paletteSidebarModeBar { background: transparent; }"
		"QToolButton[sidebarMode=\"true\"] {"
		"background: rgba(37, 41, 47, 224);"
		"border: 1px solid rgba(255, 255, 255, 16);"
		"border-radius: 11px;"
		"color: rgb(192, 199, 208);"
		"font-size: 12px;"
		"font-weight: 600;"
		"padding: 7px 10px;"
		"}"
		"QToolButton[sidebarMode=\"true\"]:checked {"
		"background: rgba(70, 132, 214, 210);"
		"border: 1px solid rgba(95, 164, 255, 240);"
		"color: white;"
		"}"
		"QLabel[sidebarTitle=\"true\"] { font-size: 24px; font-weight: 700; color: rgb(241, 244, 247); }"
		"QLabel[sidebarDescription=\"true\"] { font-size: 13px; color: rgb(178, 187, 198); }"
		"QFrame[sidebarBody=\"true\"] { background: rgba(33, 37, 43, 230); border: 1px solid rgba(255, 255, 255, 16); border-radius: 16px; }"
		"QFrame[sidebarPaletteHost=\"true\"] { background: transparent; border: 0; }"
		"QLabel[sidebarHint=\"true\"] { font-size: 11px; color: rgb(132, 141, 153); padding-top: 2px; }"
		"#paletteSidebar QLineEdit, #paletteSidebar QComboBox, #paletteSidebar QSpinBox {"
		"background: rgba(24, 27, 31, 235);"
		"border: 1px solid rgba(255, 255, 255, 18);"
		"border-radius: 10px;"
		"padding: 6px 10px;"
		"color: rgb(232, 236, 241);"
		"selection-background-color: rgba(95, 164, 255, 210);"
		"}"
		"#paletteSidebar QLineEdit:focus, #paletteSidebar QComboBox:focus, #paletteSidebar QSpinBox:focus {"
		"border: 1px solid rgba(95, 164, 255, 235);"
		"}"
		"#paletteSidebar QListView, #paletteSidebar QListWidget {"
		"background: rgba(24, 27, 31, 210);"
		"border: 1px solid rgba(255, 255, 255, 14);"
		"border-radius: 12px;"
		"padding: 4px;"
		"color: rgb(232, 236, 241);"
		"outline: 0;"
		"}"
		"#paletteSidebar QListView::item, #paletteSidebar QListWidget::item {"
		"padding: 6px 8px;"
		"border-radius: 8px;"
		"}"
		"#paletteSidebar QListView::item:selected, #paletteSidebar QListWidget::item:selected {"
		"background: rgba(95, 164, 255, 210);"
		"color: white;"
		"}"
		"#paletteSidebar QPushButton, #paletteSidebar QToolButton, #paletteSidebar QRadioButton, #paletteSidebar QCheckBox {"
		"color: rgb(229, 233, 239);"
		"}"
		"#paletteSidebar QSlider::groove:horizontal {"
		"height: 6px;"
		"background: rgba(255, 255, 255, 20);"
		"border-radius: 999px;"
		"}"
		"#paletteSidebar QSlider::handle:horizontal {"
		"width: 16px;"
		"margin: -5px 0;"
		"background: rgb(95, 164, 255);"
		"border-radius: 8px;"
		"}"
		"#paletteSidebar QGroupBox {"
		"border: 0;"
		"margin-top: 6px;"
		"font-weight: 700;"
		"color: rgb(214, 221, 229);"
		"}"
		"#paletteSidebar QLabel { color: rgb(214, 221, 229); }"
	);

	auto* sidebar_layout = new QVBoxLayout(sidebar_root);
	sidebar_layout->setContentsMargins(12, 12, 12, 12);
	sidebar_layout->setSpacing(12);

	sidebar_mode_bar = new QWidget(sidebar_root);
	sidebar_mode_bar->setObjectName("paletteSidebarModeBar");
	auto* mode_layout = new QHBoxLayout(sidebar_mode_bar);
	mode_layout->setContentsMargins(0, 0, 0, 0);
	mode_layout->setSpacing(8);

	auto create_mode_button = [this, mode_layout](const QString& text, const QIcon& icon, const auto slot) {
		auto* button = new QToolButton(sidebar_mode_bar);
		button->setProperty("sidebarMode", true);
		button->setCheckable(true);
		button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		button->setIcon(icon);
		button->setText(text);
		button->setAutoRaise(false);
		mode_layout->addWidget(button);
		connect(button, &QToolButton::clicked, this, [this, slot]() { (this->*slot)(); });
		return button;
	};

	terrain_mode_button = create_mode_button("Terrain", QIcon("data/icons/ribbon/heightmap.png"), &HiveWE::toggle_terrain_sidebar);
	doodad_mode_button = create_mode_button("Doodads", QIcon("data/icons/ribbon/doodads.png"), &HiveWE::toggle_doodad_palette);
	unit_mode_button = create_mode_button("Units", QIcon("data/icons/ribbon/units.png"), &HiveWE::toggle_unit_palette);
	pathing_mode_button = create_mode_button("Pathing", QIcon("data/icons/ribbon/pathing.png"), &HiveWE::toggle_pathing_sidebar);
	region_mode_button = create_mode_button("Regions", QIcon("data/icons/ribbon/select.png"), &HiveWE::toggle_region_palette);

	sidebar_header = new QWidget(sidebar_root);
	sidebar_header->setObjectName("paletteSidebarHeader");
	auto* header_layout = new QVBoxLayout(sidebar_header);
	header_layout->setContentsMargins(0, 0, 0, 0);
	header_layout->setSpacing(6);

	sidebar_title = new QLabel(sidebar_header);
	sidebar_title->setProperty("sidebarTitle", true);
	sidebar_title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

	sidebar_description = new QLabel(sidebar_header);
	sidebar_description->setProperty("sidebarDescription", true);
	sidebar_description->setWordWrap(true);
	sidebar_description->setAlignment(Qt::AlignLeft | Qt::AlignTop);

	header_layout->addWidget(sidebar_title);
	header_layout->addWidget(sidebar_description);

	auto* sidebar_body = new QFrame(sidebar_root);
	sidebar_body->setProperty("sidebarBody", true);
	auto* body_layout = new QVBoxLayout(sidebar_body);
	body_layout->setContentsMargins(14, 14, 14, 14);
	body_layout->setSpacing(0);

	sidebar_stack = new QStackedWidget(sidebar_body);
	sidebar_stack->setContentsMargins(0, 0, 0, 0);
	body_layout->addWidget(sidebar_stack);

	auto create_host = [this](const char* object_name) {
		auto* host = new QFrame(sidebar_stack);
		host->setObjectName(object_name);
		host->setProperty("sidebarPaletteHost", true);
		host->setFrameShape(QFrame::NoFrame);
		host->setContentsMargins(0, 0, 0, 0);
		auto* layout = new QVBoxLayout(host);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);
		sidebar_stack->addWidget(host);
		return host;
	};

	terrain_host = create_host("terrainSidebarHost");
	doodad_host = create_host("doodadSidebarHost");
	unit_host = create_host("unitSidebarHost");
	pathing_host = create_host("pathingSidebarHost");
	region_host = create_host("regionSidebarHost");

	sidebar_hint = new QLabel(sidebar_root);
	sidebar_hint->setProperty("sidebarHint", true);
	sidebar_hint->setWordWrap(true);
	sidebar_hint->setAlignment(Qt::AlignLeft | Qt::AlignTop);

	sidebar_layout->addWidget(sidebar_mode_bar);
	sidebar_layout->addWidget(sidebar_header);
	sidebar_layout->addWidget(sidebar_body, 1);
	sidebar_layout->addWidget(sidebar_hint);
	content_layout->addWidget(sidebar_root);

	sidebar_root->hide();
	update_active_palette_visuals();
}

void HiveWE::load_map(const fs::path& directory) {
	window_handler.close_all();
	ui->widget->makeCurrent();

	delete terrain_palette;
	terrain_palette = nullptr;
	delete pathing_palette;
	pathing_palette = nullptr;
	delete doodad_palette;
	doodad_palette = nullptr;
	delete unit_palette;
	unit_palette = nullptr;
	delete region_palette;
	region_palette = nullptr;
	remove_custom_tab();
	current_sidebar_view = SidebarPaletteView::none;
	sidebar_root->hide();
	ui->ribbon->terrain_palette->setChecked(false);
	ui->ribbon->doodad_palette->setChecked(false);
	ui->ribbon->unit_palette->setChecked(false);
	ui->ribbon->pathing_palette->setChecked(false);
	ui->ribbon->region_palette->setChecked(false);
	update_active_palette_visuals();

	delete map;
	resource_manager.clear();
	skinned_mesh_globals.reset();
	map = new Map();

	connect(&map->terrain, &Terrain::minimap_changed, minimap, &Minimap::set_minimap);
	connect(&map->terrain, &Terrain::tileset_changed, [&]() {
		if (terrain_palette) {
			terrain_palette->refresh();
		}
	});

	map->load(directory);
	add_recent_map(directory);

	map->render_manager.resize_framebuffers(ui->widget->width(), ui->widget->height());
	setWindowTitle("HiveWE 0.11 - " + QString::fromStdString(map->filesystem_path.string()));
}

void HiveWE::add_recent_map(const fs::path& directory) {
	QSettings settings;
	QStringList recent_maps = settings.value("recentMaps").toStringList();
	const QString map_path = QString::fromStdString(directory.lexically_normal().string());

	recent_maps.removeAll(map_path);
	recent_maps.prepend(map_path);

	while (recent_maps.size() > 3) {
		recent_maps.removeLast();
	}

	settings.setValue("recentMaps", recent_maps);
	update_recent_maps_menu();
}

void HiveWE::update_recent_maps_menu() {
	QSettings settings;
	QStringList recent_maps = settings.value("recentMaps").toStringList();

	int button_index = 0;
	for (const auto& map_path : recent_maps) {
		if (button_index >= static_cast<int>(ui->ribbon->recent_maps.size())) {
			break;
		}

		const fs::path directory = map_path.toStdString();
		if (!fs::exists(directory / "war3map.w3i")) {
			continue;
		}

		auto* button = ui->ribbon->recent_maps[button_index];
		const QFileInfo info(map_path);
		button->setText(info.fileName());
		button->setToolTip(map_path);
		button->setStatusTip(map_path);
		button->show();
		++button_index;
	}

	for (; button_index < static_cast<int>(ui->ribbon->recent_maps.size()); ++button_index) {
		ui->ribbon->recent_maps[button_index]->hide();
	}
}

void HiveWE::open_recent_map(int index) {
	QSettings settings;
	const QStringList recent_maps = settings.value("recentMaps").toStringList();
	if (index < 0 || index >= recent_maps.size()) {
		return;
	}

	const fs::path directory = recent_maps[index].toStdString();
	if (!fs::exists(directory / "war3map.w3i")) {
		QMessageBox::information(this, "Opening map failed", "The recent map path no longer exists.");
		update_recent_maps_menu();
		return;
	}

	load_map(directory);
}

void HiveWE::load_folder() {
	QSettings settings;

	QString folder_name = QFileDialog::getExistingDirectory(this, "Open Map Directory",
															settings.value("openDirectory", QDir::current().path()).toString(),
															QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (folder_name == "") {
		return;
	}

	settings.setValue("openDirectory", folder_name);

	fs::path directory = folder_name.toStdString();

	if (!fs::exists(directory / "war3map.w3i")) {
		QMessageBox::information(this, "Opening map failed", "Opening the map failed. Select a map that is saved in folder mode or use the Open Map (MPQ) option");
		return;
	}

	QMessageBox* loading_box = new QMessageBox(QMessageBox::Icon::Information, "Loading Map", "Loading " + QString::fromStdString(directory.filename().string()));
	loading_box->show();

	load_map(directory);

	loading_box->close();
	delete loading_box;
}

/// Load MPQ will extract all files from the archive in a user specified location
void HiveWE::load_mpq() {
	QSettings settings;

	// Choose an MPQ
	QString file_name = QFileDialog::getOpenFileName(this, "Open File",
													 settings.value("openDirectory", QDir::current().path()).toString(),
													 "Warcraft III Scenario (*.w3m *.w3x)");

	if (file_name.isEmpty()) {
		return;
	}

	settings.setValue("openDirectory", file_name);

	fs::path mpq_path = file_name.toStdWString();

	mpq::MPQ mpq;
	bool opened = mpq.open(mpq_path);
	if (!opened) {
		const auto message = std::format("Opening the map archive failed. It might be opened in another program.\nError Code {}", GetLastError());
		QMessageBox::critical(this, "Opening map failed", QString::fromStdString(message));
		return;
	}

	fs::path unpack_location = QFileDialog::getExistingDirectory(
								   this, "Choose Unpacking Location",
								   settings.value("openDirectory", QDir::current().path()).toString(),
								   QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks)
								   .toStdString();

	if (unpack_location.empty()) {
		return;
	}

	fs::path final_directory = unpack_location / mpq_path.stem();

	try {
		fs::create_directory(final_directory);
	} catch (std::filesystem::filesystem_error& e) {
		QMessageBox::critical(this, "Error creating directory", "Failed to create the directory to unpack into with error:\n" + QString::fromStdString(e.what()), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
		return;
	}

	bool unpacked = mpq.unpack(final_directory);
	if (!unpacked) {
		QMessageBox::critical(this, "Unpacking failed", "There was an error unpacking the archive.");
		std::println("{}", GetLastError());
		return;
	}

	load_map(final_directory);
}

void HiveWE::save() {
	emit saving_initiated();
	if (map->save(map->filesystem_path)) {
		show_transient_notice("Map saved");
	}
};

void HiveWE::save_as() {
	QSettings settings;
	const QString directory = settings.value("openDirectory", QDir::current().path()).toString() + "/" + QString::fromStdString(map->name);

	fs::path file_name = QFileDialog::getExistingDirectory(this, "Choose Save Location",
														   settings.value("openDirectory", QDir::current().path()).toString(),
														   QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks)
							 .toStdString();

	if (file_name.empty()) {
		return;
	}

	emit saving_initiated();
	if (fs::exists(file_name) && fs::equivalent(file_name, map->filesystem_path)) {
		if (map->save(map->filesystem_path)) {
			show_transient_notice("Map saved");
		}
	} else {
		fs::create_directories(file_name / map->name);

		hierarchy.map_directory = file_name / map->name;
		if (map->save(file_name / map->name)) {
			show_transient_notice("Map saved");
		}
	}

	setWindowTitle("HiveWE 0.11 - " + QString::fromStdString(map->filesystem_path.string()));
}

void HiveWE::export_mpq() {
	QSettings settings;
	const QString directory = settings.value("openDirectory", QDir::current().path()).toString() + "/" + QString::fromStdString(map->filesystem_path.filename().string());
	std::wstring file_name = QFileDialog::getSaveFileName(this, "Export Map to MPQ", directory, "Warcraft III Scenario (*.w3x)").toStdWString();

	if (file_name.empty()) {
		return;
	}

	fs::remove(file_name);

	emit saving_initiated();
	map->save(map->filesystem_path);

	uint64_t file_count = std::distance(fs::recursive_directory_iterator{ map->filesystem_path }, {});

	HANDLE handle;
	bool open = SFileCreateArchive(file_name.c_str(), MPQ_CREATE_LISTFILE | MPQ_CREATE_ATTRIBUTES, file_count, &handle);
	if (!open) {
		QMessageBox::critical(this, "Exporting failed", "There was an error creating the archive.");
		std::println("{}", GetLastError());
		return;
	}

	for (const auto& entry : fs::recursive_directory_iterator(map->filesystem_path)) {
		if (entry.is_regular_file()) {
			bool success = SFileAddFileEx(handle, entry.path().c_str(), entry.path().lexically_relative(map->filesystem_path).string().c_str(), MPQ_FILE_COMPRESS, MPQ_COMPRESSION_ZLIB, MPQ_COMPRESSION_NEXT_SAME);
			if (!success) {
				std::println("Error {} adding file {}", GetLastError(), entry.path().string());
			}
		}
	}
	SFileCompactArchive(handle, nullptr, false);
	SFileCloseArchive(handle);
}

void HiveWE::play_test() {
	emit saving_initiated();
	if (!map->save(map->filesystem_path)) {
		return;
	}
	QProcess* warcraft = new QProcess;
	const QString warcraft_path = QString::fromStdString(fs::canonical(hierarchy.root_directory / "x86_64" / "Warcraft III.exe").string());
	QStringList arguments;
	arguments << "-launch"
			  << "-loadfile" << QString::fromStdString(fs::canonical(map->filesystem_path).string());

	QSettings settings;
	if (settings.value("testArgs").toString() != "")
		arguments << settings.value("testArgs").toString().split(' ');

	warcraft->start(warcraft_path, arguments);
}

void HiveWE::closeEvent(QCloseEvent* event) {
	int choice = QMessageBox::question(this, "Do you want to quit?", "Are you sure you want to quit?", QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

	if (choice == QMessageBox::Yes) {
		QApplication::closeAllWindows();
		event->accept();
	} else {
		event->ignore();
	}
}

void HiveWE::resizeEvent(QResizeEvent* event) {
	QMainWindow::resizeEvent(event);
	position_transient_notice();
	QTimer::singleShot(0, [&] { save_window_state(); });
}

void HiveWE::moveEvent(QMoveEvent* event) {
	QMainWindow::moveEvent(event);
	position_transient_notice();
	QTimer::singleShot(0, [&] { save_window_state(); });
}

void HiveWE::switch_warcraft() {
	QSettings settings;
	fs::path directory;
	do {
		directory = QFileDialog::getExistingDirectory(this, "Select Warcraft Directory", "/home", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks).toStdWString();
		if (directory == "") {
			directory = settings.value("warcraftDirectory").toString().toStdString();
		}
	} while (!hierarchy.open_casc(directory));

	if (directory != hierarchy.warcraft_directory) {
		settings.setValue("warcraftDirectory", QString::fromStdString(directory.string()));
	}
}

// ToDo move to terrain class?
void HiveWE::import_heightmap() {
	QMessageBox::information(this, "Heightmap information", "Will read the red channel and map this onto the range -16 to +16");
	QSettings settings;
	const QString directory = settings.value("openDirectory", QDir::current().path()).toString() + "/" + QString::fromStdString(map->filesystem_path.filename().string());

	QString file_name = QFileDialog::getOpenFileName(this, "Open Heightmap Image", directory);

	if (file_name == "") {
		return;
	}

	int width;
	int height;
	int channels;
	uint8_t* image_data = SOIL_load_image(file_name.toStdString().c_str(), &width, &height, &channels, SOIL_LOAD_AUTO);

	if (width != map->terrain.width || height != map->terrain.height) {
		QMessageBox::warning(this, "Incorrect Image Size", QString("Image Size: %1x%2 does not match terrain size: %3x%4").arg(QString::number(width), QString::number(height), QString::number(map->terrain.width), QString::number(map->terrain.height)));
		return;
	}

	for (int j = 0; j < height; j++) {
		for (int i = 0; i < width; i++) {
			map->terrain.corner_height[map->terrain.ci(i, j)] = (image_data[((height - 1 - j) * width + i) * channels] - 128.f) / 8.f;
		}
	}

	map->terrain.update_ground_heights({ 0, 0, width, height });
	delete image_data;
}

void HiveWE::save_window_state() {
	QSettings settings;

	if (!isMaximized()) {
		settings.setValue("MainWindow/geometry", saveGeometry());
	}

	settings.setValue("MainWindow/maximized", isMaximized());
	settings.setValue("MainWindow/windowState", saveState());
}

void HiveWE::restore_window_state() {
	QSettings settings;

	if (settings.contains("MainWindow/windowState")) {
		restoreGeometry(settings.value("MainWindow/geometry").toByteArray());
		restoreState(settings.value("MainWindow/windowState").toByteArray());
		if (settings.value("MainWindow/maximized").toBool()) {
			showMaximized();
		} else {
			showNormal();
		}
	} else {
		showMaximized();
	}
}

void HiveWE::set_current_custom_tab(QRibbonTab* tab, QString name) {
	if (current_custom_tab == tab) {
		return;
	}

	if (current_custom_tab != nullptr) {
		emit palette_changed(tab);
	}

	remove_custom_tab();
	current_custom_tab = tab;
	ui->ribbon->addTab(tab, name);
	ui->ribbon->setCurrentIndex(ui->ribbon->count() - 1);
}

void HiveWE::remove_custom_tab() {
	for (int i = 0; i < ui->ribbon->count(); i++) {
		if (ui->ribbon->widget(i) == current_custom_tab) {
			ui->ribbon->removeTab(i);
			current_custom_tab = nullptr;
			return;
		}
	}
}

Palette* HiveWE::palette_for_view(const SidebarPaletteView view) const {
	switch (view) {
	case SidebarPaletteView::terrain:
		return terrain_palette;
	case SidebarPaletteView::doodad:
		return doodad_palette;
	case SidebarPaletteView::unit:
		return unit_palette;
	case SidebarPaletteView::pathing:
		return pathing_palette;
	case SidebarPaletteView::region:
		return region_palette;
	case SidebarPaletteView::none:
		return nullptr;
	}

	return nullptr;
}

QWidget* HiveWE::host_for_view(const SidebarPaletteView view) const {
	switch (view) {
	case SidebarPaletteView::terrain:
		return terrain_host;
	case SidebarPaletteView::doodad:
		return doodad_host;
	case SidebarPaletteView::unit:
		return unit_host;
	case SidebarPaletteView::pathing:
		return pathing_host;
	case SidebarPaletteView::region:
		return region_host;
	case SidebarPaletteView::none:
		return nullptr;
	}

	return nullptr;
}

QString HiveWE::sidebar_title_for_view(const SidebarPaletteView view) const {
	switch (view) {
	case SidebarPaletteView::terrain:
		return "Terrain";
	case SidebarPaletteView::doodad:
		return "Doodads";
	case SidebarPaletteView::unit:
		return "Units";
	case SidebarPaletteView::pathing:
		return "Pathing";
	case SidebarPaletteView::region:
		return "Regions";
	case SidebarPaletteView::none:
		return "";
	}

	return "";
}

QString HiveWE::sidebar_description_for_view(const SidebarPaletteView view) const {
	switch (view) {
	case SidebarPaletteView::terrain:
		return "Brush families, tiles, and terrain operations in one focused panel without stacking unrelated palettes below it.";
	case SidebarPaletteView::doodad:
		return "Search, filter, and place doodads with the active selection and placement rules kept in one consistent sidebar.";
	case SidebarPaletteView::unit:
		return "Pick units by owner and search context, then keep selection-aware placement tools close to the active asset.";
	case SidebarPaletteView::pathing:
		return "Pathing brush operations, mask selection, and import or export tools in a dedicated utility view.";
	case SidebarPaletteView::region:
		return "Region selection, creation, and property editing in one compact mode-specific inspector.";
	case SidebarPaletteView::none:
		return "";
	}

	return "";
}

QString HiveWE::sidebar_hint_for_view(const SidebarPaletteView view) const {
	switch (view) {
	case SidebarPaletteView::terrain:
		return "Shortcut: T. Space switches selection mode where relevant.";
	case SidebarPaletteView::doodad:
		return "Shortcut: D. Ctrl+F jumps to search. Middle-click can sync from the map selection.";
	case SidebarPaletteView::unit:
		return "Shortcut: U. Ctrl+F jumps to search and player ownership stays visible in the palette.";
	case SidebarPaletteView::pathing:
		return "Shortcut: P. Keep pathing operation and brush mask readable while the viewport remains dominant.";
	case SidebarPaletteView::region:
		return "Shortcut: R. Keep region properties close to the selected region instead of in a separate dialog flow.";
	case SidebarPaletteView::none:
		return "";
	}

	return "";
}

QString HiveWE::sidebar_feedback_label(const SidebarPaletteView view) const {
	switch (view) {
	case SidebarPaletteView::terrain:
		return "Terrain sidebar";
	case SidebarPaletteView::doodad:
		return "Doodad sidebar";
	case SidebarPaletteView::unit:
		return "Unit sidebar";
	case SidebarPaletteView::pathing:
		return "Pathing sidebar";
	case SidebarPaletteView::region:
		return "Region sidebar";
	case SidebarPaletteView::none:
		return "Sidebar";
	}

	return "Sidebar";
}

void HiveWE::ensure_palette_view(const SidebarPaletteView view) {
	if ((view == SidebarPaletteView::terrain || view == SidebarPaletteView::pathing) && !terrain_palette) {
		terrain_palette = new TerrainPalette(terrain_host);
		terrain_host->layout()->addWidget(terrain_palette);
		connect(terrain_palette, &Palette::activated, this, [this]() { activate_palette(terrain_palette); });
		connect(terrain_palette, &Palette::ribbon_tab_requested, this, &HiveWE::set_current_custom_tab);
		connect(this, &HiveWE::palette_changed, terrain_palette, &Palette::deactivate);
	}

	if ((view == SidebarPaletteView::terrain || view == SidebarPaletteView::pathing) && !pathing_palette) {
		pathing_palette = new PathingPalette(pathing_host);
		pathing_host->layout()->addWidget(pathing_palette);
		connect(pathing_palette, &Palette::activated, this, [this]() { activate_palette(pathing_palette); });
		connect(pathing_palette, &Palette::ribbon_tab_requested, this, &HiveWE::set_current_custom_tab);
		connect(this, &HiveWE::palette_changed, pathing_palette, &Palette::deactivate);
		connect(&pathing_palette->current_brush(), &Brush::size_changed, this, [this](glm::ivec2) {
			if (terrain_palette) {
				const QSignalBlocker blocker(terrain_palette->current_brush());
				terrain_palette->current_brush().set_size(pathing_palette->current_brush().get_size());
				terrain_palette->sync_brush_controls(map->brush);
			}
		});
		if (terrain_palette) {
			connect(&terrain_palette->current_brush(), &Brush::size_changed, this, [this](glm::ivec2) {
				if (pathing_palette) {
					const QSignalBlocker blocker(pathing_palette->current_brush());
					pathing_palette->current_brush().set_size(terrain_palette->current_brush().get_size());
					terrain_palette->sync_brush_controls(map->brush);
				}
			});
		}
	}

	if (view == SidebarPaletteView::doodad && !doodad_palette) {
		doodad_palette = new DoodadPalette(doodad_host);
		doodad_host->layout()->addWidget(doodad_palette);
		connect(doodad_palette, &Palette::activated, this, [this]() { activate_palette(doodad_palette); });
		connect(doodad_palette, &Palette::ribbon_tab_requested, this, &HiveWE::set_current_custom_tab);
		connect(this, &HiveWE::palette_changed, doodad_palette, &Palette::deactivate);
	}

	if (view == SidebarPaletteView::unit && !unit_palette) {
		unit_palette = new UnitPalette(unit_host);
		unit_host->layout()->addWidget(unit_palette);
		connect(unit_palette, &Palette::activated, this, [this]() { activate_palette(unit_palette); });
		connect(unit_palette, &Palette::ribbon_tab_requested, this, &HiveWE::set_current_custom_tab);
		connect(this, &HiveWE::palette_changed, unit_palette, &Palette::deactivate);
	}

	if (view == SidebarPaletteView::region && !region_palette) {
		region_palette = new RegionPalette(region_host);
		region_host->layout()->addWidget(region_palette);
		connect(region_palette, &Palette::activated, this, [this]() { activate_palette(region_palette); });
		connect(region_palette, &Palette::ribbon_tab_requested, this, &HiveWE::set_current_custom_tab);
		connect(this, &HiveWE::palette_changed, region_palette, &Palette::deactivate);
	}
}

void HiveWE::update_active_palette_visuals() {
	const bool sidebar_visible = sidebar_root && sidebar_root->isVisible() && current_sidebar_view != SidebarPaletteView::none;

	auto set_checked = [](QAbstractButton* button, const bool checked) {
		if (!button) {
			return;
		}

		const QSignalBlocker blocker(button);
		button->setChecked(checked);
	};

	set_checked(ui->ribbon->terrain_palette, sidebar_visible && current_sidebar_view == SidebarPaletteView::terrain);
	set_checked(ui->ribbon->doodad_palette, sidebar_visible && current_sidebar_view == SidebarPaletteView::doodad);
	set_checked(ui->ribbon->unit_palette, sidebar_visible && current_sidebar_view == SidebarPaletteView::unit);
	set_checked(ui->ribbon->pathing_palette, sidebar_visible && current_sidebar_view == SidebarPaletteView::pathing);
	set_checked(ui->ribbon->region_palette, sidebar_visible && current_sidebar_view == SidebarPaletteView::region);
	set_checked(terrain_mode_button, sidebar_visible && current_sidebar_view == SidebarPaletteView::terrain);
	set_checked(doodad_mode_button, sidebar_visible && current_sidebar_view == SidebarPaletteView::doodad);
	set_checked(unit_mode_button, sidebar_visible && current_sidebar_view == SidebarPaletteView::unit);
	set_checked(pathing_mode_button, sidebar_visible && current_sidebar_view == SidebarPaletteView::pathing);
	set_checked(region_mode_button, sidebar_visible && current_sidebar_view == SidebarPaletteView::region);

	if (!sidebar_visible) {
		sidebar_title->clear();
		sidebar_description->clear();
		sidebar_hint->clear();
		return;
	}

	sidebar_title->setText(sidebar_title_for_view(current_sidebar_view));
	sidebar_description->setText(sidebar_description_for_view(current_sidebar_view));
	sidebar_hint->setText(sidebar_hint_for_view(current_sidebar_view));
}

void HiveWE::hide_palette_sidebar() {
	const SidebarPaletteView previous_view = current_sidebar_view;
	current_sidebar_view = SidebarPaletteView::none;
	sidebar_root->hide();

	if (auto* palette = palette_for_view(previous_view)) {
		if ((terrain_palette && palette == terrain_palette && map->brush == &terrain_palette->current_brush())
			|| (pathing_palette && palette == pathing_palette && map->brush == &pathing_palette->current_brush())
			|| (doodad_palette && palette == doodad_palette && map->brush == &doodad_palette->current_brush())
			|| (unit_palette && palette == unit_palette && map->brush == &unit_palette->current_brush())
			|| (region_palette && palette == region_palette && map->brush == &region_palette->current_brush())) {
			map->brush = nullptr;
		}
	}

	remove_custom_tab();
	update_active_palette_visuals();
}

void HiveWE::show_palette_view(const SidebarPaletteView view, const bool show_feedback) {
	ensure_palette_view(view);

	auto* host = host_for_view(view);
	auto* palette = palette_for_view(view);
	if (!host || !palette) {
		return;
	}

	current_sidebar_view = view;
	sidebar_stack->setCurrentWidget(host);
	sidebar_root->show();
	update_active_palette_visuals();
	activate_palette(palette, false);

	if (show_feedback) {
		show_transient_notice(sidebar_feedback_label(view), 850);
	}
}

void HiveWE::activate_palette(Palette* palette, const bool show_feedback) {
	bool already_active = false;
	if (terrain_palette && palette == terrain_palette) {
		already_active = map->brush == &terrain_palette->current_brush();
	} else if (pathing_palette && palette == pathing_palette) {
		already_active = map->brush == &pathing_palette->current_brush();
	} else if (doodad_palette && palette == doodad_palette) {
		already_active = map->brush == &doodad_palette->current_brush();
	} else if (unit_palette && palette == unit_palette) {
		already_active = map->brush == &unit_palette->current_brush();
	} else if (region_palette && palette == region_palette) {
		already_active = map->brush == &region_palette->current_brush();
	}

	if (terrain_palette && pathing_palette) {
		if (auto* terrain = dynamic_cast<TerrainPalette*>(palette)) {
			if (dynamic_cast<PathingBrush*>(map->brush)) {
				const QSignalBlocker blocker(terrain->current_brush());
				terrain->current_brush().set_size(pathing_palette->current_brush().get_size());
			}
		} else if (auto* pathing = dynamic_cast<PathingPalette*>(palette)) {
			if (dynamic_cast<TerrainBrush*>(map->brush)) {
				const QSignalBlocker blocker(pathing->current_brush());
				pathing->current_brush().set_size(terrain_palette->current_brush().get_size());
			}
		}
	}

	if (auto* terrain = dynamic_cast<TerrainPalette*>(palette)) {
		terrain->activate_palette();
	} else if (auto* pathing = dynamic_cast<PathingPalette*>(palette)) {
		pathing->activate_palette();
	} else if (auto* doodad = dynamic_cast<DoodadPalette*>(palette)) {
		doodad->activate_palette();
	} else if (auto* unit = dynamic_cast<UnitPalette*>(palette)) {
		unit->activate_palette();
	} else if (auto* region = dynamic_cast<RegionPalette*>(palette)) {
		region->activate_palette();
	}

	if (terrain_palette) {
		terrain_palette->sync_brush_controls(map->brush);
	}

	update_active_palette_visuals();

	if (!show_feedback || already_active) {
		return;
	}

	QString label = "Palette active";
	if (dynamic_cast<TerrainPalette*>(palette)) {
		label = "Terrain mode";
	} else if (dynamic_cast<PathingPalette*>(palette)) {
		label = "Pathing mode";
	} else if (dynamic_cast<DoodadPalette*>(palette)) {
		label = "Doodad mode";
	} else if (dynamic_cast<UnitPalette*>(palette)) {
		label = "Unit mode";
	} else if (dynamic_cast<RegionPalette*>(palette)) {
		label = "Region mode";
	}

	show_transient_notice(label, 850);
}

void HiveWE::toggle_terrain_sidebar() {
	if (sidebar_root->isVisible() && current_sidebar_view == SidebarPaletteView::terrain) {
		hide_palette_sidebar();
		show_transient_notice("Terrain sidebar hidden", 700);
		return;
	}

	show_palette_view(SidebarPaletteView::terrain, false);
	show_transient_notice("Terrain sidebar shown", 700);
}

void HiveWE::toggle_pathing_sidebar() {
	if (sidebar_root->isVisible() && current_sidebar_view == SidebarPaletteView::pathing) {
		hide_palette_sidebar();
		show_transient_notice("Pathing sidebar hidden", 700);
		return;
	}

	show_palette_view(SidebarPaletteView::pathing, false);
	show_transient_notice("Pathing sidebar shown", 700);
}

void HiveWE::toggle_doodad_palette() {
	if (sidebar_root->isVisible() && current_sidebar_view == SidebarPaletteView::doodad) {
		hide_palette_sidebar();
		show_transient_notice("Doodad sidebar hidden", 700);
		return;
	}

	show_palette_view(SidebarPaletteView::doodad, false);
	show_transient_notice("Doodad sidebar shown", 700);
}

void HiveWE::toggle_unit_palette() {
	if (sidebar_root->isVisible() && current_sidebar_view == SidebarPaletteView::unit) {
		hide_palette_sidebar();
		show_transient_notice("Unit sidebar hidden", 700);
		return;
	}

	show_palette_view(SidebarPaletteView::unit, false);
	show_transient_notice("Unit sidebar shown", 700);
}

void HiveWE::toggle_region_palette() {
	if (sidebar_root->isVisible() && current_sidebar_view == SidebarPaletteView::region) {
		hide_palette_sidebar();
		show_transient_notice("Region sidebar hidden", 700);
		return;
	}

	show_palette_view(SidebarPaletteView::region, false);
	show_transient_notice("Region sidebar shown", 700);
}

void HiveWE::show_transient_notice(const QString& text, int timeout_ms) {
	transient_notice->setText(text);
	transient_notice->adjustSize();
	transient_notice->resize(transient_notice->width() + 24, transient_notice->height() + 16);
	position_transient_notice();
	transient_notice->show();
	transient_notice->raise();
	transient_notice_timer.start(timeout_ms);
}

void HiveWE::position_transient_notice() {
	if (!transient_notice) {
		return;
	}

	const int x = (width() - transient_notice->width()) / 2;
	const int y = (height() - transient_notice->height()) / 2;
	transient_notice->move(std::max(0, x), std::max(0, y));
}
