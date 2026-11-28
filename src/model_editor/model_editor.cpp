#include "model_editor.h"

#include "model_editor_glwidget.h"
#include "animation_timeline.h"
#include "model_inspector.h"

#include <QObject>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSlider>
#include <QTableView>
#include <QTextStream>
#include <QTimer>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QLabel>
#include <QLocale>
#include <QVBoxLayout>
#include <QToolBar>
#include <QToolButton>
#include <QSettings>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenuBar>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStatusBar>
#include <QUrl>

#include <DockAreaTitleBar.h>
#include <DockAreaTabBar.h>
#include <DockComponentsFactory.h>

#include "hivewe.h"
#include "browse_models_dialog.h"
#include "theme.h"

import std;
import BinaryReader;
import BinaryWriter;
import Hierarchy;
import MDX;
import GPUTexture;
import Utilities;

class CCustomComponentsFactory: public ads::CDockComponentsFactory {
  public:
	using Super = ads::CDockComponentsFactory;

	explicit CCustomComponentsFactory(ModelEditor& model_editor) : Super(), model_editor(model_editor) {}

	ModelEditor& model_editor;

	ads::CDockAreaTitleBar* createDockAreaTitleBar(ads::CDockAreaWidget* dock_area) const override {
		const auto title_bar = new ads::CDockAreaTitleBar(dock_area);

		const auto add_tab = new QToolButton(title_bar);
		add_tab->setToolTip("Browse Models");
		add_tab->setText("Browse Models");
		add_tab->setMinimumSize(110, 28);
		add_tab->setAutoRaise(false);

		const int index = title_bar->indexOf(title_bar->tabBar());
		title_bar->insertWidget(index + 1, add_tab);

		QObject::connect(add_tab, &QToolButton::clicked, [=] {
			model_editor.browse_models(dock_area);
		});

		return title_bar;
	}
};

ModelEditor::ModelEditor(QWidget* parent) : QMainWindow(parent) {
	setWindowTitle("Model Editor");
	setAttribute(Qt::WA_DeleteOnClose);
	resize(1280, 800);

	// setWindowFlag(Qt::ExpandedClientAreaHint, true);
	// setWindowFlag(Qt::NoTitleBarBackgroundHint, true);
	// setAttribute(Qt::WA_LayoutOnEntireRect, true);

	dock_manager = new ads::CDockManager;
	dock_manager->setStyleSheet("");
	dock_manager->setConfigFlag(ads::CDockManager::DockAreaHasCloseButton, false);
	dock_manager->setConfigFlag(ads::CDockManager::DockAreaHasTabsMenuButton, false);
	dock_manager->setConfigFlag(ads::CDockManager::DockAreaHasUndockButton, false);
	dock_manager->setConfigFlag(ads::CDockManager::DockAreaDynamicTabsMenuButtonVisibility, false);
	dock_manager->setConfigFlag(ads::CDockManager::AlwaysShowTabs, true);

	dock_manager->setComponentsFactory(new CCustomComponentsFactory(*this));

	// The model tabs sit above the timeline strip.
	timeline = new AnimationTimeline;
	auto* central = new QWidget;
	auto* central_layout = new QVBoxLayout(central);
	central_layout->setContentsMargins(0, 0, 0, 0);
	central_layout->setSpacing(0);
	central_layout->addWidget(dock_manager, 1);
	central_layout->addWidget(timeline);
	setCentralWidget(central);

	QLabel* image = new QLabel();
	image->setPixmap(QPixmap("data/icons/model_editor/background.png"));
	image->setAlignment(Qt::AlignCenter);

	const auto centraldock_widget = dock_manager->createDockWidget("CentralWidget");
	centraldock_widget->setWidget(image);
	centraldock_widget->setFeature(ads::CDockWidget::NoTab, true);
	dock_area = dock_manager->setCentralWidget(centraldock_widget);

	build_toolbar();
	build_status_bar();

	inspector = new ModelInspector(this);
	connect(inspector, &ModelInspector::sequence_selected, this, [this](const int index) {
		if (active) {
			active->set_sequence(index);
		}
	});
	connect(inspector, &ModelInspector::geoset_visibility_changed, this, [this](const int geoset, const bool visible) {
		if (active) {
			active->set_geoset_visible(geoset, visible);
		}
	});

	connect(timeline, &AnimationTimeline::play_toggled, this, [this](const bool playing) {
		if (active) {
			active->set_playing(playing);
		}
	});
	connect(timeline, &AnimationTimeline::restart_requested, this, [this] {
		if (active) {
			active->set_frame(active->sequence_start());
		}
	});
	connect(timeline, &AnimationTimeline::frame_scrubbed, this, [this](const double frame) {
		if (active) {
			active->set_frame(frame);
		}
	});
	connect(timeline, &AnimationTimeline::loop_toggled, this, [this](const bool loop) {
		if (active) {
			active->set_loop(loop);
		}
	});
	connect(timeline, &AnimationTimeline::speed_changed, this, [this](const double speed) {
		if (active) {
			active->set_speed(speed);
		}
	});

	// Same convention as the main window: geometry and dock layout in QSettings, never written by --qa-offscreen runs.
	if (!HiveWE::qa_offscreen) {
		const QSettings settings;
		restoreGeometry(settings.value("ModelEditor/geometry").toByteArray());
		restoreState(settings.value("ModelEditor/windowState").toByteArray());
	}

	set_active(nullptr);
	refresh_icons();
	show();
}

void ModelEditor::build_toolbar() {
	auto* toolbar = addToolBar("Model Editor");
	toolbar->setObjectName("modelEditorToolbar");
	toolbar->setMovable(false);
	toolbar->setIconSize(QSize(20, 20));
	toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

	const auto add = [this, toolbar](const QString& text, const QString& icon, const QString& tooltip, const bool with_text) {
		QAction* action = toolbar->addAction(text);
		action->setToolTip(tooltip);
		toolbar_icons.push_back({ action, icon, with_text });
		return action;
	};

	connect(add("Open", "open", "Open a model file (.mdx or .mdl)", true), &QAction::triggered, this, [this] { open_file(); });
	connect(add("Browse Models", "model-editor", "Browse the game and map models", true), &QAction::triggered, this, [this] {
		browse_models(dock_area);
	});
	toolbar->addSeparator();

	wireframe_action = add("Wireframe", "view-wireframe", "Draw the model as wireframe", false);
	wireframe_action->setCheckable(true);
	connect(wireframe_action, &QAction::toggled, this, [this](const bool on) {
		if (active) {
			active->set_wireframe(on);
		}
	});

	reset_camera_action = add("Reset Camera", "frame-model", "Frame the whole model again", false);
	connect(reset_camera_action, &QAction::triggered, this, [this] {
		if (active) {
			active->reset_camera();
		}
	});
	toolbar->addSeparator();

	edit_mdl_action = add("Edit MDL", "description", "Write the model as MDL to a temporary file and open it", true);
	connect(edit_mdl_action, &QAction::triggered, this, [this] { edit_as_mdl(); });
	save_mdx_action = add("Save to MDX", "save", "Save a copy of the model as MDX", true);
	connect(save_mdx_action, &QAction::triggered, this, [this] { save_as_mdx(); });
	toolbar->addSeparator();

	auto* label = new QLabel("Sequence", toolbar);
	label->setObjectName("toolbarLabel");
	toolbar->addWidget(label);
	sequence_box = new QComboBox(toolbar);
	sequence_box->setObjectName("sequenceCombo");
	sequence_box->setMinimumWidth(160);
	sequence_box->setToolTip("Sequence to play");
	toolbar->addWidget(sequence_box);
	connect(sequence_box, &QComboBox::activated, this, [this](const int index) {
		if (active) {
			active->set_sequence(index);
		}
	});
}

void ModelEditor::build_status_bar() {
	const auto add = [this](const QString& tooltip) {
		auto* label = new QLabel(statusBar());
		label->setObjectName("modelStatusItem");
		label->setToolTip(tooltip);
		statusBar()->addPermanentWidget(label);
		return label;
	};
	status_name = add("Model file");
	status_vertices = add("Vertices of the drawn geosets (LOD 0), as in the Info panel");
	status_triangles = add("Triangles of the drawn geosets (LOD 0), as in the Info panel");
	status_bones = add("Bones");
	status_camera = add("Viewport camera");
	status_camera->setText("Camera: orbit");
	statusBar()->showMessage("Ready");
}

void ModelEditor::refresh_icons() {
	const bool flat = theme::current_is_flat();
	for (const auto& [action, icon, with_text] : toolbar_icons) {
		action->setIcon(flat ? theme::icon(icon) : QIcon());
		auto* toolbar = findChild<QToolBar*>("modelEditorToolbar");
		if (auto* button = qobject_cast<QToolButton*>(toolbar ? toolbar->widgetForAction(action) : nullptr)) {
			// Without an icon (old themes) every button needs its text.
			button->setToolButtonStyle(!flat ? Qt::ToolButtonTextOnly : (with_text ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly));
		}
	}
}

void ModelEditor::changeEvent(QEvent* event) {
	if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
		refresh_icons();
	}
	QMainWindow::changeEvent(event);
}

void ModelEditor::closeEvent(QCloseEvent* event) {
	if (!HiveWE::qa_offscreen) {
		QSettings settings;
		settings.setValue("ModelEditor/geometry", saveGeometry());
		settings.setValue("ModelEditor/windowState", saveState());
	}
	QMainWindow::closeEvent(event);
}

void ModelEditor::set_active(ModelEditorGLWidget* widget) {
	active = widget;
	const bool has_model = widget != nullptr && widget->mdx != nullptr;

	timeline->setEnabled(has_model);
	sequence_box->setEnabled(has_model);
	for (QAction* action : { wireframe_action, reset_camera_action, edit_mdl_action, save_mdx_action }) {
		action->setEnabled(has_model);
	}

	{
		const QSignalBlocker blocker(sequence_box);
		sequence_box->clear();
		if (has_model) {
			for (const auto& sequence : widget->mdx->sequences) {
				sequence_box->addItem(QString::fromStdString(sequence.name));
			}
		}
	}

	std::vector<bool> hidden;
	if (has_model) {
		hidden.resize(widget->mdx->geosets.size());
		for (std::size_t i = 0; i < hidden.size(); ++i) {
			hidden[i] = !widget->geoset_visible(static_cast<int>(i));
		}
		timeline->set_loop(widget->loop());
		timeline->set_speed(widget->speed());
		timeline->set_playing(widget->playing());
	} else {
		timeline->set_playing(false);
	}
	inspector->set_model(has_model ? widget->mdx : nullptr, std::move(hidden));

	refresh_sequence_ui();
	refresh_status();
}

void ModelEditor::refresh_sequence_ui() {
	if (!active || active->sequence() < 0) {
		timeline->set_range(0.0, 0.0);
		return;
	}
	{
		const QSignalBlocker blocker(sequence_box);
		sequence_box->setCurrentIndex(active->sequence());
	}
	inspector->set_current_sequence(active->sequence());
	timeline->set_range(active->sequence_start(), active->sequence_end());
	timeline->set_frame(active->frame());
}

void ModelEditor::refresh_status() {
	if (!active || !active->mdx) {
		statusBar()->showMessage("Ready");
		for (QLabel* label : { status_name, status_vertices, status_triangles, status_bones }) {
			label->clear();
		}
		return;
	}

	const DrawnCounts counts = drawn_counts(*active->mdx);
	const QLocale locale;
	statusBar()->showMessage("Ready");
	status_name->setText(QString::fromStdString(active->mdx->name));
	status_vertices->setText("Vertices " + locale.toString(static_cast<qulonglong>(counts.vertices)));
	status_triangles->setText("Triangles " + locale.toString(static_cast<qulonglong>(counts.triangles)));
	status_bones->setText("Bones " + locale.toString(static_cast<qulonglong>(active->mdx->bones.size())));
}

void ModelEditor::toggle_playback() {
	if (active) {
		active->set_playing(!active->playing());
	}
}

void ModelEditor::edit_as_mdl() {
	if (!active || !active->mdx) {
		return;
	}

	auto mdl = active->mdx->to_mdl();
	QString name = QString::fromStdString(active->mdx->name);
	name.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
	if (name.isEmpty()) {
		name = "model";
	}
	const QString path = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/" + name + ".mdl";

	std::ofstream file(fs::path(path.toStdWString()));
	if (file) {
		file.write(mdl.data(), mdl.size());
		file.close();
	}
	if (!file.good()) {
		// Do not hand a truncated file to the user's MDL editor.
		file.close();
		std::error_code ec;
		fs::remove(fs::path(path.toStdWString()), ec);
		QMessageBox::warning(this, "Edit MDL failed", "Could not write " + path);
		return;
	}

	QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void ModelEditor::save_as_mdx() {
	if (!active || !active->mdx) {
		return;
	}

	const QString file_name = QFileDialog::getSaveFileName(
		this,
		"Save MDX",
		QStandardPaths::writableLocation(QStandardPaths::TempLocation),
		"MDX (*.mdx *.MDX)"
	);
	if (file_name.isEmpty()) {
		return;
	}

	if (const QString error = write_active_mdx(fs::path(file_name.toStdWString())); !error.isEmpty()) {
		QMessageBox::warning(this, "Error saving MDX file", error);
	}
}

/// Serializes the active model and writes it to `path`. Returns an empty string on success, otherwise the message to show.
QString ModelEditor::write_active_mdx(const fs::path& path) {
	if (!active || !active->mdx) {
		return "No model is open.";
	}

	// Save at the loaded version: a v1800 model keeps its u16 SKIN layout instead of being downgraded to v1200,
	// where the writer refuses bone indices above 255.
	BinaryWriter mdx_data;
	try {
		mdx_data = active->mdx->to_mdx(active->mdx->version);
	} catch (const std::exception& e) {
		return QString("Could not save the model: %1").arg(QString::fromStdString(e.what()));
	}
	std::ofstream file(path, std::ios::binary);
	if (file) {
		file.write(reinterpret_cast<const char*>(mdx_data.buffer.data()), mdx_data.buffer.size());
		file.close();
	}
	if (!file.good()) {
		return "Could not write " + QString::fromStdWString(path.wstring()) + ". The file may be incomplete.";
	}
	return {};
}

void ModelEditor::open_file() {
	const QSettings settings;

	const QString file_name = QFileDialog::getOpenFileName(
		this,
		"Open Model File",
		settings.value("openDirectory", QDir::current().path()).toString(),
		"MDLX (*.mdl *.MDL *.mdx *.MDX)"
	);

	if (file_name == "") {
		return;
	}

	show_model(fs::path(file_name.toStdWString()), true);
}

void ModelEditor::browse_models(ads::CDockAreaWidget* parent) {
	auto* dialog = new BrowseModelsDialog(parent);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->setWindowModality(Qt::WindowModality::WindowModal);

	connect(dialog, &QDialog::accepted, this, [this, dialog, parent] {
		const QString path = dialog->selected_path();
		if (!path.isEmpty()) {
			show_model(fs::path(path.toStdWString()), false, parent);
		}
	});

	dialog->show();
}

bool ModelEditor::show_model(const fs::path& path, const bool local_file, ads::CDockAreaWidget* area, std::string* error) {
	const auto result = open_model(path, local_file);
	if (!result.has_value()) {
		if (error) {
			*error = result.error();
		} else {
			QMessageBox::information(this, "Opening model failed", QString::fromStdString(result.error()));
		}
		return false;
	}
	dock_manager->addDockWidget(ads::CenterDockWidgetArea, result.value(), area ? area : dock_area);
	return true;
}

std::expected<ads::CDockWidget*, std::string> ModelEditor::open_model(const fs::path& path, const bool local_file) {
	std::shared_ptr<mdx::MDX> mdx;
	// path::string() throws for characters the system code page cannot hold.
	const QString file_name = QString::fromStdWString(path.filename().wstring());
	const QString extension = QString::fromStdWString(path.extension().wstring()).toLower();
	try {
		if (extension == ".mdl") {
			const auto file = read_text_file(path);
			auto result = mdx::MDX::from_mdl(file);
			if (!result.has_value()) {
				return std::unexpected(result.error());
			}
			mdx = std::make_shared<mdx::MDX>(std::move(result.value()));
		} else if (extension == ".mdx") {
			auto result = [&] {
				if (local_file) {
					return read_file(path);
				} else {
					return hierarchy.open_file(path);
				}
			}();

			if (!result.has_value()) {
				return std::unexpected(result.error());
			}
			mdx = std::make_shared<mdx::MDX>(result.value());
		} else {
			return std::unexpected("Unsupported file type");
		}
	} catch (const std::exception& e) {
		return std::unexpected(std::format("{} is not a readable model: {}", file_name.toStdString(), e.what()));
	}

	// A model from disk looks for its textures in its own folder first; map and game models resolve through the Hierarchy alone.
	auto* gl_widget = new ModelEditorGLWidget(nullptr, mdx, false, local_file ? ModelTextureSource::beside(path) : ModelTextureSource {});
	gl_widget->set_wireframe(wireframe_action->isChecked());

	// Only the tab that is on top drives the timeline, inspector and status bar.
	connect(gl_widget, &ModelEditorGLWidget::sequence_changed, this, [this, gl_widget] {
		if (active == gl_widget) {
			refresh_sequence_ui();
		}
	});
	connect(gl_widget, &ModelEditorGLWidget::frame_changed, this, [this, gl_widget](const double frame) {
		if (active == gl_widget) {
			timeline->set_frame(frame);
		}
	});
	connect(gl_widget, &ModelEditorGLWidget::playing_changed, this, [this, gl_widget](const bool playing) {
		if (active == gl_widget) {
			timeline->set_playing(playing);
		}
	});
	auto* dock_tab = dock_manager->createDockWidget("");
	dock_tab->setFeature(ads::CDockWidget::DockWidgetFeature::DockWidgetDeleteOnClose, true);
	dock_tab->setWidget(gl_widget);
	dock_tab->setWindowTitle(file_name);
	// Closing the last tab leaves nothing to take over, so the panels are cleared here (another tab's visibilityChanged has already run otherwise).
	connect(dock_tab, &ads::CDockWidget::closed, this, [this, gl_widget] {
		if (active == gl_widget) {
			set_active(nullptr);
		}
	});
	connect(dock_tab, &ads::CDockWidget::visibilityChanged, this, [this, gl_widget](const bool visible) {
		if (visible && active != gl_widget) {
			set_active(gl_widget);
		}
	});

	return dock_tab;
}

// --- --qa-model-editor-script -------------------------------------------------------------------------------------------------------------
// Steps ("<command>[:<argument>]", separated by ';') act on the real widgets, so the same slots run as for a click:
//   wait:<ms>  play  pause  restart  loop:on|off  speed:<0.25|0.5|1|1.5|2>  wireframe:on|off  geoset:<n>:on|off
//   seq-combo:<n>  seq-list:<n>  slider:<0..1000>  mark  state  shot:<png of the window>  viewshot:<png of the viewport>
//   save-mdx:<name.mdx>  (runs the Save to MDX write path into a plain file name beside the result file, re-parses it, compares counts)  quit
//   open:<model file>  (opens a second model from disk in a new tab, as File > Open does; follow with wait)
//   seq:<sequence name>  (case-insensitive, whole name)
//   at:<ms>  (pause <ms> into the sequence with a fresh camera, particles and global sequences stepped from its start: repeatable)
//   textures  (one INFO line per model texture with the file used, then FAIL if any shows a placeholder or did not decode)
//   camera:<yaw>,<pitch>  (orbit angles in degrees after at:, default -90,-56: in front of the model; -90,-89 looks down from above)
//   layers  (one INFO line per geoset layer in draw order at the current time: geoset, layer, filter, plane, centre Z, pass,
//            current texture id, UV transform t(u,v) r(z,w) s(u,v), geoset/layer alpha, visible, hidden)
//   expect:advanced | same | time=<ms>[/tol] | timefrac=<0..1> | rate=<x> | playing=0|1 | loop=0|1 | speed=<x> | wireframe=0|1
//          | hidden=<n>/0|1 | sequence=<n> | viewfewer | viewdifferent | viewmin=<pixels>
//          | order=<g>[.<l>],<g>[.<l>],...  (every geoset layer in draw order; <g> = index into the model's geosets, <l> = layer, default 0)
//          | texture=<g>[.<l>]/<texture id>  (the layer's current texture after the flipbook)
//          | uv=<g>[.<l>]/<tu>,<tv>[,<rz>,<rw>,<su>,<sv>]  (the layer's current UV transform, each within 0.001)
// "mark" remembers the playhead and the clock; advanced, same and rate compare against it.

void ModelEditor::run_qa_script(const QString& steps, const QString& out_file) {
	if (out_file.isEmpty()) {
		std::println(stderr, "--qa-model-editor-script needs --qa-script-out <file>.");
		QApplication::exit(2);
		return;
	}
	qa_steps = steps.split(';', Qt::SkipEmptyParts);
	qa_index = 0;
	qa_out = out_file;
	qa_log.clear();
	qa_view_counts.clear();
	// The model opens from the event loop (see the hook), so give its GL widget time to initialise.
	QTimer::singleShot(0, this, &ModelEditor::qa_continue);
}

/// An offscreen window is never exposed, so Qt stops its frameSwapped repaint loop; paint by hand while waiting so playback time advances.
void ModelEditor::qa_tick() {
	if (active) {
		(void)active->grabFramebuffer(); // renders through paintGL even when the window is never exposed (repaint() does not)
	}
	if (qa_wait_timer.elapsed() >= qa_wait_ms) {
		qa_continue();
	} else {
		QTimer::singleShot(16, this, &ModelEditor::qa_tick);
	}
}

void ModelEditor::qa_finish() {
	qa_log << "DONE";
	QFile file(qa_out);
	if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
		QTextStream(&file) << qa_log.join('\n') << '\n';
	} else {
		std::println(stderr, "--qa-script-out: cannot write {}", qa_out.toStdString());
	}
	QApplication::quit();
}

void ModelEditor::qa_continue() {
	while (qa_index < qa_steps.size()) {
		const QString step = qa_steps[qa_index++].trimmed();
		const int colon = step.indexOf(':');
		const QString command = colon < 0 ? step : step.left(colon);
		const QString argument = colon < 0 ? QString() : step.mid(colon + 1);

		const auto log = [&](const QString& kind, const QString& text) { qa_log << QString("%1 [%2] %3").arg(kind, step, text); };
		const auto check = [&](const bool condition, const QString& text) { log(condition ? "PASS" : "FAIL", text); };
		// PNG names are plain file names next to the result file: no folders, no drive letters, no "..".
		const auto png_path = [&](const QString& name) -> QString {
			const bool plain = !name.isEmpty() && name.endsWith(".png", Qt::CaseInsensitive) && !name.contains("..") && !name.contains('/')
				&& !name.contains('\\')&& !name.contains(':') && !QFileInfo(name).isAbsolute();
			if (!plain) {
				log("FAIL", "refused: a PNG name must be a plain file name ending in .png");
				return {};
			}
			return QFileInfo(qa_out).absoluteDir().filePath(name);
		};

		if (command == "wait") {
			qa_wait_ms = argument.toInt();
			qa_wait_timer.start();
			qa_tick();
			return;
		}
		if (command == "quit") {
			break;
		}
		if (!active) {
			log("FAIL", "no model is open");
			continue;
		}

		const double start = active->sequence_start();
		const double length = active->sequence_end() - start;
		const QString state = QString("sequence=%1 frame=%2 time=%3/%4 playing=%5 loop=%6 speed=%7 wireframe=%8 combo=%9 list=%10 slider=%11 readout='%12'")
			.arg(active->sequence())
			.arg(active->frame(), 0, 'f', 1)
			.arg(active->frame() - start, 0, 'f', 1)
			.arg(length)
			.arg(active->playing())
			.arg(active->loop())
			.arg(active->speed())
			.arg(active->wireframe_enabled())
			.arg(sequence_box->currentIndex())
			.arg(inspector->sequence_table()->currentIndex().row())
			.arg(timeline->findChild<QSlider*>("timelineSlider")->value())
			.arg(timeline->findChild<QLabel*>("timelineReadout")->text());

		if (command == "play" || command == "pause") {
			auto* button = timeline->findChild<QToolButton*>("timelinePlay");
			if (active->playing() != (command == "play")) {
				button->click();
			}
			log("INFO", QString("playing=%1").arg(active->playing()));
		} else if (command == "restart") {
			timeline->findChild<QToolButton*>("timelineRestart")->click();
			log("INFO", QString("frame=%1").arg(active->frame()));
		} else if (command == "loop") {
			timeline->findChild<QCheckBox*>("timelineLoop")->setChecked(argument == "on");
			log("INFO", QString("loop=%1").arg(active->loop()));
		} else if (command == "speed") {
			auto* combo = timeline->findChild<QComboBox*>("timelineSpeed");
			const int index = combo->findData(argument.toDouble());
			if (index < 0) {
				log("FAIL", "no such speed");
			} else {
				combo->setCurrentIndex(index);
				log("INFO", QString("speed=%1").arg(active->speed()));
			}
		} else if (command == "wireframe") {
			wireframe_action->setChecked(argument == "on");
			log("INFO", QString("wireframe=%1").arg(active->wireframe_enabled()));
		} else if (command == "geoset") {
			const QStringList parts = argument.split(':');
			auto* model = inspector->geoset_table()->model();
			const bool shown = parts.value(1) == "on";
			if (parts.size() != 2 || !model->setData(model->index(parts[0].toInt(), 3), shown ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole)) {
				log("FAIL", "geoset row refused the change");
			} else {
				log("INFO", QString("geoset %1 shown=%2").arg(parts[0]).arg(active->geoset_visible(parts[0].toInt())));
			}
		} else if (command == "seq-combo") {
			sequence_box->setCurrentIndex(argument.toInt());
			emit sequence_box->activated(argument.toInt());
			log("INFO", state);
		} else if (command == "seq-list") {
			auto* table = inspector->sequence_table();
			table->setCurrentIndex(table->model()->index(argument.toInt(), 0));
			log("INFO", state);
		} else if (command == "open") {
			std::string error;
			const bool opened = show_model(fs::path(argument.toStdWString()), true, nullptr, &error);
			check(opened, opened ? QString("opened in a new tab") : QString::fromStdString(error));
		} else if (command == "seq") {
			const auto& sequences = active->mdx->sequences;
			const auto found = std::ranges::find_if(sequences, [&](const auto& sequence) {
				return QString::fromStdString(sequence.name).compare(argument, Qt::CaseInsensitive) == 0;
			});
			if (found == sequences.end()) {
				log("FAIL", "no sequence with that name");
			} else {
				const int index = static_cast<int>(found - sequences.begin());
				sequence_box->setCurrentIndex(index);
				emit sequence_box->activated(index);
				check(active->sequence() == index, QString("sequence=%1").arg(active->sequence()));
			}
		} else if (command == "at") {
			bool number = false;
			const double ms = argument.toDouble(&number);
			if (!number || !active->show_exact_time(ms)) {
				log("FAIL", "needs a time in ms and a shown sequence (wait first)");
			} else {
				active->reset_camera(); // framed on this sequence, whichever one the editor picked when it opened the model
				log("INFO", QString("sequence=%1 time=%2 ms").arg(active->sequence()).arg(active->frame() - active->sequence_start()));
			}
		} else if (command == "textures") {
			if (!active->mesh) {
				log("FAIL", "nothing is shown (wait first)");
				continue;
			}
			int missing = 0;
			for (std::size_t i = 0; i < active->mesh->textures.size(); ++i) {
				const auto& texture = *active->mesh->textures[i];
				const mdx::Texture& wanted = active->mdx->textures[i];
				missing += texture.missing;
				log(texture.missing ? "FAIL" : "INFO",
					QString("%1 replaceable=%2 '").arg(i).arg(wanted.replaceable_id) + QString::fromStdWString(wanted.file_name.wstring()) + "' <- "
						+ QString::fromStdString(texture.source) + (texture.missing ? " (MISSING)" : ""));
			}
			check(missing == 0, QString("%1 textures, %2 missing").arg(active->mesh->textures.size()).arg(missing));
		} else if (command == "camera") {
			// Orbit angles in degrees, after at: (which resets the camera). Pitch -90 (straight down) is refused: the camera's up axis.
			const QStringList parts = argument.split(',');
			bool yaw_ok = false;
			bool pitch_ok = false;
			const double yaw = parts.value(0).toDouble(&yaw_ok);
			const double pitch = parts.value(1).toDouble(&pitch_ok);
			if (parts.size() != 2 || !yaw_ok || !pitch_ok || std::abs(pitch) >= 90.0) {
				log("FAIL", "needs <yaw>,<pitch> in degrees, pitch between -90 and 90 exclusive");
				continue;
			}
			active->camera.horizontal_angle = static_cast<float>(yaw * std::numbers::pi / 180.0);
			active->camera.vertical_angle = static_cast<float>(pitch * std::numbers::pi / 180.0);
			active->camera.update(0);
			log("INFO", QString("camera yaw=%1 pitch=%2").arg(yaw).arg(pitch));
		} else if (command == "layers") {
			if (!active->mesh) {
				log("FAIL", "nothing is shown (wait first)");
				continue;
			}
			static constexpr std::array<const char*, 7> filters = { "None", "Transparent", "Blend", "Additive", "AddAlpha", "Modulate", "Modulate2x" };
			const auto& mesh = *active->mesh;
			for (std::size_t i = 0; i < mesh.draw_items.size(); ++i) {
				const auto layer = mesh.layer_state(i, active->skeleton);
				log("INFO",
					QString("draw %1: geoset=%2 layer=%3 filter=%4 plane=%5 z=%6 pass=%7 texture=%8 uv=t(%9,%10) r(%11,%12) s(%13,%14) "
							"alpha=%15/%16 visible=%17 hidden=%18")
						.arg(i)
						.arg(layer.geoset)
						.arg(layer.layer)
						.arg(layer.blend_mode < filters.size() ? QString(filters[layer.blend_mode]) : QString::number(layer.blend_mode))
						.arg(static_cast<std::int32_t>(layer.priority_plane))
						.arg(layer.depth_key, 0, 'f', 2)
						.arg(layer.opaque ? "opaque" : "translucent")
						.arg(layer.texture ? QString::number(*layer.texture) : QString("none"))
						.arg(layer.uv.translation.x, 0, 'g', 6)
						.arg(layer.uv.translation.y, 0, 'g', 6)
						.arg(layer.uv.rotation.x, 0, 'g', 6)
						.arg(layer.uv.rotation.y, 0, 'g', 6)
						.arg(layer.uv.scale.x, 0, 'g', 6)
						.arg(layer.uv.scale.y, 0, 'g', 6)
						.arg(layer.geoset_alpha, 0, 'f', 3)
						.arg(layer.layer_alpha, 0, 'f', 3)
						.arg(layer.visible)
						.arg(layer.hidden));
			}
			log("INFO", QString("%1 layers, %2 opaque").arg(mesh.draw_items.size()).arg(mesh.opaque_draw_count));
		} else if (command == "slider") {
			timeline->findChild<QSlider*>("timelineSlider")->setValue(argument.toInt());
			log("INFO", state);
		} else if (command == "mark") {
			qa_mark_frame = active->frame();
			qa_mark_timer.start();
			log("INFO", QString("frame=%1").arg(qa_mark_frame));
		} else if (command == "state") {
			log("INFO", state);
		} else if (command == "shot") {
			if (const QString path = png_path(argument); !path.isEmpty()) {
				check(grab().save(path), "window saved");
			}
		} else if (command == "viewshot") {
			const QString path = png_path(argument);
			if (path.isEmpty()) {
				continue;
			}
			const QImage image = active->grabFramebuffer();
			long long drawn = 0;
			if (!image.isNull()) {
				const QRgb background = image.pixel(0, 0);
				for (int y = 0; y < image.height(); ++y) {
					for (int x = 0; x < image.width(); ++x) {
						const QRgb pixel = image.pixel(x, y);
						drawn += (std::abs(qRed(pixel) - qRed(background)) + std::abs(qGreen(pixel) - qGreen(background))
									 + std::abs(qBlue(pixel) - qBlue(background)))
							> 24;
					}
				}
			}
			qa_view_counts.push_back(drawn);
			check(!image.isNull() && image.save(path), QString("viewport saved, %1 pixels differ from the background").arg(drawn));
		} else if (command == "save-mdx") {
			// Same write path as the Save to MDX button, into a plain .mdx name next to the result file, then re-parsed.
			const bool plain = argument.endsWith(".mdx", Qt::CaseInsensitive) && !argument.contains("..") && !argument.contains('/')
				&& !argument.contains('\\') && !argument.contains(':') && !QFileInfo(argument).isAbsolute();
			if (!plain) {
				log("FAIL", "refused: an MDX name must be a plain file name ending in .mdx");
				continue;
			}
			const fs::path path = fs::path(QFileInfo(qa_out).absoluteDir().filePath(argument).toStdWString());
			if (const QString error = write_active_mdx(path); !error.isEmpty()) {
				log("FAIL", error);
				continue;
			}
			auto reader = read_file(path);
			if (!reader) {
				log("FAIL", "re-read failed: " + QString::fromStdString(reader.error()));
				continue;
			}
			const qsizetype bytes = static_cast<qsizetype>(reader->buffer.size());
			try {
				const mdx::MDX reparsed(*reader);
				check(
					reparsed.geosets.size() == active->mdx->geosets.size() && reparsed.sequences.size() == active->mdx->sequences.size()
						&& reparsed.bones.size() == active->mdx->bones.size(),
					QString("saved %1 bytes, re-parsed: geosets %2/%3, sequences %4/%5, bones %6/%7 (re-parsed/original)")
						.arg(bytes)
						.arg(reparsed.geosets.size()).arg(active->mdx->geosets.size())
						.arg(reparsed.sequences.size()).arg(active->mdx->sequences.size())
						.arg(reparsed.bones.size()).arg(active->mdx->bones.size())
				);
			} catch (const std::exception& e) {
				log("FAIL", QString("re-parse failed: %1").arg(e.what()));
			}
		} else if (command == "expect") {
			const QString key = argument.section('=', 0, 0);
			const QString value = argument.section('=', 1);
			const double elapsed_ms = static_cast<double>(qa_mark_timer.nsecsElapsed()) / 1e6;
			if (key == "advanced") {
				check(std::abs(active->frame() - qa_mark_frame) > 1.0, QString("frame %1 -> %2").arg(qa_mark_frame).arg(active->frame()));
			} else if (key == "same") {
				check(std::abs(active->frame() - qa_mark_frame) < 1.0, QString("frame %1 -> %2").arg(qa_mark_frame).arg(active->frame()));
			} else if (key == "time") {
				const double tolerance = value.contains('/') ? value.section('/', 1).toDouble() : 60.0;
				check(
					std::abs((active->frame() - start) - value.section('/', 0, 0).toDouble()) <= tolerance,
					QString("time into sequence %1 ms").arg(active->frame() - start)
				);
			} else if (key == "timefrac") {
				const double fraction = length > 0 ? (active->frame() - start) / length : 0.0;
				check(std::abs(fraction - value.toDouble()) <= 0.02, QString("fraction %1").arg(fraction));
			} else if (key == "rate") {
				const double rate = elapsed_ms > 0 ? (active->frame() - qa_mark_frame) / elapsed_ms : 0.0;
				check(
					std::abs(rate - value.toDouble()) <= value.toDouble() * 0.3,
					QString("measured %1 sequence-ms per ms over %2 ms").arg(rate).arg(elapsed_ms)
				);
			} else if (key == "playing") {
				check(active->playing() == (value == "1"), QString("playing=%1").arg(active->playing()));
			} else if (key == "loop") {
				check(active->loop() == (value == "1"), QString("loop=%1").arg(active->loop()));
			} else if (key == "speed") {
				check(std::abs(active->speed() - value.toDouble()) < 1e-9, QString("speed=%1").arg(active->speed()));
			} else if (key == "wireframe") {
				check(active->wireframe_enabled() == (value == "1"), QString("wireframe=%1").arg(active->wireframe_enabled()));
			} else if (key == "hidden") {
				const int geoset = value.section('/', 0, 0).toInt();
				const bool want_hidden = value.section('/', 1) == "1";
				if (geoset < 0 || geoset >= static_cast<int>(active->mdx->geosets.size())) {
					log("FAIL", QString("geoset %1 is out of range (0..%2)").arg(geoset).arg(active->mdx->geosets.size()));
					continue;
				}
				const bool mesh_hidden = active->mesh && geoset < static_cast<int>(active->mesh->hidden_geosets.size()) && active->mesh->hidden_geosets[geoset];
				check(
					!active->geoset_visible(geoset) == want_hidden && mesh_hidden == want_hidden,
					QString("widget hidden=%1, render mask hidden=%2").arg(!active->geoset_visible(geoset)).arg(mesh_hidden)
				);
			} else if (key == "sequence") {
				const int want = value.toInt();
				if (want < 0 || want >= static_cast<int>(active->mdx->sequences.size())) {
					log("FAIL", QString("sequence %1 is out of range (0..%2)").arg(want).arg(active->mdx->sequences.size()));
					continue;
				}
				const auto readout = timeline->findChild<QLabel*>("timelineReadout")->text();
				const QString want_length = QString("/ %1 ms").arg(active->mdx->sequences[want].end_frame - active->mdx->sequences[want].start_frame);
				check(
					active->sequence() == want && sequence_box->currentIndex() == want && inspector->sequence_table()->currentIndex().row() == want
						&& readout.endsWith(want_length),
					state
				);
			} else if (key == "viewfewer" || key == "viewdifferent") {
				if (qa_view_counts.size() < 2) {
					log("FAIL", "needs two viewshots");
				} else {
					const long long before = qa_view_counts[qa_view_counts.size() - 2];
					const long long after = qa_view_counts.back();
					check(key == "viewfewer" ? after < before : after != before, QString("drawn pixels %1 -> %2").arg(before).arg(after));
				}
			} else if (key == "viewmin") {
				bool number = false;
				const long long minimum = value.toLongLong(&number);
				if (!number || minimum <= 0) {
					log("FAIL", "viewmin needs a pixel count above 0");
				} else if (qa_view_counts.empty()) {
					log("FAIL", "needs a viewshot");
				} else {
					check(qa_view_counts.back() >= minimum, QString("drawn pixels %1").arg(qa_view_counts.back()));
				}
			} else if (key == "order" || key == "texture" || key == "uv") {
				if (!active->mesh) {
					log("FAIL", "nothing is shown (wait first)");
					continue;
				}
				const auto& mesh = *active->mesh;
				// "<g>" or "<g>.<l>": geoset index into the model's geosets and layer index (default 0).
				const auto parse_layer = [](const QString& text, int& geoset, int& layer) {
					bool geoset_ok = false;
					bool layer_ok = true;
					geoset = text.section('.', 0, 0).toInt(&geoset_ok);
					layer = text.contains('.') ? text.section('.', 1).toInt(&layer_ok) : 0;
					return geoset_ok && layer_ok;
				};
				if (key == "order") {
					QStringList actual;
					for (std::size_t i = 0; i < mesh.draw_items.size(); ++i) {
						const auto layer = mesh.layer_state(i, active->skeleton);
						actual << (layer.layer == 0 ? QString::number(layer.geoset) : QString("%1.%2").arg(layer.geoset).arg(layer.layer));
					}
					QStringList wanted;
					bool parsed = true;
					for (const QString& token : value.split(',', Qt::SkipEmptyParts)) {
						int geoset = 0;
						int layer = 0;
						parsed = parsed && parse_layer(token.trimmed(), geoset, layer);
						wanted << (layer == 0 ? QString::number(geoset) : QString("%1.%2").arg(geoset).arg(layer));
					}
					if (!parsed || wanted.isEmpty()) {
						log("FAIL", "order needs <g>[.<l>] tokens separated by ','");
					} else {
						check(actual == wanted, QString("draw order %1").arg(actual.join(',')));
					}
					continue;
				}
				int geoset = 0;
				int layer_index = 0;
				if (!parse_layer(value.section('/', 0, 0), geoset, layer_index) || !value.contains('/')) {
					log("FAIL", QString("%1 needs <g>[.<l>]/<value>").arg(key));
					continue;
				}
				std::optional<EditableMesh::LayerState> found;
				for (std::size_t i = 0; i < mesh.draw_items.size() && !found; ++i) {
					const auto layer = mesh.layer_state(i, active->skeleton);
					if (layer.geoset == geoset && layer.layer == layer_index) {
						found = layer;
					}
				}
				if (!found) {
					log("FAIL", QString("geoset %1 layer %2 is not drawn by this model").arg(geoset).arg(layer_index));
					continue;
				}
				const QString wanted = value.section('/', 1);
				if (key == "texture") {
					bool number = false;
					const uint wanted_id = wanted.toUInt(&number);
					check(number && found->texture && *found->texture == wanted_id,
						QString("texture %1").arg(found->texture ? QString::number(*found->texture) : QString("none")));
				} else {
					const QStringList parts = wanted.split(',');
					const std::array<float, 6> actual = {
						found->uv.translation.x, found->uv.translation.y, found->uv.rotation.x,
						found->uv.rotation.y,    found->uv.scale.x,       found->uv.scale.y,
					};
					bool matches = parts.size() == 2 || parts.size() == 6;
					for (qsizetype i = 0; matches && i < parts.size(); ++i) {
						bool number = false;
						const double part = parts[i].toDouble(&number);
						matches = number && std::abs(part - actual[static_cast<std::size_t>(i)]) <= 0.001;
					}
					check(matches,
						QString("uv t(%1,%2) r(%3,%4) s(%5,%6)").arg(actual[0]).arg(actual[1]).arg(actual[2]).arg(actual[3]).arg(actual[4]).arg(actual[5]));
				}
			} else {
				log("FAIL", "unknown expectation");
			}
		} else {
			log("FAIL", "unknown command");
		}
	}
	qa_finish();
}
