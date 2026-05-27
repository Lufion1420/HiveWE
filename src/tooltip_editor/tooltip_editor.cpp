#include "tooltip_editor.h"
#include "template_manager.h"

import std;
import Globals;
import TableModel;

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QToolBar>
#include <QColorDialog>
#include <QPushButton>

namespace {
	struct ColorPreset {
		const char* label;
		const char* code;
		const char* bg;
		const char* fg;
	};

	constexpr std::array COLOR_PRESETS = {
		ColorPreset{"Gold",       "|cffffcc00", "#ffcc00", "black"},
		ColorPreset{"White",      "|cffffffff", "#e8e8e8", "black"},
		ColorPreset{"Red",        "|cffff0303", "#ff0303", "white"},
		ColorPreset{"Green",      "|cff1ce6b9", "#1ce6b9", "black"},
		ColorPreset{"Blue",       "|cff0042ff", "#0042ff", "white"},
		ColorPreset{"Orange",     "|cffff8000", "#ff8000", "black"},
		ColorPreset{"Purple",     "|cff9b00ff", "#9b00ff", "white"},
		ColorPreset{"Dark Red",   "|cff9b0000", "#9b0000", "white"},
		ColorPreset{"Cyan",       "|cff00ffff", "#00ffff", "black"},
		ColorPreset{"Dark Green", "|cff008000", "#008000", "white"},
	};
} // namespace

QString TooltipEditor::wc3_to_html(const QString& text) {
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

QWidget* TooltipEditor::build_tab(TabData* tab, slk::SLK& slk, QAbstractItemModel* list_model,
                                   const char* tip_field, const char* ubertip_field) {
	tab->slk = &slk;
	tab->tip_field = tip_field;
	tab->ubertip_field = ubertip_field;

	auto* splitter = new QSplitter(Qt::Horizontal);

	// Left panel: search + list
	auto* left_panel = new QWidget;
	auto* left_layout = new QVBoxLayout(left_panel);
	left_layout->setContentsMargins(4, 4, 4, 4);
	left_layout->setSpacing(4);

	tab->search_bar = new QLineEdit;
	tab->search_bar->setPlaceholderText("Search...");
	tab->search_bar->setClearButtonEnabled(true);

	tab->filter_model = new QSortFilterProxyModel(left_panel);
	tab->filter_model->setSourceModel(list_model);
	tab->filter_model->setFilterCaseSensitivity(Qt::CaseInsensitive);
	tab->filter_model->sort(0, Qt::AscendingOrder);

	tab->list_view = new QListView;
	tab->list_view->setModel(tab->filter_model);
	tab->list_view->setIconSize({24, 24});
	tab->list_view->setUniformItemSizes(true);

	connect(tab->search_bar, &QLineEdit::textChanged, tab->filter_model, &QSortFilterProxyModel::setFilterFixedString);

	left_layout->addWidget(tab->search_bar);
	left_layout->addWidget(tab->list_view);
	left_panel->setMinimumWidth(160);
	left_panel->setMaximumWidth(260);

	// Right panel: editor fields + preview
	auto* right_panel = new QWidget;
	auto* right_layout = new QVBoxLayout(right_panel);
	right_layout->setContentsMargins(8, 4, 4, 4);
	right_layout->setSpacing(6);

	auto add_field = [&](const QString& label, QPlainTextEdit*& edit_out, int max_height) {
		right_layout->addWidget(new QLabel(label));
		edit_out = new QPlainTextEdit;
		edit_out->setEnabled(false);
		edit_out->setMaximumHeight(max_height);
		right_layout->addWidget(edit_out);
	};

	add_field("Name:", tab->name_edit, 40);
	add_field("Tooltip (Tip):", tab->tip_edit, 60);
	add_field("Description (Ubertip):", tab->ubertip_edit, 120);

	right_layout->addWidget(new QLabel("Preview (Ubertip):"));
	tab->preview = new QTextBrowser;
	tab->preview->setMinimumHeight(80);
	tab->preview->setMaximumHeight(150);
	tab->preview->setStyleSheet("background: #1c1c1c; color: #ffffff; border-radius: 4px;");
	right_layout->addWidget(tab->preview);
	right_layout->addStretch();

	// Wire up editor signals
	auto connect_edit = [&](QPlainTextEdit* edit) {
		connect(edit, &QPlainTextEdit::textChanged, this, [this, tab]() {
			save_current(*tab);
			update_preview(*tab);
		});
		connect(edit, &QPlainTextEdit::cursorPositionChanged, this, [this, edit]() {
			active_edit = edit;
		});
	};
	connect_edit(tab->name_edit);
	connect_edit(tab->tip_edit);
	connect_edit(tab->ubertip_edit);

	// Entry selection from list
	connect(tab->list_view->selectionModel(), &QItemSelectionModel::currentChanged,
	        this, [this, tab](const QModelIndex& current) {
		if (!current.isValid()) {
			return;
		}
		save_current(*tab);
		const int row = tab->filter_model->mapToSource(current).row();
		if (row < 0 || !tab->slk->index_to_row.contains(static_cast<size_t>(row))) {
			return;
		}
		load_entry(*tab, tab->slk->index_to_row.at(static_cast<size_t>(row)));
	});

	splitter->addWidget(left_panel);
	splitter->addWidget(right_panel);
	splitter->setStretchFactor(0, 0);
	splitter->setStretchFactor(1, 1);

	return splitter;
}

void TooltipEditor::load_entry(TabData& tab, const std::string& id) {
	tab.current_id = id;

	auto read = [&](const char* field) -> QString {
		if (!tab.slk->column_headers.contains(field)) {
			return {};
		}
		return QString::fromStdString(tab.slk->data<std::string>(field, id));
	};

	{
		const QSignalBlocker b1(tab.name_edit), b2(tab.tip_edit), b3(tab.ubertip_edit);
		tab.name_edit->setPlainText(read("name"));
		tab.tip_edit->setPlainText(read(tab.tip_field));
		tab.ubertip_edit->setPlainText(read(tab.ubertip_field));
	}

	tab.name_edit->setEnabled(true);
	tab.tip_edit->setEnabled(tab.slk->column_headers.contains(tab.tip_field));
	tab.ubertip_edit->setEnabled(tab.slk->column_headers.contains(tab.ubertip_field));

	update_preview(tab);
}

void TooltipEditor::save_current(TabData& tab) {
	if (tab.current_id.empty()) {
		return;
	}
	tab.slk->set_shadow_data("name", tab.current_id, tab.name_edit->toPlainText().toStdString());
	if (tab.slk->column_headers.contains(tab.tip_field)) {
		tab.slk->set_shadow_data(tab.tip_field, tab.current_id, tab.tip_edit->toPlainText().toStdString());
	}
	if (tab.slk->column_headers.contains(tab.ubertip_field)) {
		tab.slk->set_shadow_data(tab.ubertip_field, tab.current_id, tab.ubertip_edit->toPlainText().toStdString());
	}
}

void TooltipEditor::update_preview(TabData& tab) {
	tab.preview->setHtml(wc3_to_html(tab.ubertip_edit->toPlainText()));
}

void TooltipEditor::insert_at_cursor(const QString& text) {
	if (!active_edit || !active_edit->isEnabled()) {
		return;
	}
	active_edit->insertPlainText(text);
	active_edit->setFocus();
}

void TooltipEditor::insert_template_text(const QString& text) {
	insert_at_cursor(text);
}

TooltipEditor::TooltipEditor(QWidget* parent)
	: QMainWindow(parent) {
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowTitle("Tooltip Editor");
	setMinimumSize(860, 600);

	// Formatting toolbar
	auto* toolbar = new QToolBar("Tooltip Formatting", this);
	toolbar->setMovable(false);
	addToolBar(toolbar);

	for (const auto& p : COLOR_PRESETS) {
		auto* btn = new QPushButton(QString::fromUtf8(p.label));
		btn->setFixedWidth(72);
		btn->setToolTip(QString("Insert %1 color code (%2)").arg(p.label).arg(p.code));
		btn->setStyleSheet(
			QString("QPushButton{background:%1;color:%2;border-radius:3px;padding:2px 4px;font-size:11px;}"
			        "QPushButton:hover{border:1px solid rgba(255,255,255,140);}")
				.arg(p.bg)
				.arg(p.fg));
		const QString code = QString::fromUtf8(p.code);
		connect(btn, &QPushButton::clicked, this, [this, code]() { insert_at_cursor(code); });
		toolbar->addWidget(btn);
	}

	toolbar->addSeparator();

	auto* custom_btn = new QPushButton("Custom...");
	custom_btn->setToolTip("Pick a custom color");
	connect(custom_btn, &QPushButton::clicked, this, [this]() {
		const QColor color = QColorDialog::getColor(Qt::white, this, "Pick Color");
		if (!color.isValid()) {
			return;
		}
		const QString code = QString("|cff%1%2%3")
		                         .arg(color.red(), 2, 16, QChar('0'))
		                         .arg(color.green(), 2, 16, QChar('0'))
		                         .arg(color.blue(), 2, 16, QChar('0'));
		insert_at_cursor(code);
	});
	toolbar->addWidget(custom_btn);

	auto* end_color_btn = new QPushButton("|r");
	end_color_btn->setToolTip("End color sequence");
	connect(end_color_btn, &QPushButton::clicked, this, [this]() { insert_at_cursor("|r"); });
	toolbar->addWidget(end_color_btn);

	toolbar->addSeparator();

	auto* newline_btn = new QPushButton("|n  New Line");
	newline_btn->setToolTip("Insert WC3 line break (|n)");
	connect(newline_btn, &QPushButton::clicked, this, [this]() { insert_at_cursor("|n"); });
	toolbar->addWidget(newline_btn);

	toolbar->addSeparator();

	auto* templates_btn = new QPushButton("Templates...");
	templates_btn->setToolTip("Open the template manager");
	connect(templates_btn, &QPushButton::clicked, this, [this]() {
		if (!template_mgr) {
			template_mgr = new TemplateManager(this);
			connect(template_mgr, &TemplateManager::template_insert_requested,
			        this, &TooltipEditor::insert_template_text);
		}
		template_mgr->show();
		template_mgr->raise();
		template_mgr->activateWindow();
	});
	toolbar->addWidget(templates_btn);

	// Central area with tabs
	auto* central = new QWidget(this);
	auto* main_layout = new QVBoxLayout(central);
	main_layout->setContentsMargins(0, 0, 0, 0);

	tab_widget = new QTabWidget;
	main_layout->addWidget(tab_widget);
	setCentralWidget(central);

	if (!units_table || !items_table || !abilities_table) {
		auto* label = new QLabel("No map loaded. Please open a map first.");
		label->setAlignment(Qt::AlignCenter);
		tab_widget->addTab(label, "No Map");
		return;
	}

	auto* unit_list = new UnitListModel(this);
	unit_list->setSourceModel(units_table);

	auto* item_list = new ItemListModel(this);
	item_list->setSourceModel(items_table);

	auto* ability_list = new AbilityListModel(this);
	ability_list->setSourceModel(abilities_table);

	// Abilities use level-based tip fields (tip1/ubertip1) if non-leveled ones are absent
	const char* ability_tip = abilities_slk.column_headers.contains("tip") ? "tip" : "tip1";
	const char* ability_ubertip = abilities_slk.column_headers.contains("ubertip") ? "ubertip" : "ubertip1";

	tab_widget->addTab(build_tab(&unit_tab, units_slk, unit_list), "Units");
	tab_widget->addTab(build_tab(&item_tab, items_slk, item_list), "Items");
	tab_widget->addTab(build_tab(&ability_tab, abilities_slk, ability_list, ability_tip, ability_ubertip), "Abilities");
}

#include "tooltip_editor.moc"
