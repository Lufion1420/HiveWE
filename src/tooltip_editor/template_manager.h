#pragma once

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QLabel>
#include <QPushButton>
#include <vector>

class TemplateManager : public QWidget {
	Q_OBJECT

	struct Template {
		QString name;
		QString tip_text;    // Tooltip Normal / title field
		QString text;        // Tooltip Extended / body field
		bool favorite = false;
	};

	QListWidget* template_list;
	QLineEdit* name_edit;
	QPlainTextEdit* tip_edit;    // Tooltip Normal input
	QPlainTextEdit* text_edit;   // Tooltip Extended input
	QTextBrowser* preview;
	QPushButton* fav_btn;
	QLabel* fav_count_label;
	QPlainTextEdit* active_field = nullptr;  // which field currently has cursor focus

	std::vector<Template> templates;
	int current_index = -1;
	bool loading = false;

	void load_from_settings();
	void save_to_settings() const;
	void refresh_list();
	void load_template(int index);
	void commit_current();
	void update_preview();
	void update_fav_ui();
	int favorite_count() const;
	void insert_color_code(const QString& code);

	static QString wc3_to_html(const QString& text);

public:
	explicit TemplateManager(QWidget* parent = nullptr);
	~TemplateManager() override;

signals:
	void template_apply_requested(const QString& tip_text, const QString& ubertip_text);
	void favorites_changed();
};
