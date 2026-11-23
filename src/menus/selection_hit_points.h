#pragma once

#include <string>

/// The hit points of a placed unit. Unit::health is a percentage of the maximum, as war3mapUnits.doo stores it, with -1 meaning the default
/// (100%). The Unit Properties dialog and map_script.cpp (health / 100 * life) read it the same way. `max_hit_points` is the SLK hp (0 if unknown).
inline int selection_hit_points_percent(const int health) {
	return health < 0 ? 100 : health;
}

/// "100% (420)": the percentage and, when the maximum is known, the absolute value that percentage is of the SLK hp.
inline std::string selection_hit_points_text(const int health, const int max_hit_points) {
	const int percent = selection_hit_points_percent(health);
	std::string text = std::to_string(percent) + "%";
	if (max_hit_points > 0) {
		const int absolute = static_cast<int>((static_cast<long long>(max_hit_points) * percent + 50) / 100);
		text += " (" + std::to_string(absolute) + ")";
	}
	return text;
}
