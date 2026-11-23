#include "model_editor_glwidget.h"

#include <QTimer>

import std;
import OpenGLUtilities;
import BinaryReader;
import Hierarchy;
import MDX;
import Camera;
import ResourceManager;
import TimelineMath;

namespace fs = std::filesystem;

// 2025/07/30 These aren't in the class due to some compiler issue with modules
InputHandler my_input_handler;

ModelEditorGLWidget::ModelEditorGLWidget(QWidget* parent, std::shared_ptr<mdx::MDX> mdx, const bool compact_preview) :
	QOpenGLWidget(parent),
	mdx(mdx),
	compact_preview(compact_preview) {
	if (!compact_preview) {
		makeCurrent();
	}

	setMouseTracking(true);
	setFocus();
	setFocusPolicy(Qt::WheelFocus);

	connect(this, &QOpenGLWidget::frameSwapped, [&]() { update(); });
}

void ModelEditorGLWidget::initializeGL() {
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glClearColor(0, 0, 0, 1);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	// A dock tab that is re-parented gets a new context and lands here again: build_scene keeps the chosen sequence and playhead.
	build_scene();

	shader_sd = resource_manager.load<Shader>({ "data/shaders/editable_mesh_sd.vert", "data/shaders/editable_mesh_sd.frag" }).value();
	shader_hd = resource_manager.load<Shader>({ "data/shaders/editable_mesh_hd.vert", "data/shaders/editable_mesh_hd.frag" }).value();
	initialized = true;
}

void ModelEditorGLWidget::build_scene() {
	if (!mdx) {
		return;
	}

	try {
		mesh = std::make_shared<EditableMesh>(mdx, std::nullopt);
	} catch (const std::exception& e) {
		// EditableMesh refuses skin data the renderer cannot hold (a bone index above 255): show nothing rather than crash.
		std::println("Model Editor: cannot display the model: {}", e.what());
		mesh.reset();
		skeleton = {};
		return;
	}
	apply_geoset_mask();
	skeleton = SkeletalModelInstance(mdx);

	if (valid_sequence(current_sequence)) {
		skeleton.set_sequence(current_sequence);
		skeleton.current_frame = static_cast<int>(playhead);
		last_skeleton_frame = skeleton.current_frame;
	} else {
		SkeletalModelInstance::pick_preview_sequence(skeleton, *mdx);
		current_sequence = valid_sequence(skeleton.sequence_index) ? skeleton.sequence_index : -1;
		playhead = skeleton.current_frame;
		last_skeleton_frame = skeleton.current_frame;
	}

	recenter_camera();
	frame_signal_timer.start();
	emit sequence_changed(current_sequence);
	emit frame_changed(playhead);
}

void ModelEditorGLWidget::resizeGL(const int w, const int h) {
	glViewport(0, 0, w, h);
	if (h <= 0) {
		return;
	}
	camera.aspect_ratio = double(w) / h;
	camera.update(delta);
	delta = elapsed_timer.nsecsElapsed() / 1'000'000'000.0;
}

void ModelEditorGLWidget::paintGL() {
	makeCurrent();

	delta = elapsed_timer.nsecsElapsed() / 1'000'000'000.0;
	elapsed_timer.start();

	if (!mesh || !mdx) {
		glClearColor(0.18f, 0.18f, 0.18f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		return;
	}

	// The playhead is owned here (a double, so slow speeds and fast frame rates do not lose time) and handed to the skeleton.
	// Global-sequence tracks follow the wall clock inside SkeletalModelInstance, so pausing or scrubbing does not freeze them.
	bool wrapped = false;
	if (valid_sequence(current_sequence)) {
		if (is_playing) {
			const auto advanced = advance_frame(playhead, sequence_start(), sequence_end(), is_looping, playback_speed, delta);
			playhead = advanced.frame;
			wrapped = advanced.wrapped;
			if (advanced.finished) {
				is_playing = false;
				emit playing_changed(false);
				emit frame_changed(playhead);
			} else if (frame_signal_timer.elapsed() >= frame_signal_interval_ms) {
				frame_signal_timer.restart();
				emit frame_changed(playhead);
			}
		}
		// The skeleton only sees the frame we hand it, so a wrap (or a backwards scrub) has to be reported explicitly or the
		// squirt-style particle emitters never restart.
		const int new_frame = static_cast<int>(playhead);
		wrapped = wrapped || new_frame < last_skeleton_frame;
		last_skeleton_frame = new_frame;
		skeleton.current_frame = new_frame;
	}

	skeleton.update_location(glm::vec3(0.f), glm::quat(), glm::vec3(1.f));
	skeleton.update(is_playing ? delta * playback_speed : 0.0, false, wrapped);

	camera.update(delta);

	glBindVertexArray(vao);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(true);
	glClearColor(0.3f, 0.3f, 0.3f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);

	// Opaque passes — depth test/write on, blend off (state restored per-layer inside render_opaque)
	glEnable(GL_BLEND);
	glDepthMask(true);
	glEnable(GL_DEPTH_TEST);

	shader_sd->use();
	mesh->render_opaque(false, 0, skeleton, camera.projection_view, camera.direction);
	shader_hd->use();
	mesh->render_opaque(true, 0, skeleton, camera.projection_view, camera.direction);

	// Transparent passes — depth write off; depth test/blend func driven per-layer inside render_transparent
	glEnable(GL_BLEND);
	glDepthMask(false);

	shader_sd->use();
	mesh->render_transparent(false, 0, skeleton, camera.projection_view, camera.direction);
	shader_hd->use();
	mesh->render_transparent(true, 0, skeleton, camera.projection_view, camera.direction);

	glDepthMask(true);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	mesh->render_particles(skeleton, camera.projection_view, camera.X, camera.Y, camera.direction);

	glEnable(GL_BLEND);

	glBindVertexArray(0);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void ModelEditorGLWidget::keyPressEvent(QKeyEvent* event) {
	if (event->key() == Qt::Key_Space) {
		// Let the Model Editor window toggle playback.
		event->ignore();
		return;
	}
	my_input_handler.keys_pressed.emplace(event->key());
}

void ModelEditorGLWidget::keyReleaseEvent(QKeyEvent* event) {
	my_input_handler.keys_pressed.erase(event->key());
}

void ModelEditorGLWidget::mouseMoveEvent(QMouseEvent* event) {
	my_input_handler.mouse_move_event(event);
	camera.mouse_move_event(event, my_input_handler);
}

void ModelEditorGLWidget::mousePressEvent(QMouseEvent* event) {
	makeCurrent();

	camera.mouse_press_event(event);
}

void ModelEditorGLWidget::mouseReleaseEvent(QMouseEvent* event) {
	camera.mouse_release_event(event);
}

void ModelEditorGLWidget::wheelEvent(QWheelEvent* event) {
	camera.mouse_scroll_event(event);
}

void ModelEditorGLWidget::set_model(std::shared_ptr<mdx::MDX> model) {
	if (mdx == model) {
		return;
	}

	mdx = std::move(model);
	current_sequence = -1;
	playhead = 0.0;
	hidden_geosets.clear();

	if (!mdx) {
		if (initialized) {
			makeCurrent();
		}
		mesh.reset();
		skeleton = {};
		update();
		return;
	}

	if (!initialized) {
		update();
		return;
	}

	makeCurrent();
	build_scene();
	update();
}

bool ModelEditorGLWidget::valid_sequence(const int index) const {
	return mdx && index >= 0 && index < static_cast<int>(mdx->sequences.size());
}

double ModelEditorGLWidget::sequence_start() const {
	return valid_sequence(current_sequence) ? mdx->sequences[current_sequence].start_frame : 0.0;
}

double ModelEditorGLWidget::sequence_end() const {
	return valid_sequence(current_sequence) ? mdx->sequences[current_sequence].end_frame : 0.0;
}

void ModelEditorGLWidget::set_sequence(const int index) {
	if (!valid_sequence(index)) {
		return;
	}

	current_sequence = index;
	playhead = sequence_start();
	last_skeleton_frame = static_cast<int>(playhead);
	if (initialized && mesh) {
		// set_sequence only recalculates keyframe data and the frame; it needs no GL.
		skeleton.set_sequence(index);
	}
	emit sequence_changed(index);
	emit frame_changed(playhead);
}

void ModelEditorGLWidget::set_frame(const double frame) {
	if (!valid_sequence(current_sequence)) {
		return;
	}
	playhead = clamp_frame(frame, sequence_start(), sequence_end());
	emit frame_changed(playhead);
}

void ModelEditorGLWidget::set_speed(const double new_speed) {
	playback_speed = std::max(0.0, new_speed);
}

void ModelEditorGLWidget::set_loop(const bool new_loop) {
	is_looping = new_loop;
}

void ModelEditorGLWidget::set_playing(const bool new_playing) {
	if (is_playing == new_playing) {
		return;
	}
	// Playing again from the last frame of a non-looping sequence starts it over.
	if (new_playing && !is_looping && playhead >= sequence_end()) {
		playhead = sequence_start();
		emit frame_changed(playhead);
	}
	is_playing = new_playing;
	emit playing_changed(is_playing);
}

void ModelEditorGLWidget::set_geoset_visible(const int index, const bool visible) {
	if (!mdx || index < 0 || index >= static_cast<int>(mdx->geosets.size())) {
		return;
	}
	if (hidden_geosets.size() < mdx->geosets.size()) {
		hidden_geosets.resize(mdx->geosets.size(), false);
	}
	hidden_geosets[index] = !visible;
	apply_geoset_mask();
}

bool ModelEditorGLWidget::geoset_visible(const int index) const {
	return !(index >= 0 && index < static_cast<int>(hidden_geosets.size()) && hidden_geosets[index]);
}

void ModelEditorGLWidget::apply_geoset_mask() {
	if (mesh) {
		mesh->hidden_geosets = hidden_geosets;
	}
}

void ModelEditorGLWidget::set_wireframe(const bool enabled) {
	wireframe = enabled;
}

void ModelEditorGLWidget::reset_camera() {
	camera.reset();
	recenter_camera();
}

void ModelEditorGLWidget::recenter_camera() {
	if (!mesh || !mesh->mdx) {
		return;
	}

	// Fit mesh extents AABB into screen. Some assets do not expose a usable sequence extent.
	const bool has_sequence_extent = skeleton.sequence_index >= 0 && skeleton.sequence_index < static_cast<int>(mesh->mdx->sequences.size());
	const auto& extent = has_sequence_extent ? mesh->mdx->sequences[skeleton.sequence_index].extent : mesh->mdx->extent;
	const glm::vec3 size = extent.maximum - extent.minimum;
	const float radius = length(size) * 0.5f * 1.1f;
	const float dist = radius / std::sin(camera.fov_rad * 0.5f);
	camera.distance = dist;
	camera.position.z = extent.minimum.z + size.z / 2.f;
}
