#include "template_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QPushButton>
#include <QLabel>
#include <QSettings>
#include <QMessageBox>

QString TemplateManager::wc3_to_html(const QString& text) {
	QString html;
	html.reserve(text.size() * 2);

	int i = 0;
	while (i < text.size()) {
		if (text[i] == '|' && i + 1 < text.size()) {
			const char next = text[i + 1].toLatin1();
			if (next == 'c' && i + 10 <= text.size()) {
				const QString hex = text.mid(i + 2, 8);
				if (hex.size() == 8) {
					const QString rr = hex.mid(2, 2);
					const QString gg = hex.mid(4, 2);
					const QString bb = hex.mid(6, 2);
					html += "<span style=\"color:#" + rr + gg + bb + "\">";
					i += 10;
					continue;
				}
			} else if (next == 'r') {
				html += "</span>";
				i += 2;
				continue;
			} else if (next == 'n') {
				html += "<br>";
				i += 2;
				continue;
			}
		}

		if (text[i] == '<') {
			html += "&lt;";
		} else if (text[i] == '>') {
			html += "&gt;";
		} else if (text[i] == '&') {
			html += "&amp;";
		} else {
			html += text[i];
		}
		++i;
	}
	return html;
}

void TemplateManager::load_from_settings() {
	QSettings settings;
	settings.beginGroup("TooltipEditor");
	const int count = settings.beginReadArray("templates");
	templates.clear();
	templates.reserve(count);
	for (int i = 0; i < count; ++i) {
		settings.setArrayIndex(i);
		templates.push_back({settings.value("name").toString(), settings.value("text").toString()});
	}
	settings.endArray();
	settings.endGroup();
}

void TemplateManager::save_to_settings() const {
	QSettings settings;
	settings.beginGroup("TooltipEditor");
	settings.beginWriteArray("templates");
	for (int i = 0; i < static_cast<int>(templates.size()); ++i) {
		settings.setArrayIndex(i);
		settings.setValue("name", templates[i].name);
		settings.setValue("text", templates[i].text);
	}
	settings.endArray();
	settings.endGroup();
}

void TemplateManager::refresh_list() {
	const QSignalBlocker blocker(template_list);
	template_list->clear();
	for (const auto& t : templates) {
		template_list->addItem(t.name);
	}
}

void TemplateManager::load_template(int index) {
	if (index < 0 || index >= static_cast<int>(templates.size())) {
		return;
	}
	current_index = index;
	loading = true;
	name_edit->setText(templates[index].name);
	text_edit->setPlainText(templates[index].text);
	preview->setHtml(wc3_to_html(templates[index].text));
	loading = false;
}

void TemplateManager::commit_current() {
	if (loading || current_index < 0 || current_index >= static_cast<int>(templates.size())) {
		return;
	}
	templates[current_index].name = name_edit->text().trimmed();
	templates[current_index].text = text_edit->toPlainText();

	if (auto* item = template_list->item(current_index)) {
		const QSignalBlocker blocker(template_list);
		item->setText(templates[current_index].name);
	}
	save_to_settings();
}

TemplateManager::TemplateManager(QWidget* parent)
	: QWidget(parent) {
	setWindowTitle("Template Manager");
	setWindowFlags(Qt::Window);
	setMinimumSize(620, 440);

	load_from_settings();

	auto* main_layout = new QHBoxLayout(this);
	auto* splitter = new QSplitter(Qt::Horizontal);
	main_layout->addWidget(splitter);

	// Left panel: list + action buttons
	auto* left_panel = new QWidget;
	auto* left_layout = new QVBoxLayout(left_panel);
	left_layout->setContentsMargins(4, 4, 4, 4);
	left_layout->setSpacing(4);

	left_layout->addWidget(new QLabel("Templates:"));
	template_list = new QListWidget;
	left_layout->addWidget(template_list);

	auto* btn_row = new QHBoxLayout;
	auto* add_btn = new QPushButton("New");
	auto* del_btn = new QPushButton("Delete");
	btn_row->addWidget(add_btn);
	btn_row->addWidget(del_btn);
	left_layout->addLayout(btn_row);

	auto* insert_btn = new QPushButton("Insert into Tooltip Editor");
	insert_btn->setStyleSheet("QPushButton { background-color: #3a6ba0; color: white; padding: 5px; border-radius: 3px; }"
	                          "QPushButton:hover { background-color: #4a7bb0; }");
	left_layout->addWidget(insert_btn);

	left_panel->setMinimumWidth(160);
	left_panel->setMaximumWidth(220);

	// Right panel: name + text editor + preview
	auto* right_panel = new QWidget;
	auto* right_layout = new QVBoxLayout(right_panel);
	right_layout->setContentsMargins(4, 4, 4, 4);
	right_layout->setSpacing(6);

	right_layout->addWidget(new QLabel("Template Name:"));
	name_edit = new QLineEdit;
	name_edit->setPlaceholderText("Template name...");
	right_layout->addWidget(name_edit);

	right_layout->addWidget(new QLabel("Template Text (supports WC3 color codes and |n line breaks):"));
	text_edit = new QPlainTextEdit;
	text_edit->setPlaceholderText("Enter template text here. Use |cFFRRGGBB for colors and |r to end color. Use |n for line breaks.");
	right_layout->addWidget(text_edit);

	right_layout->addWidget(new QLabel("Preview:"));
	preview = new QTextBrowser;
	preview->setMaximumHeight(120);
	preview->setStyleSheet("background: #1c1c1c; color: #ffffff; border-radius: 4px;");
	right_layout->addWidget(preview);

	splitter->addWidget(left_panel);
	splitter->addWidget(right_panel);
	splitter->setStretchFactor(0, 0);
	splitter->setStretchFactor(1, 1);

	refresh_list();

	// Signals
	connect(template_list, &QListWidget::currentRowChanged, this, [this](int row) {
		commit_current();
		load_template(row);
	});

	connect(add_btn, &QPushButton::clicked, this, [this]() {
		commit_current();
		templates.push_back({"New Template", ""});
		save_to_settings();
		refresh_list();
		template_list->setCurrentRow(static_cast<int>(templates.size()) - 1);
	});

	connect(del_btn, &QPushButton::clicked, this, [this]() {
		const int row = template_list->currentRow();
		if (row < 0 || row >= static_cast<int>(templates.size())) {
			return;
		}
		if (QMessageBox::question(this, "Delete Template",
		                          QString("Delete \"%1\"?").arg(templates[row].name))
		    != QMessageBox::Yes) {
			return;
		}
		templates.erase(templates.begin() + row);
		current_index = -1;
		save_to_settings();
		refresh_list();
		name_edit->clear();
		text_edit->clear();
		preview->clear();
	});

	connect(name_edit, &QLineEdit::textEdited, this, [this]() {
		commit_current();
	});

	connect(text_edit, &QPlainTextEdit::textChanged, this, [this]() {
		if (loading) {
			return;
		}
		if (current_index >= 0 && current_index < static_cast<int>(templates.size())) {
			templates[current_index].text = text_edit->toPlainText();
			preview->setHtml(wc3_to_html(text_edit->toPlainText()));
		}
		save_to_settings();
	});

	connect(insert_btn, &QPushButton::clicked, this, [this]() {
		if (current_index >= 0 && current_index < static_cast<int>(templates.size())) {
			emit template_insert_requested(templates[current_index].text);
		}
	});

	if (!templates.empty()) {
		template_list->setCurrentRow(0);
	}
}

TemplateManager::~TemplateManager() {
	commit_current();
}

#include "template_manager.moc"
