#pragma once

#include <filesystem>

namespace fs = std::filesystem;

#include <QMainWindow>
#include <QFileDialog>
#include <QSettings>
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QGridLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QKeyEvent>
#include <QStackedWidget>
#include <QToolButton>
#include <QToolBar>
#include <QAction>
#include <QMenu>
#include <vector>
#include <utility>
#include <QString>
#include <memory>

#include "global_search.h"

import QRibbon;
import WindowHandler;
import <glm/glm.hpp>;
import <glm/gtc/matrix_transform.hpp>;
import <glm/gtc/quaternion.hpp>;
import "palette.h";
import "minimap.h";

namespace Ui {
class HiveWEClass;
}

namespace mdx {
class MDX;
}

class TerrainPalette;
class PathingPalette;
class DoodadPalette;
class UnitPalette;
class RegionPalette;
class SelectionPropertiesPanel;
class ModelEditorGLWidget;

class HiveWE : public QMainWindow {
	Q_OBJECT

public:
	explicit HiveWE(QWidget* parent = nullptr);
	~HiveWE();

	/// Set by --qa-offscreen before the window exists: show it outside all monitors, never activate it, never save its position.
	static inline bool qa_offscreen = false;
	/// Set by --window-size: size of the --qa-offscreen window.
	static inline QSize qa_window_size = QSize(1600, 1000);
	/// Set by --palette: the palette (terrain, doodads, units, pathing, regions) opened after the map has loaded, through the mode button's slot.
	static inline QString qa_palette;
	/// Set by --qa-select-first: with --palette units or doodads, select the first unit or doodad of the map.
	static inline bool qa_select_first = false;
	void open_qa_palette();
	/// Set by --qa-open-dialog (needs --qa-offscreen): the dialog or tool window opened after the map has loaded, so it can be captured without clicks.
	static inline QString qa_open_dialog;
	/// Set by --qa-dialog-tab: tab index selected in the first tab widget of the --qa-open-dialog window.
	static inline int qa_dialog_tab = -1;
	/// Set by --qa-model-editor-script (needs --qa-offscreen and --qa-open-dialog model-editor): steps run against the Model Editor, see ModelEditor::run_qa_script.
	static inline QString qa_model_editor_script;
	/// Set by --qa-script-out: the file that receives the result lines of that script.
	static inline QString qa_script_out;
	/// Set by --qa-model: the model the model-editor hook opens (default units/human/footman/footman.mdx).
	static inline QString qa_model = "units/human/footman/footman.mdx";
	/// Opens qa_open_dialog with sample data. Never saves: nothing here accepts a dialog.
	void open_qa_dialog();
	/// Set by --qa-change-tileset <out folder> (needs --qa-offscreen, --qa-tile-texture and --qa-script-out): runs Change Tileset on the loaded
	/// map without clicks, saves to the out folder, reloads it and writes PASS/FAIL lines to qa_script_out. Exit code 0 only when all pass.
	static inline QString qa_change_tileset;
	/// Set by --qa-tile-texture: the BLP or TGA imported as a custom tile texture by that hook.
	static inline QString qa_tile_texture;
	void run_qa_change_tileset();
	/// Set by --qa-doodad-undo-check (needs --qa-offscreen and --palette doodads; --qa-script-out <file> optional): adds three doodads in memory, then
	/// checks delete, undo, redo, undo and cut with undo through DoodadBrush and writes PASS/FAIL lines. Never saves. Exit code 0 only when all pass.
	static inline bool qa_doodad_undo_check = false;
	void run_qa_doodad_undo_check();
	/// Set by --qa-dump-layout (needs --qa-offscreen): file that dump_qa_layout() writes.
	static inline QString qa_dump_layout_file;
	/// Writes the widget tree of the open palette (class, objectName, size hints, size policy, geometry) to qa_dump_layout_file, so a too wide
	/// minimum width can be traced to a widget.
	void dump_qa_layout() const;

	void load_map(const fs::path& directory);

	void load_folder();
	void load_mpq();
	void save();
	void save_as();
	void export_mpq();
	void play_test();

private:
	enum class SidebarPaletteView {
		none,
		terrain,
		doodad,
		unit,
		pathing,
		region,
	};

	std::unique_ptr<Ui::HiveWEClass> ui;
	QRibbonTab* current_custom_tab = nullptr;
	Minimap* minimap = new Minimap(this);

	QElapsedTimer double_shift_timer;

	void keyPressEvent(QKeyEvent* event) override {
		if (event->key() == Qt::Key_Shift && !event->isAutoRepeat()) {
			if (double_shift_timer.isValid() && double_shift_timer.elapsed() < 400) {

				GlobalSearchWidget search_widget = new GlobalSearchWidget(this);
				double_shift_timer.invalidate();
			} else {
				double_shift_timer.start();
			}
		}
		QMainWindow::keyPressEvent(event);
	}

	void closeEvent(QCloseEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void moveEvent(QMoveEvent* event) override;

	void switch_warcraft();
	void import_heightmap();
	void export_object_data_binary();
	void export_object_data_text();
	void import_object_data_binary();
	void import_object_data_map_folder();
	void import_object_data_text();
	void create_new_map();
	void save_window_state();
	void restore_window_state();
	void add_recent_map(const fs::path& directory);
	void update_recent_maps_menu();
	void open_recent_map(int index);
	void show_transient_notice(const QString& text, int timeout_ms = 1800);
	void position_transient_notice();
	void setup_palette_sidebar();
	/// flat_chrome.cpp: menu bar and compact toolbar of the Flat themes. Every action forwards to the matching ribbon button, so the slots
	/// and shortcuts stay the ones wired in the constructor.
	void setup_flat_chrome();
	/// Shows the menu bar and toolbar (Flat themes) or the ribbon (old themes), restyles the sidebar and refreshes the icon colours.
	void apply_theme_chrome();
	void apply_sidebar_style(bool flat);
	/// Flat themes: the ribbon is only shown while a palette has its tool page in it. Old themes: always shown.
	void update_ribbon_visibility();
	void refresh_flat_icons();
	void update_flat_recent_menus(const QStringList& recent_maps);
	/// Flat themes: left column (mode buttons, Minimap, palette), viewport, right column (Properties, Preview). Old themes: one right sidebar.
	void apply_flat_layout(bool flat);
	void setup_flat_status_bar();
	void update_status_bar();
	/// Shows or hides the palette panel (the whole sidebar in the old layout, the palette dock in the Flat layout).
	void set_sidebar_shown(bool shown);
	bool sidebar_shown() const { return palette_panel_visible; }
	void on_tool_page_activated(QRibbonTab* tab);
	/// Deletes the palettes so they are rebuilt for the layout of the active theme.
	void reset_palettes();
	QString sidebar_flat_title_for_view(SidebarPaletteView view) const;
	QAction* link_ribbon_action(QAbstractButton* source, const QString& text, const QString& icon_name, const QString& tooltip = {});
	void update_active_palette_visuals();
	void hide_palette_sidebar();
	void show_palette_view(SidebarPaletteView view, bool show_feedback = true);
	void ensure_palette_view(SidebarPaletteView view);
	void reset_map_session();
	void update_map_context(const QString& status, const QString& detail_override = {});
	bool save_current_map();
	bool save_current_map_as();
	bool confirm_map_replacement(const QString& action);
	static QString sanitized_folder_name(const QString& map_name);
	Palette* palette_for_view(SidebarPaletteView view) const;
	QWidget* host_for_view(SidebarPaletteView view) const;
	QString sidebar_title_for_view(SidebarPaletteView view) const;
	QString sidebar_description_for_view(SidebarPaletteView view) const;
	QString sidebar_hint_for_view(SidebarPaletteView view) const;
	QString sidebar_feedback_label(SidebarPaletteView view) const;
	void activate_palette(Palette* palette, bool show_feedback = true);
	void toggle_terrain_sidebar();
	void toggle_pathing_sidebar();
	void toggle_doodad_palette();
	void toggle_unit_palette();
	void toggle_region_palette();
	void update_palette_model_preview(std::shared_ptr<mdx::MDX> model, const QString& title);
	void update_doodad_model_preview(const QString& id, int variation, const QString& title);
	void update_unit_model_preview(const QString& id, const QString& title);

	/// Adds the tab to the ribbon and sets the current index to this tab
	void set_current_custom_tab(QRibbonTab* tab, QString name);
	void remove_custom_tab();

	QLabel* transient_notice = nullptr;
	QTimer transient_notice_timer;
	QWidget* sidebar_root = nullptr;
	QHBoxLayout* content_layout = nullptr;
	QFrame* sidebar_body = nullptr;
	bool palette_panel_visible = false;
	bool flat_layout_active = false;
	QRibbonTab* current_flat_tab = nullptr;
	QWidget* flat_left = nullptr;
	QWidget* flat_right = nullptr;
	QFrame* flat_minimap_dock = nullptr;
	QFrame* flat_palette_dock = nullptr;
	QFrame* flat_properties_dock = nullptr;
	/// Read-only selection list inside flat_properties_dock. Owned by the Qt tree; null until the Flat layout has been built.
	SelectionPropertiesPanel* selection_panel = nullptr;
	QFrame* flat_preview_dock = nullptr;
	QTimer status_timer;
	QLabel* status_cursor = nullptr;
	QLabel* status_tileset = nullptr;
	QLabel* status_map = nullptr;
	QLabel* status_brush = nullptr;
	QLabel* status_unsaved = nullptr;
	QString sidebar_legacy_stylesheet;
	QToolBar* flat_toolbar = nullptr;
	QMenu* recent_maps_menu = nullptr;
	QMenu* open_dropdown_menu = nullptr;
	QAction* open_mpq_action = nullptr;
	std::vector<std::pair<QAction*, QString>> flat_icon_actions;
	QWidget* sidebar_header = nullptr;
	QWidget* sidebar_mode_bar = nullptr;
	QLabel* sidebar_title = nullptr;
	QLabel* sidebar_description = nullptr;
	QLabel* sidebar_hint = nullptr;
	QFrame* sidebar_minimap_frame = nullptr;
	QFrame* sidebar_preview_frame = nullptr;
	QLabel* sidebar_preview_title = nullptr;
	QLabel* sidebar_preview_placeholder = nullptr;
	ModelEditorGLWidget* sidebar_preview = nullptr;
	QStackedWidget* sidebar_stack = nullptr;
	QToolButton* terrain_mode_button = nullptr;
	QToolButton* doodad_mode_button = nullptr;
	QToolButton* unit_mode_button = nullptr;
	QToolButton* pathing_mode_button = nullptr;
	QToolButton* region_mode_button = nullptr;
	QFrame* doodad_host = nullptr;
	QFrame* unit_host = nullptr;
	QFrame* terrain_host = nullptr;
	QFrame* pathing_host = nullptr;
	QFrame* region_host = nullptr;
	SidebarPaletteView current_sidebar_view = SidebarPaletteView::none;
	TerrainPalette* terrain_palette = nullptr;
	PathingPalette* pathing_palette = nullptr;
	DoodadPalette* doodad_palette = nullptr;
	UnitPalette* unit_palette = nullptr;
	RegionPalette* region_palette = nullptr;

signals:
	void tileset_changed();
	void palette_changed(QRibbonTab* tab);

	void saving_initiated();
};
