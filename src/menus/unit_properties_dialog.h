#pragma once

#include <QDialog>

import Units;

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;
class QTabWidget;

class UnitPropertiesDialog : public QDialog {
	Q_OBJECT

  public:
	UnitPropertiesDialog(Unit* unit, QWidget* parent = nullptr);

  private:
	void refresh_attribute_state() const;
	void apply_changes();

	Unit* unit = nullptr;

	QLabel* title = nullptr;
	QLabel* meta = nullptr;
	QLabel* icon = nullptr;

	QTabWidget* tabs = nullptr;
	QComboBox* player = nullptr;
	QDoubleSpinBox* facing = nullptr;
	QSpinBox* hit_points_percent = nullptr;
	QLabel* max_hit_points = nullptr;
	QSpinBox* mana_points = nullptr;
	QLabel* max_mana = nullptr;
	QSpinBox* level = nullptr;
	QCheckBox* default_attributes = nullptr;
	QSpinBox* strength = nullptr;
	QSpinBox* agility = nullptr;
	QSpinBox* intelligence = nullptr;
};
