#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSlider>
#include <QToolButton>
#include <QWidget>

/// Play button, restart button, scrub slider, "340 / 1000 ms" readout, Loop box and speed list of the Model Editor. It holds no
/// playback state of its own: the viewport widget does, and this only shows it (set_* never emit) and reports what the user does.
class AnimationTimeline : public QWidget {
	Q_OBJECT

  public:
	explicit AnimationTimeline(QWidget* parent = nullptr);

	/// Frame range (ms) of the shown sequence.
	void set_range(double start, double end);
	/// The playhead, in the same ms as the range.
	void set_frame(double frame);
	void set_playing(bool playing);
	void set_loop(bool loop);
	void set_speed(double speed);

	static constexpr int slider_steps = 1000;

  signals:
	void play_toggled(bool playing);
	void restart_requested();
	/// The user dragged or clicked the slider; `frame` is inside the range.
	void frame_scrubbed(double frame);
	void loop_toggled(bool loop);
	void speed_changed(double speed);

  private:
	void refresh_readout();
	void refresh_icons();
	void changeEvent(QEvent* event) override;

	QToolButton* play_button;
	QToolButton* restart_button;
	QSlider* slider;
	QLabel* readout;
	QCheckBox* loop_box;
	QComboBox* speed_box;

	double range_start = 0.0;
	double range_end = 0.0;
	double frame = 0.0;
	bool playing = false;
};
