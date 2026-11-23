export module TileAnimation;

import std;
import TimelineMath;
import Utilities;

/// Logic of the animated tiles in the Browse Models grid, without Qt or GL: which sequence a tile plays, which rows animate, and how
/// a tile's clock moves. The GL side is in ModelThumbnailCache, the Qt side in BrowseModelsDialog.

/// The one switch: false keeps every tile a still thumbnail (nothing is loaded or drawn for animation).
export inline constexpr bool animate_browse_tiles = true;
/// At most this many tiles play at once: the visible ones, the hovered and the selected tile first.
export inline constexpr std::size_t max_animated_tiles = 12;
/// Frames per second of the animated tiles (the big preview is not limited by this).
export inline constexpr int animated_tile_fps = 15;
/// A tick that comes late (a busy event loop) moves a tile by at most this much, so it does not jump.
export inline constexpr double max_tile_step_seconds = 0.25;

export struct SequenceCandidate {
	std::string_view name;
	/// False for a sequence with nothing to show (an empty placeholder or no length); it is never picked.
	bool usable = true;
};

/// The sequence a preview plays first: a plain "Stand" ("Stand - 1" too), else the first one with "stand" in its name ("Stand Alternate"
/// can be a curled-up pose, so it only comes second), else the first sequence. Unusable sequences are skipped at every step. -1 when
/// nothing is usable (the tile stays a still picture).
export int preferred_preview_sequence(const std::span<const SequenceCandidate> sequences) {
	int exact_stand = -1;
	int contains_stand = -1;
	int first = -1;
	for (int i = 0; i < static_cast<int>(sequences.size()); ++i) {
		if (!sequences[i].usable) {
			continue;
		}
		if (first < 0) {
			first = i;
		}
		const std::string lowered = to_lowercase_copy(sequences[i].name);
		if (exact_stand < 0 && (lowered == "stand" || lowered == "stand - 1")) {
			exact_stand = i;
		}
		if (contains_stand < 0 && lowered.contains("stand")) {
			contains_stand = i;
		}
	}
	return exact_stand >= 0 ? exact_stand : contains_stand >= 0 ? contains_stand : first;
}

/// The rows that animate: the hovered row, then the selected row (each only when it is in `visible_rows`), then the visible rows in order,
/// each once, at most `cap`.
export std::vector<int> choose_animated_rows(const std::span<const int> visible_rows, const int hovered, const int selected, const std::size_t cap) {
	std::vector<int> chosen;
	const auto add = [&](const int row) {
		if (chosen.size() < cap && row >= 0 && std::ranges::contains(visible_rows, row) && !std::ranges::contains(chosen, row)) {
			chosen.push_back(row);
		}
	};
	add(hovered);
	add(selected);
	for (const int row : visible_rows) {
		add(row);
	}
	return chosen;
}

/// A tile's next frame: always looping, at normal speed, with the step limited to max_tile_step_seconds.
export FrameAdvance next_tile_frame(const double frame, const double start, const double end, const double delta_seconds) {
	return advance_frame(frame, start, end, true, 1.0, std::min(delta_seconds, max_tile_step_seconds));
}
