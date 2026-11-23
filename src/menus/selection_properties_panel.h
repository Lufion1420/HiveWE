#pragma once

#include <QTreeWidget>

#include <vector>

class UnitBrush;
class DoodadBrush;

/// Read-only Property/Value list of the selected unit or doodad, shown in the Properties tab of the Flat layout. Sections: Unit (or Doodad),
/// Position, Custom. With several selected objects it shows the first one and a count.
///
/// It keeps a pointer to the brush it shows (not to the selected objects) and rebuilds from the brush's current selection set on every
/// show_selection() and refresh() call. The owner must call clear_selection() before that brush is deleted.
class SelectionPropertiesPanel : public QTreeWidget {
public:
	explicit SelectionPropertiesPanel(QWidget* parent = nullptr);

	/// Shows the first selected unit (or the empty state when nothing is selected).
	void show_selection(const UnitBrush& brush);
	void show_selection(const DoodadBrush& brush);
	void clear_selection();
	/// Rebuilds the rows from the brush shown last, so a moved, rotated or edited object is up to date. Does nothing visible when no row changed.
	void refresh();
	/// True if the rows currently shown come from this brush, so another brush's "nothing selected" does not wipe them.
	bool owned_by(const void* brush) const { return brush != nullptr && (brush == unit_source || brush == doodad_source); }

protected:
	/// A theme switch changes the palette the section rows take their colours from.
	void changeEvent(QEvent* event) override;

private:
	struct Row {
		QString property;
		QString value;
		bool operator==(const Row&) const = default;
	};
	struct Section {
		QString title;
		std::vector<Row> rows;
		bool operator==(const Section&) const = default;
	};

	void show_sections(const std::vector<Section>& new_sections);
	void rebuild();

	const UnitBrush* unit_source = nullptr;
	const DoodadBrush* doodad_source = nullptr;
	std::vector<Section> sections;
};
