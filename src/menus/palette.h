#pragma once

#include <QBoxLayout>
#include <QDialog>
#include <QShortcut>

#include <vector>

import QRibbon;

/// Palette is the base for all other palette kinds and facilitates things like brush switching and shortcut management
class Palette : public QDialog {
	Q_OBJECT

public:
	Palette(QWidget* parent = nullptr);
	~Palette();

	std::vector<QShortcut*> shortcuts;
	void monitor_activation(QWidget* widget = nullptr);

	/// Flat themes: puts the controls of the ribbon page `tab` into the palette itself, as titled groups in a scroll area at the end of the
	/// palette layout. Built once; every call announces the page through tool_page_activated. Without a box layout the page goes to the ribbon.
	void attach_tool_page(QRibbonTab* tab);

	/// Flat themes: lets the content shrink to the narrow sidebar column instead of pushing the palette wider. Flow layouts wrap at any width,
	/// form rows wrap, combo boxes elide instead of taking the width of their longest item. Call once the palette is built.
	void fit_to_column();

	//void addShortcut(const QKeySequence sequence, const std::vector<QWidget*>& attach_to);

signals:
	void ribbon_tab_requested(QRibbonTab* tab, QString name);
	void activated();
	/// A palette with its tool page inside (Flat themes) became the active one.
	void tool_page_activated(QRibbonTab* tab);

public slots:
	virtual void deactivate(QRibbonTab* tab) = 0;

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;

	/// Replaces a row of buttons (a QHBoxLayout inside a QBoxLayout) by a wrapping flow layout. Spacers are dropped.
	static void wrap_button_row(QBoxLayout* row);

private:
	QWidget* tool_page = nullptr;
};
