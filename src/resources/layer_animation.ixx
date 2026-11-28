export module LayerAnimation;

import std;
import MDX;
import MathOperations;
import <glm/glm.hpp>;
import <glm/gtc/quaternion.hpp>;

/// GL-free per-layer rules of the Model Editor renderer (EditableMesh): track sampling for texture animation (TXAN) and the layer
/// texture flipbook (KMTF), the UV transform, the draw order of geoset layers and the visibility cull. The rules follow WarMash's
/// SD renderer (SdSequence, SetupGroups, BatchGroup, MdxShaders); tests in tests/layer_animation_test.cpp.
export namespace layer_animation {
	/// The clock a track is sampled at: the current sequence (if any) and the global-sequence clock.
	struct TrackTime {
		bool has_sequence = false;
		std::int64_t sequence_start = 0;
		std::int64_t sequence_end = 0;
		std::int64_t frame = 0;
		long long global_ms = 0;
		std::span<const std::uint32_t> global_sequences;
	};

	/// The time window one track reads: the sequence, or its global sequence (start 0). Not valid: the track reads its default.
	struct TrackWindow {
		bool valid = false;
		bool global = false;
		std::int64_t start = 0;
		std::int64_t end = 0;
		std::int64_t frame = 0;
	};

	TrackWindow track_window(const int global_sequence_id, const TrackTime& time) {
		TrackWindow window;
		// As WarMash: a global sequence id counts only when the model has global sequences at all.
		if (global_sequence_id != -1 && !time.global_sequences.empty()) {
			if (global_sequence_id < 0 || static_cast<std::size_t>(global_sequence_id) >= time.global_sequences.size()) {
				return window; // a damaged id: the default value, never a read past the list
			}
			const std::int64_t length = time.global_sequences[static_cast<std::size_t>(global_sequence_id)];
			window.valid = true;
			window.global = true;
			window.start = 0;
			window.end = length;
			if (length > 0) {
				window.frame = time.global_ms % length;
				if (window.frame < 0) {
					window.frame += length;
				}
			}
			return window;
		}
		if (!time.has_sequence) {
			return window;
		}
		window.valid = true;
		window.start = time.sequence_start;
		window.end = time.sequence_end;
		window.frame = time.frame;
		return window;
	}

	template <typename T>
	T lerp_track_value(const T& start, const T& out_tan, const T& in_tan, const T& end, const float t, const mdx::InterpolationType type) {
		if constexpr (std::is_same_v<T, std::uint32_t>) {
			// Texture ids: stepped holds, the others are read as numbers and cut to an integer (WarMash UInt32Sd).
			if (type == mdx::InterpolationType::none) {
				return start;
			}
			const double value = interpolate(
				static_cast<float>(start), static_cast<float>(out_tan), static_cast<float>(in_tan), static_cast<float>(end), t, static_cast<int>(type)
			);
			if (!std::isfinite(value) || value < 0.0 || value > static_cast<double>(std::numeric_limits<std::uint32_t>::max())) {
				return start;
			}
			return static_cast<std::uint32_t>(value);
		} else {
			return interpolate(start, out_tan, in_tan, end, t, static_cast<int>(type));
		}
	}

	/// The value of `header` at `time`. The rules (as WarMash and the game read MDX tracks; this is an own implementation of them):
	/// 1. A track without keys, a time without a sequence, or a damaged global sequence id reads `missing_default`.
	/// 2. The window is the current sequence; for a track on a global sequence it is [0, length] at the global clock modulo the
	///    length (time 0 for a zero length). A global sequence id counts only when the model has global sequences at all.
	/// 3. Only keys whose frame lies inside the window (both ends included) count, in file order. On a global sequence a first key
	///    past the window end counts too, placed at the window start when the window has no length, else at its own frame.
	/// 4. No counting key reads `unkeyed_default` (the track type's own default). One counting key, or counting keys that all hold the
	///    same value, read that value. A time before the window start reads the first counting key.
	/// 5. Otherwise, when a counting key later than the time follows another counting key, the value goes from that other key toward
	///    the later key by the elapsed share of the gap between their frames.
	/// 6. Else (before the first counting key, or at or after the last one) the value goes from the last key toward the first across
	///    the wrap gap: first frame minus last frame, plus the window length when that is negative; a zero gap gives share 0. The share
	///    is measured from the last key when the time is at or after it, otherwise from the first key, so before the first key the share
	///    is negative (the known game and World Editor behaviour, which a linear track extrapolates).
	/// 7. Interpolation follows the track type; stepped tracks hold the earlier key. Unsorted keys never read outside the list.
	template <typename T>
	T sample_track(const mdx::TrackHeader<T>& header, const TrackTime& time, const T& missing_default, const T& unkeyed_default) {
		if (header.tracks.empty()) {
			return missing_default;
		}
		const TrackWindow window = track_window(header.global_sequence_ID, time);
		if (!window.valid) {
			return missing_default;
		}

		struct CountedKey {
			std::int64_t frame;
			const mdx::Track<T>* track;
		};
		std::vector<CountedKey> keys;
		const mdx::Track<T>& first_in_file = header.tracks.front();
		if (window.global && first_in_file.frame > window.end) {
			keys.push_back({ window.start == window.end ? window.start : first_in_file.frame, &first_in_file });
		}
		for (const auto& track : header.tracks) {
			if (track.frame >= window.start && track.frame <= window.end) {
				keys.push_back({ track.frame, &track });
			}
		}
		if (keys.empty()) {
			return unkeyed_default;
		}
		const T& first_value = keys.front().track->value;
		const bool one_value = std::ranges::all_of(keys, [&](const CountedKey& key) { return key.track->value == first_value; });
		if (one_value || window.frame < window.start) {
			return first_value;
		}

		const auto later = std::ranges::find_if(keys, [&](const CountedKey& key) { return key.frame > window.frame; });
		const CountedKey* from = nullptr;
		const CountedKey* to = nullptr;
		double share = 0.0;
		if (later != keys.begin() && later != keys.end()) {
			from = &*std::prev(later);
			to = &*later;
			share = static_cast<double>(window.frame - from->frame) / static_cast<double>(to->frame - from->frame);
		} else {
			from = &keys.back();
			to = &keys.front();
			std::int64_t gap = to->frame - from->frame;
			if (gap < 0) {
				gap += window.end - window.start;
			}
			const std::int64_t measured_from = window.frame >= from->frame ? from->frame : to->frame;
			share = gap == 0 ? 0.0 : static_cast<double>(window.frame - measured_from) / static_cast<double>(gap);
		}

		const bool tangents = header.interpolation_type == mdx::InterpolationType::hermite || header.interpolation_type == mdx::InterpolationType::bezier;
		const T zero {};
		return lerp_track_value(
			from->track->value,
			tangents ? from->track->outTan : zero,
			tangents ? to->track->inTan : zero,
			to->track->value,
			static_cast<float>(share),
			header.interpolation_type
		);
	}

	/// sample_track with one default for a missing track and for a track without keys in the window.
	template <typename T>
	T sample_track(const mdx::TrackHeader<T>& header, const TrackTime& time, const T& default_value) {
		return sample_track(header, time, default_value, default_value);
	}

	/// A layer's texture-coordinate transform: uv += translation; rotate about (0.5, 0.5) by the quaternion's z and w; scale about
	/// (0.5, 0.5) per axis (WarMash fsComplex and UvAnimLayout, with KTAS y scaling V).
	struct UvTransform {
		glm::vec2 translation = glm::vec2(0.f);
		/// z and w of the KTAR quaternion.
		glm::vec2 rotation = glm::vec2(0.f, 1.f);
		glm::vec2 scale = glm::vec2(1.f);

		bool operator==(const UvTransform&) const = default;
	};

	/// The transform of texture animation `texture_animation_id` at `time`. An id past the model's list (0xFFFFFFFF is "none") is no
	/// animation, as in WarMash. A non-finite sampled part falls back to that part's identity.
	UvTransform texture_animation_transform(const mdx::MDX& model, const std::uint32_t texture_animation_id, const TrackTime& time) {
		UvTransform out;
		if (texture_animation_id >= model.texture_animations.size()) {
			return out;
		}
		const auto& animation = model.texture_animations[texture_animation_id]; // mdx::TextureAnimation is not exported
		const glm::vec3 translation = sample_track(animation.KTAT, time, glm::vec3(0.f));
		const glm::quat rotation = sample_track(animation.KTAR, time, glm::quat(1.f, 0.f, 0.f, 0.f));
		const glm::vec3 scale = sample_track(animation.KTAS, time, glm::vec3(1.f));
		if (std::isfinite(translation.x) && std::isfinite(translation.y)) {
			out.translation = glm::vec2(translation.x, translation.y);
		}
		if (std::isfinite(rotation.z) && std::isfinite(rotation.w)) {
			out.rotation = glm::vec2(rotation.z, rotation.w);
		}
		if (std::isfinite(scale.x) && std::isfinite(scale.y)) {
			out.scale = glm::vec2(scale.x, scale.y);
		}
		return out;
	}

	/// The CPU mirror of the shader's UV transform (data/shaders/editable_mesh_sd.vert).
	glm::vec2 apply_uv_transform(const UvTransform& transform, glm::vec2 uv) {
		uv += transform.translation;
		const glm::vec2 v = uv - 0.5f;
		const float qz = transform.rotation.x;
		const float qw = transform.rotation.y;
		const glm::vec2 a(-qz * v.y, qz * v.x);
		const glm::vec2 b(-qz * a.y, qz * a.x);
		uv = v + 2.f * (a * qw + b) + 0.5f;
		return transform.scale * (uv - 0.5f) + 0.5f;
	}

	/// The texture a layer slot shows at `time`. Without a KMTF flipbook (no keys) or without a sequence: its static id. With one:
	/// the flipbook value, and texture 0 when the flipbook has no key in the current window (the track's own default, as WarMash).
	/// A negative value (as a signed 32-bit number) reads as texture 0, as WarMash. A value past `texture_count` falls back to the
	/// static id (WarMash would fail on it). Nothing (std::nullopt) when neither names a texture of the model.
	std::optional<std::uint32_t> layer_texture_id(const mdx::LayerTexture& slot, const TrackTime& time, const std::size_t texture_count) {
		std::uint32_t id = sample_track(slot.KMTF, time, slot.id, 0u);
		if (static_cast<std::int32_t>(id) < 0) {
			id = 0;
		}
		if (id < texture_count) {
			return id;
		}
		if (slot.id < texture_count) {
			return slot.id;
		}
		return std::nullopt;
	}

	/// WarMash draws a geoset layer only if geoset alpha and layer alpha are each above 0.01 (BatchGroup). NaN is not drawn.
	bool layer_visible(const float geoset_alpha, const float layer_alpha) {
		return geoset_alpha > 0.01f && layer_alpha > 0.01f;
	}

	/// None (0) and Transparent (1) draw first, in model order; every other filter mode, unknown ones too, is sorted.
	bool is_opaque_filter(const std::uint32_t blend_mode) {
		return blend_mode < 2;
	}

	/// The alpha-test threshold of a filter mode: Transparent cuts out below 0.75, None never discards (WarMash), the blended modes
	/// keep the old 0.01.
	float alpha_test_threshold(const std::uint32_t blend_mode) {
		if (blend_mode == 0) {
			return 0.f;
		}
		return blend_mode == 1 ? 0.75f : 0.01f;
	}

	/// The one rule WarMash leaves open: its SetupGroups third sort key is the geoset extent centre Z, negated in its stand-alone
	/// viewer (ShaderEnvironmentType MENU, MdxViewer's default) and not negated in the game (GAME, War3MapViewer). The Model Editor
	/// follows the game: low centre Z first. Flip `menu_viewer_order` to get the stand-alone viewer's order. Pinned by a test.
	inline constexpr bool menu_viewer_order = false;

	float translucent_depth_key(const mdx::Extent& extent) {
		const float centre = (extent.minimum.z + extent.maximum.z) / 2.f;
		return menu_viewer_order ? -centre : centre;
	}

	/// One geoset layer to draw. `geoset` and `layer` are the caller's indices; they do not take part in the order.
	struct DrawCandidate {
		int geoset = 0;
		int layer = 0;
		std::uint32_t blend_mode = 0;
		std::uint32_t priority_plane = 0;
		float depth_key = 0.f;
	};

	/// Java's Float.compare: a total order with -0 before +0 and NaN after everything (NaN equals NaN), so the sort below keeps a
	/// strict weak ordering on any input.
	int float_compare(const float a, const float b) {
		if (a < b) {
			return -1;
		}
		if (a > b) {
			return 1;
		}
		const bool a_nan = std::isnan(a);
		const bool b_nan = std::isnan(b);
		if (a_nan || b_nan) {
			return a_nan == b_nan ? 0 : (a_nan ? 1 : -1);
		}
		const bool a_negative = std::signbit(a);
		const bool b_negative = std::signbit(b);
		return a_negative == b_negative ? 0 : (a_negative ? -1 : 1);
	}

	/// WarMash's draw order (SetupGroups): indices into `candidates`, first every None/Transparent layer in the given (model) order,
	/// then the rest stably sorted by priority plane, filter mode, then depth key. Planes compare as WarMash's signed 32-bit ints, but
	/// without its subtraction overflow.
	std::vector<std::size_t> draw_order(const std::span<const DrawCandidate> candidates) {
		std::vector<std::size_t> opaque;
		std::vector<std::size_t> translucent;
		for (std::size_t i = 0; i < candidates.size(); ++i) {
			(is_opaque_filter(candidates[i].blend_mode) ? opaque : translucent).push_back(i);
		}
		std::ranges::stable_sort(translucent, [&](const std::size_t left, const std::size_t right) {
			const DrawCandidate& a = candidates[left];
			const DrawCandidate& b = candidates[right];
			const auto a_plane = static_cast<std::int32_t>(a.priority_plane);
			const auto b_plane = static_cast<std::int32_t>(b.priority_plane);
			if (a_plane != b_plane) {
				return a_plane < b_plane;
			}
			const auto a_mode = static_cast<std::int32_t>(a.blend_mode);
			const auto b_mode = static_cast<std::int32_t>(b.blend_mode);
			if (a_mode != b_mode) {
				return a_mode < b_mode;
			}
			return float_compare(a.depth_key, b.depth_key) < 0;
		});
		opaque.insert(opaque.end(), translucent.begin(), translucent.end());
		return opaque;
	}
} // namespace layer_animation
