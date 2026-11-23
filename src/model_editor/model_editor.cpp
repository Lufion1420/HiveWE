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
#include "model_view.h"
#include "theme.h"

import std;
import BinaryReader;
import Hierarchy;
import MDX;
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

	const auto mdx_data = active->mdx->to_mdx();
	std::ofstream file(fs::path(file_name.toStdWString()), std::ios::binary);
	if (file) {
		file.write(reinterpret_cast<const char*>(mdx_data.buffer.data()), mdx_data.buffer.size());
		file.close();
	}
	if (!file.good()) {
		QMessageBox::warning(this, "Error saving MDX file", "Could not write " + file_name + ". The file may be incomplete.");
	}
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
	QDialog* dialog = new QDialog(parent, Qt::WindowTitleHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->resize(1200, 720);
	dialog->setWindowModality(Qt::WindowModality::WindowModal);

	// Force native window, otherwise ModelView causes switch to OpenGL-capable
	// surface which unmaps the window and shows white flash.
	dialog->setAttribute(Qt::WA_NativeWindow);
	(void)dialog->winId();

	ModelView* view = new ModelView(dialog);

	QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(buttonBox, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
	connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
	connect(view, &ModelView::doubleClicked, dialog, &QDialog::accept);

	connect(dialog, &QDialog::accepted, this, [this, view, parent] {
		show_model(view->current_model_path().toStdString(), false, parent);
	});

	QVBoxLayout* layout = new QVBoxLayout(dialog);
	layout->addWidget(view);
	layout->addWidget(buttonBox);

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
	const std::string extension = to_lowercase_copy(path.extension().string());
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
		return std::unexpected(std::format("{} is not a readable model: {}", path.filename().string(), e.what()));
	}

	auto* gl_widget = new ModelEditorGLWidget(nullptr, mdx);
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
	dock_tab->setWindowTitle(QString::fromStdString(path.filename().string()));
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
//   seq-combo:<n>  seq-list:<n>  slider:<0..1000>  mark  state  shot:<png of the window>  viewshot:<png of the viewport>  quit
//   expect:advanced | same | time=<ms>[/tol] | timefrac=<0..1> | rate=<x> | playing=0|1 | loop=0|1 | speed=<x> | wireframe=0|1
//          | hidden=<n>/0|1 | sequence=<n> | viewfewer | viewdifferent
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
			} else {
				log("FAIL", "unknown expectation");
			}
		} else {
			log("FAIL", "unknown command");
		}
	}
	qa_finish();
}
