#pragma once

#include <memory>
#include <vector>

#include <QDialog>
#include <QElapsedTimer>
#include <QString>
#include <QStringList>

class ModelEditorGLWidget;
class ModelListModel;
class ModelThumbnailCache;
class QComboBox;
class QLabel;
class QLineEdit;
class QListView;
class QListWidget;
class QPushButton;
class QTableWidget;
class QTimer;

/// The model browser of the Model Editor: categories with their counts, a search box, a grid of thumbnails and a live preview with an
/// animation choice and an info table. The thumbnails come from ModelThumbnailCache, drawn only for the rows that are (nearly) in view.
class BrowseModelsDialog : public QDialog {
	Q_OBJECT

  public:
	explicit BrowseModelsDialog(QWidget* parent = nullptr);
	~BrowseModelsDialog() override;

	/// The model path as Hierarchy knows it (for example units/human/footman/footman.mdx), empty when nothing is selected.
	QString selected_path() const;

	/// Driven by --qa-open-dialog browse-models (needs --qa-offscreen): waits for the thumbnails of the visible rows, checks the search box,
	/// selects the first model, writes the result lines to `out_file` and a PNG of the dialog next to it (same name, `.png`), then quits.
	void run_qa(const QString& out_file);

	/// Refuses (stays open) while nothing is selected.
	void accept() override;

	/// Stops the thumbnail work at once when the dialog closes (accepted, rejected or closed).
	void done(int result) override;

  private:
	void select_category(int row);
	void apply_filter();
	void request_visible();
	void show_preview(const QString& path);
	void update_status();
	void qa_step();

	ModelThumbnailCache* cache;
	ModelListModel* list_model;

	QListWidget* categories;
	QLineEdit* search;
	QListView* view;
	QLabel* status;
	ModelEditorGLWidget* preview;
	QLabel* preview_title;
	QComboBox* animation;
	QTableWidget* info;
	QPushButton* open_button;
	QTimer* visible_timer;

	QString preview_path;
	std::size_t total_models = 0;
	std::vector<std::size_t> category_counts;

	// --qa-open-dialog browse-models
	QString qa_out;
	QStringList qa_log;
	QTimer* qa_timer = nullptr;
	QElapsedTimer qa_clock;
	QElapsedTimer qa_phase_clock;
	int qa_phase = 0;
	int qa_rows_before_search = 0;
};
