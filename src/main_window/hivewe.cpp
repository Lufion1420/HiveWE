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
#include <QFileInfo>
#include <QStringList>
import "menus/gameplay_constants_editor.h";
import "asset_manager/asset_manager.h";

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
	ui->ribbon->pathing_palette->hide();
	connect(ui->ribbon->pathing_palette, &QRibbonButton::clicked, this, &HiveWE::toggle_terrain_sidebar);
	connect(ui->ribbon->region_palette, &QRibbonButton::clicked, this, &HiveWE::toggle_region_palette);

	connect(new QShortcut(QKeySequence(Qt::Key_T), this, nullptr, nullptr, Qt::WindowShortcut), &QShortcut::activated, this, &HiveWE::toggle_terrain_sidebar);
	connect(ui->ribbon->terrain_palette, &QRibbonButton::clicked, this, &HiveWE::toggle_terrain_sidebar);

	connect(new QShortcut(QKeySequence(Qt::Key_D), this, nullptr, nullptr, Qt::WindowShortcut), &QShortcut::activated, this, &HiveWE::toggle_doodad_palette);
	connect(ui->ribbon->doodad_palette, &QRibbonButton::clicked, this, &HiveWE::toggle_doodad_palette);

	connect(new QShortcut(QKeySequence(Qt::Key_U), this, nullptr, nullptr, Qt::WindowShortcut), &QShortcut::activated, this, &HiveWE::toggle_unit_palette);
	connect(ui->ribbon->unit_palette, &QRibbonButton::clicked, this, &HiveWE::toggle_unit_palette);

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
	sidebar_root->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
	sidebar_root->setMinimumWidth(320);

	auto* sidebar_layout = new QHBoxLayout(sidebar_root);
	sidebar_layout->setContentsMargins(8, 0, 8, 0);
	sidebar_layout->setSpacing(8);

	object_column = new QWidget(sidebar_root);
	object_column->setMinimumWidth(320);
	object_column_layout = new QVBoxLayout(object_column);
	object_column_layout->setContentsMargins(0, 0, 0, 0);
	object_column_layout->setSpacing(6);

	doodad_host = new QFrame(object_column);
	doodad_host->setObjectName("doodadSidebarHost");
	auto* doodad_layout = new QVBoxLayout(doodad_host);
	doodad_layout->setContentsMargins(0, 0, 0, 0);
	doodad_layout->setSpacing(0);

	unit_host = new QFrame(object_column);
	unit_host->setObjectName("unitSidebarHost");
	auto* unit_layout = new QVBoxLayout(unit_host);
	unit_layout->setContentsMargins(0, 0, 0, 0);
	unit_layout->setSpacing(0);

	object_column_layout->addWidget(doodad_host, 1);
	object_column_layout->addWidget(unit_host, 1);

	terrain_column = new QWidget(sidebar_root);
	terrain_column->setMinimumWidth(340);
	terrain_column_layout = new QVBoxLayout(terrain_column);
	terrain_column_layout->setContentsMargins(0, 0, 0, 0);
	terrain_column_layout->setSpacing(0);

	auto* terrain_header = new QLabel("Terrain", terrain_column);
	terrain_header->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	terrain_header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	terrain_header->setFixedHeight(24);
	terrain_header->setStyleSheet("QLabel { font-size: 15px; font-weight: 700; padding: 0 2px 4px 2px; margin: 0; }");

	terrain_host = new QFrame(terrain_column);
	terrain_host->setObjectName("terrainSidebarHost");
	terrain_host->setFrameShape(QFrame::NoFrame);
	terrain_host->setContentsMargins(0, 0, 0, 0);
	auto* terrain_layout = new QVBoxLayout(terrain_host);
	terrain_layout->setContentsMargins(0, 0, 0, 0);
	terrain_layout->setSpacing(0);

	auto* pathing_separator = new QFrame(terrain_column);
	pathing_separator->setFrameShape(QFrame::HLine);
	pathing_separator->setFrameShadow(QFrame::Sunken);
	pathing_separator->setFixedHeight(8);

	auto* pathing_header = new QLabel("Pathing", terrain_column);
	pathing_header->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	pathing_header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	pathing_header->setFixedHeight(24);
	pathing_header->setStyleSheet("QLabel { font-size: 15px; font-weight: 700; padding: 2px 2px 3px 2px; margin: 0; }");

	pathing_host = new QFrame(terrain_column);
	pathing_host->setObjectName("pathingSidebarHost");
	pathing_host->setFrameShape(QFrame::NoFrame);
	pathing_host->setContentsMargins(0, 0, 0, 0);
	auto* pathing_layout = new QVBoxLayout(pathing_host);
	pathing_layout->setContentsMargins(0, 0, 0, 0);
	pathing_layout->setSpacing(0);

	auto* region_separator = new QFrame(terrain_column);
	region_separator->setFrameShape(QFrame::HLine);
	region_separator->setFrameShadow(QFrame::Sunken);
	region_separator->setFixedHeight(8);

	auto* region_header = new QLabel("Regions", terrain_column);
	region_header->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	region_header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	region_header->setFixedHeight(24);
	region_header->setStyleSheet("QLabel { font-size: 15px; font-weight: 700; padding: 2px 2px 3px 2px; margin: 0; }");

	region_host = new QFrame(terrain_column);
	region_host->setObjectName("regionSidebarHost");
	region_host->setFrameShape(QFrame::NoFrame);
	region_host->setContentsMargins(0, 0, 0, 0);
	auto* region_layout = new QVBoxLayout(region_host);
	region_layout->setContentsMargins(0, 0, 0, 0);
	region_layout->setSpacing(0);

	terrain_column_layout->addWidget(terrain_header);
	terrain_column_layout->addWidget(terrain_host, 3);
	terrain_column_layout->addWidget(pathing_separator);
	terrain_column_layout->addWidget(pathing_header);
	terrain_column_layout->addWidget(pathing_host, 2);
	terrain_column_layout->addWidget(region_separator);
	terrain_column_layout->addWidget(region_header);
	terrain_column_layout->addWidget(region_host, 2);

	sidebar_layout->addWidget(object_column);
	sidebar_layout->addWidget(terrain_column);
	content_layout->addWidget(sidebar_root);

	sidebar_root->hide();
	object_column->hide();
	terrain_column->hide();
	doodad_host->hide();
	unit_host->hide();
	terrain_host->hide();
	pathing_host->hide();
	region_host->hide();
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
	sidebar_root->hide();
	object_column->hide();
	terrain_column->hide();
	doodad_host->hide();
	unit_host->hide();
	terrain_host->hide();
	pathing_host->hide();
	region_host->hide();
	ui->ribbon->terrain_palette->setChecked(false);
	ui->ribbon->doodad_palette->setChecked(false);
	ui->ribbon->unit_palette->setChecked(false);
	ui->ribbon->region_palette->setChecked(false);

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

void HiveWE::activate_palette(Palette* palette) {
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
}

void HiveWE::toggle_terrain_sidebar() {
	if (!terrain_palette) {
		terrain_palette = new TerrainPalette(terrain_host);
		terrain_host->layout()->addWidget(terrain_palette);
		connect(terrain_palette, &Palette::activated, this, [this]() { activate_palette(terrain_palette); });
		connect(terrain_palette, &Palette::ribbon_tab_requested, this, &HiveWE::set_current_custom_tab);
		connect(this, &HiveWE::palette_changed, terrain_palette, &Palette::deactivate);
	}

	if (!pathing_palette) {
		pathing_palette = new PathingPalette(pathing_host);
		pathing_host->layout()->addWidget(pathing_palette);
		connect(pathing_palette, &Palette::activated, this, [this]() { activate_palette(pathing_palette); });
		connect(pathing_palette, &Palette::ribbon_tab_requested, this, &HiveWE::set_current_custom_tab);
		connect(this, &HiveWE::palette_changed, pathing_palette, &Palette::deactivate);
		connect(&pathing_palette->current_brush(), &Brush::size_changed, this, [this](glm::ivec2) {
			if (terrain_palette) {
				const QSignalBlocker blocker(terrain_palette->current_brush());
				terrain_palette->current_brush().set_size(pathing_palette->current_brush().get_size());
			}
			if (terrain_palette) {
				terrain_palette->sync_brush_controls(map->brush);
			}
		});
		if (terrain_palette) {
			connect(&terrain_palette->current_brush(), &Brush::size_changed, this, [this](glm::ivec2) {
				if (pathing_palette) {
					const QSignalBlocker blocker(pathing_palette->current_brush());
					pathing_palette->current_brush().set_size(terrain_palette->current_brush().get_size());
				}
				terrain_palette->sync_brush_controls(map->brush);
			});
		}
	}

	const bool visible = terrain_host->isHidden() || pathing_host->isHidden();
	terrain_palette->setVisible(visible);
	pathing_palette->setVisible(visible);
	terrain_host->setVisible(visible);
	pathing_host->setVisible(visible);
	ui->ribbon->terrain_palette->setChecked(visible);

	if (visible) {
		activate_palette(terrain_palette);
	} else if (doodad_palette && !doodad_palette->isHidden()) {
		activate_palette(doodad_palette);
	} else if (unit_palette && !unit_palette->isHidden()) {
		activate_palette(unit_palette);
	} else {
		remove_custom_tab();
	}

	update_palette_sidebar_layout();
}

void HiveWE::toggle_doodad_palette() {
	if (!doodad_palette) {
		doodad_palette = new DoodadPalette(doodad_host);
		doodad_host->layout()->addWidget(doodad_palette);
		connect(doodad_palette, &Palette::activated, this, [this]() { activate_palette(doodad_palette); });
		connect(doodad_palette, &Palette::ribbon_tab_requested, this, &HiveWE::set_current_custom_tab);
		connect(this, &HiveWE::palette_changed, doodad_palette, &Palette::deactivate);
	}

	const bool visible = doodad_host->isHidden();
	doodad_palette->setVisible(visible);
	doodad_host->setVisible(visible);
	ui->ribbon->doodad_palette->setChecked(visible);

	if (visible) {
		activate_palette(doodad_palette);
	} else if (unit_palette && !unit_palette->isHidden()) {
		activate_palette(unit_palette);
	} else if (terrain_palette && !terrain_palette->isHidden()) {
		activate_palette(terrain_palette);
	} else {
		remove_custom_tab();
	}

	update_palette_sidebar_layout();
}

void HiveWE::toggle_unit_palette() {
	if (!unit_palette) {
		unit_palette = new UnitPalette(unit_host);
		unit_host->layout()->addWidget(unit_palette);
		connect(unit_palette, &Palette::activated, this, [this]() { activate_palette(unit_palette); });
		connect(unit_palette, &Palette::ribbon_tab_requested, this, &HiveWE::set_current_custom_tab);
		connect(this, &HiveWE::palette_changed, unit_palette, &Palette::deactivate);
	}

	const bool visible = unit_host->isHidden();
	unit_palette->setVisible(visible);
	unit_host->setVisible(visible);
	ui->ribbon->unit_palette->setChecked(visible);

	if (visible) {
		activate_palette(unit_palette);
	} else if (doodad_palette && !doodad_palette->isHidden()) {
		activate_palette(doodad_palette);
	} else if (terrain_palette && !terrain_palette->isHidden()) {
		activate_palette(terrain_palette);
	} else {
		remove_custom_tab();
	}

	update_palette_sidebar_layout();
}

void HiveWE::toggle_region_palette() {
	if (!region_palette) {
		region_palette = new RegionPalette(region_host);
		region_host->layout()->addWidget(region_palette);
		connect(region_palette, &Palette::activated, this, [this]() { activate_palette(region_palette); });
		connect(region_palette, &Palette::ribbon_tab_requested, this, &HiveWE::set_current_custom_tab);
		connect(this, &HiveWE::palette_changed, region_palette, &Palette::deactivate);
	}

	const bool visible = region_host->isHidden();
	region_palette->setVisible(visible);
	region_host->setVisible(visible);
	ui->ribbon->region_palette->setChecked(visible);

	if (visible) {
		activate_palette(region_palette);
	} else if (terrain_palette && !terrain_palette->isHidden()) {
		activate_palette(terrain_palette);
	} else if (doodad_palette && !doodad_palette->isHidden()) {
		activate_palette(doodad_palette);
	} else if (unit_palette && !unit_palette->isHidden()) {
		activate_palette(unit_palette);
	} else {
		if (map->brush == &region_palette->current_brush()) {
			map->brush = nullptr;
		}
		remove_custom_tab();
	}

	update_palette_sidebar_layout();
}

void HiveWE::update_palette_sidebar_layout() {
	const bool doodad_visible = doodad_host && !doodad_host->isHidden();
	const bool unit_visible = unit_host && !unit_host->isHidden();
	const bool object_visible = doodad_visible || unit_visible;
	const bool terrain_visible = (terrain_host && !terrain_host->isHidden()) || (pathing_host && !pathing_host->isHidden())
		|| (region_host && !region_host->isHidden());

	object_column->setVisible(object_visible);
	terrain_column->setVisible(terrain_visible);
	sidebar_root->setVisible(object_visible || terrain_visible);

	if (object_column_layout) {
		object_column_layout->setStretch(0, doodad_visible && unit_visible ? 1 : (doodad_visible ? 1 : 0));
		object_column_layout->setStretch(1, doodad_visible && unit_visible ? 1 : (unit_visible ? 1 : 0));
	}
	if (terrain_column_layout) {
		terrain_column_layout->setStretch(1, 3);
		terrain_column_layout->setStretch(4, 2);
		terrain_column_layout->setStretch(7, 2);
	}
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
