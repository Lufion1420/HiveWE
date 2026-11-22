#include "palette.h"

#include <QBoxLayout>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QLineEdit>
#include <QTabBar>
#include <QTabWidget>
#include <QGroupBox>
#include <QLabel>
#include <QLayoutItem>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

import FlowLayout;

Palette::Palette(QWidget* parent) : QDialog(parent) {
	setWindowFlag(Qt::Widget, true);
	setWindowFlag(Qt::Dialog, false);
}

Palette::~Palette()
{
}

void Palette::monitor_activation(QWidget* widget) {
	QWidget* target = widget ? widget : this;
	target->installEventFilter(this);

	const auto children = target->findChildren<QWidget*>();
	for (auto* child : children) {
		child->installEventFilter(this);
	}
}

bool Palette::eventFilter(QObject* watched, QEvent* event) {
	if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::FocusIn) {
		emit activated();
	}

	return QDialog::eventFilter(watched, event);
}

//void Palette::addShortcut(const QKeySequence sequence, const std::vector<QWidget*>& attach_to) {
//	for (auto&& i : attach_to) {
//		shortcuts.push_back(new QShortcut(sequence, i));
//	}
//}

void Palette::wrap_button_row(QBoxLayout* row) {
	auto* parent = row ? qobject_cast<QBoxLayout*>(row->parent()) : nullptr;
	if (!parent) {
		return;
	}

	const int index = parent->indexOf(row);
	auto* flow = new FlowLayout(0, 6, 6);
	flow->set_shrinkable(true);
	while (row->count() > 0) {
		QLayoutItem* item = row->takeAt(0);
		if (item->widget()) {
			flow->addWidget(item->widget());
		}
		delete item;
	}
	parent->removeItem(row);
	parent->insertLayout(index, flow);
	delete row;
}

void Palette::fit_to_column() {
	for (auto* layout : findChildren<QLayout*>()) {
		if (auto* flow = dynamic_cast<FlowLayout*>(layout)) {
			flow->set_shrinkable(true);
		} else if (auto* form = qobject_cast<QFormLayout*>(layout)) {
			form->setRowWrapPolicy(QFormLayout::WrapLongRows);
		}
	}

	// A combo box is as wide as its longest item by default.
	for (auto* combo : findChildren<QComboBox*>()) {
		combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		combo->setMinimumContentsLength(6);
		combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	}

	// Widgets stacked in the palette's own vertical layout always get the full column width, so they do not need to report a minimum width:
	// with a horizontal Ignored policy a wide child (tabs, group boxes, the Active Selection card) can no longer widen the whole palette.
	// Combo boxes and line edits keep their policy; they are already narrow.
	if (auto* box = qobject_cast<QBoxLayout*>(layout()); box && box->direction() == QBoxLayout::TopToBottom) {
		for (int i = 0; i < box->count(); ++i) {
			QWidget* item = box->itemAt(i)->widget();
			if (!item || qobject_cast<QComboBox*>(item) || qobject_cast<QLineEdit*>(item)) {
				continue;
			}
			QSizePolicy policy = item->sizePolicy();
			policy.setHorizontalPolicy(QSizePolicy::Ignored);
			item->setSizePolicy(policy);
		}
	}

	for (auto* label : findChildren<QLabel*>()) {
		if (label->wordWrap()) {
			label->setMinimumWidth(0);
		}
	}
	for (auto* tabs : findChildren<QTabWidget*>()) {
		tabs->tabBar()->setUsesScrollButtons(true);
		tabs->tabBar()->setElideMode(Qt::ElideRight);
	}
}

void Palette::attach_tool_page(QRibbonTab* tab) {
	auto* box = qobject_cast<QBoxLayout*>(layout());
	if (!box) {
		emit ribbon_tab_requested(tab, "");
		return;
	}

	if (!tool_page) {
		tool_page = new QWidget;
		tool_page->setObjectName("toolPage");
		auto* page_layout = new QVBoxLayout(tool_page);
		page_layout->setContentsMargins(0, 0, 4, 0);
		page_layout->setSpacing(8);

		for (int i = 0; i < tab->sections->count(); ++i) {
			auto* section = qobject_cast<QRibbonSection*>(tab->sections->itemAt(i)->widget());
			if (!section) {
				continue;
			}

			// Buttons only: let them wrap like text. A section with sub layouts (fields, rotation buttons) is stacked top to bottom.
			bool only_widgets = true;
			for (int j = 0; j < section->section_outer->count(); ++j) {
				only_widgets = only_widgets && section->section_outer->itemAt(j)->widget() != nullptr;
			}
			if (only_widgets) {
				auto* flow = new FlowLayout(0, 4, 4);
				flow->set_shrinkable(true);
				while (section->section_outer->count() > 0) {
					QLayoutItem* item = section->section_outer->takeAt(0);
					flow->addWidget(item->widget());
					delete item;
				}
				section->section_inner->insertLayout(0, flow);
			} else {
				section->section_outer->setDirection(QBoxLayout::TopToBottom);
				section->section_outer->setSpacing(4);
			}

			for (auto* button : section->findChildren<QToolButton*>()) {
				QString text = button->text();
				text.replace(QLatin1Char(10), QLatin1Char(' '));
				button->setText(text);
				button->setIcon(QIcon());
				button->setToolButtonStyle(Qt::ToolButtonTextOnly);
				button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
				button->setProperty("toolPageButton", true);
			}
			section->section_text->hide();

			auto* group = new QGroupBox(section->section_text->text(), tool_page);
			auto* group_layout = new QVBoxLayout(group);
			group_layout->setContentsMargins(8, 12, 8, 8);
			group_layout->addWidget(section);
			page_layout->addWidget(group);
		}
		page_layout->addStretch(1);

		// No scroll area of its own: the whole palette scrolls in the sidebar host (HiveWE::create_host).
		box->addWidget(tool_page);
		fit_to_column();
	}

	emit tool_page_activated(tab);
}
