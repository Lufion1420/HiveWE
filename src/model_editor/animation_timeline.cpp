#include "animation_timeline.h"

#include <QEvent>
#include <QHBoxLayout>

#include "main_window/theme.h"

import std;
import TimelineMath;

AnimationTimeline::AnimationTimeline(QWidget* parent) : QWidget(parent) {
	setObjectName("modelTimeline");
	// A plain QWidget draws no background of its own, and the Flat sheet colours this strip by object name.
	setAttribute(Qt::WA_StyledBackground, true);

	play_button = new QToolButton(this);
	play_button->setObjectName("timelinePlay");
	// Space is handled by ModelEditor::keyPressEvent, so it never steals the key from a focused combo box or table.
	play_button->setToolTip("Play / Pause (Space)");

	restart_button = new QToolButton(this);
	restart_button->setObjectName("timelineRestart");
	restart_button->setToolTip("Restart the sequence");

	slider = new QSlider(Qt::Horizontal, this);
	slider->setObjectName("timelineSlider");
	slider->setRange(0, slider_steps);

	readout = new QLabel(this);
	readout->setObjectName("timelineReadout");
	readout->setMinimumWidth(110);
	readout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

	loop_box = new QCheckBox("Loop", this);
	loop_box->setObjectName("timelineLoop");
	loop_box->setChecked(true);

	speed_box = new QComboBox(this);
	speed_box->setObjectName("timelineSpeed");
	speed_box->setToolTip("Playback speed");
	for (const double speed : playback_speeds) {
		speed_box->addItem(QString::fromStdString(format_speed(speed)), speed);
	}
	speed_box->setCurrentIndex(2);

	auto* layout = new QHBoxLayout(this);
	layout->setContentsMargins(8, 4, 8, 4);
	layout->setSpacing(6);
	layout->addWidget(play_button);
	layout->addWidget(restart_button);
	layout->addWidget(slider, 1);
	layout->addWidget(readout);
	layout->addWidget(loop_box);
	layout->addWidget(speed_box);

	connect(play_button, &QToolButton::clicked, this, [this] {
		emit play_toggled(!playing);
	});
	connect(restart_button, &QToolButton::clicked, this, &AnimationTimeline::restart_requested);
	connect(slider, &QSlider::valueChanged, this, [this](const int value) {
		// Programmatic moves block signals (see set_frame), so every change that gets here came from the user.
		emit frame_scrubbed(slider_to_frame(value, slider_steps, range_start, range_end));
	});
	connect(loop_box, &QCheckBox::toggled, this, &AnimationTimeline::loop_toggled);
	connect(speed_box, &QComboBox::currentIndexChanged, this, [this](const int index) {
		if (index >= 0) {
			emit speed_changed(speed_box->itemData(index).toDouble());
		}
	});

	refresh_icons();
	refresh_readout();
}

void AnimationTimeline::set_range(const double start, const double end) {
	range_start = start;
	range_end = std::max(start, end);
	set_frame(start);
}

void AnimationTimeline::set_frame(const double new_frame) {
	frame = new_frame;
	// Do not move the slider under the user's finger while they drag it.
	if (!slider->isSliderDown()) {
		const QSignalBlocker blocker(slider);
		slider->setValue(frame_to_slider(frame, range_start, range_end, slider_steps));
	}
	refresh_readout();
}

void AnimationTimeline::set_playing(const bool new_playing) {
	playing = new_playing;
	refresh_icons();
}

void AnimationTimeline::set_loop(const bool loop) {
	const QSignalBlocker blocker(loop_box);
	loop_box->setChecked(loop);
}

void AnimationTimeline::set_speed(const double speed) {
	const QSignalBlocker blocker(speed_box);
	for (int i = 0; i < speed_box->count(); ++i) {
		if (std::abs(speed_box->itemData(i).toDouble() - speed) < 1e-9) {
			speed_box->setCurrentIndex(i);
			return;
		}
	}
}

void AnimationTimeline::refresh_readout() {
	readout->setText(QString::fromStdString(format_readout(frame, range_start, range_end)));
}

void AnimationTimeline::refresh_icons() {
	const bool flat = theme::current_is_flat();
	play_button->setIcon(flat ? theme::icon(playing ? "pause" : "play") : QIcon());
	play_button->setText(flat ? QString() : (playing ? "Pause" : "Play"));
	restart_button->setIcon(flat ? theme::icon("restart") : QIcon());
	restart_button->setText(flat ? QString() : "Restart");
}

void AnimationTimeline::changeEvent(QEvent* event) {
	if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
		refresh_icons();
	}
	QWidget::changeEvent(event);
}
