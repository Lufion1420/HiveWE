module;

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

module MDX;

import std;
import types;
import MathOperations;
import <glm/glm.hpp>;
import <glm/gtc/quaternion.hpp>;

namespace mdx {
	void remove_unused_materials(MDX& mdx) {
		if (mdx.geosets.empty()) {
			return;
		}

		std::set<u32> used_materials;
		for (const auto& geoset : mdx.geosets) {
			used_materials.emplace(geoset.material_id);
		}

		std::vector<u32> mapping(mdx.materials.size(), 0);
		size_t current_slot = 0;
		for (const auto& i : used_materials) {
			mapping[i] = current_slot++;
		}

		for (auto& geoset : mdx.geosets) {
			geoset.material_id = mapping[geoset.material_id];
		}

		size_t write = 0;
		for (size_t read = 0; read < mdx.materials.size(); ++read) {
			if (used_materials.contains(read)) {
				mdx.materials[write++] = mdx.materials[read];
			}
		}
		mdx.materials.resize(write);
	}

	/// Removes textures that aren't referenced by any MDX materials
	/// ToDo also check if textures are used by other MDX chunks
	void remove_unused_textures(MDX& mdx) {
		if (mdx.textures.empty()) {
			return;
		}

		// An id outside the list (damaged input) marks nothing used and is left as is: it stays outside the shorter list.
		const size_t texture_count = mdx.textures.size();
		std::set<u32> used_textures;
		for (auto& material : mdx.materials) {
			for (auto& layer : material.layers) {
				for_each_layer_texture_id(layer, [&](const u32& id) {
					if (id < texture_count) {
						used_textures.emplace(id);
					}
				});
			}
		}

		std::vector<u32> mapping(texture_count, 0);
		size_t current_slot = 0;
		for (const auto& i : used_textures) {
			mapping[i] = current_slot++;
		}

		for (auto& material : mdx.materials) {
			for (auto& layer : material.layers) {
				for_each_layer_texture_id(layer, [&](u32& id) {
					if (id < texture_count) {
						id = mapping[id];
					}
				});
			}
		}

		size_t write = 0;
		for (size_t read = 0; read < mdx.textures.size(); ++read) {
			if (used_textures.contains(read)) {
				mdx.textures[write++] = mdx.textures[read];
			}
		}
		mdx.textures.resize(write);
	}

	/// Deduplicate materials and geosets before this for best results
	MDX& MDX::deduplicate_textures() {
		// mapping[old id] is the position in `unique`, which keeps first occurrences in their original order
		std::unordered_map<Texture, u32> texture_map;
		std::vector<Texture> unique;
		std::vector<u32> mapping;

		for (const auto& texture : textures) {
			const auto& [elem, inserted] = texture_map.emplace(texture, static_cast<u32>(unique.size()));
			if (inserted) {
				unique.push_back(texture);
			}
			mapping.push_back(elem->second);
		}

		// An id outside the list (damaged input) is left as is: it stays outside the shorter list.
		for (auto& material : materials) {
			for (auto& layer : material.layers) {
				for_each_layer_texture_id(layer, [&](u32& id) {
					if (id < mapping.size()) {
						id = mapping[id];
					}
				});
			}
		}

		textures = std::move(unique);
		return *this;
	}

	/// Call after deduplicate_textures for best results
	/// as the texture IDs will be deduplicated so fewer differences
	/// Identical materials share one entry afterwards; first occurrences keep their original order and ids are compact.
	MDX& MDX::deduplicate_materials() {
		// mapping[old id] is the position in `unique`, which keeps first occurrences in their original order
		std::unordered_map<Material, u32> material_map;
		std::vector<Material> unique;
		std::vector<u32> mapping;

		for (const auto& material : materials) {
			const auto& [elem, inserted] = material_map.emplace(material, static_cast<u32>(unique.size()));
			if (inserted) {
				unique.push_back(material);
			}
			mapping.push_back(elem->second);
		}

		// An id outside the list (damaged input) is left as is: it stays outside the shorter list.
		const auto remap = [&](u32& id) {
			if (id < mapping.size()) {
				id = mapping[id];
			}
		};
		for (auto& geoset : geosets) {
			remap(geoset.material_id);
		}
		for (auto& ribbon : ribbons) {
			remap(ribbon.material_id);
		}

		materials = std::move(unique);
		return *this;
	}

	namespace {
		/// u16 face indices address at most this many vertices
		constexpr size_t max_geoset_vertices = static_cast<size_t>(std::numeric_limits<u16>::max()) + 1;
		/// u8 vertex groups address at most this many matrix groups
		constexpr size_t max_matrix_groups = static_cast<size_t>(std::numeric_limits<u8>::max()) + 1;

		/// Every per-vertex array matches the vertex count and every index points inside its geoset, so its data can be
		/// appended to another geoset without landing on that geoset's vertices.
		bool is_self_consistent(const Geoset& g, const size_t material_count) {
			const size_t n = g.vertices.size();
			if (g.material_id >= material_count || g.normals.size() != n) {
				return false;
			}
			if (!std::ranges::all_of(g.uv_sets, [n](const auto& set) { return set.size() == n; })) {
				return false;
			}
			if ((!g.tangents.empty() && g.tangents.size() != n) || (!g.skin.empty() && g.skin.size() != n * 8)) {
				return false;
			}
			if (!g.vertex_groups.empty() && g.vertex_groups.size() != n) {
				return false;
			}
			if (!std::ranges::all_of(g.faces, [n](const u16 face) { return face < n; })) {
				return false;
			}
			if (g.face_type_groups.size() != g.face_groups.size()
				|| std::accumulate(g.face_groups.begin(), g.face_groups.end(), uint64_t { 0 }) != g.faces.size()) {
				return false;
			}
			const size_t groups = g.matrix_groups.size();
			if (!std::ranges::all_of(g.vertex_groups, [groups](const u8 group) { return group < groups; })) {
				return false;
			}
			return std::accumulate(g.matrix_groups.begin(), g.matrix_groups.end(), uint64_t { 0 }) == g.matrix_indices.size();
		}

		/// Same material, LOD and vertex layout, and the combined geoset still fits its index types.
		bool can_join(const Geoset& target, const Geoset& source) {
			if (target.material_id != source.material_id || target.lod != source.lod || target.uv_sets.size() != source.uv_sets.size()) {
				return false;
			}
			if (target.tangents.empty() != source.tangents.empty() || target.skin.empty() != source.skin.empty()
				|| target.vertex_groups.empty() != source.vertex_groups.empty()) {
				return false;
			}
			if (target.vertices.size() + source.vertices.size() > max_geoset_vertices) {
				return false;
			}
			return target.vertex_groups.empty() || target.matrix_groups.size() + source.matrix_groups.size() <= max_matrix_groups;
		}

		void append_geoset(Geoset& target, const Geoset& source) {
			const auto vertex_offset = static_cast<u16>(target.vertices.size());
			const auto group_offset = static_cast<u8>(target.matrix_groups.size());

			target.vertices.append_range(source.vertices);
			target.normals.append_range(source.normals);
			for (const u16 face : source.faces) {
				target.faces.push_back(static_cast<u16>(face + vertex_offset));
			}
			target.face_type_groups.append_range(source.face_type_groups);
			target.face_groups.append_range(source.face_groups);
			for (const u8 group : source.vertex_groups) {
				target.vertex_groups.push_back(static_cast<u8>(group + group_offset));
			}
			target.matrix_groups.append_range(source.matrix_groups);
			target.matrix_indices.append_range(source.matrix_indices);
			target.tangents.append_range(source.tangents);
			target.skin.append_range(source.skin);
			for (size_t j = 0; j < source.uv_sets.size(); j++) {
				target.uv_sets[j].append_range(source.uv_sets[j]);
			}
			// Extents are recalculated by the caller (calculate_extents)
		}
	} // namespace

	/// Call after deduplicate_materials() for best results.
	/// Joins geosets that share a material, LOD and vertex layout into the first such geoset, keeping geoset order.
	/// A geoset named by a geoset animation keeps its own geoset, as does one with inconsistent data (damaged input) or one
	/// whose vertices would no longer fit u16 face indices. Geoset ids in geoset animations, bones and gliders are renumbered.
	/// Merging takes no account of bone animation. Requires recalculating extents after.
	MDX& MDX::deduplicate_geosets() {
		std::vector<bool> animated(geosets.size(), false);
		for (const auto& animation : animations) {
			if (animation.geoset_id < geosets.size()) {
				animated[animation.geoset_id] = true;
			}
		}

		std::vector<Geoset> result;
		std::vector<bool> joinable;
		std::vector<u32> mapping(geosets.size());
		for (size_t i = 0; i < geosets.size(); i++) {
			Geoset& geoset = geosets[i];
			const bool can_merge = !animated[i] && is_self_consistent(geoset, materials.size());
			if (can_merge) {
				size_t target = 0;
				while (target < result.size() && !(joinable[target] && can_join(result[target], geoset))) {
					target += 1;
				}
				if (target < result.size()) {
					append_geoset(result[target], geoset);
					mapping[i] = static_cast<u32>(target);
					continue;
				}
			}
			mapping[i] = static_cast<u32>(result.size());
			result.push_back(std::move(geoset));
			joinable.push_back(can_merge);
		}

		// Ids outside the list (damaged input, or -1 "none" for bones) are left as they are
		for (auto& animation : animations) {
			if (animation.geoset_id < mapping.size()) {
				animation.geoset_id = mapping[animation.geoset_id];
			}
		}
		for (auto& bone : bones) {
			if (bone.geoset_id >= 0 && static_cast<size_t>(bone.geoset_id) < mapping.size()) {
				bone.geoset_id = static_cast<int32_t>(mapping[bone.geoset_id]);
			}
		}
		for (auto& glider : gliders) {
			if (glider < mapping.size()) {
				glider = mapping[glider];
			}
		}

		geosets = std::move(result);
		return *this;
	}

	/// Removes tracks that are not inside a sequence start<->end frame
	template <typename T>
	void remove_tracks_outside_sequences(TrackHeader<T>& header, std::vector<Sequence>& sequences) {
		if (sequences.empty()) {
			return;
		}

		size_t write_index = 0;
		size_t current_sequence = 0;

		for (size_t i = 0; i < header.tracks.size(); i++) {
			const auto& current = header.tracks[i];

			if (current.frame > sequences[current_sequence].end_frame) {
				current_sequence += 1;
				if (current_sequence >= sequences.size()) {
					return;
				}
			}

			if (current.frame < sequences[current_sequence].start_frame) {
				continue;
			}

			header.tracks[write_index++] = current;
		}
	}

	/// Reduces the number of keyframes in a track by decimating the curve
	/// Uses the Douglas–Peucker algorithm
	/// Assumes all tracks are contained in a sequence
	template <typename T>
	void reduce_track(TrackHeader<T>& header, std::vector<Sequence>& sequences, float max_error_sq) {
		if (sequences.empty()) {
			return;
		}

		if (header.tracks.size() <= 2) {
			return;
		}

		size_t write_index = 1; // Always keep the first keyframe
		size_t anchor_index = 0;

		size_t current_sequence = 0;
		for (size_t i = 1; i < header.tracks.size() - 1; i++) {
			const auto& anchor = header.tracks[anchor_index];
			const auto& current = header.tracks[i];
			const auto& next = header.tracks[i + 1];

			if (next.frame > sequences[current_sequence].end_frame) {
				current_sequence += 1;
				// Write the end keyframe of the sequence
				header.tracks[write_index++] = current;
				// Write the start keyframe of the next sequence
				header.tracks[write_index++] = next;
				anchor_index = i + 1;
				// We increase loop counter i by 2
				i += 1;
				continue;
			}

			const auto total_span = static_cast<float>(next.frame - anchor.frame);
			const auto current_span = static_cast<float>(current.frame - anchor.frame);

			// Avoid division by zero
			const float t = (total_span > 0.0f) ? current_span / total_span : 0.0f;

			const auto interpolated = interpolate(anchor.value, anchor.outTan, anchor.inTan, next.value, t, static_cast<int>(header.interpolation_type));

			float error_sq;
			if constexpr (std::is_same_v<T, uint32_t>) {
				error_sq = current.value - interpolated;
			} else {
				error_sq = glm::length(current.value - interpolated);
			}

			if (error_sq > max_error_sq) {
				header.tracks[write_index++] = current;
				anchor_index = i;
			}
		}

		// Always keep the last keyframe
		header.tracks[write_index++] = header.tracks.back();
		header.tracks.resize(write_index);
	}

	// ToDo! be aware of global sequences

	MDX::OptimizationStats MDX::optimize(float max_error) {
		OptimizationStats stats;

		const size_t material_count = materials.size();
		remove_unused_materials(*this);
		stats.materials_removed += material_count - materials.size();

		const size_t texture_count = textures.size();
		remove_unused_textures(*this);
		stats.textures_removed += texture_count - textures.size();

		if (sequences.empty()) {
			return stats;
		}

		for_each_track([&, max_error]<typename T>(TrackHeader<T>& header) {
			const auto track_size = header.tracks.size();

			remove_tracks_outside_sequences(header, sequences);
			reduce_track(header, sequences, max_error);

			if (header.interpolation_type == InterpolationType::none) {
				stats.constant_tracks += track_size;
				stats.constant_tracks_removed += track_size - header.tracks.size();
			} if (header.interpolation_type == InterpolationType::linear) {
				stats.linear_tracks += track_size;
				stats.linear_tracks_removed += track_size - header.tracks.size();
			} else if (header.interpolation_type == InterpolationType::hermite) {
				stats.hermite_tracks += track_size;
				stats.hermite_tracks_removed += track_size - header.tracks.size();
			} else if (header.interpolation_type == InterpolationType::bezier) {
				stats.bezier_tracks += track_size;
				stats.bezier_tracks_removed += track_size - header.tracks.size();
			}
		});

		return stats;
	}
}