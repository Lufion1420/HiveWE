module;

#include <QAbstractProxyModel>
#include <QPainter>
#include <QFileIconProvider>
#include <QSortFilterProxyModel>

export module BaseTreeModel;

import std;
import SLK;

export class BaseTreeItem {
  public:
	QVector<BaseTreeItem*> children;
	BaseTreeItem* parent = nullptr;

	explicit BaseTreeItem(BaseTreeItem* parent = nullptr)
		: parent(parent) {
		if (parent != nullptr) {
			parent->appendChild(this);
		}
	}

	~BaseTreeItem() {
		qDeleteAll(children);
	}

	void appendChild(BaseTreeItem* item) {
		item->parent = this;
		children.append(item);
	}

	void removeChild(BaseTreeItem* item) {
		item->parent = nullptr;
		children.removeOne(item);
	}

	int row() const {
		if (parent) {
			return parent->children.indexOf(const_cast<BaseTreeItem*>(this));
		}

		return 0;
	}

	std::string id;
	std::string label;
	bool baseCategory = false;
	bool subCategory = false;
	bool customFolder = false;
};

export class BaseTreeModel : public QAbstractProxyModel {
	bool subtree_contains_custom(const BaseTreeItem* item) const {
		if (!item) {
			return false;
		}

		if (!(item->baseCategory || item->subCategory)) {
			return slk->shadow_data.contains(item->id) && slk->shadow_data.at(item->id).contains("oldid");
		}

		for (const auto* child : item->children) {
			if (subtree_contains_custom(child)) {
				return true;
			}
		}

		return false;
	}

	int rowCount(const QModelIndex& parent) const override {
		if (parent.column() > 0) {
			return 0;
		}

		BaseTreeItem* parentItem;

		if (!parent.isValid()) {
			parentItem = rootItem;
		} else {
			parentItem = static_cast<BaseTreeItem*>(parent.internalPointer());
		}

		return parentItem->children.count();
	}

	int columnCount(const QModelIndex& parent) const override {
		return 1;
	}

	Qt::ItemFlags flags(const QModelIndex& index) const override {
		if (!index.isValid()) {
			return Qt::NoItemFlags;
		}

		return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
	}

	QModelIndex index(int row, int column, const QModelIndex& parent) const override {
		if (!hasIndex(row, column, parent))
			return QModelIndex();

		BaseTreeItem* parentItem;

		if (!parent.isValid()) {
			parentItem = rootItem;
		} else {
			parentItem = static_cast<BaseTreeItem*>(parent.internalPointer());
		}

		BaseTreeItem* childItem = parentItem->children.at(row);
		if (childItem)
			return createIndex(row, column, childItem);
		return QModelIndex();
	}

	QModelIndex parent(const QModelIndex& index) const override {
		if (!index.isValid()) {
			return {};
		}

		BaseTreeItem* childItem = static_cast<BaseTreeItem*>(index.internalPointer());
		BaseTreeItem* parentItem = childItem->parent;

		if (parentItem == rootItem)
			return QModelIndex();

		return createIndex(parentItem->row(), 0, parentItem);
	}

	void rowsInserted(const QModelIndex& parent, int first, int last) {
		assert(first == last);

		const std::string id = slk->index_to_row.at(first);
		BaseTreeItem* parent_item = getFolderParent(id);

		beginInsertRows(createIndex(parent_item->row(), 0, parent_item), parent_item->children.size(), parent_item->children.size());
		BaseTreeItem* item = new BaseTreeItem(parent_item);
		item->id = id;
		items.emplace(id, item);
		endInsertRows();
	}

	void rowsRemoved(const QModelIndex& parent, int first, int last) {
		assert(first == last);

		const std::string id = slk->index_to_row.at(first);
		BaseTreeItem* child = items.at(id);

		BaseTreeItem* parent_item = child->parent;
		const int row = parent_item->children.indexOf(child);

		beginRemoveRows(createIndex(parent_item->row(), 0, parent_item), row, row);
		parent_item->removeChild(child);
		items.erase(id);
		delete child;
		endRemoveRows();
	}

	void sourceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles) {
		Q_ASSERT(topLeft.isValid());
		Q_ASSERT(topLeft.model() == this->sourceModel());
		Q_ASSERT(bottomRight.isValid());
		Q_ASSERT(bottomRight.model() == this->sourceModel());

		// If the changed field is one of those that determine the item's location in the tree we have to move it
		if (std::find(categoryChangeFields.begin(), categoryChangeFields.end(), slk->index_to_column[topLeft.column()]) != categoryChangeFields.end()) {
			const std::string id = slk->index_to_row.at(topLeft.row());

			BaseTreeItem* parent_item = items.at(id)->parent;
			int row = parent_item->children.indexOf(items.at(id));

			BaseTreeItem* new_parent = getFolderParent(id);
			const QModelIndex source_parent = createIndex(parent_item->row(), 0, parent_item);
			const QModelIndex target_parent = createIndex(new_parent->row(), 0, new_parent);

			beginMoveRows(source_parent, row, row, target_parent, new_parent->children.size());
			BaseTreeItem* child = parent_item->children[row];
			parent_item->removeChild(child);
			new_parent->appendChild(child);
			endMoveRows();
		}

		emit dataChanged(mapFromSource(topLeft), mapFromSource(bottomRight), roles);
	}

	// BaseTreeItem* newItem(std::string id);
	// void removeItem(std::string id);

	virtual BaseTreeItem* getFolderParent(const std::string& id) const {
		return nullptr;
	};

 public:
	explicit BaseTreeModel(QObject* parent = nullptr)
		: QAbstractProxyModel(parent) {
		rootItem = new BaseTreeItem();

		QFileIconProvider icons;
		folderIcon = icons.icon(QFileIconProvider::Folder);
	}

	~BaseTreeModel() {
		delete rootItem;
	}

	QVariant data(const QModelIndex& index, int role) const override {
		if (!index.isValid()) {
			return {};
		}

		BaseTreeItem* item = static_cast<BaseTreeItem*>(index.internalPointer());

		switch (role) {
			case Qt::DecorationRole:
				if (item->baseCategory || item->subCategory) {
					return folderIcon;
				}
				if (slk->column_headers.contains("art")) {
					return sourceModel()->data(sourceModel()->index(slk->row_headers.at(item->id), slk->column_headers.at("art")), role);
				} else {
					return sourceModel()->data(sourceModel()->index(slk->row_headers.at(item->id), slk->column_headers.at("buffart")), role);
				}
			case Qt::ForegroundRole:
				if (item->baseCategory || item->subCategory) {
					if (item->customFolder && subtree_contains_custom(item)) {
						return QColor("violet");
					}
					return {};
				}

				if (slk->shadow_data.contains(item->id)) {
					return QColor("violet");
				} else {
					return {};
				}
			case Qt::ToolTipRole:
				if (item->baseCategory || item->subCategory) {
					return {};
				}

				return "ID: " + QString::fromStdString(item->id);
			default:
				return {};
		}
	}

	QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override {
		if (!sourceIndex.isValid()) {
			return {};
		}

		const std::string id = slk->index_to_row.at(sourceIndex.row());
		const BaseTreeItem* parent_item = items.at(id)->parent;
		const int row = parent_item->children.indexOf(items.at(id));
		return createIndex(row, 0, items.at(id));
	}

	QModelIndex mapToSource(const QModelIndex& proxyIndex) const override {
		if (!proxyIndex.isValid()) {
			return {};
		}

		BaseTreeItem* item = static_cast<BaseTreeItem*>(proxyIndex.internalPointer());

		if (item->baseCategory || item->subCategory) {
			return {};
		}

		if (slk->column_headers.contains("name")) {
			return createIndex(slk->row_headers.at(item->id), slk->column_headers.at("name"), item);
		} else {
			return createIndex(slk->row_headers.at(item->id), slk->column_headers.at("bufftip"), item);
		}
	}

	void setSourceModel(QAbstractItemModel* sourceModel) override {
		beginResetModel();

		if (this->sourceModel()) {
			disconnect(sourceModel, &QAbstractItemModel::rowsInserted, this, &BaseTreeModel::rowsInserted);
			disconnect(sourceModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, &BaseTreeModel::rowsRemoved);
			disconnect(sourceModel, &QAbstractItemModel::dataChanged, this, &BaseTreeModel::sourceDataChanged);
		}

		QAbstractProxyModel::setSourceModel(sourceModel);

		connect(sourceModel, &QAbstractItemModel::rowsInserted, this, &BaseTreeModel::rowsInserted);
		connect(sourceModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, &BaseTreeModel::rowsRemoved);
		connect(sourceModel, &QAbstractItemModel::dataChanged, this, &BaseTreeModel::sourceDataChanged);

		endResetModel();
	}

	QModelIndex getIdIndex(const std::string& id) {
		const BaseTreeItem* parent_item = items.at(id)->parent;
		const int row = parent_item->children.indexOf(items.at(id));
		return createIndex(row, 0, items.at(id));
	}

	BaseTreeItem* rootItem;
	QIcon folderIcon;

	std::vector<std::string> categoryChangeFields;

  protected:
	[[nodiscard]] QString append_id_label(const QString& label, const std::string& id) const {
		return label + " (" + QString::fromStdString(id) + ")";
	}

	slk::SLK* slk;
	std::unordered_map<std::string, BaseTreeItem*> items;
};

export class BaseFilter : public QSortFilterProxyModel {
	Q_OBJECT

	bool filterCustom = false;

	static std::string normalized_label(std::string label) {
		std::erase(label, '&');
		std::ranges::transform(label, label.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return label;
	}

  public:
	slk::SLK* slk;

	using QSortFilterProxyModel::QSortFilterProxyModel;

	bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
		QModelIndex index0 = sourceModel()->index(sourceRow, 0, sourceParent);
		BaseTreeItem* item = static_cast<BaseTreeItem*>(index0.internalPointer());

		if (filterCustom) {
			if (item->baseCategory || item->subCategory) {
				return false;
			}

			if (!(slk->shadow_data.contains(item->id) && slk->shadow_data.at(item->id).contains("oldid"))) {
				return false;
			}
		}

		return sourceModel()->data(index0).toString().contains(filterRegularExpression());
	}

	bool lessThan(const QModelIndex& source_left, const QModelIndex& source_right) const override {
		const auto* left = static_cast<const BaseTreeItem*>(source_left.internalPointer());
		const auto* right = static_cast<const BaseTreeItem*>(source_right.internalPointer());
		if (!left || !right) {
			return QSortFilterProxyModel::lessThan(source_left, source_right);
		}

		const bool left_folder = left->baseCategory || left->subCategory;
		const bool right_folder = right->baseCategory || right->subCategory;
		if (left_folder != right_folder) {
			return left_folder;
		}

		if (left->customFolder != right->customFolder) {
			return left->customFolder;
		}

		const std::string left_name = left_folder ? left->label : sourceModel()->data(source_left, Qt::DisplayRole).toString().toStdString();
		const std::string right_name = right_folder ? right->label : sourceModel()->data(source_right, Qt::DisplayRole).toString().toStdString();

		return normalized_label(left_name) < normalized_label(right_name);
	}

  public slots:
	void setFilterCustom(const bool filter) {
		beginFilterChange();
		filterCustom = filter;
		endFilterChange(Direction::Rows);
	}
};

#include "base_tree_model.moc"
