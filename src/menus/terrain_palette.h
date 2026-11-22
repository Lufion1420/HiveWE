#pragma once

#include "ui_terrain_palette.h"

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QTabWidget>
#include <QTreeWidget>

#include <vector>

#include "palette.h"
#include "terrain_brush.h"

import QRibbon;
import FlowLayout;

class TerrainPalette: public Palette {
	Q_OBJECT

  public:
	explicit TerrainPalette(QWidget* parent = nullptr);
	~TerrainPalette() override;

	void refresh();
	void activate_palette();
	void sync_brush_controls(Brush* active_brush = nullptr);
	TerrainBrush& current_brush() { return brush; }

  private:
	Ui::TerrainPalette ui;

	TerrainBrush brush;

	QButtonGroup* textures_group = new QButtonGroup;
	FlowLayout* textures_layout = new FlowLayout;

	QButtonGroup* cliff_group = new QButtonGroup;
	FlowLayout* cliff_layout = new FlowLayout;

	QRibbonTab* ribbon_tab = new QRibbonTab;
	QRibbonButton* selection_mode = new QRibbonButton;

	QShortcut* change_mode_parent;

	// Flat themes: tabs Texture / Cliff / Deformation / Cell, a tile list and brush size buttons instead of the stacked sections.
	QWidget* hidden_store = nullptr; // holds the texture icon buttons, which stay the backing of the tile list
	QTreeWidget* tile_tree = nullptr;
	QLineEdit* tile_filter = nullptr;
	QLabel* tileset_name = nullptr;
	std::vector<QPushButton*> size_buttons;
	void build_flat_layout();
	void populate_tile_list();

	// GUI creation functions
	void update_operator_gui();

	// textures
	void update_texture_operator_gui();
	void setup_texture_operator();
	void create_terrain_buttons();
	QPushButton* terrain_button(const QIcon& icon, const char* propertyName, const QVariant& propertyValue, const QString& tileName);

	// cliff
	void update_cliff_operator_gui();
	void setup_cliff_operator();

	// deformation
	void update_deformation_operator_gui();
	void setup_deformation_operator();

	// cell operator (boundaries, water, holes)
	void update_cell_operator_gui();
	void setup_cell_operator();

	// ribbon
	void create_ribbon();

	// brush menu
	void setup_brush_menu();

  public slots:
	void deactivate(QRibbonTab* tab) override;
};
