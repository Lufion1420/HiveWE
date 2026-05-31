module;
#include <QAbstractItemModel>
#include <QString>
export module ObjectEditorUndo;

import std;
import TableModel;
import WorldUndoManager;
import SLK;

// Undo/redo for pasting a single field value on the right-side inspector.
export class ObjectFieldSetAction final : public WorldCommand {
public:
	TableModel* table = nullptr;
	std::string id;
	std::string field;
	QString old_value;
	QString new_value;

	void undo(WorldEditContext&) override { apply(old_value); }
	void redo(WorldEditContext&) override { apply(new_value); }

private:
	void apply(const QString& value) const {
		if (!table->slk->row_headers.contains(id) || !table->slk->column_headers.contains(field)) {
			return;
		}
		const QModelIndex idx = table->index(
			static_cast<int>(table->slk->row_headers.at(id)),
			static_cast<int>(table->slk->column_headers.at(field)));
		table->setData(idx, value, Qt::EditRole);
	}
};

// Undo/redo for pasting (duplicating) an object entry on the left-side explorer.
export class ObjectEntryAddAction final : public WorldCommand {
public:
	TableModel* table = nullptr;
	std::string new_id;
	std::unordered_map<std::string, std::string> base_data;
	std::unordered_map<std::string, std::string> shadow_data;

	void undo(WorldEditContext&) override {
		if (!table->slk->row_headers.contains(new_id)) {
			return;
		}
		table->deleteRow(new_id);
	}

	void redo(WorldEditContext&) override {
		if (table->slk->row_headers.contains(new_id)) {
			return;
		}
		table->addRow([this]() {
			for (const auto& [k, v] : base_data) {
				table->slk->base_data[new_id][k] = v;
			}
			for (const auto& [k, v] : shadow_data) {
				table->slk->shadow_data[new_id][k] = v;
			}
			const size_t index = table->slk->row_headers.size();
			table->slk->row_headers.emplace(new_id, index);
			table->slk->index_to_row[index] = new_id;
		});
	}
};
