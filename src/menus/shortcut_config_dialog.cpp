#include "shortcut_config_dialog.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
struct ShortcutEntry {
	QString description;
	QString shortcut;
};

const std::array shortcut_entries = {
	ShortcutEntry {"Main window: Open global search.", "Double Shift"},
	ShortcutEntry {"Main window: Undo the last action.", "Ctrl+Z"},
	ShortcutEntry {"Main window: Redo the last undone action.", "Ctrl+Y"},
	ShortcutEntry {"Main window: Open a map folder.", "Ctrl+O"},
	ShortcutEntry {"Main window: Save the current map.", "Ctrl+S"},
	ShortcutEntry {"Main window: Save the current map under a new folder.", "Ctrl+Shift+S"},
	ShortcutEntry {"Main window: Reset the camera to its default view.", "Ctrl+Shift+C"},
	ShortcutEntry {"Main window: Open the terrain palette.", "T"},
	ShortcutEntry {"Main window: Open the doodad palette.", "D"},
	ShortcutEntry {"Main window: Open the unit palette.", "U"},
	ShortcutEntry {"Main window: Open the pathing palette.", "P"},
	ShortcutEntry {"Main window: Show or hide units.", "Ctrl+U"},
	ShortcutEntry {"Main window: Show or hide doodads.", "Ctrl+D"},
	ShortcutEntry {"Main window: Show or hide the pathing overlay.", "Ctrl+P"},
	ShortcutEntry {"Main window: Turn lighting on or off.", "Ctrl+L"},
	ShortcutEntry {"Main window: Show or hide water.", "Ctrl+W"},
	ShortcutEntry {"Main window: Show or hide click helper markers.", "Ctrl+I"},
	ShortcutEntry {"Main window: Show or hide wireframe rendering.", "Ctrl+T"},
	ShortcutEntry {"Main window: Show or hide debug rendering.", "F3"},
	ShortcutEntry {"Main window: Reload the active theme.", "F5"},
	ShortcutEntry {"Any active brush: Increase brush size.", "="},
	ShortcutEntry {"Any active brush: Decrease brush size.", "-"},
	ShortcutEntry {"Terrain brush: Increase or decrease brush size without hovering the slider.", "Shift + Mouse Wheel"},
	ShortcutEntry {"Brush selection: Clear the current selection.", "Esc"},
	ShortcutEntry {"Brush selection: Delete the current selection.", "Delete"},
	ShortcutEntry {"Brush selection: Cut the current selection.", "Ctrl+X"},
	ShortcutEntry {"Brush selection: Copy the current selection.", "Ctrl+C"},
	ShortcutEntry {"Brush selection: Enter paste mode for the clipboard selection.", "Ctrl+V"},
	ShortcutEntry {"Terrain palette: Toggle selection mode.", "Space"},
	ShortcutEntry {"Doodad palette: Focus the search field.", "Ctrl+F"},
	ShortcutEntry {"Doodad palette: Toggle selection mode.", "Space"},
	ShortcutEntry {"Unit palette: Focus the search field.", "Ctrl+F"},
	ShortcutEntry {"Unit palette: Toggle selection mode.", "Space"},
	ShortcutEntry {"Trigger editor: Open global search.", "Double Shift"},
	ShortcutEntry {"Trigger editor: Focus the search window.", "Ctrl+F"},
	ShortcutEntry {"Trigger editor: Generate script.", "Ctrl+F10"},
	ShortcutEntry {"Object editor: Open global search.", "Double Shift"},
	ShortcutEntry {"Object editor: Focus the active search field.", "Ctrl+F"},
	ShortcutEntry {"Doodad or unit selection: Select all placed objects of the active type.", "Ctrl+A"},
	ShortcutEntry {"Doodad or unit selection: Move selected objects in small steps with the numpad.", "Numpad 1-9"},
	ShortcutEntry {"Doodad selection: Raise absolute height.", "Ctrl+Page Up"},
	ShortcutEntry {"Doodad selection: Lower absolute height.", "Ctrl+Page Down"},
	ShortcutEntry {"Doodad selection: Increase Z scale.", "Page Up"},
	ShortcutEntry {"Doodad selection: Decrease Z scale.", "Page Down"},
};
}

ShortcutConfigDialog::ShortcutConfigDialog(QWidget* parent) : QDialog(parent) {
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowTitle("Config");
	resize(860, 640);

	auto* layout = new QVBoxLayout(this);

	auto* intro = new QLabel("Current shortcut and input reference.");
	layout->addWidget(intro);

	auto* table = new QTableWidget(static_cast<int>(shortcut_entries.size()), 2, this);
	table->setHorizontalHeaderLabels({"Description", "Shortcut"});
	table->verticalHeader()->setVisible(false);
	table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	table->setSelectionBehavior(QAbstractItemView::SelectRows);
	table->setSelectionMode(QAbstractItemView::SingleSelection);
	table->setWordWrap(true);
	table->setAlternatingRowColors(true);
	table->horizontalHeader()->setStretchLastSection(false);
	table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

	for (int row = 0; row < static_cast<int>(shortcut_entries.size()); ++row) {
		const auto& entry = shortcut_entries[row];
		table->setItem(row, 0, new QTableWidgetItem(entry.description));
		table->setItem(row, 1, new QTableWidgetItem(entry.shortcut));
	}

	layout->addWidget(table);

	auto* button_box = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::close);
	connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::close);
	layout->addWidget(button_box);

	show();
}
