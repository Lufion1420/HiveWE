#pragma once

#include <glad/glad.h>
#include <memory>
#include <vector>
#include <QElapsedTimer>
#define QT_NO_OPENGL
#include <QObject>
#include <QOpenGLWidget>
#include <model_editor/model_editor_camera.h>

import EditableMesh;
import SkeletalModelInstance;
import MDX;
import Shader;

class ModelEditorGLWidget: public QOpenGLWidget {
	Q_OBJECT

  public:
	/// frame_changed is emitted at most this often while playing. Scrubbing and sequence changes emit at once.
	static constexpr int frame_signal_interval_ms = 250;

	QElapsedTimer elapsed_timer;
	GLuint vao;

	double delta = 0.0;

	ModelEditorGLWidget() = delete;
	/// `texture_source` belongs to `model` alone (a model opened from disk); set_model drops it.
	explicit ModelEditorGLWidget(
		QWidget* parent,
		std::shared_ptr<mdx::MDX> model,
		bool compact_preview = false,
		ModelTextureSource texture_source = {}
	);
	~ModelEditorGLWidget() = default;

	void initializeGL() override;
	void resizeGL(int w, int h) override;
	void paintGL() override;

	void keyPressEvent(QKeyEvent* event) override;
	void keyReleaseEvent(QKeyEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;

	void set_model(std::shared_ptr<mdx::MDX> model);

	/// Playback. These only change view state, never the model. Calls before the GL context exists are kept and applied on init.
	/// Out-of-range sequence indices are ignored.
	void set_sequence(int index);
	/// Moves the playhead inside the current sequence (clamped). Used by the timeline slider; the pose follows on the next paint.
	void set_frame(double frame);
	void set_speed(double new_speed);
	void set_loop(bool new_loop);
	void set_playing(bool new_playing);
	/// Hides or shows one geoset (index into mdx->geosets) in the viewport. The model data is untouched.
	void set_geoset_visible(int index, bool visible);
	bool geoset_visible(int index) const;
	bool wireframe_enabled() const { return wireframe; }
	void set_wireframe(bool enabled);
	void reset_camera();
	/// Pauses `ms` into the current sequence (clamped), with particles and global-sequence tracks rebuilt in fixed steps from the sequence
	/// start, so the same call always shows the same pose. For the --qa-model-editor-script "at" step. False when nothing is shown.
	bool show_exact_time(double ms);

	int sequence() const { return current_sequence; }
	double frame() const { return playhead; }
	bool playing() const { return is_playing; }
	bool loop() const { return is_looping; }
	double speed() const { return playback_speed; }
	/// First and last frame (ms) of the current sequence, 0 and 0 without one.
	double sequence_start() const;
	double sequence_end() const;

	std::shared_ptr<mdx::MDX> mdx;
	std::shared_ptr<EditableMesh> mesh;
	SkeletalModelInstance skeleton;
	std::shared_ptr<Shader> shader_sd;
	std::shared_ptr<Shader> shader_hd;

	int64_t optimization_file_size_reduction = 0;
	float optimization_file_size_reduction_percent = 0.f;

	ModelEditorCamera camera;
	bool compact_preview = false;
	bool initialized = false;

	void recenter_camera();

  signals:
	/// The playhead moved (throttled while playing). `frame` is in ms of the model's timeline, not relative to the sequence start.
	void frame_changed(double frame);
	void sequence_changed(int index);
	/// Emitted when playback stops by itself (a non-looping sequence reached its end) as well as on set_playing.
	void playing_changed(bool playing);

  private:
	bool valid_sequence(int index) const;
	void apply_geoset_mask();
	/// Builds mesh and skeleton for `mdx` (GL context must be current), restoring the requested sequence or picking a preview one.
	void build_scene();

	ModelTextureSource texture_source;
	int current_sequence = -1;
	double playhead = 0.0;
	/// The last frame handed to the skeleton; a smaller new frame means the sequence wrapped or was scrubbed back.
	int last_skeleton_frame = 0;
	double playback_speed = 1.0;
	bool is_looping = true;
	bool is_playing = true;
	bool wireframe = false;
	std::vector<bool> hidden_geosets;
	QElapsedTimer frame_signal_timer;
};
