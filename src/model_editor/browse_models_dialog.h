#pragma once

#include <memory>
#include <vector>

#include <QDialog>
#include <QElapsedTimer>
#include <QImage>
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

	/// Picker mode (Object Editor model fields): adds an editable path line seeded with `current`, overwritten by every grid pick, and the
	/// current value is preselected in the grid when it is a catalog model. Open is always enabled and accepts the typed text, empty included.
	void set_picker_mode(const QString& current);

	/// Picker mode result: the path line, normalized (see normalize_typed_model_path). Empty clears the field.
	QString picked_text() const;

	/// Driven by --qa-open-dialog browse-models (needs --qa-offscreen): waits for the thumbnails of the visible rows, checks the search box,
	/// selects the first model, writes the result lines to `out_file` and a PNG of the dialog next to it (same name, `.png`), then quits.
	void run_qa(const QString& out_file);

	/// Refuses (stays open) while nothing is selected.
	void accept() override;

	/// Stops the thumbnail work at once when the dialog closes (accepted, rejected or closed).
	void done(int result) override;

  protected:
	void showEvent(QShowEvent* event) override;
	void hideEvent(QHideEvent* event) override;
	void changeEvent(QEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	bool eventFilter(QObject* watched, QEvent* event) override;

  private:
	void select_category(int row);
	void apply_filter();
	void request_visible();
	/// The rows whose tile is at least partly inside the viewport, in order.
	std::vector<int> visible_rows() const;
	/// Animated tiles (see TileAnimation): the visible rows, hovered and selected first, up to max_animated_tiles; paused while the
	/// dialog is hidden, minimized or not the active window.
	void update_animation();
	void show_preview(const QString& path);
	void update_status();
	void qa_step();
	void qa_reopen_step();
	void qa_finish();
	/// Working set (or private bytes) of this process in MB; 0 where not available.
	static double qa_working_set_mb(bool private_bytes);

	// Null until the constructor sets them: the window events below can arrive while it runs (winId()).
	ModelThumbnailCache* cache = nullptr;
	ModelListModel* list_model = nullptr;

	QListWidget* categories;
	QLineEdit* search;
	QListView* view = nullptr;
	QLabel* status;
	ModelEditorGLWidget* preview;
	QLabel* preview_title;
	QComboBox* animation;
	QTableWidget* info;
	QPushButton* open_button;
	QLineEdit* path_edit = nullptr;
	bool picker = false;
	QTimer* visible_timer = nullptr;
	int hovered_row = -1;
	bool scrolling = false; // between a scroll bar move and the request_visible that settles it

	QString preview_path;
	std::size_t total_models = 0;
	std::size_t model_count = 0;
	std::size_t effect_count = 0;
	std::vector<std::size_t> category_counts;

	// --qa-open-dialog browse-models
	QString qa_out;
	QStringList qa_log;
	QTimer* qa_timer = nullptr;
	QElapsedTimer qa_clock;
	QElapsedTimer qa_phase_clock;
	int qa_phase = 0;
	int qa_rows_before_search = 0;
	QStringList qa_animated_paths;
	std::vector<QImage> qa_animated_before;
	int qa_hold_seconds = 0;
	qint64 qa_hold_samples = 0;
	int qa_cycle = 0;
	bool qa_cycle_closing = false;
	BrowseModelsDialog* qa_child = nullptr; // deleted by the reopen cycle itself
	double qa_ws_start = 0.0;
	std::size_t qa_resources_start = 0;
	std::size_t qa_resources_after_first = 0;
};
