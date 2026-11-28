export module MdxMergeExport;

import std;
import MDX;
import LayerAnimation;
import <glm/glm.hpp>;
import <glm/gtc/quaternion.hpp>;

/// The Doodad palette's Merge Selection export. Sequences are not merged (the merged model gets one 0..0 "stand"), so every track
/// that runs on the sequence timeline is first reduced to what its model shows at the start of Stand, sampled as the renderer reads
/// tracks (layer_animation::sample_track). Tracks on a global sequence keep running; merge_with appends the global sequences.
export namespace mdx {
	namespace detail {
		/// First sequence whose name starts with "stand" (any case), else the first sequence, else none.
		const Sequence* stand_sequence(const MDX& model) {
			for (const auto& sequence : model.sequences) {
				std::string prefix = sequence.name.substr(0, 5);
				std::ranges::transform(prefix, prefix.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
				if (prefix == "stand") {
					return &sequence;
				}
			}
			return model.sequences.empty() ? nullptr : &model.sequences.front();
		}

		/// True when the renderer reads this track on one of the model's global sequences.
		template <typename T>
		bool on_global_sequence(const TrackHeader<T>& header, const MDX& model) {
			return header.global_sequence_ID >= 0 && static_cast<std::size_t>(header.global_sequence_ID) < model.global_sequences.size();
		}

		template <typename T>
		void clear_track(TrackHeader<T>& header) {
			header.tracks.clear();
			header.interpolation_type = InterpolationType::none;
			header.global_sequence_ID = -1;
			header.id = -1;
		}

		/// Replaces a sequence-timeline track by its value at `time`, stored in `field`.
		template <typename T>
		void freeze_into(TrackHeader<T>& header, T& field, const MDX& model, const layer_animation::TrackTime& time) {
			if (header.tracks.empty() || on_global_sequence(header, model)) {
				return;
			}
			field = layer_animation::sample_track(header, time, field);
			clear_track(header);
		}

		/// A texture animation track has no static field: it becomes one key at frame 0 holding the value, or nothing when that value
		/// is the identity.
		template <typename T>
		void freeze_in_place(TrackHeader<T>& header, const T& identity, const MDX& model, const layer_animation::TrackTime& time) {
			if (header.tracks.empty() || on_global_sequence(header, model)) {
				return;
			}
			const T value = layer_animation::sample_track(header, time, identity);
			clear_track(header);
			if (value != identity) {
				header.tracks.push_back(Track<T> { .frame = 0, .value = value, .inTan = value, .outTan = value });
			}
		}

		bool is_default_animation(const GeosetAnimation& animation) {
			constexpr uint32_t use_color = 2;
			return animation.KGAO.tracks.empty() && animation.KGAC.tracks.empty() && animation.alpha == 1.f
				&& (animation.flags & ~use_color) == 0 && (!(animation.flags & use_color) || animation.color == glm::vec3(1.f));
		}
	} // namespace detail

	/// A copy of `source` as it looks at the start of its Stand sequence: sequence-timeline tracks of geoset animations, layers and
	/// texture animations become static values, geosets hidden there (geoset animation alpha not above 0.01, as the renderer culls)
	/// are removed with their geoset animations, and geoset animations left at their defaults are removed. Geoset ids in geoset
	/// animations, bones and gliders are renumbered. Global-sequence tracks are kept.
	MDX frozen_to_stand(const MDX& source) {
		MDX model = source;
		layer_animation::TrackTime time;
		if (const Sequence* stand = detail::stand_sequence(model)) {
			time.has_sequence = true;
			time.sequence_start = stand->start_frame;
			time.sequence_end = stand->end_frame;
			time.frame = stand->start_frame;
		}
		time.global_sequences = model.global_sequences;

		for (auto& animation : model.animations) {
			detail::freeze_into(animation.KGAO, animation.alpha, model, time);
			if (!animation.KGAC.tracks.empty() && !detail::on_global_sequence(animation.KGAC, model)) {
				// The static colour is BGR; the renderer reads the track in the swizzled order of its default (get_geoset_animation_color)
				glm::vec3 swizzled(animation.color.b, animation.color.g, animation.color.r);
				detail::freeze_into(animation.KGAC, swizzled, model, time);
				animation.color = glm::vec3(swizzled.b, swizzled.g, swizzled.r);
			}
		}
		const size_t texture_count = model.textures.size();
		for (auto& material : model.materials) {
			for (auto& layer : material.layers) {
				detail::freeze_into(layer.KMTA, layer.alpha, model, time);
				detail::freeze_into(layer.KMTE, layer.emissive_gain, model, time);
				detail::freeze_into(layer.KFC3, layer.fresnel_color, model, time);
				detail::freeze_into(layer.KFCA, layer.fresnel_opacity, model, time);
				detail::freeze_into(layer.KFTC, layer.fresnel_team_color, model, time);
				for (auto& slot : layer.textures) {
					if (slot.KMTF.tracks.empty() || detail::on_global_sequence(slot.KMTF, model)) {
						continue;
					}
					if (const auto shown = layer_animation::layer_texture_id(slot, time, texture_count)) {
						slot.id = *shown;
					}
					detail::clear_track(slot.KMTF);
				}
			}
		}
		for (auto& animation : model.texture_animations) {
			detail::freeze_in_place(animation.KTAT, glm::vec3(0.f), model, time);
			detail::freeze_in_place(animation.KTAR, glm::quat(1.f, 0.f, 0.f, 0.f), model, time);
			detail::freeze_in_place(animation.KTAS, glm::vec3(1.f), model, time);
		}
		// A texture animation left without tracks is the identity: its layers get "none" (0xFFFFFFFF) and the entry goes, so
		// layers that differ only in it can share a material. (Our TXAN reader also skips a trackless last entry.)
		{
			constexpr uint32_t none = std::numeric_limits<uint32_t>::max();
			std::vector<uint32_t> texture_animation_map(model.texture_animations.size(), none);
			uint32_t kept = 0;
			for (size_t i = 0; i < model.texture_animations.size(); i++) {
				const auto& animation = model.texture_animations[i];
				if (!animation.KTAT.tracks.empty() || !animation.KTAR.tracks.empty() || !animation.KTAS.tracks.empty()) {
					texture_animation_map[i] = kept++;
				}
			}
			for (auto& material : model.materials) {
				for (auto& layer : material.layers) {
					if (layer.texture_animation_id < texture_animation_map.size()) {
						layer.texture_animation_id = texture_animation_map[layer.texture_animation_id];
					}
				}
			}
			size_t write = 0;
			for (size_t i = 0; i < model.texture_animations.size(); i++) {
				if (texture_animation_map[i] != none) {
					model.texture_animations[write++] = std::move(model.texture_animations[i]);
				}
			}
			model.texture_animations.resize(write);
		}

		// The renderer gives a geoset the last geoset animation that names it
		std::vector<const GeosetAnimation*> animation_of(model.geosets.size(), nullptr);
		for (const auto& animation : model.animations) {
			if (animation.geoset_id < model.geosets.size()) {
				animation_of[animation.geoset_id] = &animation;
			}
		}
		std::vector<bool> keep(model.geosets.size(), true);
		for (size_t i = 0; i < model.geosets.size(); i++) {
			const GeosetAnimation* animation = animation_of[i];
			if (animation != nullptr && !detail::on_global_sequence(animation->KGAO, model)) {
				keep[i] = layer_animation::layer_visible(animation->alpha, 1.f);
			}
		}

		constexpr uint32_t gone = std::numeric_limits<uint32_t>::max();
		std::vector<uint32_t> mapping(model.geosets.size(), gone);
		std::vector<Geoset> kept;
		for (size_t i = 0; i < model.geosets.size(); i++) {
			if (keep[i]) {
				mapping[i] = static_cast<uint32_t>(kept.size());
				kept.push_back(std::move(model.geosets[i]));
			}
		}
		model.geosets = std::move(kept);

		// Animations of removed geosets go, as do animations that change nothing. Ids outside the list stay outside it.
		std::erase_if(model.animations, [&](const GeosetAnimation& animation) {
			return (animation.geoset_id < mapping.size() && mapping[animation.geoset_id] == gone) || detail::is_default_animation(animation);
		});
		for (auto& animation : model.animations) {
			if (animation.geoset_id < mapping.size()) {
				animation.geoset_id = mapping[animation.geoset_id];
			}
		}
		for (auto& bone : model.bones) {
			if (bone.geoset_id >= 0 && static_cast<size_t>(bone.geoset_id) < mapping.size()) {
				bone.geoset_id = mapping[bone.geoset_id] == gone ? -1 : static_cast<int32_t>(mapping[bone.geoset_id]);
			}
		}
		std::erase_if(model.gliders, [&](const uint32_t id) { return id < mapping.size() && mapping[id] == gone; });
		for (auto& glider : model.gliders) {
			if (glider < mapping.size()) {
				glider = mapping[glider];
			}
		}
		return model;
	}

	/// merge_with every part (frozen to its Stand start first) in order, then deduplicate textures, materials and geosets and
	/// recalculate extents. A null model is skipped.
	MDX merged_for_export(const std::vector<MDX::MergePart>& parts) {
		// Value-initialised so the extent and the other scalars start at zero
		MDX result {};
		for (const auto& part : parts) {
			if (part.model != nullptr) {
				result.merge_with(frozen_to_stand(*part.model), part.transform);
			}
		}
		result.deduplicate_textures().deduplicate_materials().deduplicate_geosets().calculate_extents();
		// merge_with keeps no bones, so the only bone is the one validate() added (Bone{}: geoset 0 and geoset animation 0).
		// It carries every vertex, so it names no geoset and no geoset animation (-1, MDL "None").
		for (auto& bone : result.bones) {
			bone.geoset_id = -1;
			bone.geoset_animation_id = -1;
		}
		return result;
	}
} // namespace mdx
