#include "browse_models_dialog.h"

#include "model_thumbnail_cache.h"
#include "model_editor_glwidget.h" // defines QT_NO_OPENGL, as in model_editor.cpp

#include <QAbstractListModel>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QLocale>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

#include "hivewe.h"
#include "model_inspector.h"

import std;
import BinaryReader;
import Hierarchy;
import MDX;
import ModelCatalog;
import Utilities;

namespace fs = std::filesystem;

namespace {
	constexpr int category_all = -1;

	QString to_qstring(const fs::path& path) {
		return QString::fromStdWString(path.generic_wstring());
	}

	/// Every model the browser can offer: the open map's folder plus the game folders, through Hierarchy (same sources as the Object Editor's
	/// model view). Sorted by category, then by path, so a category reads alphabetically.
	ModelCatalog enumerate_catalog() {
		std::vector<std::string> map_files;
		if (!hierarchy.map_directory.empty()) {
			std::error_code ec;
			if (fs::exists(hierarchy.map_directory, ec)) {
				for (auto it = fs::recursive_directory_iterator(hierarchy.map_directory, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
					if (it->is_regular_file(ec) && !is_map_write_temp_file(it->path())) {
						map_files.push_back(utf8_from_path(it->path().lexically_relative(hierarchy.map_directory)));
					}
				}
			}
		}

		std::vector<std::string> game_files;
		for (const auto& source : game_category_sources) {
			auto names = hierarchy.game_data.find_files(std::string(source.glob));
			game_files.insert(game_files.end(), std::make_move_iterator(names.begin()), std::make_move_iterator(names.end()));
		}

		ModelCatalog catalog = build_model_catalog(map_files, game_files);

		std::vector<std::pair<std::string, ModelEntry>> keyed;
		keyed.reserve(catalog.entries.size());
		for (auto& entry : catalog.entries) {
			keyed.emplace_back(model_catalog_key(utf8_from_path(entry.path)), std::move(entry));
		}
		std::ranges::sort(keyed, [](const auto& a, const auto& b) {
			return std::tie(a.second.category, a.first) < std::tie(b.second.category, b.first);
		});
		catalog.entries.clear();
		for (auto& [key, entry] : keyed) {
			catalog.entries.push_back(std::move(entry));
		}
		return catalog;
	}

	/// Name on the left, count on the right.
	class CategoryDelegate : public QStyledItemDelegate {
	  public:
		using QStyledItemDelegate::QStyledItemDelegate;

		void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
			QStyleOptionViewItem opt = option;
			initStyleOption(&opt, index);
			const QString text = opt.text;
			opt.text.clear();
			QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
			style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

			const bool selected = opt.state & QStyle::State_Selected;
			const QRect area = opt.rect.adjusted(8, 0, -8, 0);
			painter->save();
			painter->setPen(selected ? opt.palette.color(QPalette::HighlightedText) : opt.palette.color(QPalette::Text));
			painter->drawText(area, Qt::AlignVCenter | Qt::AlignLeft, text);
			painter->setPen(selected ? opt.palette.color(QPalette::HighlightedText) : opt.palette.color(QPalette::PlaceholderText));
			painter->drawText(area, Qt::AlignVCenter | Qt::AlignRight, index.data(Qt::UserRole + 1).toString());
			painter->restore();
		}
	};
} // namespace

/// The rows of the thumbnail grid: the entries that pass the current category and search. A thumbnail that is not ready shows a plain tile.
class ModelListModel : public QAbstractListModel {
  public:
	ModelListModel(std::vector<ModelEntry> entries, ModelThumbnailCache& cache, QObject* parent) :
		QAbstractListModel(parent),
		all(std::move(entries)),
		cache(cache) {
		paths.reserve(all.size());
		for (const auto& entry : all) {
			paths.push_back(to_qstring(entry.path));
		}
	}

	const std::vector<ModelEntry>& entries() const {
		return all;
	}

	void show(std::vector<std::size_t> indices) {
		beginResetModel();
		visible = std::move(indices);
		row_of.clear();
		for (std::size_t row = 0; row < visible.size(); ++row) {
			row_of.emplace(model_catalog_key(paths[visible[row]].toStdString()), static_cast<int>(row));
		}
		endResetModel();
	}

	/// The row showing `path`, or -1 when it is not in the current list.
	int row_for(const QString& path) const {
		const auto it = row_of.find(model_catalog_key(path.toStdString()));
		return it == row_of.end() ? -1 : it->second;
	}

	QString path_at(const int row) const {
		return row >= 0 && row < static_cast<int>(visible.size()) ? paths[visible[row]] : QString();
	}

	void thumbnail_ready(const QString& path) {
		if (const auto it = row_of.find(model_catalog_key(path.toStdString())); it != row_of.end()) {
			const QModelIndex changed = index(it->second);
			emit dataChanged(changed, changed, { Qt::DecorationRole });
		}
	}

	int rowCount(const QModelIndex& parent = {}) const override {
		return parent.isValid() ? 0 : static_cast<int>(visible.size());
	}

	QVariant data(const QModelIndex& index, const int role) const override {
		if (!index.isValid() || index.row() >= static_cast<int>(visible.size())) {
			return {};
		}
		const QString& path = paths[visible[index.row()]];
		switch (role) {
			case Qt::DisplayRole:
				return QString::fromStdWString(all[visible[index.row()]].path.filename().wstring());
			case Qt::DecorationRole: {
				const QPixmap pixmap = cache.pixmap(path);
				return pixmap.isNull() ? ModelThumbnailCache::placeholder(cache.failed(path)) : pixmap;
			}
			case Qt::ToolTipRole:
				return path;
			case Qt::UserRole:
				return path;
			default:
				return {};
		}
	}

  private:
	std::vector<ModelEntry> all;
	std::vector<QString> paths;
	std::vector<std::size_t> visible;
	std::unordered_map<std::string, int> row_of;
	ModelThumbnailCache& cache;
};

BrowseModelsDialog::BrowseModelsDialog(QWidget* parent) : QDialog(parent, Qt::WindowTitleHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint) {
	setWindowTitle("Browse Models");
	setObjectName("browseModelsDialog");
	resize(1200, 720);

	// Force native window, otherwise the preview's OpenGL widget switches the dialog to an OpenGL-capable surface, which unmaps the
	// window and shows a white flash.
	setAttribute(Qt::WA_NativeWindow);
	(void)winId();

	// The offscreen QA run keeps the real cache folder untouched and draws every thumbnail itself.
	cache = new ModelThumbnailCache(!HiveWE::qa_offscreen, this);

	ModelCatalog catalog = enumerate_catalog();
	total_models = catalog.total();
	category_counts.assign(catalog.counts.begin(), catalog.counts.end());
	list_model = new ModelListModel(std::move(catalog.entries), *cache, this);

	const QLocale locale;

	// Left column: categories.
	auto* categories_header = new QLabel("CATEGORIES");
	categories_header->setObjectName("browseSectionHeader");
	categories = new QListWidget;
	categories->setObjectName("browseCategories");
	categories->setItemDelegate(new CategoryDelegate(categories));
	categories->setFixedWidth(220);
	{
		auto* all_item = new QListWidgetItem("All models", categories);
		all_item->setData(Qt::UserRole, category_all);
		all_item->setData(Qt::UserRole + 1, locale.toString(static_cast<qulonglong>(total_models)));
		for (int i = 0; i < static_cast<int>(ModelCategory::Count); ++i) {
			const auto category = static_cast<ModelCategory>(i);
			auto* item = new QListWidgetItem(QString::fromUtf8(model_category_label(category).data()), categories);
			item->setData(Qt::UserRole, i);
			item->setData(Qt::UserRole + 1, locale.toString(static_cast<qulonglong>(category_counts[i])));
		}
	}

	auto* left = new QVBoxLayout;
	left->setSpacing(6);
	left->addWidget(categories_header);
	left->addWidget(categories, 1);

	// Middle column: search and thumbnails.
	search = new QLineEdit;
	search->setObjectName("browseSearch");
	search->setPlaceholderText("Search Models");
	search->setClearButtonEnabled(true);

	view = new QListView;
	view->setObjectName("browseList");
	view->setModel(list_model);
	view->setViewMode(QListView::IconMode);
	view->setIconSize(QSize(ModelThumbnailCache::thumbnail_size, ModelThumbnailCache::thumbnail_size));
	view->setGridSize(QSize(ModelThumbnailCache::thumbnail_size + 16, ModelThumbnailCache::thumbnail_size + 32));
	view->setResizeMode(QListView::Adjust);
	view->setMovement(QListView::Static);
	view->setUniformItemSizes(true);
	view->setWordWrap(false);
	view->setTextElideMode(Qt::ElideMiddle);
	view->setSelectionMode(QAbstractItemView::SingleSelection);
	view->setEditTriggers(QAbstractItemView::NoEditTriggers);
	view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	view->verticalScrollBar()->setSingleStep(40);

	status = new QLabel;
	status->setObjectName("browseStatus");

	auto* middle = new QVBoxLayout;
	middle->setSpacing(6);
	middle->addWidget(search);
	middle->addWidget(view, 1);
	middle->addWidget(status);

	// Right column: preview.
	auto* preview_header = new QLabel("PREVIEW");
	preview_header->setObjectName("browseSectionHeader");
	preview_title = new QLabel;
	preview_title->setObjectName("browsePreviewName");
	preview = new ModelEditorGLWidget(this, nullptr, true);
	preview->setObjectName("browsePreview");
	preview->setMinimumSize(300, 230);
	preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	animation = new QComboBox;
	animation->setObjectName("browseAnimation");
	animation->setEnabled(false);
	auto* animation_row = new QHBoxLayout;
	animation_row->addWidget(new QLabel("Animation"));
	animation_row->addWidget(animation, 1);

	info = new QTableWidget(5, 2);
	info->setObjectName("browseInfo");
	info->horizontalHeader()->hide();
	info->verticalHeader()->hide();
	info->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	info->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	info->setEditTriggers(QAbstractItemView::NoEditTriggers);
	info->setSelectionMode(QAbstractItemView::NoSelection);
	info->setFocusPolicy(Qt::NoFocus);
	info->setShowGrid(false);
	info->setAlternatingRowColors(true);
	info->setTextElideMode(Qt::ElideLeft);
	info->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	info->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	const QStringList info_names { "Path", "Vertices", "Triangles", "Sequences", "File size" };
	for (int row = 0; row < info_names.size(); ++row) {
		info->setItem(row, 0, new QTableWidgetItem(info_names[row]));
		info->setItem(row, 1, new QTableWidgetItem);
	}
	info->setFixedHeight(5 * info->verticalHeader()->defaultSectionSize() + 4);

	auto* right = new QVBoxLayout;
	right->setSpacing(6);
	right->addWidget(preview_header);
	right->addWidget(preview_title);
	right->addWidget(preview, 1);
	right->addLayout(animation_row);
	right->addWidget(info);

	auto* columns = new QHBoxLayout;
	columns->setSpacing(14);
	columns->addLayout(left);
	columns->addLayout(middle, 1);
	auto* right_widget = new QWidget;
	right_widget->setLayout(right);
	right_widget->setFixedWidth(330);
	right->setContentsMargins(0, 0, 0, 0);
	columns->addWidget(right_widget);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel);
	open_button = buttons->button(QDialogButtonBox::Open);
	open_button->setEnabled(false);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto* layout = new QVBoxLayout(this);
	layout->addLayout(columns, 1);
	layout->addWidget(buttons);

	visible_timer = new QTimer(this);
	visible_timer->setSingleShot(true);
	connect(visible_timer, &QTimer::timeout, this, &BrowseModelsDialog::request_visible);

	connect(cache, &ModelThumbnailCache::thumbnail_ready, this, [this](const QString& path) { list_model->thumbnail_ready(path); });
	connect(categories, &QListWidget::currentRowChanged, this, &BrowseModelsDialog::select_category);
	connect(search, &QLineEdit::textChanged, this, [this] { apply_filter(); });
	connect(view->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { visible_timer->start(60); });
	connect(view, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
		if (index.isValid()) {
			accept();
		}
	});
	connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& current) {
		open_button->setEnabled(current.isValid());
		if (current.isValid()) {
			show_preview(list_model->path_at(current.row()));
		}
	});
	connect(animation, &QComboBox::activated, this, [this](const int index) { preview->set_sequence(index); });

	// The stock units are what most people come for; "All models" when the game data is not available. The signal is blocked and the grid
	// filled by hand: currentRowChanged is not emitted when the row is already current, which would leave the grid empty.
	{
		const QSignalBlocker blocker(categories);
		categories->setCurrentRow(category_counts[static_cast<int>(ModelCategory::Units)] > 0 ? 1 + static_cast<int>(ModelCategory::Units) : 0);
	}
	apply_filter();
}

BrowseModelsDialog::~BrowseModelsDialog() {
	cache->cancel();
}

void BrowseModelsDialog::accept() {
	// Open can only be enabled with a selection, but a double click or Enter must not slip through an empty one either.
	if (selected_path().isEmpty()) {
		return;
	}
	QDialog::accept();
}

void BrowseModelsDialog::done(const int result) {
	cache->cancel();
	visible_timer->stop();
	QDialog::done(result);
}

QString BrowseModelsDialog::selected_path() const {
	return list_model->path_at(view->currentIndex().row());
}

void BrowseModelsDialog::select_category(const int row) {
	if (row >= 0) {
		apply_filter();
	}
}

void BrowseModelsDialog::apply_filter() {
	std::optional<ModelCategory> category;
	if (const auto* item = categories->currentItem(); item && item->data(Qt::UserRole).toInt() != category_all) {
		category = static_cast<ModelCategory>(item->data(Qt::UserRole).toInt());
	}
	// A model reset clears the current row without a currentChanged, so Open is switched off by hand and back on only for a real row.
	const QString previous = selected_path();
	open_button->setEnabled(false);
	list_model->show(filter_model_entries(list_model->entries(), category, search->text().toStdString()));
	if (const int row = list_model->row_for(previous); row >= 0) {
		view->setCurrentIndex(list_model->index(row, 0)); // still listed: keep it selected (currentChanged enables Open)
	}
	open_button->setEnabled(view->currentIndex().isValid());
	view->scrollToTop();
	update_status();
	visible_timer->start(0);
}

void BrowseModelsDialog::update_status() {
	const QLocale locale;
	const auto* item = categories->currentItem();
	const int category = item ? item->data(Qt::UserRole).toInt() : category_all;
	const std::size_t in_category = category == category_all ? total_models : category_counts[category];
	status->setText(QString("%1 models in %2, %3 shown")
						.arg(locale.toString(static_cast<qulonglong>(in_category)))
						.arg(item ? item->text() : QString("All models"))
						.arg(locale.toString(list_model->rowCount())));
}

/// Asks the cache for the rows in view plus a margin below them, nearest first. Anything queued earlier is dropped.
void BrowseModelsDialog::request_visible() {
	const int count = list_model->rowCount();
	if (count == 0) {
		cache->request({});
		return;
	}
	const QRect port = view->viewport()->rect();
	int first = view->indexAt(QPoint(6, 6)).row();
	int last = view->indexAt(QPoint(port.width() - 6, port.height() - 6)).row();
	if (first < 0) {
		first = 0;
	}
	if (last < first) {
		last = first + 40; // the view is not laid out yet, or its tail is empty space
	}
	last = std::min(count - 1, last + 30);

	QStringList paths;
	for (int row = first; row <= last && paths.size() < 300; ++row) {
		paths << list_model->path_at(row);
	}
	cache->request(paths);
}

void BrowseModelsDialog::show_preview(const QString& path) {
	if (path.isEmpty() || path == preview_path) {
		return;
	}
	preview_path = path;

	std::shared_ptr<mdx::MDX> mdx;
	std::string error;
	std::size_t file_size = 0;
	try {
		auto reader = hierarchy.open_file(fs::path(path.toStdWString()));
		if (!reader) {
			error = reader.error();
		} else {
			file_size = reader->buffer.size();
			mdx = std::make_shared<mdx::MDX>(reader.value());
		}
	} catch (const std::exception& e) {
		error = e.what();
		mdx.reset();
	}

	preview->set_model(mdx);
	preview_title->setText(QString::fromStdWString(fs::path(path.toStdWString()).stem().wstring()));

	const QSignalBlocker blocker(animation);
	animation->clear();
	animation->setEnabled(mdx && !mdx->sequences.empty());
	int chosen = 0;
	if (mdx) {
		// A plain "Stand" first, then any sequence with "stand" in its name ("Stand Alternate" can be a curled-up pose), else the first.
		int contains_stand = -1;
		int exact_stand = -1;
		for (int i = 0; i < static_cast<int>(mdx->sequences.size()); ++i) {
			animation->addItem(QString::fromStdString(mdx->sequences[i].name));
			const std::string lowered = to_lowercase_copy(mdx->sequences[i].name);
			if (exact_stand < 0 && (lowered == "stand" || lowered == "stand - 1")) {
				exact_stand = i;
			}
			if (contains_stand < 0 && lowered.contains("stand")) {
				contains_stand = i;
			}
		}
		chosen = exact_stand >= 0 ? exact_stand : std::max(0, contains_stand);
		if (!mdx->sequences.empty()) {
			animation->setCurrentIndex(chosen);
			preview->set_sequence(chosen);
		}
	}

	const QLocale locale;
	const auto set_value = [this](const int row, const QString& text) {
		info->item(row, 1)->setText(text);
		info->item(row, 1)->setToolTip(text);
	};
	if (!mdx) {
		set_value(0, QString("Could not load: ") + QString::fromStdString(error));
		for (int row = 1; row < 5; ++row) {
			set_value(row, "");
		}
		return;
	}
	const DrawnCounts counts = drawn_counts(*mdx);
	set_value(0, QString(path).replace('/', '\\'));
	set_value(1, locale.toString(static_cast<qulonglong>(counts.vertices)));
	set_value(2, locale.toString(static_cast<qulonglong>(counts.triangles)));
	set_value(3, locale.toString(static_cast<qulonglong>(mdx->sequences.size())));
	set_value(4, locale.toString(static_cast<qulonglong>((file_size + 512) / 1024)) + " KB");
}

// --- --qa-open-dialog browse-models ------------------------------------------------------------------------------------------------------------

void BrowseModelsDialog::run_qa(const QString& out_file) {
	qa_out = out_file;
	qa_log.clear();

	// The picture is written next to the result file with the same base name and `.png`, so a result file that is itself a .png would be
	// overwritten by it.
	if (out_file.isEmpty() || out_file.endsWith(".png", Qt::CaseInsensitive)) {
		std::println(stderr, "--qa-open-dialog browse-models needs --qa-script-out <file> that does not end in .png.");
		QApplication::exit(2);
		return;
	}

	qa_log << QString("INFO [catalog] total=%1").arg(total_models);
	for (int i = 0; i < static_cast<int>(ModelCategory::Count); ++i) {
		qa_log << QString("INFO [category] %1=%2").arg(QString::fromUtf8(model_category_label(static_cast<ModelCategory>(i)).data())).arg(category_counts[i]);
	}
	const bool plausible = category_counts[static_cast<int>(ModelCategory::Units)] > 0 && category_counts[static_cast<int>(ModelCategory::Doodads)] > 0
		&& category_counts[static_cast<int>(ModelCategory::Buildings)] > 0;
	qa_log << QString("%1 [categories] units, doodads and buildings are not empty").arg(plausible ? "PASS" : "FAIL");

	qa_clock.start();
	qa_phase_clock.start();
	qa_phase = 0;
	request_visible();
	qa_timer = new QTimer(this);
	connect(qa_timer, &QTimer::timeout, this, &BrowseModelsDialog::qa_step);
	qa_timer->start(100);
}

void BrowseModelsDialog::qa_step() {
	// An offscreen window is never exposed, so the preview's own repaint loop is stopped: paint it by hand.
	(void)preview->grabFramebuffer();

	const bool timed_out = qa_clock.elapsed() > 150'000;
	const auto settled = [&] { return (qa_phase_clock.elapsed() >= 2000 && cache->pending() == 0) || timed_out; };
	const auto next = [&](const int phase) {
		qa_phase = phase;
		qa_phase_clock.restart();
	};

	if (qa_phase == 0) {
		if (!settled()) {
			return;
		}
		qa_log << QString("%1 [thumbnails] first page drawn (pending=%2)").arg(cache->pending() == 0 ? "PASS" : "FAIL").arg(cache->pending());
		qa_rows_before_search = list_model->rowCount();
		view->setCurrentIndex(list_model->index(0, 0));
		qa_log << QString("%1 [open] enabled with a model selected (rows=%2)").arg(open_button->isEnabled() ? "PASS" : "FAIL").arg(qa_rows_before_search);
		search->setText("footman");
		next(1);
	} else if (qa_phase == 1) {
		if (qa_phase_clock.elapsed() < 400) {
			return;
		}
		const int rows = list_model->rowCount();
		qa_log << QString("%1 [search] 'footman' shows %2 of %3 rows").arg(rows > 0 && rows < qa_rows_before_search ? "PASS" : "FAIL").arg(rows).arg(qa_rows_before_search);
		// The model that was selected is not in the result, so Open must be off and nothing may be returned.
		qa_log << QString("%1 [open] disabled after the selected model was filtered out (enabled=%2, path='%3')")
					  .arg(!open_button->isEnabled() && selected_path().isEmpty() ? "PASS" : "FAIL")
					  .arg(open_button->isEnabled())
					  .arg(selected_path());
		search->setText("zzznomatchzzz");
		next(2);
	} else if (qa_phase == 2) {
		if (qa_phase_clock.elapsed() < 400) {
			return;
		}
		qa_log << QString("%1 [open] disabled after a search with no match (rows=%2, enabled=%3)")
					  .arg(list_model->rowCount() == 0 && !open_button->isEnabled() && selected_path().isEmpty() ? "PASS" : "FAIL")
					  .arg(list_model->rowCount())
					  .arg(open_button->isEnabled());
		search->clear();
		categories->setCurrentRow(1 + static_cast<int>(ModelCategory::Map)); // an empty category
		next(3);
	} else if (qa_phase == 3) {
		if (qa_phase_clock.elapsed() < 400) {
			return;
		}
		qa_log << QString("%1 [category] empty category 'Map' shows %2 rows, Open enabled=%3")
					  .arg(list_model->rowCount() == category_counts[static_cast<int>(ModelCategory::Map)] && !open_button->isEnabled() ? "PASS" : "FAIL")
					  .arg(list_model->rowCount())
					  .arg(open_button->isEnabled());
		categories->setCurrentRow(1 + static_cast<int>(ModelCategory::Units));
		next(4);
	} else if (qa_phase == 4) {
		if (!settled()) {
			return;
		}
		qa_log << QString("%1 [category] Units shows %2 of %3 rows again").arg(list_model->rowCount() == static_cast<int>(category_counts[static_cast<int>(ModelCategory::Units)]) ? "PASS" : "FAIL")
					  .arg(list_model->rowCount())
					  .arg(category_counts[static_cast<int>(ModelCategory::Units)]);
		view->setCurrentIndex(list_model->index(0, 0));
		next(5);
	} else if (qa_phase == 5) {
		if (qa_phase_clock.elapsed() < 2500) {
			return;
		}
		qa_timer->stop();

		const bool has_model = preview->mdx != nullptr;
		qa_log << QString("%1 [preview] model loaded, %2 animations, open button enabled=%3")
					  .arg(has_model && animation->count() > 0 && open_button->isEnabled() ? "PASS" : "FAIL")
					  .arg(animation->count())
					  .arg(open_button->isEnabled());
		qa_log << QString("INFO [selected] %1").arg(selected_path());

		const QImage gl_image = preview->grabFramebuffer();
		long long drawn = 0;
		if (!gl_image.isNull()) {
			const QRgb background = gl_image.pixel(0, 0);
			for (int y = 0; y < gl_image.height(); ++y) {
				for (int x = 0; x < gl_image.width(); ++x) {
					const QRgb pixel = gl_image.pixel(x, y);
					drawn += (std::abs(qRed(pixel) - qRed(background)) + std::abs(qGreen(pixel) - qGreen(background)) + std::abs(qBlue(pixel) - qBlue(background))) > 24;
				}
			}
		}
		qa_log << QString("%1 [preview] %2 viewport pixels differ from the background").arg(drawn > 0 ? "PASS" : "FAIL").arg(drawn);

		// The unexposed GL widget does not appear in grab(), so paint its frame over the dialog picture.
		QPixmap shot = grab();
		{
			QPainter painter(&shot);
			painter.drawImage(QRect(preview->mapTo(this, QPoint(0, 0)), preview->size()), gl_image);
		}
		const QFileInfo out(qa_out);
		const QString png = out.absoluteDir().filePath(out.completeBaseName() + ".png");
		qa_log << QString("%1 [capture] %2").arg(shot.save(png) ? "PASS" : "FAIL").arg(QFileInfo(png).fileName());

		qa_log << QString("INFO [thumbnails] generated=%1 from_disk=%2 failed=%3 resources_evicted=%4 timed_out=%5")
					  .arg(cache->generated())
					  .arg(cache->loaded_from_disk())
					  .arg(cache->failures())
					  .arg(cache->resources_evicted())
					  .arg(timed_out);
		qa_log << QString("%1 [thumbnails] textures loaded for the thumbnails were released again (evicted=%2)")
					  .arg(cache->generated() == 0 || cache->resources_evicted() > 0 ? "PASS" : "FAIL")
					  .arg(cache->resources_evicted());
		for (const std::string& why : cache->failure_log()) {
			qa_log << QString("INFO [failure] %1").arg(QString::fromStdString(why));
		}
		qa_log << "DONE";

		QFile file(qa_out);
		if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
			QTextStream(&file) << qa_log.join('\n') << '\n';
		} else {
			std::println(stderr, "--qa-script-out: cannot write {}", qa_out.toStdString());
		}
		QApplication::quit();
	}
}
