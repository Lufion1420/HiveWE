export module TimelineMath;

import std;

/// Playback arithmetic of the Model Editor timeline, without Qt or GL. WC3 animation frames are milliseconds.

export struct FrameAdvance {
	double frame;
	/// True when a non-looping sequence just reached its last frame.
	bool finished;
	/// True when a looping sequence wrapped past its last frame (the particle emitters restart on this).
	bool wrapped = false;
};

export inline constexpr std::array<double, 5> playback_speeds { 0.25, 0.5, 1.0, 1.5, 2.0 };

export double clamp_frame(const double frame, const double start, const double end) {
	return std::clamp(frame, start, std::max(start, end));
}

/// Moves `frame` forward by `delta_seconds` at `speed`. Looping sequences wrap past the last frame, others stop on it. A speed of
/// zero or below, or a negative delta, leaves the frame where it is (only clamped into the range).
export FrameAdvance advance_frame(const double frame, const double start, const double end, const bool loop, const double speed, const double delta_seconds) {
	const double current = clamp_frame(frame, start, end);
	if (speed <= 0.0 || delta_seconds <= 0.0 || end <= start) {
		return { current, false, false };
	}

	const double next = current + delta_seconds * 1000.0 * speed;
	if (next <= end) {
		return { next, false, false };
	}
	if (loop) {
		return { start + std::fmod(next - start, end - start), false, true };
	}
	return { end, true, false };
}

/// Slider position (0 .. maximum) to a frame of the sequence.
export double slider_to_frame(const int value, const int maximum, const double start, const double end) {
	if (maximum <= 0) {
		return start;
	}
	const double t = std::clamp(static_cast<double>(value) / maximum, 0.0, 1.0);
	return start + t * (end - start);
}

export int frame_to_slider(const double frame, const double start, const double end, const int maximum) {
	if (end <= start || maximum <= 0) {
		return 0;
	}
	const double t = (clamp_frame(frame, start, end) - start) / (end - start);
	return static_cast<int>(std::lround(t * maximum));
}

/// "340 / 1000 ms": time into the sequence and its length.
export std::string format_readout(const double frame, const double start, const double end) {
	const long long length = std::llround(std::max(start, end) - start);
	const long long elapsed = std::llround(clamp_frame(frame, start, end) - start);
	return std::format("{} / {} ms", elapsed, length);
}

export std::string format_speed(const double speed) {
	const bool one_decimal = std::abs(speed * 10.0 - std::round(speed * 10.0)) < 1e-9;
	return one_decimal ? std::format("{:.1f}x", speed) : std::format("{:.2f}x", speed);
}
