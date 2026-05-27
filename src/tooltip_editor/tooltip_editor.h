#pragma once

#include <QMainWindow>
#include <QListView>
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QTabWidget>
#include <QSortFilterProxyModel>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include <QAbstractTableModel>
#include <QWidget>
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
		QAbstractTableModel* table = nullptr;    // Used for TRIGSTR_-resolved reads
		QSortFilterProxyModel* filter_model = nullptr;
		QListView* list_view = nullptr;
		QLineEdit* search_bar = nullptr;
		QLabel* tip_label = nullptr;
		QLabel* utip_label = nullptr;
		QPlainTextEdit* tip_edit = nullptr;      // First tooltip field
		QPlainTextEdit* ubertip_edit = nullptr;  // Second tooltip field
		QTextBrowser* preview = nullptr;
		QWidget* level_row = nullptr;            // Shown only for abilities
		QSpinBox* level_spin = nullptr;
		std::string current_id;
		int current_level = 1;
		bool leveled = false;
		std::string tip_field_name;    // Base SLK column name (level suffix added if leveled)
		std::string utip_field_name;   // Base SLK column name (level suffix added if leveled)
	};

	QTabWidget* tab_widget;
	TabData unit_tab;
	TabData item_tab;
	TabData ability_tab;

	QPlainTextEdit* active_edit = nullptr;
	TemplateManager* template_mgr = nullptr;

	QWidget* build_tab(TabData* tab, slk::SLK& slk, QAbstractTableModel* table,
	                   QAbstractItemModel* list_model, bool leveled,
	                   const char* tip_label_text, const char* utip_label_text);
	void load_entry(TabData& tab, const std::string& id);
	void load_level(TabData& tab);
	void save_current(TabData& tab);
	void update_preview(TabData& tab);
	void insert_at_cursor(const QString& text);

	std::string resolve_tip_field(const TabData& tab) const;
	std::string resolve_utip_field(const TabData& tab) const;
	QString read_field(const TabData& tab, const std::string& field) const;

	static std::string meta_field_name(const slk::SLK& meta_slk, const std::string& code);
	static QString wc3_to_html(const QString& wc3_text);

public:
	explicit TooltipEditor(QWidget* parent = nullptr);

public slots:
	void insert_template_text(const QString& text);
};
