#include "tile_setter.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QImage>
#include <QMessageBox>
#include <QTimer>
#include <QFontDatabase>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>

import std;
import ResourceManager;
import OpenGLUtilities;
import MapGlobal;
import Globals;
import SLK;
import Texture;
import Hierarchy;
import TilesetChange;
import BLP;
import BinaryReader;
import Utilities;
import <soil2/SOIL2.h>;
import <glm/glm.hpp>;

namespace {
	constexpr int row_height = 30;

	std::size_t max_tiles() {
		return tileset_policy(hierarchy.hd).max_tiles;
	}

	/// An image file as RGBA, for the preview of an imported tile.
	struct DecodedImage {
		int width = 0;
		int height = 0;
		std::vector<std::uint8_t> rgba;
	};

	std::expected<DecodedImage, std::string> decode_tile_image(const std::filesystem::path& path) {
		auto reader = read_file(path);
		if (!reader) {
			return std::unexpected(reader.error());
		}

		int width = 0;
		int height = 0;
		int channels = 0;
		std::vector<std::uint8_t> pixels;
		if (path.extension() == ".blp") {
			std::uint8_t* data = blp::load(*reader, width, height, channels);
			if (!data) {
				return std::unexpected("Not a valid BLP texture.");
			}
			pixels.assign(data, data + static_cast<std::size_t>(width) * height * channels);
			delete[] data;
		} else {
			std::uint8_t* data = SOIL_load_image_from_memory(
				reader->buffer.data(), static_cast<int>(reader->buffer.size()), &width, &height, &channels, SOIL_LOAD_AUTO
			);
			if (!data) {
				return std::unexpected("The image could not be read.");
			}
			pixels.assign(data, data + static_cast<std::size_t>(width) * height * channels);
			std::free(data); // stb_image allocates with malloc
		}
		if (width <= 0 || height <= 0 || (channels != 3 && channels != 4)) {
			return std::unexpected("Unsupported image format.");
		}

		DecodedImage image { width, height, {} };
		if (channels == 4) {
			image.rgba = std::move(pixels);
		} else {
			image.rgba.reserve(static_cast<std::size_t>(width) * height * 4);
			for (std::size_t i = 0; i + 2 < pixels.size(); i += 3) {
				image.rgba.insert(image.rgba.end(), { pixels[i], pixels[i + 1], pixels[i + 2], 255 });
			}
		}
		return image;
	}

	QFont mono_font() {
		QFont font("Consolas");
		font.setStyleHint(QFont::Monospace);
		font.setBold(true);
		return font;
	}

	QLabel* make_heading(const QString& text) {
		auto* label = new QLabel(text);
		QFont font = label->font();
		font.setBold(true);
		font.setPointSizeF(font.pointSizeF() + 1);
		label->setFont(font);
		return label;
	}
}

TileSetter::TileSetter(QWidget* parent) : QDialog(parent) {
	ui.setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose);

	const QFont mono = mono_font();

	// Left column: Available Tiles
	tileset_combo = new QComboBox;
	tileset_combo->setToolTip("The tileset to choose additional tiles from.");
	tileset_combo->setMaxVisibleItems(20);

	available_list = new QTreeWidget;
	available_list->setHeaderHidden(true);
	available_list->setColumnCount(2);
	available_list->setRootIsDecorated(false);
	available_list->setIconSize({ 24, 24 });
	available_list->setSelectionMode(QAbstractItemView::SingleSelection);
	available_list->header()->setStretchLastSection(false);
	available_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	available_list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

	import_button = new QPushButton("Import Custom Tile...");
	import_button->setEnabled(false);
	import_button->setToolTip(
		"Replaces the texture of the selected tile with an image file (BLP or TGA). It is copied into the map folder when you press OK."
	);

	auto* left = new QVBoxLayout;
	left->setSpacing(6);
	left->addWidget(make_heading("Available Tiles"));
	left->addWidget(tileset_combo);
	left->addWidget(available_list, 1);
	left->addWidget(import_button);

	// Middle column: transfer buttons
	add_button = new QPushButton("Add →");
	add_button->setToolTip("Adds the selected additional tile to the map tileset.");
	remove_button = new QPushButton("← Remove");
	remove_button->setToolTip("Removes the selected tile from the tileset. Cliff tiles cannot be removed.");
	up_button = new QPushButton("Move Up");
	up_button->setToolTip("Moves the selected tile up one slot.");
	down_button = new QPushButton("Move Down");
	down_button->setToolTip("Moves the selected tile down one slot.");

	auto* middle = new QVBoxLayout;
	middle->setSpacing(6);
	middle->addStretch(1);
	middle->addWidget(add_button);
	middle->addWidget(remove_button);
	middle->addSpacing(8);
	middle->addWidget(up_button);
	middle->addWidget(down_button);
	middle->addStretch(1);

	// Right column: Map Tileset
	count_label = new QLabel;
	count_label->setEnabled(false);
	auto* right_heading = new QHBoxLayout;
	right_heading->addWidget(make_heading("Map Tileset"));
	right_heading->addWidget(count_label);
	right_heading->addStretch(1);

	map_list = new QTreeWidget;
	map_list->setColumnCount(4);
	map_list->setHeaderLabels({ "#", "Tile", "ID", "Result" });
	map_list->setRootIsDecorated(false);
	map_list->setIconSize({ 24, 24 });
	map_list->setSelectionMode(QAbstractItemView::SingleSelection);
	map_list->header()->setStretchLastSection(true);
	map_list->setColumnWidth(0, 30);
	map_list->setColumnWidth(1, 190);
	map_list->setColumnWidth(2, 55);

	auto* right = new QVBoxLayout;
	right->setSpacing(6);
	right->addLayout(right_heading);
	right->addWidget(map_list, 1);

	auto* columns = new QHBoxLayout;
	columns->setSpacing(8);
	columns->addLayout(left, 0);
	columns->addLayout(middle, 0);
	columns->addLayout(right, 1);
	available_list->setMinimumWidth(260);

	// Selected Tile group
	preview_large = new QLabel;
	preview_large->setFixedSize(96, 96);
	preview_name = new QLabel;
	QFont bold = preview_name->font();
	bold.setBold(true);
	preview_name->setFont(bold);
	preview_id = new QLabel;
	preview_id->setFont(mono);
	preview_file = new QLabel;
	preview_file->setTextInteractionFlags(Qt::TextSelectableByMouse);
	preview_size = new QLabel;

	auto* pathing = new QHBoxLayout;
	pathing->setSpacing(14);
	auto* pathing_label = new QLabel("Pathing");
	pathing_label->setEnabled(false);
	pathing->addWidget(pathing_label);
	for (const char* name : { "Unwalkable", "Unflyable", "Unbuildable" }) {
		auto* check = new QCheckBox(name);
		check->setEnabled(false);
		check->setToolTip("Pathing overrides apply to custom tiles only.");
		pathing->addWidget(check);
	}
	pathing->addStretch(1);

	auto* info = new QVBoxLayout;
	info->setSpacing(3);
	info->addWidget(preview_name);
	info->addWidget(preview_id);
	info->addWidget(preview_file);
	info->addWidget(preview_size);
	info->addLayout(pathing);
	info->addStretch(1);

	auto* selected_group = new QGroupBox("Selected Tile");
	auto* selected_layout = new QHBoxLayout(selected_group);
	selected_layout->setSpacing(14);
	selected_layout->addWidget(preview_large, 0, Qt::AlignTop);
	selected_layout->addLayout(info, 1);

	button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

	auto* root = static_cast<QVBoxLayout*>(layout());
	root->addLayout(columns, 1);
	root->addWidget(selected_group);
	root->addWidget(button_box);

	setWindowModality(Qt::ApplicationModal);
	resize(820, 660);

	// Data
	to_ids = map->terrain.tileset_ids;
	for (const auto& [key, value] : world_edit_data.section("TileSets")) {
		tileset_combo->addItem(QString::fromStdString(value[0]), QString::fromStdString(key));
	}

	// Start on the map's own tileset when it is one of the entries.
	if (!to_ids.empty() && !to_ids.front().empty()) {
		const int index = tileset_combo->findData(QString(QChar(to_ids.front().front())));
		if (index >= 0) {
			tileset_combo->setCurrentIndex(index);
		}
	}

	update_available_tiles();
	refresh_map_list();

	connect(tileset_combo, &QComboBox::currentTextChanged, this, &TileSetter::update_available_tiles);
	connect(available_list, &QTreeWidget::currentItemChanged, this, [this] {
		update_buttons();
		if (const auto* item = available_list->currentItem()) {
			update_selected_info(item->data(0, Qt::UserRole).toString().toStdString());
		}
	});
	connect(map_list, &QTreeWidget::currentItemChanged, this, [this] {
		update_buttons();
		if (const auto* item = map_list->currentItem(); item && item->data(0, Qt::UserRole).isValid()) {
			update_selected_info(item->data(0, Qt::UserRole).toString().toStdString());
		}
	});
	connect(import_button, &QPushButton::clicked, this, &TileSetter::import_custom_tile);
	connect(add_button, &QPushButton::clicked, this, &TileSetter::add_tile);
	connect(remove_button, &QPushButton::clicked, this, &TileSetter::remove_tile);
	connect(up_button, &QPushButton::clicked, this, [this] { move_tile(-1); });
	connect(down_button, &QPushButton::clicked, this, [this] { move_tile(1); });
	connect(button_box, &QDialogButtonBox::accepted, this, &TileSetter::save_tiles);
	connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

	if (map_list->topLevelItemCount() > 0) {
		map_list->setCurrentItem(map_list->topLevelItem(0));
	}
	update_buttons();

	show();
}

QIcon TileSetter::tile_icon(const std::string& id) const {
	if (const auto pending = pending_textures.find(id); pending != pending_textures.end()) {
		return pending->second.icon;
	}
	if (const auto found = icon_cache.find(id); found != icon_cache.end()) {
		return found->second;
	}
	const slk::SLK& slk = map->terrain.terrain_slk;
	QIcon icon;
	if (const auto image = resource_manager.load<Texture>(slk.data("dir", id) + "\\" + slk.data("file", id))) {
		icon = ground_texture_to_icon((*image)->data.data(), (*image)->width, (*image)->height);
	}
	icon_cache.emplace(id, icon);
	return icon;
}

bool TileSetter::is_cliff_tile(const std::string& id) const {
	const auto& cliff_tiles = map->terrain.cliff_to_ground_texture;
	const auto found = map->terrain.ground_texture_to_id.find(id);
	if (found == map->terrain.ground_texture_to_id.end()) {
		return false;
	}
	return std::ranges::find(cliff_tiles, found->second) != cliff_tiles.end();
}

void TileSetter::update_available_tiles() {
	available_list->clear();

	const std::string tileset = tileset_combo->currentData().toString().toStdString();
	if (tileset.empty()) {
		return;
	}

	const QFont mono = mono_font();
	const slk::SLK& slk = map->terrain.terrain_slk;
	for (const auto& [key, value] : slk.row_headers) {
		if (key.front() != tileset.front()) {
			continue;
		}

		auto* item = new QTreeWidgetItem(available_list);
		item->setIcon(0, tile_icon(key));
		item->setText(0, QString::fromUtf8(slk.data<std::string_view>("comment", key)) + (pending_textures.contains(key) ? "  (custom)" : ""));
		item->setText(1, QString::fromStdString(key));
		item->setFont(1, mono);
		item->setData(0, Qt::UserRole, QString::fromStdString(key));
		item->setSizeHint(0, QSize(0, row_height + 4));
	}
	update_buttons();
}

void TileSetter::refresh_map_list(const std::string& select_id) {
	// Remember the selection when the caller did not ask for one.
	std::string keep = select_id;
	if (keep.empty()) {
		if (const auto* current = map_list->currentItem(); current && current->data(0, Qt::UserRole).isValid()) {
			keep = current->data(0, Qt::UserRole).toString().toStdString();
		}
	}

	const QSignalBlocker blocker(map_list);
	map_list->clear();

	const QFont mono = mono_font();
	const slk::SLK& slk = map->terrain.terrain_slk;
	const auto& original = map->terrain.tileset_ids;
	const auto tile_name = [&](const std::string& id) { return QString::fromUtf8(slk.data<std::string_view>("comment", id)); };

	QTreeWidgetItem* to_select = nullptr;
	for (std::size_t i = 0; i < to_ids.size(); i++) {
		const std::string& id = to_ids[i];
		const bool kept = std::ranges::find(original, id) != original.end();

		auto* item = new QTreeWidgetItem(map_list);
		item->setText(0, QString::number(i + 1));
		item->setIcon(1, tile_icon(id));
		item->setText(1, tile_name(id));
		item->setText(2, QString::fromStdString(id));
		item->setFont(2, mono);
		const bool custom = pending_textures.contains(id);
		if (kept) {
			QString result = is_cliff_tile(id) ? "Kept, cliff tile" : "Kept";
			if (custom) {
				result += ", custom texture";
			}
			item->setText(3, result);
			item->setForeground(3, custom ? QColor(166, 107, 224) : map_list->palette().color(QPalette::Disabled, QPalette::Text));
		} else {
			item->setText(3, custom ? "New, custom texture" : "New");
			item->setForeground(3, custom ? QColor(166, 107, 224) : QColor(46, 160, 67));
		}
		item->setData(0, Qt::UserRole, QString::fromStdString(id));
		item->setSizeHint(0, QSize(0, row_height));
		if (id == keep) {
			to_select = item;
		}
	}

	// Removed tiles: each needs a replacement for the map cells that used it.
	std::vector<std::string> removed;
	for (const auto& id : original) {
		if (std::ranges::find(to_ids, id) == to_ids.end()) {
			removed.push_back(id);
		}
	}

	if (!removed.empty() && !to_ids.empty()) {
		auto* group = new QTreeWidgetItem(map_list);
		group->setText(0, "Removed tiles: map cells are repainted with");
		QFont font = group->font(0);
		font.setBold(true);
		group->setFont(0, font);
		group->setFirstColumnSpanned(true);
		group->setFlags(Qt::ItemIsEnabled);
		group->setSizeHint(0, QSize(0, row_height));

		for (const auto& id : removed) {
			auto& replacement = replacements[id];
			if (std::ranges::find(to_ids, replacement) == to_ids.end()) {
				replacement = to_ids.front();
			}

			auto* item = new QTreeWidgetItem(map_list);
			item->setFlags(Qt::ItemIsEnabled);
			item->setIcon(1, tile_icon(id));
			item->setText(1, tile_name(id));
			QFont struck = item->font(1);
			struck.setStrikeOut(true);
			item->setFont(1, struck);
			item->setText(2, QString::fromStdString(id));
			item->setFont(2, mono);
			item->setForeground(1, map_list->palette().color(QPalette::Disabled, QPalette::Text));
			item->setForeground(2, map_list->palette().color(QPalette::Disabled, QPalette::Text));
			item->setSizeHint(0, QSize(0, row_height));

			auto* combo = new QComboBox;
			combo->setToolTip("The tile that replaces this one on the map.");
			for (const auto& to_id : to_ids) {
				combo->addItem(tile_icon(to_id), tile_name(to_id), QString::fromStdString(to_id));
			}
			combo->setCurrentIndex(static_cast<int>(std::ranges::find(to_ids, replacement) - to_ids.begin()));
			connect(combo, &QComboBox::currentIndexChanged, this, [this, id, combo](int index) {
				if (index >= 0) {
					replacements[id] = combo->itemData(index).toString().toStdString();
				}
			});
			map_list->setItemWidget(item, 3, combo);
		}
	}

	if (!to_select && map_list->topLevelItemCount() > 0 && !to_ids.empty()) {
		to_select = map_list->topLevelItem(0);
	}
	if (to_select) {
		map_list->setCurrentItem(to_select);
	}

	count_label->setText(QString("%1 of %2 tiles after changes").arg(to_ids.size()).arg(max_tiles()));
	button_box->button(QDialogButtonBox::Ok)->setEnabled(!to_ids.empty());
	update_buttons();
	if (const auto* current = map_list->currentItem(); current && current->data(0, Qt::UserRole).isValid()) {
		update_selected_info(current->data(0, Qt::UserRole).toString().toStdString());
	}
}

void TileSetter::update_buttons() {
	// Add
	bool can_add = false;
	if (const auto* item = available_list->currentItem()) {
		const std::string id = item->data(0, Qt::UserRole).toString().toStdString();
		can_add = to_ids.size() < max_tiles() && std::ranges::find(to_ids, id) == to_ids.end();
	}
	add_button->setEnabled(can_add);
	import_button->setEnabled(!selected_id.empty() && tileset_policy(hierarchy.hd).write_sd_override);

	// Remove and move
	bool can_remove = false;
	bool can_up = false;
	bool can_down = false;
	if (const auto* item = map_list->currentItem(); item && item->data(0, Qt::UserRole).isValid()) {
		const int index = map_list->indexOfTopLevelItem(const_cast<QTreeWidgetItem*>(item));
		const std::string id = item->data(0, Qt::UserRole).toString().toStdString();
		can_remove = !is_cliff_tile(id) && to_ids.size() > 1;
		can_up = index > 0;
		can_down = index < static_cast<int>(to_ids.size()) - 1;
	}
	remove_button->setEnabled(can_remove);
	up_button->setEnabled(can_up);
	down_button->setEnabled(can_down);
}

void TileSetter::update_selected_info(const std::string& id) {
	const slk::SLK& slk = map->terrain.terrain_slk;
	preview_name->setText(QString::fromUtf8(slk.data<std::string_view>("comment", id)));
	preview_id->setText("Tile ID  " + QString::fromStdString(id));

	selected_id = id;
	import_button->setEnabled(tileset_policy(hierarchy.hd).write_sd_override);

	const std::string path = slk.data("dir", id) + "\\" + slk.data("file", id);
	const auto pending = pending_textures.find(id);
	if (pending != pending_textures.end()) {
		preview_name->setText(preview_name->text() + "  (custom texture)");
		preview_file->setText("File  " + QString::fromStdString(path) + "." + QString::fromStdString(pending->second.extension)
			+ "  (imported from " + QString::fromStdString(pending->second.source.filename().string()) + ")");
	} else {
		preview_file->setText("File  " + QString::fromStdString(path));
	}

	const QIcon icon = tile_icon(id);
	preview_large->setPixmap(icon.pixmap(QSize(64, 64)).scaled(96, 96, Qt::KeepAspectRatio, Qt::FastTransformation));

	if (pending != pending_textures.end()) {
		preview_size->setText(QString("Size  %1 × %2 (4 × 4 tile atlas)").arg(pending->second.width).arg(pending->second.height));
	} else if (const auto image = resource_manager.load<Texture>(path)) {
		preview_size->setText(QString("Size  %1 × %2 (4 × 4 tile atlas)").arg((*image)->width).arg((*image)->height));
	} else {
		preview_size->setText("Size  unknown");
	}
}

void TileSetter::add_tile() {
	const auto* item = available_list->currentItem();
	if (!item || to_ids.size() >= max_tiles()) {
		return;
	}
	const std::string id = item->data(0, Qt::UserRole).toString().toStdString();
	if (std::ranges::find(to_ids, id) != to_ids.end()) {
		return;
	}
	to_ids.push_back(id);
	refresh_map_list(id);
}

void TileSetter::remove_tile() {
	const auto* item = map_list->currentItem();
	if (!item || !item->data(0, Qt::UserRole).isValid() || to_ids.size() <= 1) {
		return;
	}
	const std::string id = item->data(0, Qt::UserRole).toString().toStdString();
	if (is_cliff_tile(id)) {
		return;
	}
	const auto found = std::ranges::find(to_ids, id);
	if (found == to_ids.end()) {
		return;
	}
	const auto index = found - to_ids.begin();
	to_ids.erase(found);
	refresh_map_list(to_ids[std::min<std::size_t>(index, to_ids.size() - 1)]);
}

void TileSetter::move_tile(int delta) {
	const auto* item = map_list->currentItem();
	if (!item || !item->data(0, Qt::UserRole).isValid()) {
		return;
	}
	const std::string id = item->data(0, Qt::UserRole).toString().toStdString();
	const auto found = std::ranges::find(to_ids, id);
	if (found == to_ids.end()) {
		return;
	}
	const auto index = found - to_ids.begin();
	const auto target = index + delta;
	if (target < 0 || target >= static_cast<std::ptrdiff_t>(to_ids.size())) {
		return;
	}
	std::swap(to_ids[index], to_ids[target]);
	refresh_map_list(id);
}

void TileSetter::qa_apply_sample_changes() {
	for (int i = 0; i < available_list->topLevelItemCount(); i++) {
		auto* item = available_list->topLevelItem(i);
		const std::string id = item->data(0, Qt::UserRole).toString().toStdString();
		if (std::ranges::find(to_ids, id) == to_ids.end()) {
			available_list->setCurrentItem(item);
			add_tile();
			break;
		}
	}
	const std::string added = to_ids.back();

	const auto& original = map->terrain.tileset_ids;
	for (auto it = original.rbegin(); it != original.rend(); ++it) {
		if (!is_cliff_tile(*it) && std::ranges::find(to_ids, *it) != to_ids.end()) {
			std::erase(to_ids, *it);
			break;
		}
	}
	refresh_map_list(added);
}

void TileSetter::import_custom_tile() {
	if (selected_id.empty()) {
		return;
	}
	const std::string id = selected_id;

	const QString chosen = QFileDialog::getOpenFileName(
		this,
		"Import Custom Tile",
		QString::fromStdString(hierarchy.map_directory.string()),
		"Textures (*.blp *.tga)"
	);
	if (chosen.isEmpty()) {
		return;
	}

	const std::filesystem::path source = chosen.toStdWString();
	std::string extension = source.extension().string();
	if (!extension.empty()) {
		extension.erase(0, 1);
	}
	std::ranges::transform(extension, extension.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (extension == "dds") {
		QMessageBox::warning(
			this,
			"Import Custom Tile",
			"A DDS texture cannot replace a tile: Warcraft III finds the stock BLP before it looks for a DDS. Convert the image to BLP or TGA."
		);
		return;
	}
	if (!is_tile_texture_extension(extension)) {
		QMessageBox::warning(this, "Import Custom Tile", "Only BLP and TGA textures can be used as a tile.");
		return;
	}

	const auto image = decode_tile_image(source);
	if (!image) {
		QMessageBox::warning(this, "Import Custom Tile", QString::fromStdString(image.error()));
		return;
	}
	// A tile texture is a 4 x 4 atlas of square tiles (or 8 x 4 with the extended variations).
	if (image->height < 4 || (image->width != image->height && image->width != image->height * 2)) {
		QMessageBox::warning(
			this,
			"Import Custom Tile",
			QString("The image is %1 x %2. A tile texture is a 4 x 4 atlas of square tiles, so it must be square (or twice as wide as high).")
				.arg(image->width)
				.arg(image->height)
		);
		return;
	}

	QImage preview_image(image->rgba.data(), image->width, image->height, QImage::Format_RGBA8888);
	const int tile_size = image->height / 4;
	QIcon icon;
	icon.addPixmap(QPixmap::fromImage(preview_image.copy(0, 0, tile_size, tile_size)), QIcon::Normal, QIcon::Off);

	pending_textures[id] = { source, extension, icon, image->width, image->height };
	refresh_map_list(std::ranges::contains(to_ids, id) ? id : std::string());
	update_available_tiles();
	update_selected_info(id);
}

/// Copies the imported textures into the map folder under the stock tile's path, all or nothing. False (after telling the user) on failure.
bool TileSetter::write_pending_overrides() {
	const slk::SLK& slk = map->terrain.terrain_slk;
	std::vector<TileOverride> overrides;
	for (const auto& [id, pending] : pending_textures) {
		if (std::ranges::contains(to_ids, id)) {
			overrides.push_back({ id, slk.data("dir", id), slk.data("file", id), pending.extension, pending.source });
		}
	}

	const auto written = write_tile_overrides(tileset_policy(hierarchy.hd), hierarchy.map_directory, overrides);
	if (!written) {
		QString text = QString::fromStdString(written.error().message);
		if (written.error().leftovers.empty()) {
			text += "\n\nThe tileset was not changed.";
		} else {
			// The rollback could not put a previous override back: say where it is, so it is not lost.
			text += "\n\nThe tileset was not changed, but a previous texture file could not be restored. It is still here, rename it back by removing \".old.hivewe-tmp\" from its name:";
			for (const auto& leftover : written.error().leftovers) {
				text += "\n" + QString::fromStdWString(leftover.wstring());
			}
		}
		QMessageBox::critical(this, "Change Tileset", text);
		return false;
	}
	return true;
}

void TileSetter::qa_report_limits(const QString& out_file) {
	QStringList lines;
	const auto report = [&](const QString& state) {
		lines << QString("tile-setter-limits %1: tiles=%2 ok_enabled=%3 remove_enabled=%4")
					 .arg(state)
					 .arg(to_ids.size())
					 .arg(button_box->button(QDialogButtonBox::Ok)->isEnabled())
					 .arg(remove_button->isEnabled());
	};

	// One tile: a non-cliff tile (cliff tiles cannot be removed anyway), selected, so Remove is blocked only because it is the last tile.
	if (to_ids.size() > 1) {
		const auto plain = std::ranges::find_if(to_ids, [this](const std::string& id) { return !is_cliff_tile(id); });
		const std::string keep = plain != to_ids.end() ? *plain : to_ids.front();
		to_ids = { keep };
	}
	refresh_map_list(to_ids.empty() ? std::string() : to_ids.front());
	report("one-tile");

	to_ids.clear();
	refresh_map_list();
	report("empty-list");

	for (const QString& line : lines) {
		std::println("{}", line.toStdString());
	}
	if (!out_file.isEmpty()) {
		QFile file(out_file);
		if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
			file.write(lines.join(QChar('\n')).toUtf8() + "\n");
		}
		// The result is written: end the run like the browse-models hook does (a capture without --qa-script-out keeps the dialog open).
		QTimer::singleShot(0, qApp, &QCoreApplication::quit);
	}
}

void TileSetter::save_tiles() {
	if (to_ids.empty()) {
		return;
	}

	if (!write_pending_overrides()) {
		return;
	}

	const std::vector<int> from_to_id = build_tile_remap(map->terrain.tileset_ids, to_ids, replacements);

	std::vector<std::string> reloaded;
	for (const auto& [id, pending] : pending_textures) {
		if (std::ranges::contains(to_ids, id)) {
			reloaded.push_back(id);
		}
	}

	// The tile textures are created with GL calls, which need the viewport's context (this runs from a dialog slot).
	context->makeCurrent();
	map->terrain.change_tileset(to_ids, from_to_id, reloaded);
	// Undo snapshots of the terrain hold old texture slot indices, so the whole undo history ends here (like Map::resize).
	map->world_undo.clear_all_undo();
	close();
}
