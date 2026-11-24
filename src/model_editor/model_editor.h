#pragma once

#include <filesystem>
#include <QElapsedTimer>
namespace fs = std::filesystem;
#include <expected>

#include "global_search.h"

#include <QMainWindow>
#include <QKeyEvent>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <vector>

#include <DockManager.h>
#include <DockAreaWidget.h>

class AnimationTimeline;
class ModelEditorGLWidget;
class ModelInspector;
class QAction;
class QComboBox;
class QLabel;

class ModelEditor : public QMainWindow {
	Q_OBJECT

public:
	explicit ModelEditor(QWidget* parent = nullptr);

	void open_file();
	/// Parent specifies which dock area widget the model will open in
	void browse_models(ads::CDockAreaWidget* parent);

	/// Loads a model and builds its dock tab without adding it anywhere. The tab is owned by the caller until it is added.
	[[nodiscard]] std::expected<ads::CDockWidget*, std::string> open_model(const fs::path& path, bool local_file);
	/// Loads a model and shows it in a new tab of `area` (the main area when null). A failure is reported in a message box.
	/// With `error` set, a failure is stored there instead of shown in a message box (the --qa-offscreen hook must never block on one).
	bool show_model(const fs::path& path, bool local_file, ads::CDockAreaWidget* area = nullptr, std::string* error = nullptr);

	/// Driven by --qa-model-editor-script (needs --qa-offscreen): runs the steps through the real widgets and slots, one after the other
	/// from the event loop, writes PASS/FAIL/INFO lines to `out_file` and quits the application. Steps are separated by ';', see model_editor.cpp.
	void run_qa_script(const QString& steps, const QString& out_file);

	void keyPressEvent(QKeyEvent* event) override {
		if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
			// Only reaches the window when the focused widget did not use Space (a combo box or table does).
			toggle_playback();
			event->accept();
			return;
		}
		if (event->key() == Qt::Key_Shift && !event->isAutoRepeat()) {
			if (double_shift_timer.isValid() && double_shift_timer.elapsed() < 400) {

				GlobalSearchWidget search_widget = new GlobalSearchWidget(this);
				double_shift_timer.invalidate();
			} else {
				double_shift_timer.start();
			}
		}
		QMainWindow::keyPressEvent(event);
	}

protected:
	void closeEvent(QCloseEvent* event) override;
	void changeEvent(QEvent* event) override;

private:
	void build_toolbar();
	void build_status_bar();
	/// Points the timeline, inspector, sequence list and status bar at `widget` (null for no model).
	void set_active(ModelEditorGLWidget* widget);
	void refresh_status();
	/// Selected sequence, range and playhead of the active widget into the combo box, the Sequences panel and the timeline.
	void refresh_sequence_ui();
	/// Toolbar icons in the active theme's colour (Flat themes), plain text buttons otherwise.
	void refresh_icons();
	void toggle_playback();
	void qa_continue();
	void qa_tick();
	void qa_finish();
	void edit_as_mdl();
	void save_as_mdx();
	QString write_active_mdx(const fs::path& path);

	ads::CDockManager* dock_manager;
	ads::CDockAreaWidget* dock_area = nullptr;

	AnimationTimeline* timeline = nullptr;
	ModelInspector* inspector = nullptr;
	QComboBox* sequence_box = nullptr;
	QAction* wireframe_action = nullptr;
	QAction* reset_camera_action = nullptr;
	QAction* edit_mdl_action = nullptr;
	QAction* save_mdx_action = nullptr;
	QLabel* status_name = nullptr;
	QLabel* status_vertices = nullptr;
	QLabel* status_triangles = nullptr;
	QLabel* status_bones = nullptr;
	QLabel* status_camera = nullptr;

	struct ToolbarIcon {
		QAction* action;
		QString icon;
		bool with_text;
	};
	std::vector<ToolbarIcon> toolbar_icons;

	QPointer<ModelEditorGLWidget> active;

	QStringList qa_steps;
	int qa_index = 0;
	QString qa_out;
	QStringList qa_log;
	double qa_mark_frame = 0.0;
	QElapsedTimer qa_mark_timer;
	QElapsedTimer qa_wait_timer;
	int qa_wait_ms = 0;
	std::vector<long long> qa_view_counts;

	QElapsedTimer double_shift_timer;
};
