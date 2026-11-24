#pragma once

#include <QDialog>
#include <QString>
#include <QIcon>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include "ui_tile_setter.h"

class QComboBox;
class QDialogButtonBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class QCheckBox;

class TileSetter : public QDialog {
	Q_OBJECT

public:
	explicit TileSetter(QWidget* parent = nullptr);

	/// QA hook only: adds the first addable tile and removes the last removable one, so a capture shows pending changes.
	void qa_apply_sample_changes();
	/// QA hook only: reduces the list to one tile, then to none, and prints the OK and Remove button states of both (also to `out_file`).
	void qa_report_limits(const QString& out_file);
	/// QA hook only: the OK path without clicks. Grows the tile list to `tile_count` tiles (when it is longer than the current list), imports
	/// `texture` through the same code as Import Custom Tile for the first kept non-cliff tile and for the first added tile, then runs the
	/// OK handler. Errors are returned in `error` instead of a message box (a modal box hangs an offscreen run). True when the map's
	/// tileset now equals the list.
	/// If an import fails, it returns at once with the earlier import still pending and the dialog open; that is fine because the hook exits next.
	/// `custom_tiles` receives the ids of the tiles that got the texture.
	bool qa_accept_with_custom_tiles(const std::filesystem::path& texture, std::size_t tile_count, QString& error, std::vector<std::string>& custom_tiles);

	/// QA hook only: sets the tile list to `ids`, optionally imports `texture` for the first non-cliff tile, then runs the OK handler. Errors go to
	/// `error` (no message box). True when the map's tileset now equals `ids`.
	bool qa_apply_list(const std::vector<std::string>& ids, const std::filesystem::path& texture, QString& error);

private:
	/// Validates and decodes a texture file for tile `id` and records it as pending. False with the reason in `error`.
	bool import_texture_file(const std::string& id, const std::filesystem::path& source, QString& error);
	void add_tile();
	void remove_tile();
	void move_tile(int delta);
	void update_available_tiles();
	void refresh_map_list(const std::string& select_id = {});
	void update_buttons();
	void update_selected_info(const std::string& id);
	bool is_cliff_tile(const std::string& id) const;
	QIcon tile_icon(const std::string& id) const;
	void import_custom_tile();
	void save_tiles();
	bool write_pending_overrides();

	Ui::TileSetter ui;

	QComboBox* tileset_combo = nullptr;
	QTreeWidget* available_list = nullptr;
	QTreeWidget* map_list = nullptr;
	QLabel* count_label = nullptr;
	QPushButton* add_button = nullptr;
	QPushButton* remove_button = nullptr;
	QPushButton* up_button = nullptr;
	QPushButton* down_button = nullptr;
	QPushButton* import_button = nullptr;
	QDialogButtonBox* button_box = nullptr;

	QLabel* preview_large = nullptr;
	QLabel* preview_name = nullptr;
	QLabel* preview_id = nullptr;
	QLabel* preview_file = nullptr;
	QLabel* preview_size = nullptr;

	mutable std::map<std::string, QIcon> icon_cache;

	/// A texture picked with Import Custom Tile, not yet written to the map folder (that happens on OK).
	struct PendingTexture {
		std::filesystem::path source;
		std::string extension;
		QIcon icon;
		int width = 0;
		int height = 0;
	};
	/// By tile id: the tiles whose texture the map will override.
	std::map<std::string, PendingTexture> pending_textures;
	/// The tile shown in the Selected Tile group, the target of Import Custom Tile.
	std::string selected_id;

	/// The new tileset, in slot order.
	std::vector<std::string> to_ids;
	/// For each removed tile, the kept tile its cells are repainted with.
	std::map<std::string, std::string> replacements;
	/// Set by the QA hook: write_pending_overrides stores its error text here instead of opening a message box.
	QString* qa_error = nullptr;
};
