module MDX;

import std;
import BinaryReader;
import <glm/glm.hpp>;

namespace mdx {
	void read_GEOS(BinaryReader& reader, MDX& mdx) {
		const uint32_t size = reader.read<uint32_t>();
		uint32_t total_size = 0;

		while (total_size < size) {
			total_size += reader.read<uint32_t>();

			Geoset geoset;
			reader.advance(4); // VRTX
			const uint32_t vertex_count = reader.read<uint32_t>();
			geoset.vertices = reader.read_vector<glm::vec3>(vertex_count);

			reader.advance(4); // NRMS
			const uint32_t normal_count = reader.read<uint32_t>();
			geoset.normals = reader.read_vector<glm::vec3>(normal_count);

			reader.advance(4); // PTYP
			const uint32_t face_type_groups_count = reader.read<uint32_t>();
			geoset.face_type_groups = reader.read_vector<uint32_t>(face_type_groups_count);

			reader.advance(4); // PCNT
			const uint32_t face_groups_count = reader.read<uint32_t>();
			geoset.face_groups = reader.read_vector<uint32_t>(face_groups_count);

			reader.advance(4); // PVTX
			const uint32_t faces_count = reader.read<uint32_t>();
			geoset.faces = reader.read_vector<uint16_t>(faces_count);

			reader.advance(4); // GNDX
			const uint32_t vertex_groups_count = reader.read<uint32_t>();
			geoset.vertex_groups = reader.read_vector<uint8_t>(vertex_groups_count);

			reader.advance(4); // MTGC
			const uint32_t matrix_group_count = reader.read<uint32_t>();
			geoset.matrix_groups = reader.read_vector<uint32_t>(matrix_group_count);

			reader.advance(4); // MATS
			const uint32_t matrix_indices_count = reader.read<uint32_t>();
			geoset.matrix_indices = reader.read_vector<uint32_t>(matrix_indices_count);

			geoset.material_id = reader.read<uint32_t>();
			geoset.selection_group = reader.read<uint32_t>();
			geoset.selection_flags = reader.read<uint32_t>();

			if (mdx.version > 800) {
				geoset.lod = reader.read<uint32_t>();
				geoset.lod_name = reader.read_string(80); // lod name
			} else {
				geoset.lod = 0;
			}

			geoset.extent = Extent(reader);
			const uint32_t extents_count = reader.read<uint32_t>();
			for (size_t i = 0; i < extents_count; i++) {
				geoset.sequence_extents.emplace_back(Extent(reader));
			}

			std::string tag = reader.read_string(4);

			if (tag == "TANG") {
				uint32_t structure_count = reader.read<uint32_t>();
				geoset.tangents = reader.read_vector<glm::vec4>(structure_count);
				tag = reader.read_string(4); // Maybe SKIN, maybe UVAS
			}

			if (tag == "SKIN") {
				// The count is the number of entries (8 per vertex); from v1400 on each entry is a u16.
				// Needs verification: the 1400 boundary follows upstream HiveWE; only v1800 (u16) and v1000-v1200 (u8)
				// files were measured, no stock v1300-v1700 sample exists. Keep in step with write_GEOS.
				const uint32_t skin_count = reader.read<uint32_t>();
				if (mdx.version >= 1400) {
					geoset.skin = reader.read_vector<uint16_t>(skin_count);
				} else {
					const std::vector<uint8_t> narrow = reader.read_vector<uint8_t>(skin_count);
					geoset.skin.assign(narrow.begin(), narrow.end());
				}
				reader.advance(4); // UVAS
			}

			const uint32_t texture_coordinate_sets_count = reader.read<uint32_t>();
			for (size_t i = 0; i < texture_coordinate_sets_count; i++) {
				reader.advance(4); // UVBS
				const uint32_t texture_coordinates_count = reader.read<uint32_t>();
				geoset.uv_sets.push_back(reader.read_vector<glm::vec2>(texture_coordinates_count));
			}

			mdx.geosets.push_back(std::move(geoset));
		}
	}

	// Transform from the old version format to our new v1100 based internal representation
	void read_MTLS_texs_pre_v1100(BinaryReader& reader, bool is_hd, uint32_t version, Material& material, int& unique_tracks) {
		reader.advance(4);
		const uint32_t layers_count = reader.read<uint32_t>();

		// These older versions encoded HD materials by having a layer for each PBR material
		// We combine these into the new format
		if (is_hd) {
			Layer layer{};
			layer.hd = is_hd;

			for (size_t i = 0; i < layers_count; i++) {
				const size_t reader_pos = reader.position;
				const uint32_t size = reader.read<uint32_t>();

				// Only the first layer's properties matter
				if (i > 0) {
					reader.advance(8);
					LayerTexture layer_texture;
					layer_texture.id = reader.read<uint32_t>();
					layer.textures.push_back(layer_texture);

					reader.advance(size - 16);
					continue;
				}

				layer.blend_mode = reader.read<uint32_t>();
				layer.shading_flags = reader.read<uint32_t>();
				LayerTexture layer_texture;
				layer_texture.id = reader.read<uint32_t>();
				layer.texture_animation_id = reader.read<uint32_t>();
				layer.coord_id = reader.read<uint32_t>();
				layer.alpha = reader.read<float>();

				layer.emissive_gain = reader.read<float>();
				layer.fresnel_color = reader.read<glm::vec3>();
				layer.fresnel_opacity = reader.read<float>();
				layer.fresnel_team_color = reader.read<float>();

				while (reader.position < reader_pos + size) {
					const TrackTag tag = static_cast<TrackTag>(reader.read<int32_t>());
					if (tag == TrackTag::KMTF) {
						layer_texture.KMTF = TrackHeader<uint32_t>(reader, unique_tracks++);
					} else if (tag == TrackTag::KMTA) {
						layer.KMTA = TrackHeader<float>(reader, unique_tracks++);
					} else if (tag == TrackTag::KMTE) {
						layer.KMTE = TrackHeader<float>(reader, unique_tracks++);
					} else if (tag == TrackTag::KFC3) {
						layer.KFC3 = TrackHeader<glm::vec3>(reader, unique_tracks++);
					} else if (tag == TrackTag::KFCA) {
						layer.KFCA = TrackHeader<float>(reader, unique_tracks++);
					} else if (tag == TrackTag::KFTC) {
						layer.KFTC = TrackHeader<float>(reader, unique_tracks++);
					} else {
						std::print("Unknown track tag {}\n", static_cast<uint32_t>(tag));
					}
				}

				layer.textures.push_back(layer_texture);
			}

			material.layers.push_back(std::move(layer));
		} else {
			for (size_t i = 0; i < layers_count; i++) {
				const size_t reader_pos = reader.position;
				const uint32_t size = reader.read<uint32_t>();
				Layer layer{};
				layer.hd = is_hd;
				layer.blend_mode = reader.read<uint32_t>();
				layer.shading_flags = reader.read<uint32_t>();

				// Stored in layer.textures only after the tracks below: its KMTF track is read into it.
				LayerTexture layer_texture;
				layer_texture.id = reader.read<uint32_t>();

				layer.texture_animation_id = reader.read<uint32_t>();
				layer.coord_id = reader.read<uint32_t>();
				layer.alpha = reader.read<float>();

				if (version > 800) {
					layer.emissive_gain = reader.read<float>();
					layer.fresnel_color = reader.read<glm::vec3>();
					layer.fresnel_opacity = reader.read<float>();
					layer.fresnel_team_color = reader.read<float>();
				}

				while (reader.position < reader_pos + size) {
					const TrackTag tag = static_cast<TrackTag>(reader.read<int32_t>());
					if (tag == TrackTag::KMTF) {
						layer_texture.KMTF = TrackHeader<uint32_t>(reader, unique_tracks++);
					} else if (tag == TrackTag::KMTA) {
						layer.KMTA = TrackHeader<float>(reader, unique_tracks++);
					} else if (tag == TrackTag::KMTE) {
						layer.KMTE = TrackHeader<float>(reader, unique_tracks++);
					} else if (tag == TrackTag::KFC3) {
						layer.KFC3 = TrackHeader<glm::vec3>(reader, unique_tracks++);
					} else if (tag == TrackTag::KFCA) {
						layer.KFCA = TrackHeader<float>(reader, unique_tracks++);
					} else if (tag == TrackTag::KFTC) {
						layer.KFTC = TrackHeader<float>(reader, unique_tracks++);
					} else {
						std::print("Unknown track tag {}\n", static_cast<uint32_t>(tag));
					}
				}

				layer.textures.push_back(std::move(layer_texture));
				material.layers.push_back(std::move(layer));
			}
		}
	}

	void read_MTLS_texs_post_v1100(BinaryReader& reader, Material& material, int& unique_tracks) {
		reader.advance(4);
		const uint32_t layers_count = reader.read<uint32_t>();
		for (size_t i = 0; i < layers_count; i++) {
			const size_t reader_pos = reader.position;
			Layer layer{};
			const uint32_t size = reader.read<uint32_t>();
			layer.blend_mode = reader.read<uint32_t>();
			layer.shading_flags = reader.read<uint32_t>();
			reader.advance(4); // skip texture_id
			layer.texture_animation_id = reader.read<uint32_t>();
			layer.coord_id = reader.read<uint32_t>();
			layer.alpha = reader.read<float>();

			layer.emissive_gain = reader.read<float>();
			layer.fresnel_color = reader.read<glm::vec3>();
			layer.fresnel_opacity = reader.read<float>();
			layer.fresnel_team_color = reader.read<float>();

			layer.hd = reader.read<uint32_t>();
			uint32_t texs = reader.read<uint32_t>();
			for (size_t j = 0; j < texs; j++) {
				LayerTexture layer_texture;
				layer_texture.id = reader.read<uint32_t>();
				uint32_t slot = reader.read<uint32_t>(); // always a garbage value?

				const TrackTag tag = static_cast<TrackTag>(reader.read<int32_t>());
				if (tag == TrackTag::KMTF) {
					layer_texture.KMTF = TrackHeader<uint32_t>(reader, unique_tracks++);
				} else {
					reader.advance(-4);
				}
				layer.textures.push_back(layer_texture);
			}

			while (reader.position < reader_pos + size) {
				const TrackTag tag = static_cast<TrackTag>(reader.read<int32_t>());
				if (tag == TrackTag::KMTA) {
					layer.KMTA = TrackHeader<float>(reader, unique_tracks++);
				} else if (tag == TrackTag::KMTE) {
					layer.KMTE = TrackHeader<float>(reader, unique_tracks++);
				} else if (tag == TrackTag::KFC3) {
					layer.KFC3 = TrackHeader<glm::vec3>(reader, unique_tracks++);
				} else if (tag == TrackTag::KFCA) {
					layer.KFCA = TrackHeader<float>(reader, unique_tracks++);
				} else if (tag == TrackTag::KFTC) {
					layer.KFTC = TrackHeader<float>(reader, unique_tracks++);
				} else {
					std::print("Unknown track tag {}\n", static_cast<uint32_t>(tag));
				}
			}

			material.layers.push_back(std::move(layer));
		}
	}

	void read_MTLS(BinaryReader& reader, MDX& mdx) {
		const uint32_t size = reader.read<uint32_t>();
		uint32_t total_size = 0;

		while (total_size < size) {
			total_size += reader.read<uint32_t>();

			Material material;
			material.priority_plane = reader.read<uint32_t>();
			material.flags = reader.read<uint32_t>();

			if (mdx.version < 1100) {
				bool is_hd = false;
				if (mdx.version == 900 || mdx.version == 1000) {
					is_hd = !reader.read_string(80).empty();
				}
				read_MTLS_texs_pre_v1100(reader, is_hd, mdx.version, material, mdx.unique_tracks);
			} else {
				read_MTLS_texs_post_v1100(reader, material, mdx.unique_tracks);
			}

			mdx.materials.push_back(std::move(material));
		}
	}

	void read_SEQS(BinaryReader& reader, MDX& mdx) {
		const uint32_t size = reader.read<uint32_t>();
		for (size_t i = 0; i < size / 132; i++) {
			mdx.sequences.push_back(Sequence {
				.name = reader.read_string(80),
				.start_frame = reader.read<uint32_t>(),
				.end_frame = reader.read<uint32_t>(),
				.movespeed = reader.read<float>(),
				.flags = reader.read<uint32_t>(),
				.rarity = reader.read<float>(),
				.sync_point = reader.read<uint32_t>(),
				.extent = Extent(reader),
			});
		}
	}

	void read_GEOA(BinaryReader& reader, MDX& mdx) {
		uint32_t remaining_size = reader.read<uint32_t>();

		while (remaining_size > 0) {
			const size_t reader_pos = reader.position;
			const uint32_t inclusive_size = reader.read<uint32_t>();
			remaining_size -= inclusive_size;

			GeosetAnimation animation;
			animation.alpha = reader.read<float>();
			animation.flags = reader.read<uint32_t>();
			animation.color = reader.read<glm::vec3>();
			animation.geoset_id = reader.read<uint32_t>();

			while (reader.position < reader_pos + inclusive_size) {
				const TrackTag tag = static_cast<TrackTag>(reader.read<int32_t>());
				if (tag == TrackTag::KGAO) {
					animation.KGAO = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KGAC) {
					animation.KGAC = TrackHeader<glm::vec3>(reader, mdx.unique_tracks++);
				} else {
					std::print("Unknown track tag {}\n", static_cast<uint32_t>(tag));
				}
			}

			mdx.animations.push_back(std::move(animation));
		}
	}

	void read_BONE(BinaryReader& reader, MDX& mdx) {
		const size_t reader_pos = reader.position;
		const uint32_t size = reader.read<uint32_t>();

		while (reader.position < reader_pos + size) {
			mdx.bones.push_back(Bone { 
				.node = Node(reader, mdx.unique_tracks),
				.geoset_id = reader.read<int32_t>(),
				.geoset_animation_id = reader.read<int32_t>(),
			});
		}
	}

	void read_TEXS(BinaryReader& reader, MDX& mdx) {
		const uint32_t size = reader.read<uint32_t>();
		for (size_t i = 0; i < size / 268; i++) {
			mdx.textures.push_back(Texture{
				.replaceable_id = reader.read<uint32_t>(),
				.file_name = reader.read_string(260),
				.flags = reader.read<uint32_t>(),
			});
		}
	}

	void read_GLBS(BinaryReader& reader, MDX& mdx) {
		const uint32_t size = reader.read<uint32_t>();
		mdx.global_sequences = reader.read_vector<uint32_t>(size / 4);
	}

	void read_LITE(BinaryReader& reader, MDX& mdx) {
		const uint32_t size = reader.read<uint32_t>();
		const size_t chunk_end = reader.position + size;
		if (chunk_end > reader.buffer.size()) {
			throw std::runtime_error("MDX LITE chunk extends past the end of the file");
		}

		while (reader.position < chunk_end) {
			Light light;
			const size_t node_reader_pos = reader.position;
			const uint32_t inclusive_size = reader.read<uint32_t>();
			const size_t entry_end = node_reader_pos + inclusive_size;
			if (entry_end > chunk_end) {
				throw std::runtime_error("MDX light entry extends past the end of the LITE chunk");
			}
			light.node = Node(reader, mdx.unique_tracks);
			light.type = reader.read<uint32_t>();
			if (mdx.version >= 1300) {
				light.shadow_casting = reader.read<uint32_t>();
			}
			light.attenuation_start = reader.read<float>();
			light.attenuation_end = reader.read<float>();
			light.color = reader.read<glm::vec3>();
			light.intensity = reader.read<float>();
			light.ambient_color = reader.read<glm::vec3>();
			light.ambient_intensity = reader.read<float>();
			if (mdx.version >= 1200) {
				light.shadow_intensity = reader.read<float>();
			} else {
				light.shadow_intensity = 0.4f;
			}
			if (mdx.version >= 1300) {
				light.shadow_casting_start = reader.read<float>();
				light.shadow_casting_end = reader.read<float>();
			}
			if (mdx.version >= 1600) {
				light.quadratic_falloff = reader.read<float>();
				light.linear_falloff = reader.read<float>();
				light.damping = reader.read<float>();
			}
			if (reader.position > entry_end) {
				throw std::runtime_error("MDX light entry is too small for the fixed light fields");
			}

			const auto keep_raw = [&](const size_t from, const size_t to) {
				light.raw_tracks.insert(light.raw_tracks.end(), reader.buffer.begin() + from, reader.buffer.begin() + to);
			};
			const auto keep_rest_raw = [&](const size_t from, const std::string_view reason) {
				std::println("MDX light \"{}\": {}, the rest of the entry is kept as raw bytes", light.node.name, reason);
				keep_raw(from, entry_end);
				reader.position = entry_end;
			};
			// A track that is not valid at this version (e.g. KLSS below 1300) is kept raw, as the writer would drop it
			const auto read_track = [&]<typename T>(TrackHeader<T>& target, const size_t tag_start, const bool in_version) {
				TrackHeader<T> track;
				try {
					track = TrackHeader<T>(reader, mdx.unique_tracks);
				} catch (const std::out_of_range&) {
					keep_rest_raw(tag_start, "a track runs past the end of the file");
					return;
				}
				if (reader.position > entry_end) {
					keep_rest_raw(tag_start, "a track overruns the light entry");
					return;
				}
				if (!in_version) {
					keep_raw(tag_start, reader.position);
					return;
				}
				mdx.unique_tracks++;
				target = std::move(track);
			};

			const bool v1300 = mdx.version >= 1300;
			const bool v1600 = mdx.version >= 1600;
			while (reader.position < entry_end) {
				const size_t tag_start = reader.position;
				if (tag_start + 4 > entry_end) {
					keep_rest_raw(tag_start, "trailing bytes shorter than a track tag");
					break;
				}
				const TrackTag tag = static_cast<TrackTag>(reader.read<int32_t>());
				if (tag == TrackTag::KLAS) {
					read_track(light.KLAS, tag_start, true);
				} else if (tag == TrackTag::KLAE) {
					read_track(light.KLAE, tag_start, true);
				} else if (tag == TrackTag::KLAC) {
					read_track(light.KLAC, tag_start, true);
				} else if (tag == TrackTag::KLAI) {
					read_track(light.KLAI, tag_start, true);
				} else if (tag == TrackTag::KLBI) {
					read_track(light.KLBI, tag_start, true);
				} else if (tag == TrackTag::KLBC) {
					read_track(light.KLBC, tag_start, true);
				} else if (tag == TrackTag::KLAV) {
					read_track(light.KLAV, tag_start, true);
				} else if (tag == TrackTag::KLSS) {
					read_track(light.KLSS, tag_start, v1300);
				} else if (tag == TrackTag::KLSE) {
					read_track(light.KLSE, tag_start, v1300);
				} else if (tag == TrackTag::KLQF) {
					read_track(light.KLQF, tag_start, v1600);
				} else if (tag == TrackTag::KLLF) {
					read_track(light.KLLF, tag_start, v1600);
				} else if (tag == TrackTag::KLDA) {
					read_track(light.KLDA, tag_start, v1600);
				} else {
					// A track does not state its value size, so an unknown one cannot be skipped
					keep_rest_raw(tag_start, std::format("unknown track tag {}", static_cast<uint32_t>(tag)));
				}
			}
			mdx.lights.push_back(std::move(light));
		}
	}

	void read_HELP(BinaryReader& reader, MDX& mdx) {
		const size_t reader_pos = reader.position;
		const uint32_t size = reader.read<uint32_t>();
		while (reader.position < reader_pos + size) {
			mdx.help_bones.push_back(Node(reader, mdx.unique_tracks));
		}
	}

	void read_ATCH(BinaryReader& reader, MDX& mdx) {
		const size_t reader_pos = reader.position;
		const uint32_t size = reader.read<uint32_t>();

		while (reader.position < reader_pos + size) {
			Attachment attachment;
			const int node_reader_pos = reader.position;
			const uint32_t inclusive_size = reader.read<uint32_t>();
			attachment.node = Node(reader, mdx.unique_tracks);
			attachment.path = reader.read_string(256);
			attachment.reserved = reader.read<uint32_t>();
			attachment.attachment_id = reader.read<uint32_t>();
			while (reader.position < node_reader_pos + inclusive_size) {
				const TrackTag tag = static_cast<TrackTag>(reader.read<int32_t>());
				attachment.KATV = TrackHeader<float>(reader, mdx.unique_tracks++);
				if (tag != TrackTag::KATV) {
					std::print("Unknown track tag {}\n", static_cast<uint32_t>(tag));
				}
			}
			mdx.attachments.push_back(std::move(attachment));
		}
	}

	void read_PIVT(BinaryReader& reader, MDX& mdx) {
		const uint32_t size = reader.read<uint32_t>();
		mdx.pivots = reader.read_vector<glm::vec3>(size / 12);
	}

	void read_PREM(BinaryReader& reader, MDX& mdx) {
		const size_t reader_pos = reader.position;
		const uint32_t size = reader.read<uint32_t>();

		while (reader.position < reader_pos + size) {
			ParticleEmitter1 emitter;
			const int node_reader_pos = reader.position;
			const uint32_t inclusive_size = reader.read<uint32_t>();
			emitter.node = Node(reader, mdx.unique_tracks);
			emitter.emission_rate = reader.read<float>();
			emitter.gravity = reader.read<float>();
			emitter.longitude = reader.read<float>();
			emitter.latitude = reader.read<float>();
			emitter.path = reader.read_string(260);
			emitter.life_span = reader.read<float>();
			emitter.speed = reader.read<float>();
			while (reader.position < node_reader_pos + inclusive_size) {
				const TrackTag tag = static_cast<TrackTag>(reader.read<int32_t>());
				if (tag == TrackTag::KPEE) {
					emitter.KPEE = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KPEG) {
					emitter.KPEG = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KPLN) {
					emitter.KPLN = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KPLT) {
					emitter.KPLT = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KPEL) {
					emitter.KPEL = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KPES) {
					emitter.KPES = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KPEV) {
					emitter.KPEV = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else {
					std::print("Unknown track tag {}\n", static_cast<uint32_t>(tag));
				}
			}
			mdx.emitters1.push_back(std::move(emitter));
		}
	}

	void read_PRE2(BinaryReader& reader, MDX& mdx) {
		const size_t reader_pos = reader.position;
		const uint32_t size = reader.read<uint32_t>();

		while (reader.position < reader_pos + size) {
			ParticleEmitter2 emitter2;
			const int node_reader_pos = reader.position;
			const uint32_t inclusive_size = reader.read<uint32_t>();
			emitter2.node = Node(reader, mdx.unique_tracks);

			emitter2.speed = reader.read<float>();
			emitter2.variation = reader.read<float>();
			emitter2.latitude = reader.read<float>();
			emitter2.gravity = reader.read<float>();
			emitter2.life_span = reader.read<float>();
			emitter2.emission_rate = reader.read<float>();
			emitter2.length = reader.read<float>();
			emitter2.width = reader.read<float>();
			emitter2.filter_mode = reader.read<uint32_t>();
			emitter2.rows = reader.read<uint32_t>();
			emitter2.columns = reader.read<uint32_t>();
			emitter2.head_or_tail = reader.read<uint32_t>();
			emitter2.tail_length = reader.read<float>();
			emitter2.time_middle = reader.read<float>();

			emitter2.start_segment_color = reader.read<glm::vec3>();
			emitter2.middle_segment_color = reader.read<glm::vec3>();
			emitter2.end_segment_color = reader.read<glm::vec3>();

			emitter2.segment_alphas = reader.read<glm::u8vec3>();
			emitter2.segment_scaling = reader.read<glm::vec3>();
			emitter2.head_intervals = reader.read<glm::uvec3>();
			emitter2.head_decay_intervals = reader.read<glm::uvec3>();
			emitter2.tail_intervals = reader.read<glm::uvec3>();
			emitter2.tail_decay_intervals = reader.read<glm::uvec3>();

			emitter2.texture_id = reader.read<uint32_t>();
			emitter2.squirt = reader.read<uint32_t>();
			emitter2.priority_plane = reader.read<uint32_t>();
			emitter2.replaceable_id = reader.read<uint32_t>();

			while (reader.position < node_reader_pos + inclusive_size) {
				const TrackTag tag = static_cast<TrackTag>(reader.read<int32_t>());
				if (tag == TrackTag::KP2S) {
					emitter2.KP2S = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KP2R) {
					emitter2.KP2R = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KP2L) {
					emitter2.KP2L = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KP2G) {
					emitter2.KP2G = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KP2E) {
					emitter2.KP2E = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KP2N) {
					emitter2.KP2N = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KP2W) {
					emitter2.KP2W = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KP2V) {
					emitter2.KP2V = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else {
					std::print("Unknown track tag {}\n", static_cast<uint32_t>(tag));
				}
			}
			mdx.emitters2.push_back(std::move(emitter2));
		}
	}

	void read_RIBB(BinaryReader& reader, MDX& mdx) {
		const size_t reader_pos = reader.position;
		const uint32_t size = reader.read<uint32_t>();

		while (reader.position < reader_pos + size) {
			RibbonEmitter emitter;
			const int node_reader_pos = reader.position;
			const uint32_t inclusive_size = reader.read<uint32_t>();
			emitter.node = Node(reader, mdx.unique_tracks);
			emitter.height_above = reader.read<float>();
			emitter.height_below = reader.read<float>();
			emitter.alpha = reader.read<float>();
			emitter.color = reader.read<glm::vec3>();
			emitter.life_span = reader.read<float>();
			emitter.texture_slot = reader.read<uint32_t>();
			emitter.emission_rate = reader.read<uint32_t>();
			emitter.rows = reader.read<uint32_t>();
			emitter.columns = reader.read<uint32_t>();
			emitter.material_id = reader.read<uint32_t>();
			emitter.gravity = reader.read<float>();
			while (reader.position < node_reader_pos + inclusive_size) {
				const TrackTag tag = static_cast<TrackTag>(reader.read<int32_t>());
				if (tag == TrackTag::KRHA) {
					emitter.KRHA = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KRHB) {
					emitter.KRHB = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KRAL) {
					emitter.KRAL = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KRCO) {
					emitter.KRCO = TrackHeader<glm::vec3>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KRTX) {
					emitter.KRTX = TrackHeader<uint32_t>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KRVS) {
					emitter.KRVS = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else {
					std::print("Unknown track tag {}\n", static_cast<uint32_t>(tag));
				}
			}
			mdx.ribbons.push_back(std::move(emitter));
		}
	}

	void read_EVTS(BinaryReader& reader, MDX& mdx) {
		const size_t reader_pos = reader.position;
		const uint32_t size = reader.read<uint32_t>();

		while (reader.position < reader_pos + size) {
			EventObject evt;
			evt.node = Node(reader, mdx.unique_tracks);
			reader.advance(4); // read KEVT
			uint32_t count = reader.read<uint32_t>();
			evt.global_sequence_id = reader.read<int32_t>(); // signed
			evt.times = reader.read_vector<uint32_t>(count);
			mdx.event_objects.push_back(std::move(evt));
		}
	}

	void read_CLID(BinaryReader& reader, MDX& mdx) {
		const size_t reader_pos = reader.position;
		const uint32_t size = reader.read<uint32_t>();

		while (reader.position < reader_pos + size) {
			CollisionShape shape;
			shape.node = Node(reader, mdx.unique_tracks);
			shape.type = static_cast<CollisionShape::Shape>(reader.read<uint32_t>());

			for (int i = 0; i < 3; i++) {
				if (reader.remaining() <= 0) {
					shape.vertices[0][i] = 0.f;
				} else {
					shape.vertices[0][i] = reader.read<float>();
				}
			}

			if (shape.type != CollisionShape::Shape::Sphere) {
				for (int i = 0; i < 3; i++) {
					if (reader.remaining() <= 0) {
						shape.vertices[1][i] = 0.f;
					} else {
						shape.vertices[1][i] = reader.read<float>();
					}
				}
			}

			if (shape.type == CollisionShape::Shape::Sphere || shape.type == CollisionShape::Shape::Cylinder) {
				if (reader.remaining() > 0) {
					shape.radius = reader.read<float>();
				} else {
					shape.radius = 0.f;
				}
			}
			mdx.collision_shapes.push_back(std::move(shape));
		}
	}

	void read_CORN(BinaryReader& reader, MDX& mdx) {
		const size_t reader_pos = reader.position;
		const uint32_t size = reader.read<uint32_t>();

		while (reader.position < reader_pos + size) {
			CornEmitter emitter;
			const int node_reader_pos = reader.position;
			const uint32_t inclusive_size = reader.read<uint32_t>();
			emitter.node = Node(reader, mdx.unique_tracks);
			emitter.data = reader.read_vector<uint8_t>(inclusive_size - (reader.position - node_reader_pos));
			mdx.corn_emitters.push_back(std::move(emitter));
		}
	}

	/// Skips a camera track with an unknown tag (KCVS, DOF tracks), reader positioned after the tag.
	/// The value size is unknown, so try the common sizes and accept one that ends exactly at the entry end
	/// or right before a known camera track tag. Returns false if none fits.
	bool skip_unknown_camera_track(BinaryReader& reader, const size_t entry_end) {
		const size_t start = reader.position;
		if (start + 12 > entry_end) {
			return false;
		}
		const uint64_t key_count = reader.read<uint32_t>();
		const auto interpolation = static_cast<InterpolationType>(reader.read<int32_t>());
		reader.advance(4); // global sequence id
		const uint64_t values_per_key =
			(interpolation == InterpolationType::hermite || interpolation == InterpolationType::bezier) ? 3 : 1;

		for (const uint64_t value_size : {4, 12, 8, 16}) {
			const uint64_t end = start + 12 + key_count * (4 + value_size * values_per_key);
			if (end == entry_end) {
				reader.position = end;
				return true;
			}
			if (end + 4 <= entry_end) {
				uint32_t next;
				std::memcpy(&next, reader.buffer.data() + end, 4);
				const auto next_tag = static_cast<TrackTag>(next);
				if (next_tag == TrackTag::KCTR || next_tag == TrackTag::IDUF || next_tag == TrackTag::ELAF || next_tag == TrackTag::PTSF ||next_tag == TrackTag::KCRL || next_tag == TrackTag::KTTR) {
					reader.position = end;
					return true;
				}
			}
		}
		reader.position = start;
		return false;
	}

	void read_CAMS(BinaryReader& reader, MDX& mdx) {
		const uint32_t size = reader.read<uint32_t>();
		const size_t chunk_end = reader.position + size;
		if (chunk_end > reader.buffer.size()) {
			throw std::runtime_error("MDX CAMS chunk extends past the end of the file");
		}

		while (reader.position < chunk_end) {
			const size_t entry_start = reader.position;
			const CameraWord word = decode_camera_word(reader.read<uint32_t>());
			const size_t entry_end = entry_start + word.size;
			if (entry_end > chunk_end) {
				throw std::runtime_error("MDX camera entry extends past the end of the CAMS chunk");
			}

			Camera camera;
			camera.name = reader.read_string(80);
			camera.position = reader.read<glm::vec3>();
			camera.field_of_view = reader.read<float>();
			camera.far_clip = reader.read<float>();
			camera.near_clip = reader.read<float>();
			reader.advance(word.extra);
			camera.target_position = reader.read<glm::vec3>();
			camera.variant = static_cast<uint8_t>(word.variant);

			while (reader.position < entry_end) {
				const size_t tag_start = reader.position;
				const TrackTag tag = static_cast<TrackTag>(reader.read<int32_t>());
				if (tag == TrackTag::IDUF) {
					camera.IDUF = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::ELAF) {
					camera.ELAF = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::PTSF) {
					camera.PTSF = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KCTR) {
					camera.KCTR = TrackHeader<glm::vec3>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KCRL) {
					camera.KCRL = TrackHeader<float>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KTTR) {
					camera.KTTR = TrackHeader<glm::vec3>(reader, mdx.unique_tracks++);
				} else if (skip_unknown_camera_track(reader, entry_end)) {
					// Kept verbatim so it is written back
					camera.raw_tracks.emplace_back(reader.buffer.begin() + tag_start, reader.buffer.begin() + reader.position);
				} else {
					// Unknown data with no recognisable track layout: keep the rest of the entry verbatim
					reader.position = entry_end;
					camera.raw_tracks.emplace_back(reader.buffer.begin() + tag_start, reader.buffer.begin() + entry_end);
				}
			}
			if (reader.position != entry_end) {
				throw std::runtime_error("MDX camera track data overruns the camera entry");
			}

			mdx.cameras.push_back(std::move(camera));
		}
	}

	void read_BPOS(BinaryReader& reader, MDX& mdx) {
		const uint32_t size = reader.read<uint32_t>();
		mdx.bind_poses = reader.read_vector<float>(reader.read<uint32_t>() * 12);
	}

	void read_TXAN(BinaryReader& reader, MDX& mdx) {
		const size_t reader_pos = reader.position;
		const uint32_t size = reader.read<uint32_t>();

		while (reader.position < reader_pos + size) {
			const size_t entry_start = reader.position;
			const uint32_t inclusive_size = reader.read<uint32_t>();

			TextureAnimation animation;
			while (reader.position < entry_start + inclusive_size) {
				const TrackTag tag = static_cast<TrackTag>(reader.read<int32_t>());
				if (tag == TrackTag::KTAT) {
					animation.KTAT = TrackHeader<glm::vec3>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KTAR) {
					animation.KTAR = TrackHeader<glm::quat>(reader, mdx.unique_tracks++);
				} else if (tag == TrackTag::KTAS) {
					animation.KTAS = TrackHeader<glm::vec3>(reader, mdx.unique_tracks++);
				} else {
					std::print("Unknown track tag {}\n", static_cast<uint32_t>(tag));
				}
			}

			mdx.texture_animations.push_back(std::move(animation));
		}
	}

	void read_DILG(BinaryReader& reader, MDX& mdx) {
		const uint32_t size = reader.read<uint32_t>();
		if (size % 4 != 0) {
			throw std::runtime_error(std::format("MDX DILG chunk size {} is not a multiple of 4", size));
		}
		if (size > reader.buffer.size() - reader.position) {
			throw std::runtime_error("MDX DILG chunk extends past the end of the file");
		}
		// A second DILG chunk appends, so no entry is lost
		mdx.gliders.append_range(reader.read_vector<uint32_t>(size / 4));
	}

	void read_FAFX(BinaryReader& reader, MDX& mdx) {
		const uint32_t size = reader.read<uint32_t>();
		for (size_t i = 0; i < size / 340; i++) {
			FaceFX facefx;
			facefx.name = reader.read_string(80);
			facefx.path = reader.read_string(260);
			mdx.facefxes.push_back(std::move(facefx));
		}
	}

	void MDX::load(BinaryReader& reader) {
		const std::string magic_number = reader.read_string(4);
		if (magic_number != "MDLX") {
			std::print("Incorrect file magic number, expected MDLX but got {}\n", magic_number);
			return;
		}

		while (reader.remaining() > 0) {
			uint32_t header = reader.read<uint32_t>();

			switch (static_cast<ChunkTag>(header)) {
				case ChunkTag::VERS:
					reader.advance(4);
					version = reader.read<uint32_t>();
					break;
				case ChunkTag::MODL:
					reader.advance(4);
					name = reader.read_string(80);
					animation_filename = reader.read_string(260);
					extent = Extent(reader);
					blend_time = reader.read<uint32_t>();
					break;
				case ChunkTag::GEOS:
					read_GEOS(reader, *this);
					break;
				case ChunkTag::MTLS:
					read_MTLS(reader, *this);
					break;
				case ChunkTag::SEQS:
					read_SEQS(reader, *this);
					break;
				case ChunkTag::GLBS:
					read_GLBS(reader, *this);
					break;
				case ChunkTag::GEOA:
					read_GEOA(reader, *this);
					break;
				case ChunkTag::BONE:
					read_BONE(reader, *this);
					break;
				case ChunkTag::TEXS:
					read_TEXS(reader, *this);
					break;
				case ChunkTag::LITE:
					read_LITE(reader, *this);
					break;
				case ChunkTag::HELP:
					read_HELP(reader, *this);
					break;
				case ChunkTag::ATCH:
					read_ATCH(reader, *this);
					break;
				case ChunkTag::PIVT:
					read_PIVT(reader, *this);
					break;
				case ChunkTag::PREM:
					read_PREM(reader, *this);
					break;
				case ChunkTag::PRE2:
					read_PRE2(reader, *this);
					break;
				case ChunkTag::RIBB:
					read_RIBB(reader, *this);
					break;
				case ChunkTag::EVTS:
					read_EVTS(reader, *this);
					break;
				case ChunkTag::CLID:
					read_CLID(reader, *this);
					break;
				case ChunkTag::CORN:
					read_CORN(reader, *this);
					break;
				case ChunkTag::FAFX:
					read_FAFX(reader, *this);
					break;
				case ChunkTag::CAMS:
					read_CAMS(reader, *this);
					break;
				case ChunkTag::BPOS:
					read_BPOS(reader, *this);
					break;
				case ChunkTag::TXAN:
					read_TXAN(reader, *this);
					break;
				case ChunkTag::DILG:
					read_DILG(reader, *this);
					break;
				default:
					reader.advance(reader.read<uint32_t>());
			}
		}

		validate();
	}
}
