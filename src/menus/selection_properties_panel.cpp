#include "selection_properties_panel.h"

#include <QFont>
#include <QHeaderView>

#include "unit_brush.h"
#include "doodad_brush.h"
#include "selection_hit_points.h"

#include <QEvent>

import std;
import Globals;
import MapGlobal;
import Map;
import Units;
import Doodad;
import TableModel;

namespace {
QString number(const float value) {
	return QString::number(value, 'f', 1);
}

QString id_and_name(const std::string& id, const QString& name) {
	const QString id_text = QString::fromStdString(id);
	return name.isEmpty() ? id_text : QString("%1 (%2)").arg(id_text, name);
}

/// "Player 1 (Red)": the colour name is the WESTRING_UNITCOLOR_xx string the Unit Palette's Owner combo uses.
QString owner_text(const int player) {
	if (player == 24) {
		return "Neutral Hostile";
	}
	if (player == 27) {
		return "Neutral Passive";
	}
	QString text = QString("Player %1").arg(player + 1);
	if (player >= 0 && player < 24) {
		const std::string key = std::format("WESTRING_UNITCOLOR_{:02}", player);
		// The game string holds the colour name in quotes ("Red").
		const QString color = QString::fromStdString(std::string { world_edit_strings.data("WorldEditStrings", key) }).remove('"');
		if (!color.isEmpty()) {
			text += QString(" (%1)").arg(color);
		}
	}
	return text;
}

QString unit_name(const Unit& unit) {
	if (unit.id == "sloc") {
		return "Player Start Location";
	}
	if (units_slk.row_headers.contains(unit.id)) {
		return units_table->data(unit.id, "name").toString();
	}
	if (items_slk.row_headers.contains(unit.id)) {
		return items_table->data(unit.id, "name").toString();
	}
	return {};
}

QString doodad_name(const Doodad& doodad) {
	if (doodads_slk.row_headers.contains(doodad.id)) {
		return doodads_table->data(doodad.id, "name").toString();
	}
	if (destructibles_slk.row_headers.contains(doodad.id)) {
		return destructibles_table->data(doodad.id, "name").toString();
	}
	return {};
}

template <typename T>
bool in_vector(const std::vector<T>& items, const T* item) {
	return !items.empty() && !std::less<const T*>()(item, items.data()) && std::less<const T*>()(item, items.data() + items.size());
}

/// The selection line: the first object's name and how many more are selected.
QString selection_text(const QString& name, const std::size_t count) {
	return count > 1 ? QString("%1 (+%2 more)").arg(name).arg(count - 1) : name;
}
}

SelectionPropertiesPanel::SelectionPropertiesPanel(QWidget* parent) : QTreeWidget(parent) {
	setObjectName("selectionProperties");
	setColumnCount(2);
	setHeaderLabels({ "Property", "Value" });
	setRootIsDecorated(false);
	setAlternatingRowColors(true);
	setSelectionMode(QAbstractItemView::NoSelection);
	setFocusPolicy(Qt::NoFocus);
	setEditTriggers(QAbstractItemView::NoEditTriggers);
	setUniformRowHeights(true);
	setIndentation(0);
	header()->setStretchLastSection(true);
	header()->setSectionsMovable(false);
	header()->setSectionResizeMode(0, QHeaderView::Interactive);
	setColumnWidth(0, 120);

	clear_selection();
}

void SelectionPropertiesPanel::show_sections(const std::vector<Section>& new_sections) {
	// refresh() runs on a timer: leave the tree alone (no flicker, scroll position kept) unless a row changed.
	if (new_sections == sections) {
		return;
	}
	sections = new_sections;
	rebuild();
}

void SelectionPropertiesPanel::changeEvent(QEvent* event) {
	QTreeWidget::changeEvent(event);
	if (event->type() == QEvent::PaletteChange) {
		rebuild();
	}
}

void SelectionPropertiesPanel::rebuild() {
	clear();

	for (const Section& section : sections) {
		auto* heading = new QTreeWidgetItem(this, { section.title });
		QFont font = heading->font(0);
		font.setBold(true);
		heading->setFont(0, font);
		// A section row is a label, not a value: a band in the window colour with the muted text colour. Both come from the theme palette
		// (color-window and color-text-muted in Flat.tokens), so Flat Dark and Flat Light each get their own.
		for (int column = 0; column < 2; ++column) {
			heading->setBackground(column, palette().color(QPalette::Window));
			heading->setForeground(column, palette().color(QPalette::PlaceholderText));
		}
		heading->setFirstColumnSpanned(true);
		heading->setFlags(Qt::ItemIsEnabled);
		for (const Row& row : section.rows) {
			auto* item = new QTreeWidgetItem(heading, { row.property, row.value });
			item->setFlags(Qt::ItemIsEnabled);
			item->setToolTip(1, row.value);
		}
		heading->setExpanded(true);
	}
}

void SelectionPropertiesPanel::clear_selection() {
	unit_source = nullptr;
	doodad_source = nullptr;
	show_sections({ { "Selection", { { "Selected", "Nothing selected" } } } });
}

void SelectionPropertiesPanel::refresh() {
	if (unit_source) {
		show_selection(*unit_source);
	} else if (doodad_source) {
		show_selection(*doodad_source);
	}
}

void SelectionPropertiesPanel::show_selection(const UnitBrush& brush) {
	// The selection holds raw pointers into map->units.units: an undo that shrinks the vector does not always clear them.
	if (map == nullptr || brush.selections.empty() || !in_vector(map->units.units, *brush.selections.begin())) {
		clear_selection();
		return;
	}
	unit_source = &brush;
	doodad_source = nullptr;

	const Unit& unit = **brush.selections.begin();
	const QString name = unit_name(unit);
	const bool is_item = items_slk.row_headers.contains(unit.id);
	const bool is_start = unit.id == "sloc";

	std::vector<Section> result;
	Section main { is_item ? "Item" : "Unit", {} };
	main.rows.push_back({ "Selected", selection_text(name.isEmpty() ? QString::fromStdString(unit.id) : name, brush.selections.size()) });
	main.rows.push_back({ "Type", id_and_name(unit.id, name) });
	main.rows.push_back({ "Owner", owner_text(unit.player) });
	main.rows.push_back({ "Facing", number(glm::degrees(unit.angle)) });
	if (!is_item && !is_start) {
		const int max_hp = units_slk.row_headers.contains(unit.id) ? static_cast<int>(units_slk.data<float>("hp", unit.id)) : 0;
		main.rows.push_back({ "Hit points", QString::fromStdString(selection_hit_points_text(unit.health, max_hp)) });
	}
	result.push_back(std::move(main));

	// Warcraft coordinates, as the saved file and the status bar use them (HiveWE units are tiles of 128).
	result.push_back({ "Position",
						 { { "X", number(unit.position.x * 128.f + map->terrain.offset.x) },
						   { "Y", number(unit.position.y * 128.f + map->terrain.offset.y) },
						   { "Z", number(unit.position.z * 128.f) } } });

	Section custom { "Custom", {} };
	custom.rows.push_back({ "Variation", QString::number(unit.variation) });
	custom.rows.push_back({ "Scale", QString("%1, %2, %3").arg(number(unit.scale.x)).arg(number(unit.scale.y)).arg(number(unit.scale.z)) });
	if (!is_item && !is_start) {
		if (unit.mana >= 0) {
			custom.rows.push_back({ "Mana", QString::number(unit.mana) });
		}
		// Hero ids start with an upper case letter.
		if (!unit.id.empty() && std::isupper(static_cast<unsigned char>(unit.id[0]))) {
			custom.rows.push_back({ "Hero level", QString::number(unit.level) });
		}
	}
	result.push_back(std::move(custom));

	show_sections(result);
}

void SelectionPropertiesPanel::show_selection(const DoodadBrush& brush) {
	if (map == nullptr || brush.selections.empty() || !in_vector(map->doodads.doodads, *brush.selections.begin())) {
		clear_selection();
		return;
	}
	doodad_source = &brush;
	unit_source = nullptr;

	const Doodad& doodad = **brush.selections.begin();
	const QString name = doodad_name(doodad);
	const bool is_doodad = doodads_slk.row_headers.contains(doodad.id);

	std::vector<Section> result;
	Section main { is_doodad ? "Doodad" : "Destructible", {} };
	main.rows.push_back({ "Selected", selection_text(name.isEmpty() ? QString::fromStdString(doodad.id) : name, brush.selections.size()) });
	main.rows.push_back({ "Type", id_and_name(doodad.id, name) });
	// Doodads have no owner.
	main.rows.push_back({ "Owner", "-" });
	main.rows.push_back({ "Facing", number(glm::degrees(doodad.angle)) });
	main.rows.push_back({ "Life", QString("%1%").arg(doodad.life) });
	result.push_back(std::move(main));

	const glm::vec3 position = doodad.final_position(map->terrain);
	result.push_back({ "Position",
						 { { "X", number(position.x * 128.f + map->terrain.offset.x) },
						   { "Y", number(position.y * 128.f + map->terrain.offset.y) },
						   { "Z", number(position.z * 128.f) } } });

	result.push_back({ "Custom",
					   { { "Variation", QString::number(doodad.variation) },
						 { "Scale", QString("%1, %2, %3").arg(number(doodad.scale.x)).arg(number(doodad.scale.y)).arg(number(doodad.scale.z)) },
						 { "Z mode", doodad.fixed_z ? "Fixed height" : "On terrain" } } });

	show_sections(result);
}
