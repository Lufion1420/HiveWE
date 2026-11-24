#include "terrain_palette.h"
#include "theme.h"
#include "terrain_operators.h"
#include "pathing_brush.h"

#include <QButtonGroup>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

import MapGlobal;
import Globals;

import std;
import SLK;
import Texture;
import OpenGLUtilities;
import ResourceManager;

TerrainPalette::TerrainPalette(QWidget* parent) : Palette(parent) {
	ui.setupUi(this);
	setObjectName("terrainPalette");
	ui.verticalLayout->setAlignment(Qt::AlignTop);
	ui.verticalLayout->setContentsMargins(0, 0, 0, 0);
	ui.verticalLayout->setSpacing(10);
	ui.verticalLayout->removeItem(ui.horizontalLayout_2);
	ui.brushSize1->hide();
	ui.brushSize3->hide();
	ui.brushSize5->hide();
	ui.brushSize7->hide();
	ui.brushSize9->hide();
	ui.brushSize11->hide();
	ui.textureCheckbox->setText("Texture Paint");
	ui.cliffCheckbox->setText("Cliff Tools");
	ui.deformationCheckbox->setText("Height Tools");
	ui.cellCheckbox->setText("Water / Boundaries");
	ui.label_2->setText("Brush Size");
	ui.label->setText("Brush Shape");
	setStyleSheet(theme::current_is_flat() ? QString() : QString(
		"QCheckBox { font-size: 14px; font-weight: 700; color: rgb(232, 236, 241); spacing: 8px; padding-top: 4px; }"
		"QLabel { color: rgb(212, 219, 227); }"
		"QLabel#label, QLabel#label_2 { font-size: 11px; font-weight: 600; color: rgb(140, 149, 162); padding-top: 2px; }"
		"QPushButton {"
		"background: rgba(24, 27, 31, 205);"
		"border: 1px solid rgba(255, 255, 255, 16);"
		"border-radius: 10px;"
		"}"
		"QPushButton:hover { background: rgba(34, 38, 44, 220); }"
		"QPushButton:checked { background: rgba(95, 164, 255, 200); border: 1px solid rgba(95, 164, 255, 235); }"
		"QSpinBox {"
		"background: rgba(24, 27, 31, 235);"
		"border: 1px solid rgba(255, 255, 255, 18);"
		"border-radius: 10px;"
		"padding: 6px 10px;"
		"color: rgb(232, 236, 241);"
		"}"
		"QSlider::groove:horizontal { height: 6px; background: rgba(255, 255, 255, 20); border-radius: 999px; }"
		"QSlider::handle:horizontal { width: 16px; margin: -5px 0; background: rgb(95, 164, 255); border-radius: 8px; }"
	));
	monitor_activation();

	brush.texture_operator.tile_id = map->terrain.tileset_ids.front();

	change_mode_parent = new QShortcut(Qt::Key_Space, window(), nullptr, nullptr, Qt::ShortcutContext::WindowShortcut);

	ui.flowLayout_placeholder->addLayout(textures_layout);
	ui.flowLayout_placeholder_2->addLayout(cliff_layout);

	if (theme::current_is_flat()) {
		hidden_store = new QWidget(this);
		hidden_store->hide();
	}

	refresh();

	// ribbon
	create_ribbon();

	// setup operators
	setup_texture_operator();
	setup_cliff_operator();
	setup_deformation_operator();
	setup_cell_operator();

	// brush menu
	setup_brush_menu();
	if (hidden_store) {
		build_flat_layout();
	}
	sync_brush_controls(&brush);
	monitor_activation();
}

TerrainPalette::~TerrainPalette() {
	map->brush = nullptr;
	delete change_mode_parent;
	delete ribbon_tab;
}

void TerrainPalette::refresh() {
	create_terrain_buttons();
	if (map->terrain.tile_list_changed) {
		brush.clear_clipboard();
	}
	if (tileset_name) {
		tileset_name->setText(QString::fromStdString(world_edit_data.data<std::string>("TileSets", std::string(1, map->terrain.tileset))));
	}
	if (std::ranges::find(map->terrain.tileset_ids, brush.texture_operator.tile_id) == map->terrain.tileset_ids.end()) {
		brush.texture_operator.tile_id = map->terrain.tileset_ids.front();
	}
}

void TerrainPalette::activate_palette() {
	change_mode_parent->setEnabled(true);
	map->brush = &brush;
	sync_brush_controls(&brush);
	if (theme::current_is_flat()) {
		attach_tool_page(ribbon_tab);
	} else {
		emit ribbon_tab_requested(ribbon_tab, "Terrain Palette");
	}
}

void TerrainPalette::sync_brush_controls(Brush* active_brush) {
	Brush* target = active_brush ? active_brush : (map && map->brush ? map->brush : &brush);
	const auto size = target->get_size();
	const bool use_odd_steps = dynamic_cast<TerrainBrush*>(target) || dynamic_cast<PathingBrush*>(target);

	const QSignalBlocker slider_blocker(ui.brushSizeSlider);
	const QSignalBlocker spinbox_blocker(ui.brushSize);
	const QSignalBlocker size_1_blocker(ui.brushSize1);
	const QSignalBlocker size_3_blocker(ui.brushSize3);
	const QSignalBlocker size_5_blocker(ui.brushSize5);
	const QSignalBlocker size_7_blocker(ui.brushSize7);
	const QSignalBlocker size_9_blocker(ui.brushSize9);
	const QSignalBlocker size_11_blocker(ui.brushSize11);
	ui.brushSizeSlider->setSingleStep(use_odd_steps ? 4 : 1);
	ui.brushSize->setSingleStep(use_odd_steps ? 4 : 1);
	ui.brushSizeSlider->setValue(size.x);
	ui.brushSize->setValue(size.x);

	for (auto* button : size_buttons) {
		const QSignalBlocker blocker(button);
		button->setChecked(button->property("brushSize").toInt() == size.x);
	}
}

void TerrainPalette::deactivate(QRibbonTab* tab) {
	if (tab != ribbon_tab) {
		brush.clear_selection();
		change_mode_parent->setEnabled(false);
	}
}

void TerrainPalette::update_operator_gui() {
	update_texture_operator_gui();
	update_cliff_operator_gui();
	update_deformation_operator_gui();
	update_cell_operator_gui();
}

void TerrainPalette::update_cell_operator_gui() {
	const bool is_enabled = brush.cell_operator.is_enabled();

	// update checkbox state (disable events for the change)
	{
		QSignalBlocker blocker(ui.cellCheckbox);
		ui.cellCheckbox->setChecked(is_enabled);
	}
	// update cell operation buttons
	const auto operation = brush.cell_operator.get_operation_type();
	{
		QSignalBlocker blocker(ui.addWater);
		ui.addWater->setChecked(is_enabled && operation == CellOperator::cell_operation::add_water);
	}
	{
		QSignalBlocker blocker(ui.removeWater);
		ui.removeWater->setChecked(is_enabled && operation == CellOperator::cell_operation::remove_water);
	}
	{
		QSignalBlocker blocker(ui.addBoundary);
		ui.addBoundary->setChecked(is_enabled && operation == CellOperator::cell_operation::add_boundary);
	}
	{
		QSignalBlocker blocker(ui.removeBoundary);
		ui.removeBoundary->setChecked(is_enabled && operation == CellOperator::cell_operation::remove_boundary);
	}
}

void TerrainPalette::update_deformation_operator_gui() {
	const bool is_enabled = brush.height_operator.is_enabled();

	// update checkbox state (disable events for the change)
	{
		QSignalBlocker blocker(ui.deformationCheckbox);
		ui.deformationCheckbox->setChecked(is_enabled);
	}
	// update height change operation buttons
	const auto deformation = brush.height_operator.deformation_type;
	{
		QSignalBlocker blocker(ui.terrainRaise);
		ui.terrainRaise->setChecked(is_enabled && deformation == HeightOperator::deformation::raise);
	}
	{
		QSignalBlocker blocker(ui.terrainLower);
		ui.terrainLower->setChecked(is_enabled && deformation == HeightOperator::deformation::lower);
	}
	{
		QSignalBlocker blocker(ui.terrainPlateau);
		ui.terrainPlateau->setChecked(is_enabled && deformation == HeightOperator::deformation::plateau);
	}
	{
		QSignalBlocker blocker(ui.terrainRipple);
		ui.terrainRipple->setChecked(is_enabled && deformation == HeightOperator::deformation::ripple);
	}
	{
		QSignalBlocker blocker(ui.terrainSmooth);
		ui.terrainSmooth->setChecked(is_enabled && deformation == HeightOperator::deformation::smooth);
	}
}

void TerrainPalette::update_cliff_operator_gui() {
	const bool is_enabled = brush.cliff_operator.is_enabled();

	// update checkbox state (disable events for the change)
	{
		QSignalBlocker blocker(ui.cliffCheckbox);
		ui.cliffCheckbox->setChecked(is_enabled);
	}
	// update which cliff button is selected
	const int cliff_id = brush.cliff_operator.cliff_id;
	for (auto* button : cliff_group->buttons()) {
		QSignalBlocker blocker(button);
		if (button->property("cliffID").toInt() == cliff_id) {
			button->setChecked(is_enabled);
		} else {
			button->setChecked(false);
		}
	}

	// update which operation is selected (i.e. ramp, level, deep water...)
	const auto operation = brush.cliff_operator.cliff_operation_type;
	{
		QSignalBlocker blocker(ui.cliffLower2);
		ui.cliffLower2->setChecked(is_enabled && operation == CliffOperator::cliff_operation::lower2);
	}
	{
		QSignalBlocker blocker(ui.cliffLower1);
		ui.cliffLower1->setChecked(is_enabled && operation == CliffOperator::cliff_operation::lower1);
	}
	{
		QSignalBlocker blocker(ui.cliffLevel);
		ui.cliffLevel->setChecked(is_enabled && operation == CliffOperator::cliff_operation::level);
	}
	{
		QSignalBlocker blocker(ui.cliffRaise1);
		ui.cliffRaise1->setChecked(is_enabled && operation == CliffOperator::cliff_operation::raise1);
	}
	{
		QSignalBlocker blocker(ui.cliffRaise2);
		ui.cliffRaise2->setChecked(is_enabled && operation == CliffOperator::cliff_operation::raise2);
	}
	{
		QSignalBlocker blocker(ui.cliffDeepWater);
		ui.cliffDeepWater->setChecked(is_enabled && operation == CliffOperator::cliff_operation::deep_water);
	}
	{
		QSignalBlocker blocker(ui.cliffShallowWater);
		ui.cliffShallowWater->setChecked(is_enabled && operation == CliffOperator::cliff_operation::shallow_water);
	}
	{
		QSignalBlocker blocker(ui.cliffRamp);
		ui.cliffRamp->setChecked(is_enabled && operation == CliffOperator::cliff_operation::ramp);
	}
}

void TerrainPalette::update_texture_operator_gui() {
	const bool is_enabled = brush.texture_operator.is_enabled();

	// update checkbox state (disable events for the change)
	{
		QSignalBlocker blocker(ui.textureCheckbox);
		ui.textureCheckbox->setChecked(is_enabled);
	}
	// update which texture button is selected
	const QString tile_id = QString::fromStdString(brush.texture_operator.tile_id);
	for (auto* button : textures_group->buttons()) {
		QSignalBlocker blocker(button);
		if (button->property("tileID").toString() == tile_id) {
			button->setChecked(is_enabled);
		} else {
			button->setChecked(false);
		}
	}

	if (tile_tree) {
		const QSignalBlocker blocker(tile_tree);
		tile_tree->clearSelection();
		for (int i = 0; i < tile_tree->topLevelItemCount(); ++i) {
			auto* item = tile_tree->topLevelItem(i);
			if (is_enabled && item->text(1) == tile_id) {
				item->setSelected(true);
				tile_tree->setCurrentItem(item);
			}
		}
	}
}

void TerrainPalette::setup_texture_operator() {
	// activate/deactivate texture operator when the checkbox is clicked
	connect(ui.textureCheckbox, &QCheckBox::clicked, [&](bool checked) {
		if (checked) {
			brush.activate_operator(brush.texture_operator);
		} else {
			brush.deactivate_operator(brush.texture_operator);
		}

		update_operator_gui();
	});

	// when a texture icon is clicked on, we have to enable TextureOperator
	// and then select the proper ground texture
	connect(textures_group, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked), [&](QAbstractButton* button) {
		// set the correct texture (blight included)
		brush.texture_operator.tile_id = button->property("tileID").toString().toStdString();

		// enable the operator
		// note that TerrainBrush will automatically deactivate all incompatible operators
		brush.activate_operator(brush.texture_operator);

		update_operator_gui();
	});
}

void TerrainPalette::setup_cliff_operator() {
	// allow the GUI to deselect all cliff operations
	ui.cliffButtonGroup->setExclusive(false);

	// activate/deactivate cliff operator when the checkbox is clicked
	connect(ui.cliffCheckbox, &QCheckBox::clicked, [&](bool checked) {
		if (checked) {
			brush.activate_operator(brush.cliff_operator);
		} else {
			brush.deactivate_operator(brush.cliff_operator);
		}

		update_operator_gui();
	});

	// cliff texture buttons
	connect(cliff_group, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked), [&](QAbstractButton* button) {
		// update operator value and activate it
		// note that terrain brush will automatically deactivate all incompatible operators
		brush.cliff_operator.cliff_id = button->property("cliffID").toInt();
		brush.activate_operator(brush.cliff_operator);
		update_operator_gui();
	});

	// specific cliff operations
	connect(ui.cliffLower2, &QPushButton::clicked, [&]() {
		brush.cliff_operator.cliff_operation_type = CliffOperator::cliff_operation::lower2;
		brush.activate_operator(brush.cliff_operator);
		update_operator_gui();
	});

	connect(ui.cliffLower1, &QPushButton::clicked, [&]() {
		brush.cliff_operator.cliff_operation_type = CliffOperator::cliff_operation::lower1;
		brush.activate_operator(brush.cliff_operator);
		update_operator_gui();
	});

	connect(ui.cliffLevel, &QPushButton::clicked, [&]() {
		brush.cliff_operator.cliff_operation_type = CliffOperator::cliff_operation::level;
		brush.activate_operator(brush.cliff_operator);
		update_operator_gui();
	});

	connect(ui.cliffRaise1, &QPushButton::clicked, [&]() {
		brush.cliff_operator.cliff_operation_type = CliffOperator::cliff_operation::raise1;
		brush.activate_operator(brush.cliff_operator);
		update_operator_gui();
	});

	connect(ui.cliffRaise2, &QPushButton::clicked, [&]() {
		brush.cliff_operator.cliff_operation_type = CliffOperator::cliff_operation::raise2;
		brush.activate_operator(brush.cliff_operator);
		update_operator_gui();
	});

	connect(ui.cliffDeepWater, &QPushButton::clicked, [&]() {
		brush.cliff_operator.cliff_operation_type = CliffOperator::cliff_operation::deep_water;
		brush.activate_operator(brush.cliff_operator);
		update_operator_gui();
	});

	connect(ui.cliffShallowWater, &QPushButton::clicked, [&]() {
		brush.cliff_operator.cliff_operation_type = CliffOperator::cliff_operation::shallow_water;
		brush.activate_operator(brush.cliff_operator);
		update_operator_gui();
	});

	connect(ui.cliffRamp, &QPushButton::clicked, [&]() {
		brush.cliff_operator.cliff_operation_type = CliffOperator::cliff_operation::ramp;
		brush.activate_operator(brush.cliff_operator);
		update_operator_gui();
	});
}

void TerrainPalette::setup_cell_operator() {
	// allow the GUI to deselect all cell operations
	ui.cellButtonGroup->setExclusive(false);

	// setup operator
	connect(ui.cellCheckbox, &QCheckBox::clicked, [&](bool checked) {
		if (checked) {
			brush.activate_operator(brush.cell_operator);
		} else {
			brush.deactivate_operator(brush.cell_operator);
		}

		update_operator_gui();
	});

	// cell operation choice
	connect(ui.addWater, &QPushButton::clicked, [&]() {
		brush.cell_operator.set_operation_type(CellOperator::cell_operation::add_water);
		brush.activate_operator(brush.cell_operator);
		update_operator_gui();
	});
	connect(ui.removeWater, &QPushButton::clicked, [&]() {
		brush.cell_operator.set_operation_type(CellOperator::cell_operation::remove_water);
		brush.activate_operator(brush.cell_operator);
		update_operator_gui();
	});
	connect(ui.addBoundary, &QPushButton::clicked, [&]() {
		brush.cell_operator.set_operation_type(CellOperator::cell_operation::add_boundary);
		brush.activate_operator(brush.cell_operator);
		update_operator_gui();
	});
	connect(ui.removeBoundary, &QPushButton::clicked, [&]() {
		brush.cell_operator.set_operation_type(CellOperator::cell_operation::remove_boundary);
		brush.activate_operator(brush.cell_operator);
		update_operator_gui();
	});
}

void TerrainPalette::setup_deformation_operator() {
	// allow the GUI to deselect all deformation operations
	ui.deformationButtonGroup->setExclusive(false);

	// activate/deactivate height operator when the checkbox is clicked
	connect(ui.deformationCheckbox, &QCheckBox::clicked, [&](bool checked) {
		if (checked) {
			brush.activate_operator(brush.height_operator);
		} else {
			brush.deactivate_operator(brush.height_operator);
		}

		update_operator_gui();
	});

	// height change operations - set deformation type first, then activate
	connect(ui.terrainRaise, &QPushButton::clicked, [&]() {
		brush.height_operator.deformation_type = HeightOperator::deformation::raise;
		brush.activate_operator(brush.height_operator);
		update_operator_gui();
	});
	connect(ui.terrainLower, &QPushButton::clicked, [&]() {
		brush.height_operator.deformation_type = HeightOperator::deformation::lower;
		brush.activate_operator(brush.height_operator);
		update_operator_gui();
	});
	connect(ui.terrainPlateau, &QPushButton::clicked, [&]() {
		brush.height_operator.deformation_type = HeightOperator::deformation::plateau;
		brush.activate_operator(brush.height_operator);
		update_operator_gui();
	});
	connect(ui.terrainRipple, &QPushButton::clicked, [&]() {
		brush.height_operator.deformation_type = HeightOperator::deformation::ripple;
		brush.activate_operator(brush.height_operator);
		update_operator_gui();
	});
	connect(ui.terrainSmooth, &QPushButton::clicked, [&]() {
		brush.height_operator.deformation_type = HeightOperator::deformation::smooth;
		brush.activate_operator(brush.height_operator);
		update_operator_gui();
	});
}

void TerrainPalette::create_ribbon() {
	QRibbonSection* selection_section = new QRibbonSection;
	selection_section->setText("Selection");

	selection_mode->setText("Selection\nMode");
	selection_mode->setIcon(QIcon("data/icons/ribbon/select.png"));
	selection_mode->setCheckable(true);
	selection_section->addWidget(selection_mode);

	QRibbonSection* general_section = new QRibbonSection;
	general_section->setText("General");

	QRibbonButton* enforce_water_height_limit = new QRibbonButton;
	enforce_water_height_limit->setText("Enforce Water\nHeight Limit");
	enforce_water_height_limit->setIcon(QIcon("data/icons/ribbon/variation.png"));
	enforce_water_height_limit->setCheckable(true);
	enforce_water_height_limit->setChecked(true);
	general_section->addWidget(enforce_water_height_limit);

	QRibbonButton* change_doodad_heights = new QRibbonButton;
	change_doodad_heights->setText("Update\nDoodad Z");
	change_doodad_heights->setIcon(QIcon("data/icons/ribbon/changeheight.png"));
	change_doodad_heights->setCheckable(true);
	change_doodad_heights->setChecked(true);
	general_section->addWidget(change_doodad_heights);

	QRibbonSection* cliff_section = new QRibbonSection;
	cliff_section->setText("Cliff");

	QRibbonButton* relative_cliff_heights = new QRibbonButton;
	relative_cliff_heights->setText("Relative\nHeight");
	relative_cliff_heights->setIcon(QIcon("data/icons/ribbon/changeheight.png"));
	relative_cliff_heights->setCheckable(true);
	relative_cliff_heights->setChecked(false);
	relative_cliff_heights->setEnabled(false);
	cliff_section->addWidget(relative_cliff_heights);

	QRibbonSection* pathing_section = new QRibbonSection;
	pathing_section->setText("Pathing");

	QRibbonButton* apply_cliff_pathing = new QRibbonButton;
	apply_cliff_pathing->setText("Cliff\nPathing");
	apply_cliff_pathing->setIcon(QIcon("data/icons/ribbon/rock.png"));
	apply_cliff_pathing->setCheckable(true);
	apply_cliff_pathing->setChecked(true);
	pathing_section->addWidget(apply_cliff_pathing);

	QRibbonButton* apply_tile_pathing = new QRibbonButton;
	apply_tile_pathing->setText("Tile\nPathing");
	apply_tile_pathing->setIcon(QIcon("data/icons/ribbon/tileset.png"));
	apply_tile_pathing->setCheckable(true);
	apply_tile_pathing->setChecked(true);
	pathing_section->addWidget(apply_tile_pathing);

	QRibbonButton* apply_water_pathing = new QRibbonButton;
	apply_water_pathing->setText("Water\nPathing");
	apply_water_pathing->setIcon(QIcon("data/icons/ribbon/water.png"));
	apply_water_pathing->setCheckable(true);
	apply_water_pathing->setChecked(true);
	pathing_section->addWidget(apply_water_pathing);

	QRibbonSection* deformation_section = new QRibbonSection;
	deformation_section->setText("Deformation");

	QRibbonButton* deform_ground = new QRibbonButton;
	deform_ground->setText("Ground");
	deform_ground->setIcon(QIcon("data/icons/ribbon/heightmap.png"));
	deform_ground->setCheckable(true);
	deform_ground->setChecked(brush.deform_ground);
	deformation_section->addWidget(deform_ground);

	QRibbonButton* deform_water = new QRibbonButton;
	deform_water->setText("Water");
	deform_water->setIcon(QIcon("data/icons/ribbon/water.png"));
	deform_water->setCheckable(true);
	deform_water->setChecked(brush.deform_water);
	deformation_section->addWidget(deform_water);

	ribbon_tab->addSection(selection_section);
	ribbon_tab->addSection(general_section);
	ribbon_tab->addSection(cliff_section);
	ribbon_tab->addSection(pathing_section);
	ribbon_tab->addSection(deformation_section);

	connect(selection_mode, &QRibbonButton::toggled, [&]() {
		brush.switch_mode();
	});
	connect(change_mode_parent, &QShortcut::activated, selection_mode, &QToolButton::click);

	connect(enforce_water_height_limit, &QRibbonButton::toggled, [&](bool checked) {
		brush.enforce_water_height_limits = checked;
	});
	connect(change_doodad_heights, &QRibbonButton::toggled, [&](bool checked) {
		brush.change_doodad_heights = checked;
	});
	connect(relative_cliff_heights, &QRibbonButton::toggled, [&](bool checked) {
		brush.relative_cliff_heights = checked;
	});

	connect(enforce_water_height_limit, &QRibbonButton::toggled, [&](bool checked) {
		brush.enforce_water_height_limits = checked;
	});

	connect(apply_cliff_pathing, &QRibbonButton::toggled, [&](bool checked) {
		brush.apply_cliff_pathing = checked;
	});
	connect(apply_tile_pathing, &QRibbonButton::toggled, [&](bool checked) {
		brush.apply_tile_pathing = checked;
	});
	connect(apply_water_pathing, &QRibbonButton::toggled, [&](bool checked) {
		brush.apply_water_pathing = checked;
	});

	connect(deform_ground, &QRibbonButton::toggled, [&](bool checked) {
		brush.deform_ground = checked;
	});
	connect(deform_water, &QRibbonButton::toggled, [&](bool checked) {
		brush.deform_water = checked;
	});
}

void TerrainPalette::setup_brush_menu() {
	connect(ui.brushSizeSlider, &QSlider::valueChanged, [&](int value) {
		Brush* target = map && map->brush ? map->brush : &brush;
		target->set_size(glm::ivec2(value));
		ui.brushSize->setValue(value);
	});
	connect(&brush, &Brush::size_changed, this, [&](glm::ivec2 size) {
		sync_brush_controls(&brush);
	});

	connect(ui.deformationCheckbox, &QCheckBox::clicked, [&](bool checked) {
		if (checked) {
			brush.activate_operator(brush.height_operator);
		} else {
			brush.deactivate_operator(brush.height_operator);
		}
	});

	connect(ui.brushShapeCircle, &QPushButton::clicked, [&]() {
		Brush* target = map && map->brush ? map->brush : &brush;
		target->set_shape(Brush::Shape::circle);
	});
	connect(ui.brushShapeSquare, &QPushButton::clicked, [&]() {
		Brush* target = map && map->brush ? map->brush : &brush;
		target->set_shape(Brush::Shape::square);
	});
	connect(ui.brushShapeDiamond, &QPushButton::clicked, [&]() {
		Brush* target = map && map->brush ? map->brush : &brush;
		target->set_shape(Brush::Shape::diamond);
	});
}

/// Idempotent
void TerrainPalette::create_terrain_buttons() {
	textures_layout->clear();
	cliff_layout->clear();
	if (hidden_store) {
		qDeleteAll(textures_group->buttons());
	}

	// allow button groups to have no buttons checked
	textures_group->setExclusive(false);
	cliff_group->setExclusive(false);

	// Ground Tiles
	const slk::SLK& slk = map->terrain.terrain_slk;
	for (const auto& i : map->terrain.tileset_ids) {
		const auto image = resource_manager.load<Texture>(slk.data("dir", i) + "/" + slk.data("file", i)).value();
		const auto icon = ground_texture_to_icon(image->data.data(), image->width, image->height);

		QPushButton* button =
			terrain_button(icon, "tileID", QString::fromStdString(i), QString::fromUtf8(slk.data<std::string_view>("comment", i)));

		if (hidden_store) {
			button->setParent(hidden_store);
		} else {
			textures_layout->addWidget(button);
		}
		textures_group->addButton(button);

		// check if we are dealing with a cliff tile - if so, add it to cliff operator
		auto& cliff_tiles = map->terrain.cliff_to_ground_texture;
		const auto is_cliff_tile = std::ranges::find(cliff_tiles, map->terrain.ground_texture_to_id[i]);
		if (is_cliff_tile != cliff_tiles.end()) {
			const int index = std::distance(cliff_tiles.begin(), is_cliff_tile);

			button = terrain_button(icon, "cliffID", QString::number(index), QString::fromUtf8(slk.data<std::string_view>("comment", i)));
			cliff_layout->addWidget(button);
			cliff_group->addButton(button);
		}
	}

	// add blight texture to the texture painter tool
	// this is a small divergence from vanilla WE
	const auto image = resource_manager.load<Texture>("TerrainArt/Blight/Ashen_Blight.dds").value();
	const auto icon = ground_texture_to_icon(image->data.data(), image->width, image->height);

	QPushButton* blight_button = terrain_button(icon, "tileID", "blight", "Blight");
	if (hidden_store) {
		blight_button->setParent(hidden_store);
	} else {
		textures_layout->addWidget(blight_button);
	}
	textures_group->addButton(blight_button);

	populate_tile_list();
}

void TerrainPalette::populate_tile_list() {
	if (!tile_tree) {
		return;
	}

	tile_tree->clear();
	const auto buttons = textures_group->buttons();
	for (int i = 0; i < buttons.size(); ++i) {
		const QString tile_id = buttons[i]->property("tileID").toString();
		const QString name = buttons[i]->property("tileName").toString();

		auto* item = new QTreeWidgetItem(tile_tree);
		item->setText(0, name.isEmpty() ? tile_id : name);
		item->setIcon(0, buttons[i]->icon());
		item->setText(1, tile_id);
		item->setData(0, Qt::UserRole, i);
	}

	if (tile_filter) {
		emit tile_filter->textChanged(tile_filter->text());
	}
	update_texture_operator_gui();
}

void TerrainPalette::build_flat_layout() {
	auto* root = ui.verticalLayout;
	root->setAlignment({});
	root->setSpacing(8);

	const auto make_page = [&](QCheckBox* enable, const QString& enable_text, std::initializer_list<QLayout*> layouts) {
		auto* page = new QWidget;
		auto* layout = new QVBoxLayout(page);
		layout->setContentsMargins(8, 8, 8, 8);
		layout->setSpacing(8);

		root->removeWidget(enable);
		enable->setText(enable_text);
		layout->addWidget(enable);
		for (auto* sub_layout : layouts) {
			root->removeItem(sub_layout);
			layout->addLayout(sub_layout);
		}
		layout->addStretch(1);
		return page;
	};

	// Texture: tileset, filter, tile list. The icon grid is replaced by the list; the list drives the same buttons.
	auto* texture_page = new QWidget;
	auto* texture_layout = new QVBoxLayout(texture_page);
	texture_layout->setContentsMargins(8, 8, 8, 8);
	texture_layout->setSpacing(8);

	root->removeWidget(ui.textureCheckbox);
	ui.textureCheckbox->setText("Paint textures");
	texture_layout->addWidget(ui.textureCheckbox);
	root->removeItem(ui.textureLayout);

	auto* tileset_row = new QHBoxLayout;
	tileset_row->setSpacing(8);
	auto* tileset_label = new QLabel("Tileset", texture_page);
	tileset_label->setProperty("mutedLabel", true);
	tileset_name = new QLabel(texture_page);
	tileset_name->setObjectName("tilesetName");
	tileset_name->setText(QString::fromStdString(world_edit_data.data<std::string>("TileSets", std::string(1, map->terrain.tileset))));
	tileset_name->setToolTip("Change the tileset with Terrain > Change Tileset");
	tileset_row->addWidget(tileset_label);
	tileset_row->addWidget(tileset_name, 1);
	texture_layout->addLayout(tileset_row);

	tile_filter = new QLineEdit(texture_page);
	tile_filter->setObjectName("tileFilter");
	tile_filter->setPlaceholderText("Filter tiles");
	tile_filter->setClearButtonEnabled(true);
	texture_layout->addWidget(tile_filter);

	tile_tree = new QTreeWidget(texture_page);
	tile_tree->setObjectName("tileList");
	tile_tree->setColumnCount(2);
	tile_tree->setHeaderLabels({ "Tile", "ID" });
	tile_tree->setRootIsDecorated(false);
	tile_tree->setAlternatingRowColors(true);
	tile_tree->setIconSize({ 14, 14 });
	tile_tree->setUniformRowHeights(true);
	tile_tree->setSelectionMode(QAbstractItemView::SingleSelection);
	tile_tree->header()->setStretchLastSection(false);
	tile_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	tile_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	tile_tree->setMinimumHeight(140);
	texture_layout->addWidget(tile_tree, 1);

	connect(tile_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item) {
		const auto buttons = textures_group->buttons();
		const int index = item->data(0, Qt::UserRole).toInt();
		if (index >= 0 && index < buttons.size()) {
			buttons[index]->click();
		}
	});
	connect(tile_filter, &QLineEdit::textChanged, this, [this](const QString& text) {
		for (int i = 0; i < tile_tree->topLevelItemCount(); ++i) {
			auto* item = tile_tree->topLevelItem(i);
			item->setHidden(!text.isEmpty() && !item->text(0).contains(text, Qt::CaseInsensitive) && !item->text(1).contains(text, Qt::CaseInsensitive));
		}
	});

	auto* cliff_page = make_page(ui.cliffCheckbox, "Cliff tools", { ui.cliffRaiseLayout, ui.cliffWaterLayout, ui.flowLayout_placeholder_2 });
	auto* deformation_page = make_page(ui.deformationCheckbox, "Height tools", { ui.deformationLayout });
	auto* cell_page = make_page(ui.cellCheckbox, "Water / boundaries", { ui.cellLayoutTop });
	// Rows of 5 fixed 48 px buttons are wider than the column: they wrap instead. Not used again after this, so the dangling ui pointers are safe.
	for (QBoxLayout* row : { static_cast<QBoxLayout*>(ui.cliffRaiseLayout), static_cast<QBoxLayout*>(ui.cliffWaterLayout),
							 static_cast<QBoxLayout*>(ui.deformationLayout), static_cast<QBoxLayout*>(ui.cellLayoutTop) }) {
		wrap_button_row(row);
	}

	auto* tabs = new QTabWidget(this);
	tabs->setObjectName("terrainTabs");
	tabs->setDocumentMode(true);
	tabs->addTab(texture_page, "Texture");
	tabs->addTab(cliff_page, "Cliff");
	tabs->addTab(deformation_page, "Deformation");
	tabs->addTab(cell_page, "Cell");
	root->insertWidget(0, tabs, 1);

	// Brush size: preset buttons plus the existing field and slider; brush shape: the existing buttons.
	root->removeWidget(ui.label_2);
	ui.label_2->hide();
	root->removeWidget(ui.label);
	ui.label->hide();

	auto* size_group = new QGroupBox("Brush Size", this);
	auto* size_layout = new QVBoxLayout(size_group);
	size_layout->setContentsMargins(8, 12, 8, 8);
	size_layout->setSpacing(6);
	auto* size_row = new QHBoxLayout;
	size_row->setSpacing(4);
	for (const int value : { 1, 3, 5, 7, 9 }) {
		auto* button = new QPushButton(QString::number(value), size_group);
		button->setCheckable(true);
		button->setProperty("brushSize", value);
		button->setToolTip(QString("Brush size %1").arg(value));
		connect(button, &QPushButton::clicked, this, [this, value]() {
			// Through the slider, whose handler sets the brush size and the spin box; then resync everything, including the preset
			// buttons and the case where the slider already showed this value.
			ui.brushSizeSlider->setValue(value);
			Brush* target = map && map->brush ? map->brush : &brush;
			target->set_size(glm::ivec2(value));
			sync_brush_controls(target);
		});
		size_row->addWidget(button, 1);
		size_buttons.push_back(button);
	}
	size_layout->addLayout(size_row);
	root->removeItem(ui.horizontalLayout_4);
	size_layout->addLayout(ui.horizontalLayout_4);

	auto* shape_group = new QGroupBox("Brush Shape", this);
	auto* shape_layout = new QVBoxLayout(shape_group);
	shape_layout->setContentsMargins(8, 12, 8, 8);
	root->removeItem(ui.brushShapeLayout);
	shape_layout->addLayout(ui.brushShapeLayout);

	root->addWidget(size_group);
	root->addWidget(shape_group);

	populate_tile_list();
}

QPushButton*
TerrainPalette::terrain_button(const QIcon& icon, const char* propertyName, const QVariant& propertyValue, const QString& tileName) {
	QPushButton* button = new QPushButton;
	button->setIcon(icon);
	button->setFixedSize(48, 48);
	button->setIconSize({48, 48});
	button->setCheckable(true);
	button->setProperty(propertyName, propertyValue);
	button->setProperty("tileName", tileName);
	button->setToolTip(tileName);
	return button;
}
