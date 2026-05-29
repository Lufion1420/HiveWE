#pragma once

#include <QString>
#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;

class NewMapDialog : public QDialog {
  public:
	struct Result {
		QString map_name;
		char tileset = 'L';
		int width = 64;
		int height = 64;
	};

	explicit NewMapDialog(QWidget* parent = nullptr);

	[[nodiscard]]
	Result result() const;

  private:
	void refresh_summary() const;

	static QString size_descriptor(int width, int height);

	QLineEdit* map_name = nullptr;
	QComboBox* tileset = nullptr;
	QComboBox* width = nullptr;
	QComboBox* height = nullptr;
	QLabel* size_description = nullptr;
	QLabel* playable_summary = nullptr;
};
