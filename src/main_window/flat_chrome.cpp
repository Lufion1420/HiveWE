// Menu bar and compact toolbar of the Flat themes (designs/proposals/ui-modernization/toolbar-command-map.html).
// Every command here forwards to the existing ribbon button with the same meaning, so the slots, the shortcuts and the enabled state
// stay the ones wired in the HiveWE constructor. The ribbon object stays alive but is never visible while a Flat theme is active: the
// palettes carry their tool pages themselves (Palette::attach_tool_page). Also the Flat layout (docks) and the status bar.
#include "HiveWE.h"
#include "ui_HiveWE.h"
#include "theme.h"

#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "brush.h"
#include "selection_properties_panel.h"

import std;
import Map;
import MapGlobal;
import Camera;
import Globals;
import WorldUndoManager;

namespace fs = std::filesystem;

QAction* HiveWE::link_ribbon_action(QAbstractButton* source, const QString& text, const QString& icon_name, const QString& tooltip) {
	auto* action = new QAction(text, this);
	action->setToolTip(tooltip.isEmpty() ? source->toolTip() : tooltip);
	action->setStatusTip(action->toolTip());
	action->setCheckable(source->isCheckable());
	action->setChecked(source->isChecked());
	if (!icon_name.isEmpty()) {
		flat_icon_actions.emplace_back(action, icon_name);
	}

	// triggered -> click(): a checkable source toggles itself and emits the same toggled/clicked signals as a ribbon click.
	connect(action, &QAction::triggered, source, &QAbstractButton::click);
	connect(source, &QAbstractButton::toggled, action, [action](const bool checked) {
		const QSignalBlocker blocker(action);
		action->setChecked(checked);
	});
	return action;
}

void HiveWE::setup_flat_chrome() {
	auto* ribbon = ui->ribbon;

	// Only displays the shortcut in menus. The real shortcut is the QShortcut wired in the constructor; WidgetShortcut keeps the action
	// from also firing (an ambiguous shortcut would fire neither).
	const auto hint = [](QAction* action, const QString& keys) {
		action->setShortcut(QKeySequence(keys));
		action->setShortcutContext(Qt::WidgetShortcut);
		return action;
	};
	const auto link = [this](QAbstractButton* source, const QString& text, const QString& icon, const QString& tooltip = {}) {
		return link_ribbon_action(source, text, icon, tooltip);
	};
	// A command without a ribbon button.
	const auto command = [this](const QString& text, const QString& icon, const QString& tooltip, auto slot) {
		auto* action = new QAction(text, this);
		action->setToolTip(tooltip);
		action->setStatusTip(tooltip);
		if (!icon.isEmpty()) {
			flat_icon_actions.emplace_back(action, icon);
		}
		connect(action, &QAction::triggered, this, slot);
		return action;
	};

	// File
	auto* new_map = link(ribbon->new_map, "New Map", "new", "Create a new map");
	auto* open_folder = hint(link(ribbon->open_map_folder, "Open Map (Folder)...", "open", "Open a map folder. Shortcut: Ctrl+O"), "Ctrl+O");
	open_mpq_action = link(ribbon->open_map_mpq, "Open Map (MPQ)...", "open-mpq", "Open a map archive (.w3x or .w3m)");
	auto* save = hint(link(ribbon->save_map, "Save Map", "save", "Save the current map. Shortcut: Ctrl+S"), "Ctrl+S");
	auto* save_as = hint(link(ribbon->save_map_as, "Save Map As...", "save-as", "Save the map to a new folder. Shortcut: Ctrl+Shift+S"), "Ctrl+Shift+S");
	auto* export_mpq = link(ribbon->export_mpq, "Export MPQ...", "export-mpq", "Export the map as an MPQ archive");
	auto* test_map = hint(link(ribbon->test_map, "Test Map", "test-map", "Save and launch Warcraft III test mode. Shortcut: Ctrl+T"), "Ctrl+T");
	auto* settings = link(ribbon->settings, "Settings...", "settings", "Open the HiveWE settings, including the theme");
	auto* exit = link(ribbon->exit, "Exit", "exit", "Close HiveWE");

	// History
	auto* undo = hint(link(ribbon->undo, "Undo", "undo"), "Ctrl+Z");
	auto* redo = hint(link(ribbon->redo, "Redo", "redo"), "Ctrl+Y");

	// Visibility
	auto* units = hint(link(ribbon->units_visible, "Units", "units"), "Ctrl+U");
	auto* doodads = hint(link(ribbon->doodads_visible, "Doodads", "doodads"), "Ctrl+D");
	auto* pathing = hint(link(ribbon->pathing_visible, "Pathing", "pathing"), "Ctrl+P");
	auto* brush = link(ribbon->brush_visible, "Brush", "brush");
	auto* lighting = hint(link(ribbon->lighting_visible, "Lighting", "lighting"), "Ctrl+L");
	auto* water = hint(link(ribbon->water_visible, "Water", "water"), "Ctrl+W");
	auto* click_helpers = hint(link(ribbon->click_helpers_visible, "Click Helpers", "click-helpers"), "Ctrl+I");
	auto* wireframe = hint(link(ribbon->wireframe_visible, "Wireframe", "wireframe"), "Ctrl+Shift+W");
	auto* debug = hint(link(ribbon->debug_visible, "Debug", "debug"), "F3");
	auto* minimap_action = link(ribbon->minimap_visible, "Minimap", "minimap");
	auto* reset_camera = hint(link(ribbon->reset_camera, "Reset Camera", "reset-camera"), "Ctrl+Shift+C");

	// Editors
	auto* trigger_editor = link(ribbon->trigger_editor, "Trigger Editor", "trigger-editor", "Open the Trigger Editor");
	trigger_editor_action = trigger_editor;
	auto* object_editor = hint(link(ribbon->object_editor, "Object Editor", "object-editor", "Open the Object Editor. Shortcut: O"), "O");
	auto* model_editor = link(ribbon->model_editor, "Model Editor", "model-editor", "Open the Model Editor");
	auto* asset_manager = link(ribbon->asset_manager, "Asset Manager", "asset-manager", "Open the Asset Manager");
	auto* tooltip_editor = link(ribbon->tooltip_editor, "Tooltip Editor", "tooltip-editor");
	auto* map_protector = link(ribbon->map_protector, "Map Protector...", "map-protector");

	// Terrain and Map
	auto* import_heightmap = link(ribbon->import_heightmap, "Import Heightmap...", "import-heightmap", "Import a heightmap image into the terrain");
	auto* change_tileset = link(ribbon->change_tileset, "Change Tileset...", "change-tileset", "Change the tileset of the map");
	auto* change_tile_pathing = link(ribbon->change_tile_pathing, "Change Tile Pathing...", "change-tile-pathing", "Change the pathing of a tile type");
	auto* description = link(ribbon->map_description, "Description...", "description", "Edit the map name, author and description");
	auto* loading_screen = link(ribbon->map_loading_screen, "Loading Screen...", "loading-screen", "Edit the loading screen of the map");
	auto* options = link(ribbon->map_options, "Options...", "map-options", "Edit the map options");
	auto* gameplay_constants = link(ribbon->gameplay_constants, "Gameplay Constants...", "gameplay-constants", "Edit the gameplay constants");
	auto* scenario = link(ribbon->scenario_properties, "Players / Forces...", "players-forces");
	auto* item_tables = link(ribbon->item_tables, "Item Tables...", "item-tables");
	auto* config = link(ribbon->config, "Shortcuts and Config...", "config", "Open the shortcut and behavior reference");
	auto* switch_warcraft = link(ribbon->switch_warcraft, "Change Game Folder...", "switch-warcraft", "Select another Warcraft III installation");

	// Menu bar
	auto* menu_bar = menuBar();
	menu_bar->setNativeMenuBar(false);

	auto* file_menu = menu_bar->addMenu("File");
	file_menu->addAction(new_map);
	file_menu->addAction(open_folder);
	file_menu->addAction(open_mpq_action);
	recent_maps_menu = file_menu->addMenu("Recent Maps");
	flat_icon_actions.emplace_back(recent_maps_menu->menuAction(), "recent");
	file_menu->addSeparator();
	file_menu->addAction(save);
	file_menu->addAction(save_as);
	file_menu->addAction(export_mpq);
	file_menu->addSeparator();
	file_menu->addAction(test_map);
	file_menu->addSeparator();
	file_menu->addAction(settings);
	file_menu->addSeparator();
	file_menu->addAction(exit);

	auto* edit_menu = menu_bar->addMenu("Edit");
	edit_menu->addAction(undo);
	edit_menu->addAction(redo);

	auto* view_menu = menu_bar->addMenu("View");
	for (auto* action : { units, doodads, pathing, brush, lighting, water, click_helpers, wireframe, debug, minimap_action }) {
		view_menu->addAction(action);
	}
	view_menu->addSeparator();
	view_menu->addAction(reset_camera);
	view_menu->addSeparator();
	auto* palettes_menu = view_menu->addMenu("Palettes");
	palettes_menu->addAction(hint(command("Terrain", "terrain", "Show or hide the terrain sidebar. Shortcut: T", &HiveWE::toggle_terrain_sidebar), "T"));
	palettes_menu->addAction(hint(command("Doodads", "doodads", "Show or hide the doodad sidebar. Shortcut: D", &HiveWE::toggle_doodad_palette), "D"));
	palettes_menu->addAction(hint(command("Units", "units", "Show or hide the unit sidebar. Shortcut: U", &HiveWE::toggle_unit_palette), "U"));
	palettes_menu->addAction(hint(command("Pathing", "pathing", "Show or hide the pathing sidebar. Shortcut: P", &HiveWE::toggle_pathing_sidebar), "P"));
	palettes_menu->addAction(hint(command("Regions", "regions", "Show or hide the region sidebar. Shortcut: R", &HiveWE::toggle_region_palette), "R"));

	auto* terrain_menu = menu_bar->addMenu("Terrain");
	terrain_menu->addAction(import_heightmap);
	terrain_menu->addAction(change_tileset);
	terrain_menu->addAction(change_tile_pathing);

	auto* map_menu = menu_bar->addMenu("Map");
	for (auto* action : { description, loading_screen, options, gameplay_constants, scenario }) {
		map_menu->addAction(action);
	}
	map_menu->addSeparator();
	map_menu->addAction(item_tables);

	auto* tools_menu = menu_bar->addMenu("Tools");
	for (auto* action : { trigger_editor, object_editor, model_editor, asset_manager, tooltip_editor }) {
		tools_menu->addAction(action);
	}
	tools_menu->addSeparator();
	auto* object_data_menu = tools_menu->addMenu("Object Data");
	auto* export_objects_menu = object_data_menu->addMenu("Export Objects");
	flat_icon_actions.emplace_back(export_objects_menu->menuAction(), "export-objects");
	export_objects_menu->addAction("Binary package", this, &HiveWE::export_object_data_binary);
	export_objects_menu->addAction("Text package", this, &HiveWE::export_object_data_text);
	auto* import_objects_menu = object_data_menu->addMenu("Import Objects");
	flat_icon_actions.emplace_back(import_objects_menu->menuAction(), "import-objects");
	import_objects_menu->addAction("Binary package", this, &HiveWE::import_object_data_binary);
	import_objects_menu->addAction("Map folder", this, &HiveWE::import_object_data_map_folder);
	import_objects_menu->addAction("Text package", this, &HiveWE::import_object_data_text);
	tools_menu->addAction(map_protector);
	tools_menu->addSeparator();
	tools_menu->addAction(switch_warcraft);
	tools_menu->addAction(config);

	// Toolbar: File, History, Visibility, Camera, Editors, search field.
	flat_toolbar = new QToolBar("Main toolbar", this);
	flat_toolbar->setObjectName("mainToolbar");
	flat_toolbar->setMovable(false);
	flat_toolbar->setFloatable(false);
	flat_toolbar->setIconSize({ 20, 20 });
	flat_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
	flat_toolbar->setContextMenuPolicy(Qt::PreventContextMenu);

	open_dropdown_menu = new QMenu(this);

	flat_toolbar->addAction(new_map);
	auto* open_button = new QToolButton(flat_toolbar);
	open_button->setDefaultAction(open_folder);
	open_button->setMenu(open_dropdown_menu);
	open_button->setPopupMode(QToolButton::MenuButtonPopup);
	open_button->setToolButtonStyle(Qt::ToolButtonIconOnly);
	open_button->setIconSize({ 20, 20 });
	flat_toolbar->addWidget(open_button);
	flat_toolbar->addAction(save);
	flat_toolbar->addAction(test_map);
	flat_toolbar->addSeparator();
	flat_toolbar->addAction(undo);
	flat_toolbar->addAction(redo);
	flat_toolbar->addSeparator();
	for (auto* action : { units, doodads, pathing, lighting, water, wireframe, minimap_action }) {
		flat_toolbar->addAction(action);
	}
	flat_toolbar->addSeparator();
	flat_toolbar->addAction(reset_camera);
	flat_toolbar->addSeparator();
	for (auto* action : { trigger_editor, object_editor, model_editor, asset_manager }) {
		flat_toolbar->addAction(action);
	}

	auto* spacer = new QWidget(flat_toolbar);
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	flat_toolbar->addWidget(spacer);

	auto* search = command("Search actions   Shift Shift", "search", "Open global search. Shortcut: Double Shift",
						   [this]() { ui->ribbon->quick_search->click(); });
	auto* search_button = new QToolButton(flat_toolbar);
	search_button->setObjectName("toolbarSearch");
	search_button->setDefaultAction(search);
	search_button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	search_button->setIconSize({ 16, 16 });
	flat_toolbar->addWidget(search_button);

	// Buttons must not take keyboard focus: a focused button would make the WidgetShortcut display hints live.
	for (auto* button : flat_toolbar->findChildren<QToolButton*>()) {
		button->setFocusPolicy(Qt::NoFocus);
	}

	addToolBar(Qt::TopToolBarArea, flat_toolbar);
}

void HiveWE::update_flat_recent_menus(const QStringList& recent_maps) {
	if (!recent_maps_menu || !open_dropdown_menu) {
		return;
	}

	recent_maps_menu->clear();
	open_dropdown_menu->clear();
	open_dropdown_menu->addAction(open_mpq_action);

	bool any = false;
	for (int i = 0; i < recent_maps.size(); ++i) {
		const fs::path directory = recent_maps[i].toStdString();
		if (!fs::exists(directory / "war3map.w3i")) {
			continue;
		}

		if (!any) {
			open_dropdown_menu->addSeparator();
		}
		any = true;

		// The menu entries use the position in the stored list, which open_recent_map() also uses.
		const QString title = QFileInfo(recent_maps[i]).fileName();
		for (auto* menu : { recent_maps_menu, open_dropdown_menu }) {
			auto* action = menu->addAction(title, this, [this, i]() { open_recent_map(i); });
			action->setToolTip(recent_maps[i]);
			action->setStatusTip(recent_maps[i]);
		}
	}

	if (!any) {
		recent_maps_menu->addAction("No recent maps")->setEnabled(false);
	}
}

void HiveWE::refresh_flat_icons() {
	for (const auto& [action, name] : flat_icon_actions) {
		action->setIcon(theme::icon(name));
	}
}

namespace {
/// A dock like ads::CDockAreaWidget in widgets.css: a 24px title bar holding the title widgets, then the body.
QFrame* make_dock(QWidget* parent, const QString& object_name, const QList<QWidget*>& title_widgets, QWidget* body) {
	auto* dock = new QFrame(parent);
	dock->setObjectName(object_name);
	dock->setProperty("dockFrame", true);
	auto* layout = new QVBoxLayout(dock);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	auto* bar = new QWidget(dock);
	bar->setObjectName("dockTitleBar");
	bar->setProperty("dockTitleBar", true);
	bar->setFixedHeight(24);
	auto* bar_layout = new QHBoxLayout(bar);
	bar_layout->setContentsMargins(0, 0, 0, 0);
	bar_layout->setSpacing(0);
	for (auto* widget : title_widgets) {
		bar_layout->addWidget(widget);
	}
	bar_layout->addStretch(1);

	layout->addWidget(bar);
	layout->addWidget(body, 1);
	return dock;
}

QLabel* dock_title(QWidget* parent, const QString& text) {
	auto* label = new QLabel(text, parent);
	label->setProperty("sidebarTitle", true);
	label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	return label;
}
}

QString HiveWE::sidebar_flat_title_for_view(const SidebarPaletteView view) const {
	switch (view) {
		case SidebarPaletteView::terrain:
			return "Terrain Palette";
		case SidebarPaletteView::doodad:
			return "Doodad Palette";
		case SidebarPaletteView::unit:
			return "Unit Palette";
		case SidebarPaletteView::pathing:
			return "Pathing Palette";
		case SidebarPaletteView::region:
			return "Region Palette";
		case SidebarPaletteView::none:
			return "";
	}
	return "";
}

void HiveWE::set_sidebar_shown(const bool shown) {
	palette_panel_visible = shown;
	if (flat_layout_active) {
		if (flat_palette_dock) {
			flat_palette_dock->setVisible(shown);
		}
	} else if (sidebar_root) {
		sidebar_root->setVisible(shown);
	}
}

void HiveWE::on_tool_page_activated(QRibbonTab* tab) {
	// Same hand-over as set_current_custom_tab() does for the ribbon: the other palettes switch off their brush state.
	if (current_flat_tab != tab) {
		if (current_flat_tab != nullptr) {
			emit palette_changed(tab);
		}
		current_flat_tab = tab;
	}
}

void HiveWE::apply_flat_layout(const bool flat) {
	if (!sidebar_root || flat == flat_layout_active) {
		return;
	}
	flat_layout_active = flat;

	auto* legacy_layout = qobject_cast<QVBoxLayout*>(sidebar_root->layout());
	auto* header_layout = qobject_cast<QVBoxLayout*>(sidebar_header->layout());
	auto* body_layout = qobject_cast<QVBoxLayout*>(sidebar_body->layout());
	auto* mode_grid = qobject_cast<QGridLayout*>(sidebar_mode_bar->layout());

	// Leave whichever parent layout holds them; the addWidget calls below reparent.
	sidebar_title->setParent(nullptr);
	sidebar_body->setParent(nullptr);
	sidebar_minimap_frame->setParent(nullptr);
	sidebar_preview_frame->setParent(nullptr);
	sidebar_mode_bar->setParent(nullptr);
	sidebar_hint->setParent(nullptr);
	sidebar_header->setParent(nullptr);

	if (flat) {
		if (!flat_left) {
			flat_left = new QWidget(centralWidget());
			flat_left->setObjectName("flatLeftColumn");
			flat_left->setFixedWidth(300);
			flat_right = new QWidget(centralWidget());
			flat_right->setObjectName("flatRightColumn");
			flat_right->setFixedWidth(300);

			// Left: mode buttons, Minimap dock, palette dock (its title is the sidebar title label).
			auto* left_layout = new QVBoxLayout(flat_left);
			left_layout->setContentsMargins(0, 0, 0, 0);
			left_layout->setSpacing(4);
			flat_minimap_dock = make_dock(flat_left, "minimapDock", { dock_title(flat_left, "Minimap") }, sidebar_minimap_frame);
			flat_palette_dock = make_dock(flat_left, "paletteDock", { sidebar_title }, sidebar_body);
			left_layout->addWidget(sidebar_mode_bar);
			// Minimap: title bar plus the fixed-height minimap frame, like the mockup. The palette dock takes all the remaining height.
			flat_minimap_dock->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
			flat_minimap_dock->setFixedHeight(24 + 196 + 2);
			left_layout->addWidget(flat_minimap_dock);
			left_layout->addWidget(flat_palette_dock, 1);
			left_layout->addStretch(0);

			// Right: Properties | Regions, Preview. Properties is the read-only selection list, Regions has no backing data yet: honest placeholder.
			auto* right_layout = new QVBoxLayout(flat_right);
			right_layout->setContentsMargins(0, 0, 0, 0);
			right_layout->setSpacing(4);

			auto* pages = new QStackedWidget(flat_right);
			const auto placeholder = [pages](const QString& text) {
				auto* label = new QLabel(text, pages);
				label->setProperty("sidebarHint", true);
				label->setWordWrap(true);
				label->setAlignment(Qt::AlignCenter);
				pages->addWidget(label);
			};
			selection_panel = new SelectionPropertiesPanel(pages);
			pages->addWidget(selection_panel);
			placeholder("The regions of the map will be listed here.");

			QList<QWidget*> tabs;
			int page_index = 0;
			for (const QString& title : { QString("Properties"), QString("Regions") }) {
				auto* tab = new QToolButton(flat_right);
				tab->setText(title);
				tab->setCheckable(true);
				tab->setAutoExclusive(true);
				tab->setChecked(page_index == 0);
				tab->setProperty("dockTab", true);
				tab->setFocusPolicy(Qt::NoFocus);
				connect(tab, &QToolButton::clicked, pages, [pages, page_index]() { pages->setCurrentIndex(page_index); });
				tabs.push_back(tab);
				++page_index;
			}
			flat_properties_dock = make_dock(flat_right, "propertiesDock", tabs, pages);
			flat_preview_dock = make_dock(flat_right, "previewDock", { dock_title(flat_right, "Preview") }, sidebar_preview_frame);
			flat_preview_dock->setFixedHeight(240);
			right_layout->addWidget(flat_properties_dock, 1);
			right_layout->addWidget(flat_preview_dock);
		} else {
			// Back from the old layout: the widgets move into the docks again.
			qobject_cast<QVBoxLayout*>(flat_left->layout())->insertWidget(0, sidebar_mode_bar);
			flat_minimap_dock->layout()->addWidget(sidebar_minimap_frame);
			flat_palette_dock->layout()->addWidget(sidebar_body);
			flat_preview_dock->layout()->addWidget(sidebar_preview_frame);
			qobject_cast<QHBoxLayout*>(flat_palette_dock->findChild<QWidget*>("dockTitleBar")->layout())->insertWidget(0, sidebar_title);
		}

		content_layout->removeWidget(sidebar_root);
		sidebar_root->hide();
		content_layout->insertWidget(0, flat_left);
		content_layout->addWidget(flat_right);
		flat_left->show();
		flat_right->show();
		content_layout->setContentsMargins(4, 4, 4, 4);
		content_layout->setSpacing(4);

		sidebar_description->hide();
		sidebar_hint->hide();
		mode_grid->setSpacing(4);
		body_layout->setContentsMargins(8, 8, 8, 8);
		flat_minimap_dock->setVisible(ui->ribbon->minimap_visible->isChecked());
		flat_palette_dock->setVisible(palette_panel_visible);
	} else {
		// Old layout: everything back into the single right sidebar.
		content_layout->removeWidget(flat_left);
		content_layout->removeWidget(flat_right);
		flat_left->hide();
		flat_right->hide();

		header_layout->insertWidget(0, sidebar_title);
		sidebar_description->show();
		sidebar_hint->show();
		mode_grid->setSpacing(8);
		body_layout->setContentsMargins(14, 14, 14, 14);
		legacy_layout->addWidget(sidebar_mode_bar);
		legacy_layout->addWidget(sidebar_minimap_frame);
		legacy_layout->addWidget(sidebar_preview_frame);
		legacy_layout->addWidget(sidebar_header);
		legacy_layout->addWidget(sidebar_body, 1);
		legacy_layout->addWidget(sidebar_hint);
		content_layout->addWidget(sidebar_root);
		content_layout->setContentsMargins(0, 0, 0, 0);
		content_layout->setSpacing(0);
		sidebar_minimap_frame->setVisible(ui->ribbon->minimap_visible->isChecked());
		sidebar_root->setVisible(palette_panel_visible);
	}

	update_active_palette_visuals();
}

void HiveWE::apply_sidebar_style(const bool flat) {
	if (!sidebar_root) {
		return;
	}

	sidebar_root->setStyleSheet(flat ? QString() : sidebar_legacy_stylesheet);
	if (auto* layout = sidebar_root->layout()) {
		layout->setContentsMargins(12, 12, 12, 12);
		layout->setSpacing(12);
	}

	struct ModeIcon {
		QToolButton* button;
		const char* flat_icon;
		const char* legacy_icon;
	};
	for (const auto& mode : { ModeIcon { terrain_mode_button, "terrain", "data/icons/ribbon/heightmap.png" },
							  ModeIcon { doodad_mode_button, "doodads", "data/icons/ribbon/doodads.png" },
							  ModeIcon { unit_mode_button, "units", "data/icons/ribbon/units.png" },
							  ModeIcon { pathing_mode_button, "pathing", "data/icons/ribbon/pathing.png" },
							  ModeIcon { region_mode_button, "regions", "data/icons/ribbon/select.png" } }) {
		if (mode.button) {
			mode.button->setIcon(flat ? theme::icon(mode.flat_icon) : QIcon(mode.legacy_icon));
			mode.button->setIconSize(flat ? QSize(16, 16) : QSize(24, 24));
		}
	}
}

void HiveWE::setup_flat_status_bar() {
	auto* bar = statusBar();
	bar->setSizeGripEnabled(false);

	const auto segment = [bar](QLabel*& label) {
		label = new QLabel(bar);
		label->setProperty("statusSegment", true);
		bar->addPermanentWidget(label);
	};
	segment(status_cursor);
	segment(status_tileset);
	segment(status_map);
	segment(status_brush);
	segment(status_unsaved);

	// Hover tips from the toolbar and menu actions replace the message; "Ready" comes back when they clear.
	bar->showMessage("Ready");
	connect(bar, &QStatusBar::messageChanged, this, [bar](const QString& message) {
		if (message.isEmpty()) {
			bar->showMessage("Ready");
		}
	});

	status_timer.setParent(this);
	status_timer.setInterval(250);
	connect(&status_timer, &QTimer::timeout, this, &HiveWE::update_status_bar);
	// Drags, rotations, nudges, undo/redo and the Unit Properties dialog change the selected objects without a selection_changed signal.
	// The panel rebuilds from the brush's current selection and only touches its rows when a value differs.
	connect(&status_timer, &QTimer::timeout, this, [this]() {
		if (selection_panel && selection_panel->isVisible()) {
			selection_panel->refresh();
		}
	});
	status_timer.start();
}

void HiveWE::update_status_bar() {
	if (!status_cursor || !statusBar()->isVisible()) {
		return;
	}

	if (!map || !map->loaded) {
		for (auto* label : { status_cursor, status_tileset, status_map, status_brush, status_unsaved }) {
			label->clear();
		}
		return;
	}

	// Warcraft coordinates, like the debug overlay.
	const glm::vec3 mouse = input_handler.mouse_world;
	status_cursor->setText(QString("Cursor %1, %2")
							   .arg(mouse.x * 128.f + map->terrain.offset.x, 0, 'f', 1)
							   .arg(mouse.y * 128.f + map->terrain.offset.y, 0, 'f', 1));
	status_tileset->setText("Tileset " + QString::fromStdString(world_edit_data.data<std::string>("TileSets", std::string(1, map->terrain.tileset))));
	status_map->setText(QString("Map %1 x %2").arg(map->terrain.width - 1).arg(map->terrain.height - 1));

	QString brush_text = "No brush";
	if (map->brush) {
		const glm::ivec2 size = map->brush->get_size();
		QString name = "Brush";
		switch (current_sidebar_view) {
			case SidebarPaletteView::terrain:
				name = "Terrain brush";
				break;
			case SidebarPaletteView::pathing:
				name = "Pathing brush";
				break;
			case SidebarPaletteView::doodad:
				name = "Doodad brush";
				break;
			case SidebarPaletteView::unit:
				name = "Unit brush";
				break;
			case SidebarPaletteView::region:
				name = "Region brush";
				break;
			case SidebarPaletteView::none:
				break;
		}
		brush_text = QString("%1 %2x%3").arg(name).arg(size.x).arg(size.y);
	}
	status_brush->setText(brush_text);

	const bool unsaved = map->filesystem_path.empty() || map->world_undo.has_changes_since_save();
	status_unsaved->setText(unsaved ? "Unsaved changes" : "Saved");
	if (status_unsaved->property("statusWarn").toBool() != unsaved) {
		status_unsaved->setProperty("statusWarn", unsaved);
		status_unsaved->style()->unpolish(status_unsaved);
		status_unsaved->style()->polish(status_unsaved);
	}
}

void HiveWE::update_ribbon_visibility() {
	// The tool pages of the palettes sit inside the palettes in a Flat theme: the ribbon is never visible there.
	ui->ribbon->setVisible(!theme::current_is_flat());
}

void HiveWE::apply_theme_chrome() {
	const bool flat = theme::current_is_flat();

	// Palettes are built for one layout (tool page inside, tabs): switching themes rebuilds them.
	if (flat != flat_layout_active) {
		reset_palettes();
	}

	menuBar()->setVisible(flat);
	if (flat_toolbar) {
		flat_toolbar->setVisible(flat);
	}
	if (flat && !status_cursor) {
		setup_flat_status_bar();
	}
	if (status_cursor) {
		statusBar()->setVisible(flat);
	}

	ui->ribbon->set_palette_strip_mode(flat);
	if (flat && !flat_layout_active) {
		// The mockup shows the Minimap dock, so it starts visible the first time a Flat theme is used. After that the user's setting is kept.
		QSettings settings;
		if (!settings.contains("flatMinimapDefault")) {
			ui->ribbon->minimap_visible->setChecked(true);
			if (!qa_offscreen) {
				settings.setValue("flatMinimapDefault", true);
			}
		}
	}
	apply_flat_layout(flat);
	apply_sidebar_style(flat);
	refresh_flat_icons();
	update_ribbon_visibility();
}
