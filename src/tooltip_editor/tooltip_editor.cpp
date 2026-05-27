#include "tooltip_editor.h"
#include "template_manager.h"

import std;
import Globals;
import TableModel;
import TriggerStrings;
import Utilities;

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

// Resolves the SLK column name for a modification code by looking up the meta SLK.
// E.g. meta_field_name(abilities_meta_slk, "atp1") -> "tip"
std::string TooltipEditor::meta_field_name(const slk::SLK& meta_slk, const std::string& code) {
	std::string field = to_lowercase_copy(meta_slk.data<std::string>("field", code));
	const int data_val = meta_slk.data<int>("data", code);
	if (data_val > 0) {
		field += static_cast<char>('a' + data_val - 1);
	}
	return field;
}

// Reads a field from the SLK, resolving any TRIGSTR_ reference via the trigger strings table.
// Returns empty if the field column does not exist.
QString TooltipEditor::read_field(const TabData& tab, const std::string& field) const {
	if (!tab.slk->column_headers.contains(field)) {
		return {};
	}
	auto raw = tab.slk->data<std::string>(field, tab.current_id);
	if (raw.starts_with("TRIGSTR_")) {
		auto* tm = static_cast<TableModel*>(tab.table);
		if (tm && tm->trigger_strings) {
			return QString::fromUtf8(tm->trigger_strings->string(raw));
		}
		return {};
	}
	return QString::fromStdString(raw);
}

std::string TooltipEditor::resolve_tip_field(const TabData& tab) const {
	if (tab.leveled) {
		return tab.tip_field_name + std::to_string(tab.current_level);
	}
	return tab.tip_field_name;
}

std::string TooltipEditor::resolve_utip_field(const TabData& tab) const {
	if (tab.leveled) {
		return tab.utip_field_name + std::to_string(tab.current_level);
	}
	return tab.utip_field_name;
}

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

QWidget* TooltipEditor::build_tab(TabData* tab, slk::SLK& slk, QAbstractTableModel* table,
                                   QAbstractItemModel* list_model, bool leveled,
                                   const char* tip_label_text, const char* utip_label_text) {
	tab->slk = &slk;
	tab->table = table;
	tab->leveled = leveled;
	tab->current_level = 1;

	auto* splitter = new QSplitter(Qt::Horizontal);

	// ── Left panel: search + list ──────────────────────────────────────────
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

	// ── Right panel: editor fields + preview ───────────────────────────────
	auto* right_panel = new QWidget;
	auto* right_layout = new QVBoxLayout(right_panel);
	right_layout->setContentsMargins(8, 4, 4, 4);
	right_layout->setSpacing(6);

	// Level row (visible only for leveled tabs)
	tab->level_row = new QWidget;
	auto* level_row_layout = new QHBoxLayout(tab->level_row);
	level_row_layout->setContentsMargins(0, 0, 0, 0);
	level_row_layout->setSpacing(6);
	level_row_layout->addWidget(new QLabel("Level:"));
	tab->level_spin = new QSpinBox;
	tab->level_spin->setMinimum(1);
	tab->level_spin->setMaximum(4);
	tab->level_spin->setValue(1);
	tab->level_spin->setFixedWidth(60);
	level_row_layout->addWidget(tab->level_spin);
	level_row_layout->addStretch();
	tab->level_row->setVisible(leveled);
	right_layout->addWidget(tab->level_row);

	// First tooltip field
	tab->tip_label = new QLabel(tip_label_text);
	right_layout->addWidget(tab->tip_label);
	tab->tip_edit = new QPlainTextEdit;
	tab->tip_edit->setEnabled(false);
	tab->tip_edit->setMaximumHeight(60);
	right_layout->addWidget(tab->tip_edit);

	// Second tooltip field
	tab->utip_label = new QLabel(utip_label_text);
	right_layout->addWidget(tab->utip_label);
	tab->ubertip_edit = new QPlainTextEdit;
	tab->ubertip_edit->setEnabled(false);
	right_layout->addWidget(tab->ubertip_edit);

	// Preview
	right_layout->addWidget(new QLabel("Preview:"));
	tab->preview = new QTextBrowser;
	tab->preview->setMinimumHeight(80);
	tab->preview->setMaximumHeight(150);
	tab->preview->setStyleSheet("background: #1c1c1c; color: #ffffff; border-radius: 4px;");
	right_layout->addWidget(tab->preview);
	right_layout->addStretch();

	// Wire editor signals
	auto connect_edit = [this, tab](QPlainTextEdit* edit) {
		connect(edit, &QPlainTextEdit::textChanged, this, [this, tab]() {
			save_current(*tab);
			update_preview(*tab);
		});
		connect(edit, &QPlainTextEdit::cursorPositionChanged, this, [this, edit]() {
			active_edit = edit;
		});
	};
	connect_edit(tab->tip_edit);
	connect_edit(tab->ubertip_edit);

	connect(tab->level_spin, &QSpinBox::valueChanged, this, [this, tab](int level) {
		save_current(*tab);
		tab->current_level = level;
		load_level(*tab);
	});

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

	if (tab.leveled) {
		int max_level = 4;
		if (tab.slk->column_headers.contains("levels")) {
			const int slk_levels = tab.slk->data<int>("levels", id);
			if (slk_levels > 0) {
				max_level = slk_levels;
			}
		}
		const QSignalBlocker sb(tab.level_spin);
		tab.level_spin->setMaximum(max_level);
		tab.level_spin->setValue(1);
		tab.current_level = 1;
	}

	load_level(tab);
}

void TooltipEditor::load_level(TabData& tab) {
	if (tab.current_id.empty()) {
		return;
	}

	const std::string tf = resolve_tip_field(tab);
	const std::string uf = resolve_utip_field(tab);

	{
		const QSignalBlocker b1(tab.tip_edit), b2(tab.ubertip_edit);
		tab.tip_edit->setPlainText(read_field(tab, tf));
		tab.ubertip_edit->setPlainText(read_field(tab, uf));
	}

	tab.tip_edit->setEnabled(tab.slk->column_headers.contains(tf));
	tab.ubertip_edit->setEnabled(tab.slk->column_headers.contains(uf));

	update_preview(tab);
}

void TooltipEditor::save_current(TabData& tab) {
	if (tab.current_id.empty()) {
		return;
	}
	const std::string tf = resolve_tip_field(tab);
	const std::string uf = resolve_utip_field(tab);

	if (tab.slk->column_headers.contains(tf)) {
		tab.slk->set_shadow_data(tf, tab.current_id, tab.tip_edit->toPlainText().toStdString());
	}
	if (tab.slk->column_headers.contains(uf)) {
		tab.slk->set_shadow_data(uf, tab.current_id, tab.ubertip_edit->toPlainText().toStdString());
	}
}

void TooltipEditor::update_preview(TabData& tab) {
	const QString tip = tab.tip_edit->toPlainText();
	const QString ubertip = tab.ubertip_edit->toPlainText();

	QString html;
	if (!tip.isEmpty()) {
		html += "<b>" + wc3_to_html(tip) + "</b>";
		if (!ubertip.isEmpty()) {
			html += "<hr style='border:0; border-top:1px solid #444; margin:4px 0;'/>";
		}
	}
	if (!ubertip.isEmpty()) {
		html += wc3_to_html(ubertip);
	}
	tab.preview->setHtml(html);
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

	// ── Formatting toolbar ─────────────────────────────────────────────────
	auto* toolbar = new QToolBar("Tooltip Formatting", this);
	toolbar->setMovable(false);
	addToolBar(toolbar);

	for (const auto& p : COLOR_PRESETS) {
		auto* btn = new QPushButton(QString::fromUtf8(p.label));
		btn->setFixedWidth(72);
		btn->setToolTip(QString("Insert %1 color code (%2)").arg(p.label, p.code));
		btn->setStyleSheet(
			QString("QPushButton{background:%1;color:%2;border-radius:3px;padding:2px 4px;font-size:11px;}"
			        "QPushButton:hover{border:1px solid rgba(255,255,255,140);}")
				.arg(p.bg, p.fg));
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

	// ── Central area: tabs ─────────────────────────────────────────────────
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
		show();
		return;
	}

	// Resolve SLK column names from modification codes via meta SLKs
	unit_tab.tip_field_name = meta_field_name(units_meta_slk, "unam");
	unit_tab.utip_field_name = meta_field_name(units_meta_slk, "ides");
	item_tab.tip_field_name = meta_field_name(items_meta_slk, "utip");
	item_tab.utip_field_name = meta_field_name(items_meta_slk, "utub");
	ability_tab.tip_field_name = meta_field_name(abilities_meta_slk, "atp1");
	ability_tab.utip_field_name = meta_field_name(abilities_meta_slk, "aub1");

	auto* unit_list = new UnitListModel(this);
	unit_list->setSourceModel(units_table);

	auto* item_list = new ItemListModel(this);
	item_list->setSourceModel(items_table);

	auto* ability_list = new AbilityListModel(this);
	ability_list->setSourceModel(abilities_table);

	tab_widget->addTab(
		build_tab(&unit_tab, units_slk, units_table, unit_list, false,
		          "Text - Name:", "Text - Description:"),
		"Units");
	tab_widget->addTab(
		build_tab(&item_tab, items_slk, items_table, item_list, false,
		          "Text - Tooltip - Basic:", "Text - Tooltip - Extended:"),
		"Items");
	tab_widget->addTab(
		build_tab(&ability_tab, abilities_slk, abilities_table, ability_list, true,
		          "Tooltip Normal:", "Tooltip Extended:"),
		"Abilities");

	show();
}

#include "tooltip_editor.moc"
