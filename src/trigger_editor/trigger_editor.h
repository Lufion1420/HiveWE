#pragma once

#include "ui_trigger_editor.h"

#include <QTreeView>
#include <QTreeWidget>
#include <QPlainTextEdit>
#include <QKeyEvent>
#include <QElapsedTimer>
#include <QPointer>

#include <vector>
#include <string>
#include <unordered_map>

#include "DockManager.h"
#include "DockAreaWidget.h"
#include "trigger_explorer.h"
#include "global_search.h"

import Triggers;

class TriggerEditor : public QMainWindow {
	Q_OBJECT

public:
	explicit TriggerEditor(QWidget* parent = nullptr);
	~TriggerEditor() override;

	/// Writes what the user changed in the open tabs into the map the editor was opened for.
	void save_changes();
	/// True when an open tab holds a change that save_changes() has not written yet.
	[[nodiscard]]
	bool has_pending_changes() const;
private:
	Ui::TriggerEditor ui;

	/// The map the editor was opened for. A closed editor is deleted later, possibly after another map replaced this one.
	QPointer<QObject> edited_map;

	ads::CDockManager* dock_manager = nullptr;
	ads::CDockAreaWidget* dock_area = nullptr;

	TriggerExplorer* explorer = new TriggerExplorer;
	TreeModel* model;

	QPlainTextEdit* compile_output = new QPlainTextEdit;

	QIcon folder_icon;
	QIcon gui_icon;
	QIcon gui_icon_disabled;
	QIcon script_icon;
	QIcon script_icon_disabled;
	QIcon variable_icon;
	QIcon comment_icon;

	QIcon event_icon;
	QIcon condition_icon;
	QIcon action_icon;

	std::unordered_map<std::string, QIcon> trigger_icons;

	void focus_search_window();
	void save_tab(ads::CDockWidget* tab);

	void item_clicked(const QModelIndex& index);
	void show_gui_trigger(QTreeWidget* edit, const Trigger& trigger);

	std::string get_parameters_names(const std::vector<std::string>& string_parameters, const std::vector<TriggerParameter>& parameters) const;

	QElapsedTimer double_shift_timer;

	void keyPressEvent(QKeyEvent* event) override {
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
};