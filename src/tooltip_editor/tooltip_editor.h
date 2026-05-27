#pragma once

#include <QMainWindow>
#include <QListView>
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QTabWidget>
#include <QSortFilterProxyModel>
#include <QLineEdit>
#include <string>

import SLK;
import UnitListModel;
import ItemListModel;
import AbilityListModel;
import TableModel;

class TemplateManager;

class TooltipEditor : public QMainWindow {
	Q_OBJECT

	struct TabData {
		slk::SLK* slk = nullptr;
		QSortFilterProxyModel* filter_model = nullptr;
		QListView* list_view = nullptr;
		QLineEdit* search_bar = nullptr;
		QPlainTextEdit* name_edit = nullptr;
		QPlainTextEdit* tip_edit = nullptr;
		QPlainTextEdit* ubertip_edit = nullptr;
		QTextBrowser* preview = nullptr;
		std::string current_id;
		const char* tip_field = "tip";
		const char* ubertip_field = "ubertip";
	};

	QTabWidget* tab_widget;
	TabData unit_tab;
	TabData item_tab;
	TabData ability_tab;

	QPlainTextEdit* active_edit = nullptr;
	TemplateManager* template_mgr = nullptr;

	QWidget* build_tab(TabData* tab, slk::SLK& slk, QAbstractItemModel* list_model,
	                   const char* tip_field = "tip", const char* ubertip_field = "ubertip");
	void load_entry(TabData& tab, const std::string& id);
	void save_current(TabData& tab);
	void update_preview(TabData& tab);
	void insert_at_cursor(const QString& text);

	static QString wc3_to_html(const QString& wc3_text);

public:
	explicit TooltipEditor(QWidget* parent = nullptr);

public slots:
	void insert_template_text(const QString& text);
};
