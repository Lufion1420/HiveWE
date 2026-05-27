#pragma once

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <vector>

class TemplateManager : public QWidget {
	Q_OBJECT

	struct Template {
		QString name;
		QString text;
	};

	QListWidget* template_list;
	QLineEdit* name_edit;
	QPlainTextEdit* text_edit;
	QTextBrowser* preview;

	std::vector<Template> templates;
	int current_index = -1;
	bool loading = false;

	void load_from_settings();
	void save_to_settings() const;
	void refresh_list();
	void load_template(int index);
	void commit_current();

	static QString wc3_to_html(const QString& text);

public:
	explicit TemplateManager(QWidget* parent = nullptr);
	~TemplateManager() override;

signals:
	void template_insert_requested(const QString& text);
};
