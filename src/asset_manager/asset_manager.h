#pragma once

#include <filesystem>
#include <vector>

#include <QPointer>
#include <QDialog>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QTreeView>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QStringList>

// Shared item data roles / column indices for the Asset Manager model.
namespace asset {
	constexpr int IsUnusedRole = Qt::UserRole;       // bool, on file items
	constexpr int ObjectIdRole = Qt::UserRole + 1;   // QString, on child (reference) items
	constexpr int CategoryRole = Qt::UserRole + 2;   // int, on child items (-1 = not an object)
	constexpr int SortKeyRole = Qt::UserRole + 3;    // int, on usage-count items (sort order)
	constexpr int IsOverrideRole = Qt::UserRole + 4; // bool, on file items
	constexpr int SizeRole = Qt::UserRole + 5;       // qulonglong bytes, on file items

	constexpr int FileColumn = 0;
	constexpr int TypeColumn = 1;
	constexpr int UsageColumn = 2;
} // namespace asset

class AssetFilterModel : public QSortFilterProxyModel {
	Q_OBJECT
  public:
	using QSortFilterProxyModel::QSortFilterProxyModel;

	void set_only_unused(bool value);

  protected:
	bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
	bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

  private:
	bool only_unused = false;
};

class AssetManager : public QDialog {
	Q_OBJECT
  public:
	explicit AssetManager(QWidget* parent = nullptr);

	/// For --qa-open-dialog asset-manager: one "state, file, used by" line (tab separated) per file, sorted, then the totals.
	QStringList qa_report() const;

  private:
	void refresh();
	/// True while the list belongs to the open map and its folder. Another map or Save As makes its relative paths point elsewhere.
	[[nodiscard]]
	bool list_is_current() const;
	/// Before a delete: if the list is not current, scans again, says so and returns true.
	bool rescan_if_out_of_date();
	void update_status() const;
	void update_selection() const;
	void select_all_unused();
	void delete_selected();
	void show_context_menu(const QPoint& pos);
	void open_in_editor(const QModelIndex& proxy_index) const;
	void remove_object_references(const std::string& id);

	// Moves the given map-relative files to the OS recycle bin and removes their rows. Returns the
	// list of files that could not be moved (for error reporting).
	QStringList trash_files(const std::vector<class QStandardItem*>& file_items);

	QLineEdit* search_edit;
	QComboBox* filter_combo;
	QTreeView* tree_view;
	QLabel* stats_label;
	QLabel* selection_label;
	QPushButton* select_unused_button;
	QPushButton* delete_button;
	QStandardItemModel* model;
	AssetFilterModel* filter_model;

	/// The map and folder the list was scanned from.
	QPointer<QObject> scanned_map;
	std::filesystem::path scanned_folder;
};
