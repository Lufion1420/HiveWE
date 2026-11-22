export module WorldUndoManager;

import std;
import PathingMap;
import Units;
import Doodads;
import Regions;
import Terrain;
import "brush/brush.h";

/// So we don't have to pass a global variable around
export struct WorldEditContext {
	Terrain& terrain;
	Units& units;
	Doodads& doodads;
	Regions& regions;
	Brush* brush;
	PathingMap& pathing_map;
};

/// Describes a change in the world. E.g., changing the terrain, adding a unit in the OE, placing some doodads.
export class WorldCommand {
  public:
	virtual void undo(WorldEditContext& ctx) = 0;
	virtual void redo(WorldEditContext& ctx) = 0;

	virtual ~WorldCommand() = default;
};

export class WorldUndoManager {
	std::vector<std::vector<std::unique_ptr<WorldCommand>>> undo_actions;
	std::vector<std::vector<std::unique_ptr<WorldCommand>>> redo_actions;
	/// Counts every change of the undo state; mark_saved() remembers the value. Only world edits are counted (not Object or Trigger Editor edits).
	std::uint64_t revision = 0;
	std::uint64_t saved_revision = 0;

  public:
	void undo(WorldEditContext& ctx) {
		if (undo_actions.empty()) {
			return;
		}

		auto& actions = undo_actions.back();
		for (const auto& i : actions) {
			i->undo(ctx);
		}

		redo_actions.push_back(std::move(actions));
		undo_actions.pop_back();
		++revision;
	}

	void redo(WorldEditContext& ctx) {
		if (redo_actions.empty()) {
			return;
		}

		auto& actions = redo_actions.back();
		for (const auto& i : actions) {
			i->redo(ctx);
		}

		undo_actions.push_back(std::move(actions));
		redo_actions.pop_back();
		++revision;
	}

	void new_undo_group() {
		undo_actions.push_back({});
	}

	void add_undo_action(std::unique_ptr<WorldCommand> action) {
		if (undo_actions.empty()) {
			return;
		}

		undo_actions.back().push_back(std::move(action));
		redo_actions.clear();
		++revision;
	}

	/// Removes all undo/redo operations
	void clear_all_undo() {
		undo_actions.clear();
		redo_actions.clear();
		++revision;
	}

	/// Call after a successful save: the world is saved as it is now.
	void mark_saved() {
		saved_revision = revision;
	}

	/// True when a world edit, undo or redo happened since the last mark_saved(). Undoing back to the saved state still counts as a change.
	[[nodiscard]]
	bool has_changes_since_save() const {
		return revision != saved_revision;
	}

	[[nodiscard]]
	bool has_pending_changes() const {
		return !undo_actions.empty() || !redo_actions.empty();
	}
};
