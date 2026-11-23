#pragma once

#include <memory>
#include <utility>
#include <vector>

#include <QAbstractTableModel>
#include <QDockWidget>
#include <QMainWindow>
#include <QString>
#include <QTableView>

import MDX;

/// Read-only table models over one mdx::MDX, for the Model Editor's Sequences, Geosets, Materials and Info panels. They copy nothing
/// from the model except the shared pointer, and never write to it.

/// Vertices and triangles of the geosets that are drawn (LOD 0). The Info panel and the status bar both show these.
struct DrawnCounts {
	std::size_t vertices = 0;
	std::size_t triangles = 0;
};
DrawnCounts drawn_counts(const mdx::MDX& mdx);

class SequenceTableModel : public QAbstractTableModel {
	Q_OBJECT

  public:
	using QAbstractTableModel::QAbstractTableModel;

	void set_model(std::shared_ptr<mdx::MDX> model);

	int rowCount(const QModelIndex& parent = {}) const override;
	int columnCount(const QModelIndex& parent = {}) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

  private:
	std::shared_ptr<mdx::MDX> mdx;
};

class GeosetTableModel : public QAbstractTableModel {
	Q_OBJECT

  public:
	using QAbstractTableModel::QAbstractTableModel;

	/// `hidden` is indexed by geoset; missing entries mean visible.
	void set_model(std::shared_ptr<mdx::MDX> model, std::vector<bool> hidden);

	int rowCount(const QModelIndex& parent = {}) const override;
	int columnCount(const QModelIndex& parent = {}) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;

  signals:
	/// The user ticked or cleared the Show box of a geoset.
	void visibility_changed(int geoset, bool visible);

  private:
	std::shared_ptr<mdx::MDX> mdx;
	std::vector<bool> hidden;
};

class MaterialTableModel : public QAbstractTableModel {
	Q_OBJECT

  public:
	using QAbstractTableModel::QAbstractTableModel;

	void set_model(std::shared_ptr<mdx::MDX> model);

	int rowCount(const QModelIndex& parent = {}) const override;
	int columnCount(const QModelIndex& parent = {}) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

  private:
	std::shared_ptr<mdx::MDX> mdx;
};

class InfoTableModel : public QAbstractTableModel {
	Q_OBJECT

  public:
	using QAbstractTableModel::QAbstractTableModel;

	void set_model(const std::shared_ptr<mdx::MDX>& model);

	int rowCount(const QModelIndex& parent = {}) const override;
	int columnCount(const QModelIndex& parent = {}) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

  private:
	std::vector<std::pair<QString, QString>> rows;
};

/// Owns the models and the three docks (Sequences, Geosets/Materials, Info) of a ModelEditor window.
class ModelInspector : public QObject {
	Q_OBJECT

  public:
	/// Creates the docks as children of `window`, on its right side.
	explicit ModelInspector(QMainWindow* window);

	/// Shows `model` (null clears the panels). `hidden_geosets` is the viewport's current Show state, indexed by geoset.
	void set_model(const std::shared_ptr<mdx::MDX>& model, std::vector<bool> hidden_geosets);
	/// Highlights a sequence row without emitting sequence_selected.
	void set_current_sequence(int index);

	/// For the --qa-model-editor-script hook: the inspector's own views, so it can drive the same slots a click would.
	QTableView* sequence_table() const { return sequence_view; }
	QTableView* geoset_table() const { return geoset_view; }

	QList<QDockWidget*> docks() const { return { sequences_dock, geosets_dock, info_dock }; }

  signals:
	void sequence_selected(int index);
	void geoset_visibility_changed(int geoset, bool visible);

  private:
	SequenceTableModel* sequence_model;
	GeosetTableModel* geoset_model;
	MaterialTableModel* material_model;
	InfoTableModel* info_model;

	QTableView* sequence_view;
	QTableView* geoset_view = nullptr;
	QDockWidget* sequences_dock;
	QDockWidget* geosets_dock;
	QDockWidget* info_dock;
	bool updating_selection = false;
};
