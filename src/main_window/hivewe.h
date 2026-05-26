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

class TerrainPalette;
class PathingPalette;
class DoodadPalette;
class UnitPalette;
class RegionPalette;

class HiveWE : public QMainWindow {
	Q_OBJECT

public:
	explicit HiveWE(QWidget* parent = nullptr);
	~HiveWE();

	void load_map(const fs::path& directory);

	void load_folder();
	void load_mpq();
	void save();
	void save_as();
	void export_mpq();
	void play_test();

private:
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
	void save_window_state();
	void restore_window_state();
	void add_recent_map(const fs::path& directory);
	void update_recent_maps_menu();
	void open_recent_map(int index);
	void show_transient_notice(const QString& text, int timeout_ms = 1800);
	void position_transient_notice();
	void setup_palette_sidebar();
	void update_palette_sidebar_layout();
	void activate_palette(Palette* palette);
	void toggle_terrain_sidebar();
	void toggle_doodad_palette();
	void toggle_unit_palette();
	void toggle_region_palette();

	/// Adds the tab to the ribbon and sets the current index to this tab
	void set_current_custom_tab(QRibbonTab* tab, QString name);
	void remove_custom_tab();

	QLabel* transient_notice = nullptr;
	QTimer transient_notice_timer;
	QWidget* sidebar_root = nullptr;
	QWidget* object_column = nullptr;
	QWidget* terrain_column = nullptr;
	QFrame* doodad_host = nullptr;
	QFrame* unit_host = nullptr;
	QFrame* terrain_host = nullptr;
	QFrame* pathing_host = nullptr;
	QFrame* region_host = nullptr;
	QVBoxLayout* object_column_layout = nullptr;
	QVBoxLayout* terrain_column_layout = nullptr;
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
