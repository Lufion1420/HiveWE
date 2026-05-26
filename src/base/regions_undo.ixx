export module RegionsUndo;

import Regions;
import WorldUndoManager;

export class RegionStateAction final : public WorldCommand {
  public:
	Region old_region;
	Region new_region;

	void undo(WorldEditContext& ctx) override {
		for (auto& region : ctx.regions.regions) {
			if (region.creation_number == old_region.creation_number) {
				region = old_region;
				return;
			}
		}
	}

	void redo(WorldEditContext& ctx) override {
		for (auto& region : ctx.regions.regions) {
			if (region.creation_number == new_region.creation_number) {
				region = new_region;
				return;
			}
		}
	}
};
