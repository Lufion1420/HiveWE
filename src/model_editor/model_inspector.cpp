#include "model_inspector.h"

#include <QHeaderView>
#include <QLocale>
#include <QTabWidget>

import std;

namespace {
QString number(const std::size_t value) {
	return QLocale().toString(static_cast<qulonglong>(value));
}

/// What the Geosets and Materials panels call a material: the texture of its first layer, or Team Color / Team Glow for replaceable ids 1 and 2.
QString texture_label(const mdx::MDX& mdx, const mdx::Layer& layer) {
	if (layer.textures.empty() || layer.textures.front().id >= mdx.textures.size()) {
		return {};
	}
	const mdx::Texture& texture = mdx.textures[layer.textures.front().id];
	if (texture.replaceable_id == 1) {
		return "Team Color";
	}
	if (texture.replaceable_id == 2) {
		return "Team Glow";
	}
	return QString::fromStdString(texture.file_name.filename().string());
}

QString material_label(const mdx::MDX& mdx, const std::size_t material_id) {
	if (material_id < mdx.materials.size() && !mdx.materials[material_id].layers.empty()) {
		if (const QString label = texture_label(mdx, mdx.materials[material_id].layers.front()); !label.isEmpty()) {
			return label;
		}
	}
	return QString("Material %1").arg(material_id);
}

QString blend_name(const uint32_t blend_mode) {
	static const char* names[] = { "None", "Transparent", "Blend", "Additive", "Add Alpha", "Modulate", "Modulate 2x" };
	return blend_mode < std::size(names) ? QString::fromLatin1(names[blend_mode]) : QString("Mode %1").arg(blend_mode);
}

std::size_t triangle_count(const mdx::Geoset& geoset) {
	return geoset.faces.size() / 3;
}
} // namespace

DrawnCounts drawn_counts(const mdx::MDX& mdx) {
	DrawnCounts counts;
	for (const mdx::Geoset& geoset : mdx.geosets) {
		if (geoset.lod != 0) {
			continue; // EditableMesh does not draw these
		}
		counts.vertices += geoset.vertices.size();
		counts.triangles += triangle_count(geoset);
	}
	return counts;
}

// --- Sequences -------------------------------------------------------------------------------------------------------------------------

void SequenceTableModel::set_model(std::shared_ptr<mdx::MDX> model) {
	beginResetModel();
	mdx = std::move(model);
	endResetModel();
}

int SequenceTableModel::rowCount(const QModelIndex& parent) const {
	return parent.isValid() || !mdx ? 0 : static_cast<int>(mdx->sequences.size());
}

int SequenceTableModel::columnCount(const QModelIndex& parent) const {
	return parent.isValid() ? 0 : 2;
}

QVariant SequenceTableModel::data(const QModelIndex& index, const int role) const {
	if (!mdx || !index.isValid() || index.row() >= static_cast<int>(mdx->sequences.size())) {
		return {};
	}
	const mdx::Sequence& sequence = mdx->sequences[index.row()];
	// Frames are milliseconds. unsigned subtraction would wrap on a malformed sequence, so go through a signed type.
	const long long length = static_cast<long long>(sequence.end_frame) - static_cast<long long>(sequence.start_frame);

	if (role == Qt::DisplayRole) {
		return index.column() == 0 ? QVariant(QString::fromStdString(sequence.name)) : QVariant(QLocale().toString(length));
	}
	if (role == Qt::TextAlignmentRole && index.column() == 1) {
		return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
	}
	if (role == Qt::ToolTipRole) {
		return QString("%1 - %2 ms, %3, move speed %4, rarity %5")
			.arg(sequence.start_frame)
			.arg(sequence.end_frame)
			.arg(sequence.flags == mdx::Sequence::Flags::looping ? QStringLiteral("looping") : QStringLiteral("non-looping"))
			.arg(sequence.movespeed)
			.arg(sequence.rarity);
	}
	return {};
}

QVariant SequenceTableModel::headerData(const int section, const Qt::Orientation orientation, const int role) const {
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
		return section == 0 ? QStringLiteral("Name") : QStringLiteral("Length (ms)");
	}
	if (orientation == Qt::Horizontal && role == Qt::TextAlignmentRole && section == 1) {
		return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
	}
	return {};
}

// --- Geosets ---------------------------------------------------------------------------------------------------------------------------

void GeosetTableModel::set_model(std::shared_ptr<mdx::MDX> model, std::vector<bool> hidden_state) {
	beginResetModel();
	mdx = std::move(model);
	hidden = std::move(hidden_state);
	endResetModel();
}

int GeosetTableModel::rowCount(const QModelIndex& parent) const {
	return parent.isValid() || !mdx ? 0 : static_cast<int>(mdx->geosets.size());
}

int GeosetTableModel::columnCount(const QModelIndex& parent) const {
	return parent.isValid() ? 0 : 4;
}

QVariant GeosetTableModel::data(const QModelIndex& index, const int role) const {
	if (!mdx || !index.isValid() || index.row() >= static_cast<int>(mdx->geosets.size())) {
		return {};
	}
	const mdx::Geoset& geoset = mdx->geosets[index.row()];

	if (index.column() == 3) {
		if (role == Qt::CheckStateRole) {
			const bool is_hidden = index.row() < static_cast<int>(hidden.size()) && hidden[index.row()];
			return is_hidden ? Qt::Unchecked : Qt::Checked;
		}
		return {};
	}

	if (role == Qt::DisplayRole) {
		switch (index.column()) {
			case 0: return index.row();
			case 1: return material_label(*mdx, geoset.material_id);
			case 2: return number(triangle_count(geoset));
		}
	}
	if (role == Qt::TextAlignmentRole && index.column() == 2) {
		return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
	}
	if (role == Qt::ToolTipRole) {
		return QString("Geoset %1: %2 vertices, %3 triangles, material %4%5")
			.arg(index.row())
			.arg(geoset.vertices.size())
			.arg(triangle_count(geoset))
			.arg(geoset.material_id)
			.arg(geoset.lod != 0 ? QString(", LOD %1 (not drawn)").arg(geoset.lod) : QString());
	}
	return {};
}

QVariant GeosetTableModel::headerData(const int section, const Qt::Orientation orientation, const int role) const {
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
		static const char* names[] = { "#", "Material", "Tris", "Show" };
		return section >= 0 && section < 4 ? QString::fromLatin1(names[section]) : QString();
	}
	if (orientation == Qt::Horizontal && role == Qt::TextAlignmentRole && section == 2) {
		return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
	}
	return {};
}

bool GeosetTableModel::setData(const QModelIndex& index, const QVariant& value, const int role) {
	if (!mdx || !index.isValid() || index.column() != 3 || role != Qt::CheckStateRole || index.row() >= static_cast<int>(mdx->geosets.size())) {
		return false;
	}
	if (hidden.size() < mdx->geosets.size()) {
		hidden.resize(mdx->geosets.size(), false);
	}
	const bool visible = value.toInt() == Qt::Checked;
	hidden[index.row()] = !visible;
	emit dataChanged(index, index, { Qt::CheckStateRole });
	emit visibility_changed(index.row(), visible);
	return true;
}

Qt::ItemFlags GeosetTableModel::flags(const QModelIndex& index) const {
	Qt::ItemFlags result = QAbstractTableModel::flags(index) & ~Qt::ItemIsEditable;
	if (index.column() == 3) {
		result |= Qt::ItemIsUserCheckable;
	}
	return result;
}

// --- Materials -------------------------------------------------------------------------------------------------------------------------

void MaterialTableModel::set_model(std::shared_ptr<mdx::MDX> model) {
	beginResetModel();
	mdx = std::move(model);
	endResetModel();
}

int MaterialTableModel::rowCount(const QModelIndex& parent) const {
	return parent.isValid() || !mdx ? 0 : static_cast<int>(mdx->materials.size());
}

int MaterialTableModel::columnCount(const QModelIndex& parent) const {
	return parent.isValid() ? 0 : 4;
}

QVariant MaterialTableModel::data(const QModelIndex& index, const int role) const {
	if (!mdx || !index.isValid() || index.row() >= static_cast<int>(mdx->materials.size())) {
		return {};
	}
	const mdx::Material& material = mdx->materials[index.row()];

	if (role == Qt::DisplayRole) {
		switch (index.column()) {
			case 0: return index.row();
			case 1: return static_cast<int>(material.layers.size());
			case 2: return material.layers.empty() ? QString() : blend_name(material.layers.front().blend_mode);
			case 3: {
				QStringList names;
				for (const mdx::Layer& layer : material.layers) {
					if (const QString label = texture_label(*mdx, layer); !label.isEmpty() && !names.contains(label)) {
						names << label;
					}
				}
				return names.join(", ");
			}
		}
	}
	return {};
}

QVariant MaterialTableModel::headerData(const int section, const Qt::Orientation orientation, const int role) const {
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
		static const char* names[] = { "#", "Layers", "Blend", "Texture" };
		return section >= 0 && section < 4 ? QString::fromLatin1(names[section]) : QString();
	}
	return {};
}

// --- Info ------------------------------------------------------------------------------------------------------------------------------

void InfoTableModel::set_model(const std::shared_ptr<mdx::MDX>& model) {
	beginResetModel();
	rows.clear();
	if (model) {
		const mdx::MDX& m = *model;
		const DrawnCounts counts = drawn_counts(m);

		rows = {
			{ "Name", QString::fromStdString(m.name) },
			{ "Format version", QString::number(m.version) },
			{ "Vertices (drawn)", number(counts.vertices) },
			{ "Triangles (drawn)", number(counts.triangles) },
			{ "Bones", number(m.bones.size()) },
			{ "Geosets", number(m.geosets.size()) },
			{ "Sequences", number(m.sequences.size()) },
			{ "Materials", number(m.materials.size()) },
			{ "Textures", number(m.textures.size()) },
			{ "Attachments", number(m.attachments.size()) },
			{ "Event objects", number(m.event_objects.size()) },
			{ "Cameras", number(m.cameras.size()) },
			{ "Particle emitters", number(m.emitters1.size() + m.emitters2.size() + m.corn_emitters.size()) },
			{ "Ribbon emitters", number(m.ribbons.size()) },
			{ "Lights", number(m.lights.size()) },
			{ "Collision shapes", number(m.collision_shapes.size()) },
			{ "Global sequences", number(m.global_sequences.size()) },
			{ "Bounds radius", QString::number(m.extent.bounds_radius, 'f', 1) },
		};
	}
	endResetModel();
}

int InfoTableModel::rowCount(const QModelIndex& parent) const {
	return parent.isValid() ? 0 : static_cast<int>(rows.size());
}

int InfoTableModel::columnCount(const QModelIndex& parent) const {
	return parent.isValid() ? 0 : 2;
}

QVariant InfoTableModel::data(const QModelIndex& index, const int role) const {
	if (!index.isValid() || index.row() >= static_cast<int>(rows.size())) {
		return {};
	}
	const auto& row = rows[index.row()];
	if (role == Qt::DisplayRole) {
		return index.column() == 0 ? row.first : row.second;
	}
	if (role == Qt::ToolTipRole) {
		return row.second;
	}
	return {};
}

QVariant InfoTableModel::headerData(const int section, const Qt::Orientation orientation, const int role) const {
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
		return section == 0 ? QStringLiteral("Property") : QStringLiteral("Value");
	}
	return {};
}

// --- Docks -----------------------------------------------------------------------------------------------------------------------------

namespace {
QTableView* make_table(QAbstractItemModel* model, QWidget* parent, const QString& name) {
	auto* view = new QTableView(parent);
	view->setObjectName(name);
	view->setModel(model);
	view->setSelectionBehavior(QAbstractItemView::SelectRows);
	view->setSelectionMode(QAbstractItemView::SingleSelection);
	view->setEditTriggers(QAbstractItemView::NoEditTriggers);
	view->setAlternatingRowColors(true);
	view->setShowGrid(false);
	view->setWordWrap(false);
	view->setCornerButtonEnabled(false);
	view->verticalHeader()->hide();
	view->verticalHeader()->setDefaultSectionSize(22);
	view->horizontalHeader()->setStretchLastSection(true);
	view->horizontalHeader()->setHighlightSections(false);
	view->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	return view;
}

QDockWidget* make_dock(QMainWindow* window, const QString& title, const QString& name, QWidget* content) {
	auto* dock = new QDockWidget(title, window);
	dock->setObjectName(name);
	dock->setWidget(content);
	window->addDockWidget(Qt::RightDockWidgetArea, dock);
	return dock;
}
} // namespace

ModelInspector::ModelInspector(QMainWindow* window) : QObject(window) {
	sequence_model = new SequenceTableModel(this);
	geoset_model = new GeosetTableModel(this);
	material_model = new MaterialTableModel(this);
	info_model = new InfoTableModel(this);

	sequence_view = make_table(sequence_model, window, "sequenceTable");
	sequence_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	sequence_view->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	sequences_dock = make_dock(window, "Sequences", "dockSequences", sequence_view);

	geoset_view = make_table(geoset_model, window, "geosetTable");
	geoset_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	geoset_view->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	geoset_view->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	geoset_view->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
	geoset_view->horizontalHeader()->setStretchLastSection(false);

	auto* material_view = make_table(material_model, window, "materialTable");
	material_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	material_view->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	material_view->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

	auto* tabs = new QTabWidget(window);
	tabs->setObjectName("geosetMaterialTabs");
	tabs->setDocumentMode(true);
	tabs->addTab(geoset_view, "Geosets");
	tabs->addTab(material_view, "Materials");
	geosets_dock = make_dock(window, "Geosets and Materials", "dockGeosets", tabs);

	auto* info_view = make_table(info_model, window, "infoTable");
	info_view->setSelectionMode(QAbstractItemView::NoSelection);
	info_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	info_dock = make_dock(window, "Info", "dockInfo", info_view);

	window->resizeDocks({ sequences_dock, geosets_dock, info_dock }, { 200, 200, 200 }, Qt::Vertical);
	window->resizeDocks({ sequences_dock }, { 320 }, Qt::Horizontal);

	connect(sequence_view->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this](const QModelIndex& current) {
		if (!updating_selection && current.isValid()) {
			emit sequence_selected(current.row());
		}
	});
	connect(geoset_model, &GeosetTableModel::visibility_changed, this, &ModelInspector::geoset_visibility_changed);
}

void ModelInspector::set_model(const std::shared_ptr<mdx::MDX>& model, std::vector<bool> hidden_geosets) {
	updating_selection = true;
	sequence_model->set_model(model);
	geoset_model->set_model(model, std::move(hidden_geosets));
	material_model->set_model(model);
	info_model->set_model(model);
	updating_selection = false;
}

void ModelInspector::set_current_sequence(const int index) {
	if (index < 0 || index >= sequence_model->rowCount()) {
		return;
	}
	updating_selection = true;
	sequence_view->setCurrentIndex(sequence_model->index(index, 0));
	updating_selection = false;
}
