#include <string>
#include <doctest/doctest.h>

import std;
import WorldUndoManager;

// HiveWE::confirm_map_replacement asks "save before opening another map?" from has_changes_since_save(). Change Tileset and Map::resize
// clear the undo history, which must not hide the unsaved work from that question.

namespace {
	class NoopCommand : public WorldCommand {
	  public:
		void undo(WorldEditContext&) override {}
		void redo(WorldEditContext&) override {}
	};
}

TEST_CASE("a fresh world has no changes since save") {
	const WorldUndoManager undo;
	CHECK_FALSE(undo.has_changes_since_save());
}

TEST_CASE("clearing the undo history after an edit still counts as unsaved work") {
	WorldUndoManager undo;
	undo.new_undo_group();
	undo.add_undo_action(std::make_unique<NoopCommand>());
	undo.mark_saved();
	REQUIRE_FALSE(undo.has_changes_since_save());

	undo.new_undo_group();
	undo.add_undo_action(std::make_unique<NoopCommand>());
	undo.clear_all_undo(); // what Change Tileset and Map::resize do

	CHECK_FALSE(undo.has_pending_changes()); // the stacks are empty: the old check would say "nothing to save"
	CHECK(undo.has_changes_since_save());
}

TEST_CASE("clearing the undo history of a saved world is a change, and saving again makes it clean") {
	WorldUndoManager undo;
	undo.mark_saved();
	REQUIRE_FALSE(undo.has_changes_since_save());

	undo.clear_all_undo();
	CHECK(undo.has_changes_since_save());

	undo.mark_saved();
	CHECK_FALSE(undo.has_changes_since_save());
}
