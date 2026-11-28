#include "HiveWE.h"
#include "ui_HiveWE.h"
#include "new_map_dialog.h"
#include "shortcut_config_dialog.h"
#include "unit_properties_dialog.h"
#include "item_drop_editor.h"
#include "theme.h"
#define __STORMLIB_NO_STATIC_LINK__
#include "StormLib.h"

import std;
import Hierarchy;
import BinaryReader;
import MDX;
import MdxMergeExport;
import MPQ;
import Camera;
import Globals;
import Map;
import ModificationTables;
import Imports;
import <soil2/SOIL2.h>;
import MapGlobal;
import WorldUndoManager;
import Triggers;
import LuaSyntax;
import TableModel;
import UnitsUndo;
import SkinnedMeshGlobals;
import ResourceManager;
import WindowHandler;
import "pathing_palette.h";
import "object_editor/object_editor.h";
import "model_editor/browse_models_dialog.h";
import "model_editor/model_editor.h";
import "model_editor/model_editor_glwidget.h";
import "tile_setter.h";
import "map_info_editor.h";
import "terrain_palette.h";
import "settings_editor.h";
import "tile_pather.h";
import "palette.h";
import "terrain_palette.h";
import "doodad_palette.h";
import "unit_palette.h";
import "region_palette.h";
import "selection_properties_panel.h";
import "object_editor/icon_view.h";
import "trigger_editor.h";
#include "QMessageBox"
#include "QProcess"
#include "QKeySequence"
#include "QString"
#include <QAccessible>
#include <QApplication>
#include <QGuiApplication>
#include <QComboBox>
#include <QFile>
#include <QScrollBar>
#include <QTextStream>
#include <QScreen>
#include <QScrollArea>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStyle>
#include <QFileInfo>
#include <QMenu>
#include <QRegularExpression>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QTabWidget>
#include <QTimer>
#include <QCheckBox>
#include <QLineEdit>
#include <QListView>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTableView>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTextEdit>
#include <QStandardPaths>
#include <QDateTime>
import "menus/gameplay_constants_editor.h";
import "menus/item_tables_editor.h";
import "menus/scenario_properties_editor.h";
import "asset_manager/asset_manager.h";
import "tooltip_editor/tooltip_editor.h";
import "map_protector/map_protector.h";
#include "object_data_import_dialog.h"
import ObjectDataIo;
import ScenarioDataIo;
import MapInfo;
import MapBodyCheck;
import MapFormatCheck;
import MapArchiveCheck;
import ScriptPreservation;
import ProtectionPipeline;
import MapBackup;
import MapArchiveExport;

namespace fs = std::filesystem;

namespace {
/// A path as UTF-8 text. path::string() throws on MSVC for characters outside the system code page (CJK or Cyrillic map folders).
std::string utf8_path(const fs::path& path) {
	const std::u8string text = path.u8string();
	return std::string(text.begin(), text.end());
}

/// A path for Qt display or settings. Never use QString::fromStdString(path.string()) on a map path.
QString display_path(const fs::path& path) {
	return QString::fromStdWString(path.wstring());
}

/// The system's text for a Windows error code, without the trailing line break.
QString windows_error_text(const std::uint32_t code) {
	return QString::fromLocal8Bit(std::system_category().message(static_cast<int>(code))).trimmed();
}

/// Export MPQ's result dialog for map_archive::write. `script_not_updated`: the save before the export could not generate the map
/// script, so the archive holds the script that was in the folder before (or none).
void show_export_result(QWidget* parent, const fs::path& folder, const fs::path& target, const map_archive::Result& result,
						const bool script_not_updated) {
	using map_archive::Step;
	const QString target_text = display_path(target);
	if (result.ok) {
		const QString written = "The map was exported to\n" + target_text + "\n\n" + QString::number(result.added) + " files were added to the archive.";
		if (!script_not_updated) {
			QMessageBox::information(parent, "Export MPQ", written);
			return;
		}
		std::error_code ec;
		const bool has_script = std::ranges::any_of(map_script_file_names, [&](const std::string_view name) {
			return fs::exists(folder / fs::path(name), ec);
		});
		QMessageBox::warning(parent, "Export MPQ",
							 written + "\n\nThe map script in it is not up to date. "
								 + (has_script ? "The archive contains the previous map script, because the new one could not be generated."
											   : "The archive contains no map script: the map had none before, and the new one could not be generated.")
								 + "\n\nFix the script errors shown while saving, then export again.");
		return;
	}

	if (map_archive::is_refusal(result.step)) {
		QString reason;
		if (result.step == Step::inside_map_folder) {
			reason = "The file\n" + target_text + "\nis inside the map's own folder, so the archive would pack itself.\n\nChoose a location outside\n"
				+ display_path(folder);
		} else if (result.step == Step::target_is_folder) {
			reason = "The path\n" + target_text + "\nis a folder. Choose a file name.";
		} else if (result.step == Step::target_read_only) {
			reason = "The existing file\n" + target_text
				+ "\nis read-only, so it cannot be replaced. It was kept unchanged.\n\nClear its read-only attribute, or choose another file name.";
		} else {
			reason = "The path\n" + target_text + "\nis too long: the archive's temporary files would pass the Windows limit of "
				+ QString::number(map_archive::max_path_length) + " characters. Choose a shorter path or file name.";
		}
		QMessageBox::warning(parent, "Export MPQ", "The map was not exported. Nothing was written.\n\n" + reason);
		return;
	}

	fs::path temp = target;
	temp += map_archive::temp_suffix;
	QString what;
	switch (result.step) {
		case Step::check_target: what = "The target location could not be checked."; break;
		case Step::list_folder: what = "The map folder could not be read completely, so no archive was made."; break;
		case Step::stale_temp: what = "A temporary file left by an earlier export could not be removed:\n" + display_path(temp); break;
		case Step::create: what = "The archive could not be created."; break;
		case Step::add: what = QString::number(result.failed.size()) + " of the map's files could not be added to the archive."; break;
		case Step::nothing_added: what = "The map folder has no files to add."; break;
		case Step::flush: what = "The archive's file list and attributes could not be written."; break;
		case Step::compact: what = "The archive could not be compacted."; break;
		case Step::close: what = "The archive could not be finished (closing it failed)."; break;
		case Step::verify: what = "The finished archive could not be read back, or files were missing from it."; break;
		case Step::replace:
			what = result.target_existed ? "The new archive could not replace the existing file. Another program may have it open."
										 : "The new archive could not be moved to its place.";
			break;
		default: what = "The export failed."; break;
	}

	QString text = "The map could not be exported to\n" + target_text + "\n\n" + what;
	if (result.error != 0) {
		text += "\n\nWindows error " + QString::number(result.error) + ": " + windows_error_text(result.error);
	}
	text += result.target_existed ? "\n\nThe existing file was kept unchanged." : "\n\nNo file was written there.";
	if (!result.leftover.empty()) {
		text += "\n\nA temporary file could not be deleted:\n" + display_path(result.leftover);
	}

	QStringList names;
	for (const map_archive::FailedFile& file : result.failed) {
		names << QString::fromUtf8(file.name.data(), static_cast<qsizetype>(file.name.size())) + " (error " + QString::number(file.error) + ": "
				+ windows_error_text(file.error) + ")";
	}
	constexpr qsizetype shown = 10;
	if (!names.isEmpty()) {
		text += "\n\nFiles:\n" + names.mid(0, shown).join('\n');
		if (names.size() > shown) {
			text += QString("\n... and %1 more (see Show Details)").arg(names.size() - shown);
		}
	}

	QMessageBox box(QMessageBox::Critical, "Exporting failed", text, QMessageBox::Ok, parent);
	if (names.size() > shown) {
		box.setDetailedText(names.join('\n'));
	}
	box.exec();
}

/// Backups made before editor saves (see MapBackup). Headless --resave and the Map Protector call Map::save directly and make none.
struct SaveBackupState {
	/// The first failed backup of this editor session showed its dialog; later ones only show in the status text.
	bool warned = false;
	/// The backup before the latest save failed.
	bool last_failed = false;
};
SaveBackupState save_backup_state;

/// Identifies this editor session in the backups, so the first backup of each session is kept longer.
const std::string& editor_session_id() {
	static const std::string id = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz").toStdString() + "-"
		+ std::to_string(QCoreApplication::applicationPid());
	return id;
}

map_backup::LocalTime local_now() {
	const QDateTime now = QDateTime::currentDateTime();
	const QDate date = now.date();
	const QTime time = now.time();
	return std::chrono::local_days(std::chrono::year(date.year()) / date.month() / date.day()) + std::chrono::hours(time.hour())
		+ std::chrono::minutes(time.minute()) + std::chrono::seconds(time.second()) + std::chrono::milliseconds(time.msec());
}

/// Where backups go. QA offscreen runs never use the user's AppData: the hook's folder, or a temporary folder removed at exit. Empty when
/// none is available.
fs::path backup_root() {
	if (HiveWE::qa_offscreen) {
		if (!HiveWE::qa_backup_root.isEmpty()) {
			return fs::path(HiveWE::qa_backup_root.toStdWString());
		}
		static QTemporaryDir qa_root(QDir::tempPath() + "/HiveWE-qa-backups-XXXXXX");
		return qa_root.isValid() ? fs::path(qa_root.path().toStdWString()) : fs::path();
	}
	const QString app_data = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
	return app_data.isEmpty() ? fs::path() : fs::path(app_data.toStdWString()) / "backups";
}

/// Backs up the files Save is about to replace in `map_folder`. Never throws and never stops the save: the first failure of the session
/// shows a warning with the reason and the folder, later ones only set save_backup_state.last_failed. Returns the set that holds the
/// files from before this save (new or identical), or empty.
fs::path back_up_before_save(const fs::path& map_folder) {
	save_backup_state.last_failed = false;
	std::expected<map_backup::Result, map_backup::Error> result = std::unexpected(map_backup::Error { "No folder for backups is available.", {} });
	try {
		const fs::path root = backup_root();
		if (!root.empty()) {
			const int keep = QSettings().value("backupKeepCount", 10).toInt();
			result = map_backup::back_up(map_folder, {
				.root = root,
				.now = local_now(),
				.session_id = editor_session_id(),
				.keep = static_cast<size_t>(keep > 0 ? keep : 10),
			});
		}
	} catch (const std::exception& e) {
		result = std::unexpected(map_backup::Error { std::string("The backup failed: ") + e.what(), {} });
	} catch (...) {
		result = std::unexpected(map_backup::Error { "The backup failed (unknown error).", {} });
	}
	if (result) {
		return result->set;
	}

	save_backup_state.last_failed = true;
	const QString reason = QString::fromUtf8(result.error().message.data(), static_cast<qsizetype>(result.error().message.size()));
	const QString folder = display_path(result.error().folder);
	std::println(stderr, "Backup before saving failed: {} (backup folder: {})", reason.toStdString(), folder.toStdString());
	if (!save_backup_state.warned) {
		save_backup_state.warned = true;
		if (!map->headless) {
			QMessageBox::warning(nullptr, "Backup Failed",
								 "No backup of the map's files could be made before this save. Saving continues without one.\n\n" + reason
									 + "\n\nBackup folder:\n" + (folder.isEmpty() ? QString("(none)") : folder)
									 + "\n\nIf later saves in this session cannot make a backup either, only the status text says so.");
		}
	}
	return {};
}

/// Map::save() with any exception shown in a dialog, so a throw while saving cannot close the editor without a message. Backs up the
/// files of `path` first.
bool save_map_reporting_errors(const fs::path& path) {
	const fs::path backup_set = back_up_before_save(path);
	try {
		const bool saved = map->save(path);
		if (saved) {
			map->world_undo.mark_saved();
			map->remember_saved_state();
		}
		return saved;
	} catch (const std::exception& e) {
		QString text = "The map could not be saved completely:\n" + QString::fromLocal8Bit(e.what())
			+ "\n\nSome map files may already have been written.";
		if (!backup_set.empty()) {
			text += "\n\nThe map files from before this save are in:\n" + display_path(backup_set);
		}
		QMessageBox::critical(nullptr, "Saving failed", text);
		return false;
	}
}

std::shared_ptr<mdx::MDX> load_preview_mdx(const fs::path& path) {
	auto result = hierarchy.open_file(path);
	if (!result) {
		std::println("Preview model load failed for {} ({})", path.string(), result.error());
		return nullptr;
	}

	return std::make_shared<mdx::MDX>(result.value());
}
}

HiveWE::HiveWE(QWidget* parent)
	: QMainWindow(parent), ui(std::make_unique<Ui::HiveWEClass>()) {
	setAutoFillBackground(true);

	// Buggy as of Qt 6.9.1. Likely requires 6.9.2 or later
	// setWindowFlag(Qt::ExpandedClientAreaHint, true);
	// setWindowFlag(Qt::NoTitleBarBackgroundHint, true);
	// setAttribute(Qt::WA_LayoutOnEntireRect, true);
	ui->setupUi(this);
	context = ui->widget;
	setup_palette_sidebar();

	transient_notice = new QLabel(this);
	transient_notice->hide();
	transient_notice->setAttribute(Qt::WA_TransparentForMouseEvents);
	transient_notice->setAlignment(Qt::AlignCenter);
	transient_notice->setStyleSheet(
		"QLabel {"
		"background-color: rgba(20, 24, 30, 228);"
		"color: rgb(244, 247, 250);"
		"border: 1px solid rgba(255, 255, 255, 52);"
		"border-radius: 10px;"
		"padding: 18px 32px;"
		"font-size: 24px;"
		"font-weight: 700;"
		"}"
	);
	transient_notice_timer.setSingleShot(true);
	connect(&transient_notice_timer, &QTimer::timeout, transient_notice, &QLabel::hide);

	connect(ui->ribbon->undo, &QPushButton::clicked, [&]() {
		// ToDo: temporary, undoing should still allow a selection to persist
		if (map->brush) {
			map->brush->clear_selection();
		}

		auto context = WorldEditContext {
			.terrain = map->terrain,
			.units = map->units,
			.doodads = map->doodads,
			.regions = map->regions,
			.brush = map->brush,
			.pathing_map = map->pathing_map,
		};

		map->world_undo.undo(context);
	});
	connect(ui->ribbon->redo, &QPushButton::clicked, [&]() {
		// ToDo: temporary, undoing should still allow a selection to persist
		if (map->brush) {
			map->brush->clear_selection();
		}

		auto context = WorldEditContext {
			.terrain = map->terrain,
			.units = map->units,
			.doodads = map->doodads,
			.regions = map->regions,
			.brush = map->brush,
			.pathing_map = map->pathing_map,
		};

		map->world_undo.redo(context);
	});

	connect(new QShortcut(Qt::CTRL | Qt::Key_Z, this), &QShortcut::activated, ui->ribbon->undo, &QPushButton::click);
	connect(new QShortcut(Qt::CTRL | Qt::Key_Y, this), &QShortcut::activated, ui->ribbon->redo, &QPushButton::click);

	connect(ui->ribbon->units_visible, &QPushButton::toggled, [](bool checked) { map->render_units = checked; });
	connect(ui->ribbon->doodads_visible, &QPushButton::toggled, [](bool checked) { map->render_doodads = checked; });
	connect(ui->ribbon->pathing_visible, &QPushButton::toggled, [](bool checked) { map->render_pathing = checked; });
	connect(ui->ribbon->brush_visible, &QPushButton::toggled, [](bool checked) { map->render_brush = checked; });
	connect(ui->ribbon->lighting_visible, &QPushButton::toggled, [](bool checked) { map->render_lighting = checked; });
	connect(ui->ribbon->water_visible, &QPushButton::toggled, [](bool checked) { map->render_water = checked; });
	connect(ui->ribbon->click_helpers_visible, &QPushButton::toggled, [](bool checked) { map->render_click_helpers = checked; });
	connect(ui->ribbon->wireframe_visible, &QPushButton::toggled, [](bool checked) { map->render_wireframe = checked; });
	connect(ui->ribbon->debug_visible, &QPushButton::toggled, [](bool checked) { map->render_debug = checked; });
	connect(ui->ribbon->minimap_visible, &QPushButton::toggled, [&](bool checked) {
		(checked) ? minimap->show() : minimap->hide();
		if (sidebar_minimap_frame) {
			sidebar_minimap_frame->setVisible(checked);
		}
		if (flat_minimap_dock) {
			flat_minimap_dock->setVisible(checked);
		}
	});

	connect(new QShortcut(Qt::CTRL | Qt::Key_U, this), &QShortcut::activated, ui->ribbon->units_visible, &QPushButton::click);
	connect(new QShortcut(Qt::CTRL | Qt::Key_D, this), &QShortcut::activated, ui->ribbon->doodads_visible, &QPushButton::click);
	connect(new QShortcut(Qt::CTRL | Qt::Key_P, this), &QShortcut::activated, ui->ribbon->pathing_visible, &QPushButton::click);
	connect(new QShortcut(Qt::CTRL | Qt::Key_L, this), &QShortcut::activated, ui->ribbon->lighting_visible, &QPushButton::click);
	connect(new QShortcut(Qt::CTRL | Qt::Key_W, this), &QShortcut::activated, ui->ribbon->water_visible, &QPushButton::click);
	connect(new QShortcut(Qt::CTRL | Qt::Key_I, this), &QShortcut::activated, ui->ribbon->click_helpers_visible, &QPushButton::click);
	connect(new QShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_W, this), &QShortcut::activated, ui->ribbon->wireframe_visible, &QPushButton::click);
	connect(new QShortcut(Qt::CTRL | Qt::Key_T, this), &QShortcut::activated, ui->ribbon->test_map, &QToolButton::click);
	connect(new QShortcut(Qt::Key_F3, this), &QShortcut::activated, ui->ribbon->debug_visible, &QPushButton::click);

	// Reload theme
	connect(new QShortcut(Qt::Key_F5, this), &QShortcut::activated, [&]() {
		if (const QString error = theme::apply(theme::current()); !error.isEmpty()) {
			std::println("Failed to reload the theme: {}", error.toStdString());
			return;
		}
		apply_theme_chrome();
	});

	connect(ui->ribbon->reset_camera, &QPushButton::clicked, [&]() { camera.reset(); });
	connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C), this), &QShortcut::activated, ui->ribbon->reset_camera, &QPushButton::click);

	connect(ui->ribbon->import_heightmap, &QPushButton::clicked, this, &HiveWE::import_heightmap);

	connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_O), this, nullptr, nullptr, Qt::ApplicationShortcut), &QShortcut::activated, ui->ribbon->open_map_folder, &QToolButton::click);
	// connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_I), this, nullptr, nullptr, Qt::ApplicationShortcut), &QShortcut::activated, ui->ribbon->open_map_mpq, &QPushButton::click);
	connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_S), this, nullptr, nullptr, Qt::ApplicationShortcut), &QShortcut::activated, ui->ribbon->save_map, &QToolButton::click);
	connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), this, nullptr, nullptr, Qt::ApplicationShortcut), &QShortcut::activated, ui->ribbon->save_map_as, &QToolButton::click);

	connect(ui->ribbon->new_map, &QToolButton::clicked, this, &HiveWE::create_new_map);
	connect(ui->ribbon->open_map_folder, &QToolButton::clicked, this, &HiveWE::load_folder);
	connect(ui->ribbon->open_map_mpq, &QToolButton::clicked, this, &HiveWE::load_mpq);
	for (int i = 0; i < static_cast<int>(ui->ribbon->recent_maps.size()); ++i) {
		connect(ui->ribbon->recent_maps[i], &QToolButton::clicked, this, [this, i]() { open_recent_map(i); });
	}
	connect(ui->ribbon->save_map, &QToolButton::clicked, this, &HiveWE::save);
	connect(ui->ribbon->save_map_as, &QToolButton::clicked, this, &HiveWE::save_as);
	connect(ui->ribbon->export_mpq, &QToolButton::clicked, this, &HiveWE::export_mpq);
	connect(ui->ribbon->test_map, &QToolButton::clicked, this, &HiveWE::play_test);
	connect(ui->ribbon->quick_search, &QToolButton::clicked, this, [this]() { new GlobalSearchWidget(this); });
	connect(ui->ribbon->settings, &QToolButton::clicked, [&]() {
		connect(new SettingsEditor(this), &QDialog::accepted, this, &HiveWE::apply_theme_chrome);
	});
	connect(ui->ribbon->config, &QToolButton::clicked, [&]() { new ShortcutConfigDialog(this); });

	auto* export_menu = new QMenu(this);
	export_menu->addAction("Binary package", this, &HiveWE::export_object_data_binary);
	export_menu->addAction("Text package", this, &HiveWE::export_object_data_text);
	ui->ribbon->export_objects->setMenu(export_menu);
	ui->ribbon->export_objects->setPopupMode(QToolButton::InstantPopup);

	auto* import_menu = new QMenu(this);
	import_menu->addAction("Binary package", this, &HiveWE::import_object_data_binary);
	import_menu->addAction("Map folder", this, &HiveWE::import_object_data_map_folder);
	import_menu->addAction("Text package", this, &HiveWE::import_object_data_text);
	ui->ribbon->import_objects->setMenu(import_menu);
	ui->ribbon->import_objects->setPopupMode(QToolButton::InstantPopup);

	connect(ui->ribbon->switch_warcraft, &QToolButton::clicked, this, &HiveWE::switch_warcraft);
	// close() first, so closeEvent can ask about unsaved changes.
	connect(ui->ribbon->exit, &QToolButton::clicked, this, [this]() {
		if (close()) {
			QApplication::exit();
		}
	});

	connect(ui->ribbon->change_tileset, &QRibbonButton::clicked, [this]() { new TileSetter(this); });
	connect(ui->ribbon->change_tile_pathing, &QRibbonButton::clicked, [this]() { new TilePather(this); });

	connect(ui->ribbon->map_description, &QRibbonButton::clicked, [&]() { (new MapInfoEditor(this))->ui.tabs->setCurrentIndex(0); });
	connect(ui->ribbon->map_loading_screen, &QRibbonButton::clicked, [&]() { (new MapInfoEditor(this))->ui.tabs->setCurrentIndex(1); });
	connect(ui->ribbon->map_options, &QRibbonButton::clicked, [&]() { (new MapInfoEditor(this))->ui.tabs->setCurrentIndex(2); });
	// connect(ui, &QAction::triggered, [&]() { (new MapInfoEditor(this))->ui.tabs->setCurrentIndex(3); });

	ui->ribbon->terrain_palette->setCheckable(true);
	ui->ribbon->doodad_palette->setCheckable(true);
	ui->ribbon->unit_palette->setCheckable(true);
	ui->ribbon->region_palette->setCheckable(true);
	ui->ribbon->pathing_palette->setCheckable(true);
	connect(ui->ribbon->pathing_palette, &QRibbonButton::clicked, this, &HiveWE::toggle_pathing_sidebar);
	connect(ui->ribbon->region_palette, &QRibbonButton::clicked, this, &HiveWE::toggle_region_palette);

	connect(new QShortcut(QKeySequence(Qt::Key_T), this, nullptr, nullptr, Qt::WindowShortcut), &QShortcut::activated, this, &HiveWE::toggle_terrain_sidebar);
	connect(ui->ribbon->terrain_palette, &QRibbonButton::clicked, this, &HiveWE::toggle_terrain_sidebar);

	connect(new QShortcut(QKeySequence(Qt::Key_D), this, nullptr, nullptr, Qt::WindowShortcut), &QShortcut::activated, this, &HiveWE::toggle_doodad_palette);
	connect(ui->ribbon->doodad_palette, &QRibbonButton::clicked, this, &HiveWE::toggle_doodad_palette);

	connect(new QShortcut(QKeySequence(Qt::Key_U), this, nullptr, nullptr, Qt::WindowShortcut), &QShortcut::activated, this, &HiveWE::toggle_unit_palette);
	connect(ui->ribbon->unit_palette, &QRibbonButton::clicked, this, &HiveWE::toggle_unit_palette);

	connect(new QShortcut(QKeySequence(Qt::Key_P), this, nullptr, nullptr, Qt::WindowShortcut), &QShortcut::activated, this, &HiveWE::toggle_pathing_sidebar);
	connect(new QShortcut(QKeySequence(Qt::Key_R), this, nullptr, nullptr, Qt::WindowShortcut), &QShortcut::activated, this, &HiveWE::toggle_region_palette);

	connect(ui->ribbon->trigger_editor, &QRibbonButton::clicked, [this]() {
		// The button is disabled then (apply_trigger_editing_state); this covers a click that gets here anyway.
		if (map && map->keeps_script) {
			return;
		}
		bool created = false;
		window_handler.create_or_raise<TriggerEditor>(nullptr, created);
	});

	connect(ui->ribbon->object_editor, &QRibbonButton::clicked, [this]() {
		bool created = false;
		window_handler.create_or_raise<ObjectEditor>(nullptr, created);
	});
	connect(new QShortcut(QKeySequence(Qt::Key_O), this, nullptr, nullptr, Qt::ApplicationShortcut), &QShortcut::activated, [this]() {
		if (const auto open_editor = window_handler.get_open<ObjectEditor>(); open_editor.has_value() && *open_editor) {
			(*open_editor)->close();
			return;
		}

		bool created = false;
		window_handler.create_or_raise<ObjectEditor>(nullptr, created);
	});

	connect(ui->ribbon->model_editor, &QRibbonButton::clicked, [this]() {
		bool created = false;
		window_handler.create_or_raise<ModelEditor>(nullptr, created);
	});

	connect(ui->ribbon->gameplay_constants, &QRibbonButton::clicked, [this]() {
		bool created = false;
		window_handler.create_or_raise<GameplayConstantsEditor>(nullptr, created);
	});

	connect(ui->ribbon->item_tables, &QRibbonButton::clicked, [this]() {
		bool created = false;
		window_handler.create_or_raise<ItemTablesEditor>(nullptr, created);
	});

	connect(ui->ribbon->scenario_properties, &QRibbonButton::clicked, [this]() {
		bool created = false;
		window_handler.create_or_raise<ScenarioPropertiesEditor>(nullptr, created);
	});

	connect(ui->ribbon->asset_manager, &QRibbonButton::clicked, [this]() {
		bool created = false;
		window_handler.create_or_raise<AssetManager>(nullptr, created);
	});

	connect(ui->ribbon->tooltip_editor, &QRibbonButton::clicked, [this]() {
		bool created = false;
		window_handler.create_or_raise<TooltipEditor>(nullptr, created);
	});

	connect(ui->ribbon->map_protector, &QRibbonButton::clicked, [this]() {
		bool created = false;
		window_handler.create_or_raise<MapProtector>(nullptr, created);
	});

	setup_flat_chrome();
	restore_window_state();
	update_recent_maps_menu();
	apply_theme_chrome();

	connect(minimap, &Minimap::clicked, [](QPointF location) { camera.position = { location.x() * map->terrain.width, (1.0 - location.y()) * map->terrain.height, camera.position.z }; });
	ui->widget->makeCurrent();

	map = new Map();
	connect(&map->terrain, &Terrain::minimap_changed, minimap, &Minimap::set_minimap);
	connect(&map->terrain, &Terrain::tileset_changed, [&]() {
		if (terrain_palette) {
			terrain_palette->refresh();
		}
	});

	map->render_manager.resize_framebuffers(ui->widget->width(), ui->widget->height());
}

HiveWE::~HiveWE() = default;

void HiveWE::setup_palette_sidebar() {
	auto* main_layout = qobject_cast<QVBoxLayout*>(centralWidget()->layout());
	main_layout->removeWidget(ui->widget);

	content_layout = new QHBoxLayout;
	content_layout->setContentsMargins(0, 0, 0, 0);
	content_layout->setSpacing(0);
	content_layout->addWidget(ui->widget, 1);
	main_layout->addLayout(content_layout);

	sidebar_root = new QWidget(centralWidget());
	sidebar_root->setObjectName("paletteSidebar");
	sidebar_root->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
	sidebar_root->setFixedWidth(400);
	sidebar_legacy_stylesheet = QString(
		"#paletteSidebar { background: rgba(20, 23, 27, 236); border-left: 1px solid rgba(255, 255, 255, 18); }"
		"QWidget#paletteSidebarHeader { background: transparent; }"
		"QWidget#paletteSidebarModeBar { background: transparent; }"
		"QToolButton[sidebarMode=\"true\"] {"
		"background: rgba(37, 41, 47, 224);"
		"border: 1px solid rgba(255, 255, 255, 16);"
		"border-radius: 11px;"
		"color: rgb(192, 199, 208);"
		"font-size: 12px;"
		"font-weight: 600;"
		"padding: 7px 10px;"
		"text-align: left;"
		"}"
		"QToolButton[sidebarMode=\"true\"]:checked {"
		"background: rgba(70, 132, 214, 210);"
		"border: 1px solid rgba(95, 164, 255, 240);"
		"color: white;"
		"}"
		"QLabel[sidebarTitle=\"true\"] { font-size: 24px; font-weight: 700; color: rgb(241, 244, 247); }"
		"QLabel[sidebarDescription=\"true\"] { font-size: 13px; color: rgb(178, 187, 198); }"
		"QFrame[sidebarBody=\"true\"] { background: rgba(33, 37, 43, 230); border: 1px solid rgba(255, 255, 255, 16); border-radius: 16px; }"
		"QFrame[sidebarPaletteHost=\"true\"] { background: transparent; border: 0; }"
		"QFrame[sidebarUtilityPanel=\"true\"] { background: rgba(24, 27, 31, 200); border: 1px solid rgba(255, 255, 255, 14); border-radius: 12px; }"
		"QLabel[sidebarPreviewTitle=\"true\"] { color: rgb(241, 244, 247); font-size: 13px; font-weight: 700; }"
		"QLabel[sidebarPreviewPlaceholder=\"true\"] { color: rgb(132, 141, 153); font-size: 12px; }"
		"QLabel[sidebarHint=\"true\"] { font-size: 11px; color: rgb(132, 141, 153); padding-top: 2px; }"
		"#paletteSidebar QLineEdit, #paletteSidebar QComboBox, #paletteSidebar QSpinBox {"
		"background: rgba(24, 27, 31, 235);"
		"border: 1px solid rgba(255, 255, 255, 18);"
		"border-radius: 10px;"
		"padding: 6px 10px;"
		"color: rgb(232, 236, 241);"
		"selection-background-color: rgba(95, 164, 255, 210);"
		"}"
		"#paletteSidebar QLineEdit:focus, #paletteSidebar QComboBox:focus, #paletteSidebar QSpinBox:focus {"
		"border: 1px solid rgba(95, 164, 255, 235);"
		"}"
		"#paletteSidebar QListView, #paletteSidebar QListWidget {"
		"background: rgba(24, 27, 31, 210);"
		"border: 1px solid rgba(255, 255, 255, 14);"
		"border-radius: 12px;"
		"padding: 4px;"
		"color: rgb(232, 236, 241);"
		"outline: 0;"
		"}"
		"#paletteSidebar QListView::item, #paletteSidebar QListWidget::item {"
		"padding: 6px 8px;"
		"border-radius: 8px;"
		"}"
		"#paletteSidebar QListView::item:selected, #paletteSidebar QListWidget::item:selected {"
		"background: rgba(95, 164, 255, 210);"
		"color: white;"
		"}"
		"#paletteSidebar QPushButton, #paletteSidebar QToolButton, #paletteSidebar QRadioButton, #paletteSidebar QCheckBox {"
		"color: rgb(229, 233, 239);"
		"}"
		"#paletteSidebar QSlider::groove:horizontal {"
		"height: 6px;"
		"background: rgba(255, 255, 255, 20);"
		"border-radius: 999px;"
		"}"
		"#paletteSidebar QSlider::handle:horizontal {"
		"width: 16px;"
		"margin: -5px 0;"
		"background: rgb(95, 164, 255);"
		"border-radius: 8px;"
		"}"
		"#paletteSidebar QGroupBox {"
		"border: 0;"
		"margin-top: 6px;"
		"font-weight: 700;"
		"color: rgb(214, 221, 229);"
		"}"
		"#paletteSidebar QLabel { color: rgb(214, 221, 229); }"
	);
	sidebar_root->setStyleSheet(sidebar_legacy_stylesheet);

	auto* sidebar_layout = new QVBoxLayout(sidebar_root);
	sidebar_layout->setContentsMargins(12, 12, 12, 12);
	sidebar_layout->setSpacing(12);

	sidebar_mode_bar = new QWidget(sidebar_root);
	sidebar_mode_bar->setObjectName("paletteSidebarModeBar");
	auto* mode_layout = new QGridLayout(sidebar_mode_bar);
	mode_layout->setContentsMargins(0, 0, 0, 0);
	mode_layout->setSpacing(8);
	mode_layout->setHorizontalSpacing(8);
	mode_layout->setVerticalSpacing(8);

	auto create_mode_button = [this, mode_layout](const QString& text, const QIcon& icon, const auto slot, const int row, const int column) {
		auto* button = new QToolButton(sidebar_mode_bar);
		button->setProperty("sidebarMode", true);
		button->setCheckable(true);
		button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		button->setIcon(icon);
		button->setText(text);
		button->setAutoRaise(false);
		button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		button->setMinimumHeight(38);
		button->setToolTip(text);
		mode_layout->addWidget(button, row, column);
		// toggled, not clicked: an accessibility Toggle (UIA) calls toggle() and emits no clicked signal. update_active_palette_visuals() sets the
		// state with the signals blocked, so a real click still runs the slot exactly once.
		connect(button, &QToolButton::toggled, this, [this, slot]() { (this->*slot)(); });
		return button;
	};

	terrain_mode_button = create_mode_button("Terrain", QIcon("data/icons/ribbon/heightmap.png"), &HiveWE::toggle_terrain_sidebar, 0, 0);
	doodad_mode_button = create_mode_button("Doodads", QIcon("data/icons/ribbon/doodads.png"), &HiveWE::toggle_doodad_palette, 0, 1);
	unit_mode_button = create_mode_button("Units", QIcon("data/icons/ribbon/units.png"), &HiveWE::toggle_unit_palette, 0, 2);
	pathing_mode_button = create_mode_button("Pathing", QIcon("data/icons/ribbon/pathing.png"), &HiveWE::toggle_pathing_sidebar, 1, 0);
	region_mode_button = create_mode_button("Regions", QIcon("data/icons/ribbon/select.png"), &HiveWE::toggle_region_palette, 1, 1);
	mode_layout->setColumnStretch(0, 1);
	mode_layout->setColumnStretch(1, 1);
	mode_layout->setColumnStretch(2, 1);

	sidebar_minimap_frame = new QFrame(sidebar_root);
	sidebar_minimap_frame->setProperty("sidebarUtilityPanel", true);
	sidebar_minimap_frame->setFixedHeight(196);
	sidebar_minimap_frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	auto* minimap_layout = new QVBoxLayout(sidebar_minimap_frame);
	minimap_layout->setContentsMargins(8, 8, 8, 8);
	minimap_layout->setSpacing(0);
	minimap->setParent(sidebar_minimap_frame);
	minimap->setFixedHeight(180);
	minimap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	minimap_layout->addWidget(minimap);
	sidebar_minimap_frame->setVisible(ui->ribbon->minimap_visible->isChecked());
	minimap->setVisible(ui->ribbon->minimap_visible->isChecked());

	sidebar_preview_frame = new QFrame(sidebar_root);
	sidebar_preview_frame->setProperty("sidebarUtilityPanel", true);
	auto* preview_layout = new QVBoxLayout(sidebar_preview_frame);
	preview_layout->setContentsMargins(8, 8, 8, 8);
	preview_layout->setSpacing(6);

	sidebar_preview_title = new QLabel("Model Preview", sidebar_preview_frame);
	sidebar_preview_title->setProperty("sidebarPreviewTitle", true);
	sidebar_preview_title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	preview_layout->addWidget(sidebar_preview_title);

	sidebar_preview_placeholder = new QLabel("Select a doodad or unit to preview its model.", sidebar_preview_frame);
	sidebar_preview_placeholder->setProperty("sidebarPreviewPlaceholder", true);
	sidebar_preview_placeholder->setAlignment(Qt::AlignCenter);
	sidebar_preview_placeholder->setWordWrap(true);
	sidebar_preview_placeholder->setMinimumHeight(150);
	preview_layout->addWidget(sidebar_preview_placeholder);

	sidebar_header = new QWidget(sidebar_root);
	sidebar_header->setObjectName("paletteSidebarHeader");
	auto* header_layout = new QVBoxLayout(sidebar_header);
	header_layout->setContentsMargins(0, 0, 0, 0);
	header_layout->setSpacing(6);

	sidebar_title = new QLabel(sidebar_header);
	sidebar_title->setProperty("sidebarTitle", true);
	sidebar_title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

	sidebar_description = new QLabel(sidebar_header);
	sidebar_description->setProperty("sidebarDescription", true);
	sidebar_description->setWordWrap(true);
	sidebar_description->setAlignment(Qt::AlignLeft | Qt::AlignTop);

	header_layout->addWidget(sidebar_title);
	header_layout->addWidget(sidebar_description);

	sidebar_body = new QFrame(sidebar_root);
	sidebar_body->setProperty("sidebarBody", true);
	auto* body_layout = new QVBoxLayout(sidebar_body);
	body_layout->setContentsMargins(14, 14, 14, 14);
	body_layout->setSpacing(0);

	sidebar_stack = new QStackedWidget(sidebar_body);
	sidebar_stack->setContentsMargins(0, 0, 0, 0);
	body_layout->addWidget(sidebar_stack);

	auto create_host = [this](const char* object_name) {
		auto* host = new QFrame(sidebar_stack);
		host->setObjectName(object_name);
		host->setProperty("sidebarPaletteHost", true);
		host->setFrameShape(QFrame::NoFrame);
		host->setContentsMargins(0, 0, 0, 0);
		auto* layout = new QVBoxLayout(host);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);
		// The palette sits in a scroll area: its content height no longer sets the minimum height of the main window.
		auto* scroll = new QScrollArea(host);
		scroll->setObjectName("paletteScroll");
		scroll->setWidgetResizable(true);
		scroll->setFrameShape(QFrame::NoFrame);
		scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		layout->addWidget(scroll);
		sidebar_stack->addWidget(host);
		return host;
	};

	terrain_host = create_host("terrainSidebarHost");
	doodad_host = create_host("doodadSidebarHost");
	unit_host = create_host("unitSidebarHost");
	pathing_host = create_host("pathingSidebarHost");
	region_host = create_host("regionSidebarHost");

	sidebar_hint = new QLabel(sidebar_root);
	sidebar_hint->setProperty("sidebarHint", true);
	sidebar_hint->setWordWrap(true);
	sidebar_hint->setAlignment(Qt::AlignLeft | Qt::AlignTop);

	sidebar_layout->addWidget(sidebar_mode_bar);
	sidebar_layout->addWidget(sidebar_minimap_frame);
	sidebar_layout->addWidget(sidebar_preview_frame);
	sidebar_layout->addWidget(sidebar_header);
	sidebar_layout->addWidget(sidebar_body, 1);
	sidebar_layout->addWidget(sidebar_hint);
	content_layout->addWidget(sidebar_root);

	sidebar_root->hide();
	update_active_palette_visuals();
}

void HiveWE::load_map(const fs::path& directory) {
	// Before any teardown: a refused map must leave the open map and its unsaved edits intact.
	if (const auto check = check_map_for_opening(directory); !check) {
		const QString text =QString::fromStdString(check.error()) + "\n\nThe map was not changed.";
		if (qa_dialog_log) {
			*qa_dialog_log << text;
		} else {
			QMessageBox::critical(this, "Opening map failed", text);
		}
		return;
	}

	window_handler.close_all();
	ui->widget->makeCurrent();
	reset_map_session();

	// Map::load can still throw after the check passed: a missing texture, a map override SLK, a file changed since the check
	// (a custom object on an unknown base id no longer does: it is skipped and reported below, after the load). The open map is gone by then, so the half-loaded one is replaced
	// by the blank template that New Map uses. Doodads/Units::create wait for all their GL tasks before an error leaves.
	QString load_error;
	try {
		map->load(directory);
	} catch (const std::exception& e) {
		load_error = QString::fromLocal8Bit(e.what());
	} catch (...) {
		load_error = "Unknown error.";
	}
	if (!load_error.isEmpty()) {
		const auto report = [this](const QString& text) {
			if (qa_dialog_log) {
				*qa_dialog_log << text;
			} else {
				QMessageBox::critical(this, "Opening map failed", text);
			}
		};
		QString text = display_path(directory.filename()) + " could not be loaded:\n" + load_error
			+ "\n\nThe map that was open before had already been closed.";

		reset_map_session();
		const fs::path blank_template = fs::path("data") / "test map";
		try {
			map->load(blank_template);
		} catch (const std::exception& e) {
			report(text + " The blank map could not be opened either, so HiveWE will close:\n" + QString::fromLocal8Bit(e.what()));
			QApplication::exit(1);
			return;
		} catch (...) {
			report(text + " The blank map could not be opened either, so HiveWE will close.");
			QApplication::exit(1);
			return;
		}
		// As in create_new_map: Save asks for a folder instead of writing into the template.
		map->filesystem_path.clear();
		map->name = "Untitled";
		map->render_manager.resize_framebuffers(ui->widget->width(), ui->widget->height());
		update_map_context("Draft", "Unsaved map");
		apply_trigger_editing_state(false);
		report(text + " HiveWE opened a blank map instead.");
		return;
	}
	// The QA hooks open sandbox maps through here and must leave the user's recent-maps list alone.
	if (!qa_kept_script_check && !qa_asset_delete_check) {
		add_recent_map(directory);
	}

	map->render_manager.resize_framebuffers(ui->widget->width(), ui->widget->height());
	update_map_context("Loaded");
	apply_trigger_editing_state(true);

	// Custom objects whose base object is missing were left out of the load, and a save drops them for good.
	if (!map->skipped_objects.empty()) {
		QString text = "The map was opened, but some custom objects could not be loaded because the object they are based on "
					   "does not exist in the object data HiveWE loaded:\n\n";
		for (const SkippedObject& entry : map->skipped_objects) {
			text += QString("%1: %2 (based on %3)\n")
						.arg(QString::fromStdString(entry.file), QString::fromStdString(entry.id), QString::fromStdString(entry.base_id));
		}
		if (map->skipped_placed_units > 0) {
			text += QString("\n%1 placed unit(s) of those custom units were left out as well.\n").arg(map->skipped_placed_units);
		}
		text += "\nThese objects are dropped when you save this map.";
		if (qa_dialog_log) {
			*qa_dialog_log << text;
		} else if (qa_offscreen) {
			std::println(stderr, "{}", text.toStdString());
		} else {
			QMessageBox::warning(this, "Some objects were not loaded", text);
		}
	}
}

void HiveWE::add_recent_map(const fs::path& directory) {
	QSettings settings;
	QStringList recent_maps = settings.value("recentMaps").toStringList();
	const QString map_path = display_path(directory.lexically_normal());

	recent_maps.removeAll(map_path);
	recent_maps.prepend(map_path);

	while (recent_maps.size() > 3) {
		recent_maps.removeLast();
	}

	settings.setValue("recentMaps", recent_maps);
	update_recent_maps_menu();
}

void HiveWE::update_recent_maps_menu() {
	QSettings settings;
	QStringList recent_maps = settings.value("recentMaps").toStringList();

	update_flat_recent_menus(recent_maps);

	int button_index = 0;
	for (const auto& map_path : recent_maps) {
		if (button_index >= static_cast<int>(ui->ribbon->recent_maps.size())) {
			break;
		}

		const fs::path directory = map_path.toStdWString();
		if (!fs::exists(directory / "war3map.w3i")) {
			continue;
		}

		auto* button = ui->ribbon->recent_maps[button_index];
		const QFileInfo info(map_path);
		button->setText(info.fileName());
		button->setToolTip(map_path);
		button->setStatusTip(map_path);
		button->show();
		++button_index;
	}

	for (; button_index < static_cast<int>(ui->ribbon->recent_maps.size()); ++button_index) {
		ui->ribbon->recent_maps[button_index]->hide();
	}
}

void HiveWE::open_recent_map(int index) {
	QSettings settings;
	const QStringList recent_maps = settings.value("recentMaps").toStringList();
	if (index < 0 || index >= recent_maps.size()) {
		return;
	}

	const fs::path directory = recent_maps[index].toStdWString();
	if (!fs::exists(directory / "war3map.w3i")) {
		QMessageBox::information(this, "Opening map failed", "The recent map path no longer exists.");
		update_recent_maps_menu();
		return;
	}

	// Before the save prompt, so a refusal does not ask anything first.
	if (const auto check = check_map_for_opening(directory); !check) {
		QMessageBox::critical(this, "Opening map failed", QString::fromStdString(check.error()) + "\n\nThe map was not changed.");
		return;
	}

	if (!confirm_map_replacement("open another map")) {
		return;
	}

	load_map(directory);
}

void HiveWE::load_folder() {
	QSettings settings;

	QString folder_name = QFileDialog::getExistingDirectory(this, "Open Map Directory",
															settings.value("openDirectory", QDir::current().path()).toString(),
															QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (folder_name == "") {
		return;
	}

	settings.setValue("openDirectory", folder_name);

	fs::path directory = folder_name.toStdWString();

	if (!fs::exists(directory / "war3map.w3i")) {
		QMessageBox::information(this, "Opening map failed", "Opening the map failed. Select a map that is saved in folder mode or use the Open Map (MPQ) option");
		return;
	}

	// Before the save prompt and the loading box, so a refusal does not ask or show anything else.
	if (const auto check = check_map_for_opening(directory); !check) {
		QMessageBox::critical(this, "Opening map failed", QString::fromStdString(check.error()) + "\n\nThe map was not changed.");
		return;
	}

	if (!confirm_map_replacement("open another map")) {
		return;
	}

	QMessageBox* loading_box = new QMessageBox(QMessageBox::Icon::Information, "Loading Map", "Loading " + display_path(directory.filename()));
	loading_box->show();

	load_map(directory);

	loading_box->close();
	delete loading_box;
}

/// Load MPQ will extract all files from the archive in a user specified location
void HiveWE::load_mpq() {
	QSettings settings;

	// Choose an MPQ
	QString file_name = QFileDialog::getOpenFileName(this, "Open File",
													 settings.value("openDirectory", QDir::current().path()).toString(),
													 "Warcraft III Scenario (*.w3m *.w3x)");

	if (file_name.isEmpty()) {
		return;
	}

	settings.setValue("openDirectory", file_name);

	open_mpq(file_name.toStdWString(), {});
}

void HiveWE::open_mpq(const fs::path& mpq_path, const fs::path& unpack_location_override) {
	QSettings settings;
	mpq::MPQ mpq;
	bool opened = mpq.open(mpq_path, MPQ_OPEN_READ_ONLY);
	if (!opened) {
		const auto message = std::format("Opening the map archive failed. It might be opened in another program.\nError Code {}", GetLastError());
		QMessageBox::critical(this, "Opening map failed", QString::fromStdString(message));
		return;
	}

	// Before asking for a location: a refused map must write nothing to any folder of the user's. The
	// target may hold an earlier unpack of this map, since Foo next to Foo.w3x is what export_mpq leaves.
	{
		const QTemporaryDir header_directory;
		if (!header_directory.isValid()) {
			QMessageBox::critical(this, "Opening map failed", "Could not create a temporary folder to check the map in.");
			return;
		}
		if (const auto check = check_archive_for_opening(mpq, header_directory.path().toStdWString()); !check) {
			QMessageBox::critical(this, "Opening map failed", QString::fromStdString(check.error()) + "\n\nThe map was not changed.");
			return;
		}
	}

	if (!confirm_map_replacement("open another map")) {
		return;
	}

	fs::path unpack_location = unpack_location_override;
	if (unpack_location.empty()) {
		unpack_location = QFileDialog::getExistingDirectory(
							  this, "Choose Unpacking Location",
							  settings.value("openDirectory", QDir::current().path()).toString(),
							  QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks)
							  .toStdWString();
	}

	if (unpack_location.empty()) {
		return;
	}

	fs::path final_directory = unpack_location / mpq_path.stem();

	if (is_open_map_folder(final_directory, map->filesystem_path)) {
		const QString message = "This would unpack over the map that is open now, in\n" + QString::fromStdWString(final_directory.wstring())
			+ "\n\nChoose another unpacking location. Nothing was changed.";
		QMessageBox::critical(this, "Opening map failed", message);
		return;
	}

	try {
		fs::create_directory(final_directory);
	} catch (std::filesystem::filesystem_error& e) {
		QMessageBox::critical(this, "Error creating directory", "Failed to create the directory to unpack into with error:\n" + QString::fromStdString(e.what()), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
		return;
	}

	std::vector<std::string> failed_names;
	const bool unpacked = mpq.unpack(final_directory, &failed_names);
	if (!unpacked) {
		QString message = failed_names.empty() ? "The archive could not be read. Target folder:\n" + QString::fromStdWString(final_directory.wstring())
											   : "These files could not be extracted from the archive into\n" + QString::fromStdWString(final_directory.wstring()) + ":\n";
		for (std::size_t i = 0; i < failed_names.size() && i < 10; ++i) {
			message += "\n" + QString::fromStdString(failed_names[i]);
		}
		if (failed_names.size() > 10) {
			message += QString("\n... and %1 more").arg(failed_names.size() - 10);
		}
		message += "\n\nThe map was not opened. That folder may now contain partly extracted files.";
		QMessageBox::critical(this, "Unpacking failed", message);
		return;
	}

	// mpq.unpack() enumerates the archive via its (listfile); archives that don't have one
	// (stripped by some map protectors, or simply never written) still unpack, but unnamed
	// entries land on disk under fabricated names instead of e.g. "war3map.w3i". The core
	// map files have a fixed, well-known set of names, so fetch them directly by name too -
	// MPQ files are looked up by name hash, so this recovers them regardless of listfile
	// presence.
	// A name the archive lacks is skipped silently; one it holds but cannot extract is an error.
	// A script under scripts\ has a fixed name too, and losing it would lose the map's code.
	std::vector<std::string> names_to_extract(map->imports.blacklist.begin(), map->imports.blacklist.end());
	names_to_extract.insert(names_to_extract.end(), subfolder_script_file_names.begin(), subfolder_script_file_names.end());
	for (const std::string& name : names_to_extract) {
		if (mpq.extract_file(name, final_directory / name) == decltype(mpq)::ExtractResult::failed) {
			QMessageBox::critical(this, "Unpacking failed",
								  "The file " + QString::fromStdString(name) + " could not be extracted from the archive into\n"
									  + QString::fromStdWString(final_directory.wstring())
									  + ".\n\nThe map was not opened. That folder may now contain partly extracted files.");
			return;
		}
	}

	// The archive check skips the triggers, and the folder can hold files from before that the archive did not replace.
	// Files were written, so this must not say otherwise.
	if (const auto check = check_map_for_opening(final_directory); !check) {
		const QString message = "The map was extracted to\n" + QString::fromStdWString(final_directory.wstring())
			+ "\nbut that folder cannot be opened. A file in the map may be damaged, or files already there that the archive did not replace may be the cause.\n\n"
			+ QString::fromStdString(check.error());
		QMessageBox::critical(this, "Opening map failed", message);
		return;
	}

	load_map(final_directory);
}

void HiveWE::save() {
	save_current_map();
};

void HiveWE::save_as() {
	save_current_map_as();
}

bool HiveWE::save_current_map() {
	if (!map || !map->loaded) {
		return false;
	}

	if (map->filesystem_path.empty()) {
		return save_current_map_as();
	}

	apply_open_editors();
	if (!save_map_reporting_errors(map->filesystem_path)) {
		return false;
	}

	show_saved_status();
	return true;
}

void HiveWE::show_saved_status() {
	const bool no_backup = save_backup_state.last_failed;
	// The map files were written, but the script step failed and its error was shown: the old script is still in the map.
	if (map->script_compile_failed) {
		show_transient_notice(no_backup ? "Saved without a backup or a new map script" : "Saved without a new map script");
		update_map_context(no_backup ? "Saved without a backup, script not updated" : "Saved, script not updated");
	} else if (no_backup) {
		show_transient_notice("Saved without a backup");
		update_map_context("Saved without a backup");
	} else {
		show_transient_notice("Map saved");
		update_map_context("Saved");
	}
}

void HiveWE::apply_open_editors() {
	// The Trigger Editor keeps typed text in its tabs until they close.
	if (const auto editor = window_handler.get_open<TriggerEditor>()) {
		(*editor)->save_changes();
	}
}

bool HiveWE::save_current_map_as() {
	if (!map || !map->loaded) {
		return false;
	}

	// Before the folder is picked or created and before the map's paths are redirected, so a refusal changes neither.
	if (!map->ensure_unit_ids_fit() || !map->ensure_object_data_serializes()) {
		return false;
	}

	fs::path file_name;
	if (qa_offscreen && !qa_save_as_folder.isEmpty()) {
		// The QA hook's answer to the folder picker. Not remembered, so the run writes nothing to the registry.
		file_name = qa_save_as_folder.toStdWString();
	} else {
		QSettings settings;
		file_name = QFileDialog::getExistingDirectory(this, "Choose Save Location",
													  settings.value("openDirectory", QDir::current().path()).toString(),
													  QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks)
						.toStdWString();
		if (file_name.empty()) {
			return false;
		}
		settings.setValue("openDirectory", display_path(file_name));
	}
	if (file_name.empty()) {
		return false;
	}

	apply_open_editors();
	const auto is_current_folder = [&](const fs::path& folder) {
		std::error_code ec;
		return !map->filesystem_path.empty() && fs::exists(folder, ec) && fs::equivalent(folder, map->filesystem_path, ec);
	};

	const QString fallback_name = sanitized_folder_name(QString::fromStdString(map->name));
	const fs::path target_directory = file_name / fallback_name.toStdWString();
	if (is_current_folder(file_name) || is_current_folder(target_directory)) {
		// The map's own folder: an ordinary save.
		if (!save_map_reporting_errors(map->filesystem_path)) {
			return false;
		}
		show_saved_status();
		return true;
	}

	// Refusals come before anything is written or redirected. Save As never writes into a folder that already holds files: a new map
	// would silently replace that folder's war3map files, and copying an existing map into it stops at the first file already there.
	const QString target_text = display_path(target_directory);
	if (!map->filesystem_path.empty() && path_is_within(target_directory, map->filesystem_path)) {
		QMessageBox::warning(this, "Save As",
							 "The map was not saved. The folder\n" + target_text
								 + "\nis inside the map's own folder, so the map would be copied into itself.\n\nChoose a location outside "
								 + display_path(map->filesystem_path) + ".");
		return false;
	}
	std::error_code ec;
	const fs::file_status target_status = fs::symlink_status(target_directory, ec);
	if (target_status.type() == fs::file_type::not_found) {
		ec.clear(); // status reports a missing path as an error too
	}
	const bool target_existed = fs::exists(target_status);
	if (ec || (target_existed && (!fs::is_directory(target_directory, ec) || !fs::is_empty(target_directory, ec) || ec))) {
		QMessageBox::warning(this, "Save As",
							 "The map was not saved. The folder\n" + target_text
								 + "\nalready exists and is not empty, so saving there would replace or mix with the files in it.\n\n"
								   "Choose another location, or empty that folder first.");
		return false;
	}
	fs::create_directories(target_directory, ec);
	if (ec) {
		QMessageBox::critical(this, "Save As",
							  "The map was not saved. The folder\n" + target_text + "\ncould not be created:\n"
								  + QString::fromLocal8Bit(ec.message().c_str()));
		return false;
	}

	// Map::save and the new-map case below point these at the new folder. A failure puts them back, so the next Save does not write
	// the open map into a folder that holds a broken copy.
	const fs::path previous_directory = hierarchy.map_directory;
	const fs::path previous_path = map->filesystem_path;
	const std::string previous_name = map->name;

	bool saved = false;
	hierarchy.map_directory = target_directory;
	if (map->filesystem_path.empty()) {
		map->filesystem_path = fs::absolute(target_directory) / "";
		map->name = utf8_path(*--(--map->filesystem_path.end()));
		hierarchy.map_directory = map->filesystem_path;
		saved = save_map_reporting_errors(map->filesystem_path);
	} else {
		saved = save_map_reporting_errors(target_directory);
	}

	if (!saved) {
		hierarchy.map_directory = previous_directory;
		map->filesystem_path = previous_path;
		map->name = previous_name;

		std::error_code left_ec;
		const bool left_files = fs::exists(target_directory, left_ec) && !fs::is_empty(target_directory, left_ec);
		if (!target_existed && !left_files) {
			fs::remove(target_directory, left_ec); // the empty folder Save As made
		} else if (left_files) {
			QMessageBox::warning(this, "Save As",
								 "Save As did not finish. Some map files were already written to\n" + target_text
									 + "\nThat folder was left as it is; it is not a complete map. Delete it, or empty it before saving there again.\n\n"
									 + (previous_path.empty() ? QString("The open map is still unsaved.")
															  : "Save still writes the open map to\n" + display_path(previous_path)));
		}
		return false;
	}

	show_saved_status();
	return true;
}

void HiveWE::export_mpq() {
	if (!save_current_map()) {
		return;
	}

	// Read before the picker: the save that just ran decides whether the archive gets an up-to-date script.
	const bool script_not_updated = map->script_compile_failed;

	fs::path file_name;
	if (qa_offscreen && !qa_export_mpq_file.isEmpty()) {
		// The QA hook's answer to the file picker.
		file_name = qa_export_mpq_file.toStdWString();
	} else {
		QSettings settings;
		const QString directory = settings.value("openDirectory", QDir::current().path()).toString() + "/" + display_path(map->filesystem_path.filename());
		file_name = QFileDialog::getSaveFileName(this, "Export Map to MPQ", directory, "Warcraft III Scenario (*.w3x)").toStdWString();
	}
	if (file_name.empty()) {
		return;
	}

	const map_archive::Result result = map_archive::write(map->filesystem_path, file_name);
	if (!result.ok) {
		std::println(stderr, "Export MPQ failed at step {} (error {}), {} files added", static_cast<int>(result.step), result.error, result.added);
	}
	show_export_result(this, map->filesystem_path, file_name, result, script_not_updated);
}

void HiveWE::play_test() {
	if (!map) {
		return;
	}
	// A script that cannot be updated leaves an old or no script in the map, so the game is not started. The save's script error
	// dialog says so, which keeps it to one dialog.
	map->script_failure_note = "Warcraft III was not started, because the map script could not be updated.";
	const bool saved = save_current_map();
	map->script_failure_note.clear();
	if (!saved || map->script_compile_failed) {
		return;
	}
	QProcess* warcraft = new QProcess;
	const QString warcraft_path = display_path(fs::canonical(hierarchy.root_directory / "x86_64" / "Warcraft III.exe"));
	QStringList arguments;
	arguments << "-launch"
			  << "-loadfile" << display_path(fs::canonical(map->filesystem_path));

	QSettings settings;
	if (settings.value("testArgs").toString() != "")
		arguments << settings.value("testArgs").toString().split(' ');

	if (qa_offscreen) {
		// QA runs never start the game: the command is only recorded, for the hook that checks it.
		qa_game_launches << warcraft_path + " " + arguments.join(' ');
		delete warcraft;
		return;
	}

	warcraft->start(warcraft_path, arguments);
}

void HiveWE::create_new_map() {
	if (!confirm_map_replacement("create a new map")) {
		return;
	}

	NewMapDialog dialog(this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	const fs::path template_directory = fs::path("data") / "test map";
	if (!fs::exists(template_directory / "war3map.w3i")) {
		QMessageBox::critical(this, "Template map missing", "The bundled blank map template could not be found in data/test map.");
		return;
	}

	if (const auto check = check_map_for_opening(template_directory); !check) {
		QMessageBox::critical(this, "Template map unusable", QString::fromStdString(check.error()));
		return;
	}

	// As in load_map: an editor left open would show, and could write, the old map's data.
	window_handler.close_all();
	reset_map_session();
	map->load(template_directory);
	apply_trigger_editing_state(false);

	const NewMapDialog::Result settings = dialog.result();
	const int desired_width = settings.width + 1;
	const int desired_height = settings.height + 1;
	const int width_delta = desired_width - map->terrain.width;
	const int height_delta = desired_height - map->terrain.height;
	const int delta_left = width_delta / 2;
	const int delta_right = width_delta - delta_left;
	const int delta_bottom = height_delta / 2;
	const int delta_top = height_delta - delta_bottom;
	if (width_delta != 0 || height_delta != 0) {
		map->resize(delta_left, delta_right, delta_top, delta_bottom);
	}

	std::vector<std::string> tileset_ids;
	for (const auto& [id, row] : map->terrain.terrain_slk.row_headers) {
		if (!id.empty() && id.front() == settings.tileset) {
			tileset_ids.push_back(id);
		}
	}
	std::sort(tileset_ids.begin(), tileset_ids.end());

	std::vector<std::string> cliffset_ids;
	for (const auto& [id, row] : map->terrain.cliff_slk.row_headers) {
		if (!id.empty() && id.front() == settings.tileset) {
			cliffset_ids.push_back(id);
		}
	}
	std::sort(cliffset_ids.begin(), cliffset_ids.end());

	if (tileset_ids.empty()) {
		QMessageBox::critical(this, "New map failed", "The selected tileset does not have valid terrain data.");
		return;
	}
	if (cliffset_ids.empty()) {
		cliffset_ids = map->terrain.cliffset_ids;
	}

	// reset_map_session above already deleted the palettes, so no terrain clipboard survives into the new map.
	map->terrain.tileset = settings.tileset;
	map->terrain.cliffset_ids = cliffset_ids;
	map->terrain.change_tileset(tileset_ids, std::vector<int>(map->terrain.tileset_ids.size(), 0));

	const int terrain_width = map->terrain.width;
	const int terrain_height = map->terrain.height;
	const size_t total_corners = static_cast<size_t>(terrain_width * terrain_height);
	map->terrain.resize_corner_arrays(total_corners);

	for (int y = 0; y < terrain_height; ++y) {
		for (int x = 0; x < terrain_width; ++x) {
			const size_t idx = map->terrain.ci(x, y);
			map->terrain.corner_map_edge[idx] = x == 0 || y == 0 || x == terrain_width - 1 || y == terrain_height - 1;
			map->terrain.corner_ground_texture[idx] = 0;
			map->terrain.corner_ground_variation[idx] = 0;
			map->terrain.corner_cliff_texture[idx] = 15;
			map->terrain.corner_layer_height[idx] = 2;
		}
	}

	map->terrain.update_cliff_meshes({0, 0, terrain_width, terrain_height});
	map->terrain.update_ground_heights({0, 0, terrain_width, terrain_height});
	map->terrain.update_ground_exists({0, 0, terrain_width - 1, terrain_height - 1});
	map->terrain.update_ground_textures({0, 0, terrain_width, terrain_height});
	map->terrain.update_water({0, 0, terrain_width, terrain_height});
	map->terrain.update_minimap();

	map->pathing_map.resize(terrain_width * 4, terrain_height * 4);
	std::fill(map->pathing_map.pathing_cells_static.begin(), map->pathing_map.pathing_cells_static.end(), 0);
	std::fill(map->pathing_map.pathing_cells_dynamic.begin(), map->pathing_map.pathing_cells_dynamic.end(), 0);
	map->pathing_map.upload_static_pathing();
	map->pathing_map.upload_dynamic_pathing();
	map->shadow_map.resize((terrain_width - 1) * 4, (terrain_height - 1) * 4);

	int unplayable_left = 6;
	int unplayable_right = 6;
	int unplayable_bottom = 4;
	int unplayable_top = 8;
	map->set_playable_area(unplayable_left, unplayable_right, unplayable_top, unplayable_bottom);

	map->doodads.doodads.clear();
	map->doodads.special_doodads.clear();
	map->units.units.clear();
	map->units.items.clear();
	map->regions.regions.clear();
	map->cameras.cameras.clear();
	map->sounds.sounds.clear();

	const int default_active_player_count = std::min(4, static_cast<int>(map->info.players.size()));
	for (size_t i = 0; i < map->info.players.size(); ++i) {
		auto& player = map->info.players[i];
		player.internal_number = static_cast<int>(i);
		player.type = static_cast<int>(i) < default_active_player_count ? PlayerType::human : PlayerType::neutral;
		player.race = PlayerRace::selectable;
		player.fixed_start_position = 1;
	}

	const float left = static_cast<float>(unplayable_left + 4);
	const float bottom = static_cast<float>(unplayable_bottom + 4);
	const float right = static_cast<float>(terrain_width - 1 - unplayable_right - 4);
	const float top = static_cast<float>(terrain_height - 1 - unplayable_top - 4);
	const glm::vec2 center((left + right) * 0.5f, (bottom + top) * 0.5f);
	const float radius = std::max(6.f, std::min(right - left, top - bottom) * 0.38f);
	for (int i = 0; i < default_active_player_count; ++i) {
		const float angle = static_cast<float>(i) * static_cast<float>(std::numbers::pi_v<float> * 2.0) / default_active_player_count;
		glm::vec2 start = center + glm::vec2(std::cos(angle), std::sin(angle)) * radius;
		start.x = std::clamp(start.x, left, right);
		start.y = std::clamp(start.y, bottom, top);

		map->info.players[i].starting_position = start * 128.f + map->terrain.offset;
		[[maybe_unused]] Unit& start_location =
			map->units.add_start_location(i, {start.x, start.y, map->terrain.interpolated_height(start.x, start.y, true)});
	}

	map->trigger_strings.set_string(map->info.name, settings.map_name.toStdString());
	map->trigger_strings.set_string(map->info.description, "");
	map->trigger_strings.set_string(map->info.author, "");
	map->trigger_strings.set_string(map->info.suggested_players, "Custom");
	map->info.melee_map = default_active_player_count >= 2;
	map->filesystem_path.clear();
	map->name = sanitized_folder_name(settings.map_name).toStdString();
	map->world_undo.clear_all_undo();

	camera.position = glm::vec3(terrain_width / 2.f, terrain_height / 2.f, 0.f);
	camera.position.z = map->terrain.interpolated_height(camera.position.x, camera.position.y, true);
	map->render_manager.resize_framebuffers(ui->widget->width(), ui->widget->height());
	update_map_context("Draft", "Unsaved map");
	show_transient_notice("New map created");
}

void HiveWE::closeEvent(QCloseEvent* event) {
	if (!confirm_map_replacement("quit")) {
		event->ignore();
		return;
	}

	QApplication::closeAllWindows();
	event->accept();
}

void HiveWE::resizeEvent(QResizeEvent* event) {
	QMainWindow::resizeEvent(event);
	position_transient_notice();
	QTimer::singleShot(0, [&] { save_window_state(); });
}

void HiveWE::moveEvent(QMoveEvent* event) {
	QMainWindow::moveEvent(event);
	position_transient_notice();
	QTimer::singleShot(0, [&] { save_window_state(); });
}

void HiveWE::switch_warcraft() {
	QSettings settings;
	fs::path directory;
	do {
		directory = QFileDialog::getExistingDirectory(this, "Select Warcraft Directory", "/home", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks).toStdWString();
		if (directory == "") {
			directory = settings.value("warcraftDirectory").toString().toStdString();
		}
	} while (!hierarchy.open_casc(directory));

	if (directory != hierarchy.warcraft_directory) {
		settings.setValue("warcraftDirectory", QString::fromStdString(directory.string()));
	}
}

// ToDo move to terrain class?
void HiveWE::import_heightmap() {
	QMessageBox::information(this, "Heightmap information", "Will read the red channel and map this onto the range -16 to +16");
	QSettings settings;
	const QString directory = settings.value("openDirectory", QDir::current().path()).toString() + "/" + display_path(map->filesystem_path.filename());

	QString file_name = QFileDialog::getOpenFileName(this, "Open Heightmap Image", directory);

	if (file_name == "") {
		return;
	}

	int width;
	int height;
	int channels;
	uint8_t* image_data = SOIL_load_image(file_name.toStdString().c_str(), &width, &height, &channels, SOIL_LOAD_AUTO);

	if (width != map->terrain.width || height != map->terrain.height) {
		QMessageBox::warning(this, "Incorrect Image Size", QString("Image Size: %1x%2 does not match terrain size: %3x%4").arg(QString::number(width), QString::number(height), QString::number(map->terrain.width), QString::number(map->terrain.height)));
		return;
	}

	for (int j = 0; j < height; j++) {
		for (int i = 0; i < width; i++) {
			map->terrain.corner_height[map->terrain.ci(i, j)] = (image_data[((height - 1 - j) * width + i) * channels] - 128.f) / 8.f;
		}
	}

	map->terrain.update_ground_heights({ 0, 0, width, height });
	delete image_data;
}

namespace {

QString object_data_default_directory() {
	QSettings settings;
	if (!map) {
		return settings.value("openDirectory", QDir::current().path()).toString();
	}
	return settings.value("openDirectory", QDir::current().path()).toString() + "/" + display_path(map->filesystem_path.filename());
}

bool ensure_map_loaded_for_object_io(QWidget* parent) {
	if (map && map->loaded) {
		return true;
	}

	QMessageBox::warning(parent, "No Map Loaded", "Open a map before importing or exporting Object Editor data.");
	return false;
}

void show_import_validation_failure(QWidget* parent, const ImportValidationReport& report) {
	QStringList lines;
	for (const ValidationIssue& issue : report.issues) {
		if (issue.severity == ValidationSeverity::error) {
			lines.push_back(QString::fromStdString(issue.message));
		}
	}
	if (lines.isEmpty()) {
		lines.push_back("Import validation failed.");
	}
	QMessageBox::warning(parent, "Import Blocked", lines.join("\n"));
}

QString scenario_controller_label(const PlayerType type) {
	switch (type) {
		case PlayerType::human:
			return "User";
		case PlayerType::computer:
			return "Computer";
		case PlayerType::neutral:
			return "None";
		case PlayerType::rescuable:
			return "Rescuable";
	}
	return "?";
}

QString scenario_race_label(const PlayerRace race) {
	switch (race) {
		case PlayerRace::selectable:
			return "Selectable";
		case PlayerRace::human:
			return "Human";
		case PlayerRace::orc:
			return "Orc";
		case PlayerRace::undead:
			return "Undead";
		case PlayerRace::night_elf:
			return "Night Elf";
	}
	return "?";
}

/// Field-by-field diff between the live map->info and a parsed scenario.json, so the import
/// prompt can show what will actually change instead of a bare "N players, M forces" count.
/// Players match by internal_number (stable slot identity); forces have no persistent ID, so
/// they're matched positionally (by index) as a best-effort heuristic — accurate for the common
/// case of hand-editing an exported file without reordering entries.
QStringList compute_scenario_import_changes(const ScenarioImportData& scenario) {
	QStringList changes;

	for (const auto& imported_player : scenario.players) {
		const auto found = std::ranges::find_if(map->info.players, [&](const PlayerData& player) {
			return player.internal_number == imported_player.internal_number;
		});
		if (found == map->info.players.end()) {
			continue;
		}

		QStringList field_changes;
		const std::string current_name(map->trigger_strings.string(found->name));
		if (current_name != imported_player.name) {
			field_changes << QString("name '%1' → '%2'").arg(QString::fromStdString(current_name), QString::fromStdString(imported_player.name));
		}
		if (found->type != imported_player.controller) {
			field_changes << QString("controller %1 → %2")
								  .arg(scenario_controller_label(found->type), scenario_controller_label(imported_player.controller));
		}
		if (found->race != imported_player.race) {
			field_changes << QString("race %1 → %2").arg(scenario_race_label(found->race), scenario_race_label(imported_player.race));
		}
		const bool current_fixed_start = found->fixed_start_position != 0;
		if (current_fixed_start != imported_player.fixed_start_location) {
			field_changes << QString("fixed start %1 → %2")
								  .arg(current_fixed_start ? "on" : "off", imported_player.fixed_start_location ? "on" : "off");
		}

		if (!field_changes.isEmpty()) {
			changes << QString("Player %1 (%2): %3")
							.arg(imported_player.internal_number + 1)
							.arg(QString::fromStdString(current_name), field_changes.join(", "));
		}
	}

	const auto member_names = [](const std::set<int>& members) {
		QStringList names;
		for (const int internal_number : members) {
			const auto found = std::ranges::find_if(map->info.players, [&](const PlayerData& player) {
				return player.internal_number == internal_number;
			});
			names << (found != map->info.players.end() ? QString::fromStdString(std::string(map->trigger_strings.string(found->name)))
														 : QString("Player %1").arg(internal_number + 1));
		}
		return names.join(", ");
	};

	const size_t force_count = std::max(map->info.forces.size(), scenario.forces.size());
	for (size_t i = 0; i < force_count; ++i) {
		if (i >= map->info.forces.size()) {
			changes << QString("Force added: '%1'").arg(QString::fromStdString(scenario.forces[i].name));
			continue;
		}
		if (i >= scenario.forces.size()) {
			const std::string removed_force_name(map->trigger_strings.string(map->info.forces[i].name));
			changes << QString("Force removed: '%1'").arg(QString::fromStdString(removed_force_name));
			continue;
		}

		const auto& current_force = map->info.forces[i];
		const auto& imported_force = scenario.forces[i];

		QStringList force_changes;
		const std::string current_force_name(map->trigger_strings.string(current_force.name));
		if (current_force_name != imported_force.name) {
			force_changes << QString("name '%1' → '%2'").arg(QString::fromStdString(current_force_name), QString::fromStdString(imported_force.name));
		}

		std::set<int> current_members;
		for (const auto& player : map->info.players) {
			if (current_force.player_masks & (1 << player.internal_number)) {
				current_members.insert(player.internal_number);
			}
		}
		const std::set<int> imported_members(imported_force.players.begin(), imported_force.players.end());
		if (current_members != imported_members) {
			force_changes << QString("members [%1] → [%2]").arg(member_names(current_members), member_names(imported_members));
		}

		if (current_force.allied != imported_force.allied) {
			force_changes << QString("Allied %1").arg(imported_force.allied ? "enabled" : "disabled");
		}
		if (current_force.allied_victory != imported_force.allied_victory) {
			force_changes << QString("Allied Victory %1").arg(imported_force.allied_victory ? "enabled" : "disabled");
		}
		if (current_force.share_vision != imported_force.share_vision) {
			force_changes << QString("Share Vision %1").arg(imported_force.share_vision ? "enabled" : "disabled");
		}
		if (current_force.share_unit_control != imported_force.share_unit_control) {
			force_changes << QString("Share Unit Control %1").arg(imported_force.share_unit_control ? "enabled" : "disabled");
		}
		if (current_force.share_advanced_unit_control != imported_force.share_advanced_unit_control) {
			force_changes << QString("Share Adv. Unit Control %1").arg(imported_force.share_advanced_unit_control ? "enabled" : "disabled");
		}

		if (!force_changes.isEmpty()) {
			changes << QString("Force %1: %2").arg(QString::fromStdString(current_force_name), force_changes.join(", "));
		}
	}

	if (map->info.custom_forces != scenario.custom_forces) {
		changes << QString("Use Custom Forces %1").arg(scenario.custom_forces ? "enabled" : "disabled");
	}
	if (map->info.fixed_player_settings != scenario.fixed_player_settings) {
		changes << QString("Fixed Player Settings %1").arg(scenario.fixed_player_settings ? "enabled" : "disabled");
	}

	return changes;
}

/// Shows the actual field-level diff (not just a player/force count) and lets the user confirm
/// or cancel before anything is applied.
bool show_scenario_import_preview(QWidget* parent, const ScenarioImportData& scenario) {
	const QStringList changes = compute_scenario_import_changes(scenario);

	QDialog dialog(parent);
	dialog.setWindowTitle("Import Scenario Data");
	dialog.resize(700, 480);

	auto* layout = new QVBoxLayout(&dialog);
	layout->addWidget(new QLabel(
		changes.isEmpty() ? "This package's Scenario data (Players/Forces) is identical to the current map — nothing would change."
						  : "This package contains the following Players/Forces changes:"
	));

	auto* changes_list = new QListWidget;
	changes_list->addItems(changes);
	layout->addWidget(changes_list, 1);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	buttons->button(QDialogButtonBox::Ok)->setText("Apply");
	buttons->button(QDialogButtonBox::Ok)->setEnabled(!changes.isEmpty());
	layout->addWidget(buttons);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

	return dialog.exec() == QDialog::Accepted;
}

/// Scenario (Players/Forces) data has no per-object conflict/merge semantics like the SLK-backed
/// object categories do, so it rides along as an optional wholesale-replace prompt instead of
/// going through ObjectDataImportDialog's per-object conflict review.
void prompt_apply_scenario_import(QWidget* parent, const std::optional<ScenarioImportData>& scenario) {
	if (!scenario || !map) {
		return;
	}

	// Skip the dialog entirely when nothing would change — no need to force a confirmation.
	if (compute_scenario_import_changes(*scenario).isEmpty()) {
		return;
	}

	if (!show_scenario_import_preview(parent, *scenario)) {
		return;
	}

	apply_scenario_import(*scenario, map->info, map->trigger_strings);

	if (const auto open_editor = window_handler.get_open<ScenarioPropertiesEditor>(); open_editor.has_value() && *open_editor) {
		(*open_editor)->refresh_all();
	}
}

} // namespace

void HiveWE::export_object_data_binary() {
	if (!ensure_map_loaded_for_object_io(this)) {
		return;
	}

	const QString directory = QFileDialog::getExistingDirectory(this, "Export Object Data (Binary)", object_data_default_directory());
	if (directory.isEmpty()) {
		return;
	}

	if (const auto result = export_binary_object_data(directory.toStdWString()); !result) {
		QMessageBox::warning(this, "Export Failed", QString::fromStdString(result.error()));
		return;
	}

	show_transient_notice("Exported Object Editor binary package.");
}

void HiveWE::export_object_data_text() {
	if (!ensure_map_loaded_for_object_io(this)) {
		return;
	}

	const QString directory = QFileDialog::getExistingDirectory(this, "Export Object Data (Text)", object_data_default_directory());
	if (directory.isEmpty()) {
		return;
	}

	if (const auto result = export_text_object_data(directory.toStdWString()); !result) {
		QMessageBox::warning(this, "Export Failed", QString::fromStdString(result.error()));
		return;
	}

	show_transient_notice("Exported Object Editor text package.");
}

void HiveWE::import_object_data_binary() {
	if (!ensure_map_loaded_for_object_io(this)) {
		return;
	}

	const QString directory = QFileDialog::getExistingDirectory(this, "Import Object Data (Binary)", object_data_default_directory());
	if (directory.isEmpty()) {
		return;
	}

	const auto loaded = load_binary_object_data(directory.toStdWString(), true);
	if (!loaded) {
		show_import_validation_failure(this, loaded.error());
		return;
	}

	const std::optional<ScenarioImportData> scenario = loaded->scenario;

	ObjectDataImportDialog dialog(std::move(*loaded), this);
	if (dialog.exec() == QDialog::Accepted && dialog.dialog_result() == ObjectDataImportDialog::Result::applied) {
		show_transient_notice("Imported Object Editor data.");
	}

	prompt_apply_scenario_import(this, scenario);
}

void HiveWE::import_object_data_map_folder() {
	if (!ensure_map_loaded_for_object_io(this)) {
		return;
	}

	const QString directory = QFileDialog::getExistingDirectory(this, "Import Object Data (Map Folder)", object_data_default_directory());
	if (directory.isEmpty()) {
		return;
	}

	const auto loaded = load_binary_object_data(directory.toStdWString(), false);
	if (!loaded) {
		show_import_validation_failure(this, loaded.error());
		return;
	}

	const std::optional<ScenarioImportData> scenario = loaded->scenario;

	ObjectDataImportDialog dialog(std::move(*loaded), this);
	if (dialog.exec() == QDialog::Accepted && dialog.dialog_result() == ObjectDataImportDialog::Result::applied) {
		show_transient_notice("Imported Object Editor data from map folder.");
	}

	prompt_apply_scenario_import(this, scenario);
}

void HiveWE::import_object_data_text() {
	if (!ensure_map_loaded_for_object_io(this)) {
		return;
	}

	const QString directory = QFileDialog::getExistingDirectory(this, "Import Object Data (Text)", object_data_default_directory());
	if (directory.isEmpty()) {
		return;
	}

	const auto loaded = load_text_object_data(directory.toStdWString());
	if (!loaded) {
		show_import_validation_failure(this, loaded.error());
		return;
	}

	const std::optional<ScenarioImportData> scenario = loaded->scenario;

	ObjectDataImportDialog dialog(std::move(*loaded), this);
	if (dialog.exec() == QDialog::Accepted && dialog.dialog_result() == ObjectDataImportDialog::Result::applied) {
		show_transient_notice("Imported Object Editor text data.");
	}

	prompt_apply_scenario_import(this, scenario);
}

void HiveWE::save_window_state() {
	// --qa-offscreen runs must not overwrite the position of the user's window.
	if (qa_offscreen) {
		return;
	}

	QSettings settings;

	if (!isMaximized()) {
		settings.setValue("MainWindow/geometry", saveGeometry());
	}

	settings.setValue("MainWindow/maximized", isMaximized());
	settings.setValue("MainWindow/windowState", saveState());
}

void HiveWE::restore_window_state() {
	if (qa_offscreen) {
		// Left of every monitor and never activated or raised, so an automated screenshot run does not disturb the user.
		const QSize size = qa_window_size;
		const QRect virtual_geometry = QGuiApplication::primaryScreen()->virtualGeometry();
		setAttribute(Qt::WA_ShowWithoutActivating);
		setWindowFlag(Qt::WindowDoesNotAcceptFocus, true);
		resize(size);
		move(virtual_geometry.left() - size.width() - 200, virtual_geometry.top());
		show();
		return;
	}

	QSettings settings;

	if (settings.contains("MainWindow/windowState")) {
		restoreGeometry(settings.value("MainWindow/geometry").toByteArray());
		restoreState(settings.value("MainWindow/windowState").toByteArray());
		if (settings.value("MainWindow/maximized").toBool()) {
			showMaximized();
		} else {
			showNormal();
		}
	} else {
		showMaximized();
	}
}

void HiveWE::set_current_custom_tab(QRibbonTab* tab, QString name) {
	if (current_custom_tab == tab) {
		return;
	}

	if (current_custom_tab != nullptr) {
		emit palette_changed(tab);
	}

	remove_custom_tab();
	current_custom_tab = tab;
	ui->ribbon->addTab(tab, name);
	ui->ribbon->setCurrentIndex(ui->ribbon->count() - 1);
	update_ribbon_visibility();
}

void HiveWE::remove_custom_tab() {
	for (int i = 0; i < ui->ribbon->count(); i++) {
		if (ui->ribbon->widget(i) == current_custom_tab) {
			ui->ribbon->removeTab(i);
			current_custom_tab = nullptr;
			current_flat_tab = nullptr;
			update_ribbon_visibility();
			return;
		}
	}
}

Palette* HiveWE::palette_for_view(const SidebarPaletteView view) const {
	switch (view) {
	case SidebarPaletteView::terrain:
		return terrain_palette;
	case SidebarPaletteView::doodad:
		return doodad_palette;
	case SidebarPaletteView::unit:
		return unit_palette;
	case SidebarPaletteView::pathing:
		return pathing_palette;
	case SidebarPaletteView::region:
		return region_palette;
	case SidebarPaletteView::none:
		return nullptr;
	}

	return nullptr;
}

QWidget* HiveWE::host_for_view(const SidebarPaletteView view) const {
	switch (view) {
	case SidebarPaletteView::terrain:
		return terrain_host;
	case SidebarPaletteView::doodad:
		return doodad_host;
	case SidebarPaletteView::unit:
		return unit_host;
	case SidebarPaletteView::pathing:
		return pathing_host;
	case SidebarPaletteView::region:
		return region_host;
	case SidebarPaletteView::none:
		return nullptr;
	}

	return nullptr;
}

QString HiveWE::sidebar_title_for_view(const SidebarPaletteView view) const {
	switch (view) {
	case SidebarPaletteView::terrain:
		return "Terrain";
	case SidebarPaletteView::doodad:
		return "Doodads";
	case SidebarPaletteView::unit:
		return "Units";
	case SidebarPaletteView::pathing:
		return "Pathing";
	case SidebarPaletteView::region:
		return "Regions";
	case SidebarPaletteView::none:
		return "";
	}

	return "";
}

QString HiveWE::sidebar_description_for_view(const SidebarPaletteView view) const {
	switch (view) {
	case SidebarPaletteView::terrain:
		return "Brush families, tiles, and terrain operations in one focused panel without stacking unrelated palettes below it.";
	case SidebarPaletteView::doodad:
		return "Search, filter, and place doodads with the active selection and placement rules kept in one consistent sidebar.";
	case SidebarPaletteView::unit:
		return "Pick units by owner and search context, then keep selection-aware placement tools close to the active asset.";
	case SidebarPaletteView::pathing:
		return "Pathing brush operations, mask selection, and import or export tools in a dedicated utility view.";
	case SidebarPaletteView::region:
		return "Region selection, creation, and property editing in one compact mode-specific inspector.";
	case SidebarPaletteView::none:
		return "";
	}

	return "";
}

QString HiveWE::sidebar_hint_for_view(const SidebarPaletteView view) const {
	switch (view) {
	case SidebarPaletteView::terrain:
		return "Shortcut: T. Space switches selection mode where relevant.";
	case SidebarPaletteView::doodad:
		return "Shortcut: D. Ctrl+F jumps to search. Middle-click can sync from the map selection.";
	case SidebarPaletteView::unit:
		return "Shortcut: U. Ctrl+F jumps to search and player ownership stays visible in the palette.";
	case SidebarPaletteView::pathing:
		return "Shortcut: P. Keep pathing operation and brush mask readable while the viewport remains dominant.";
	case SidebarPaletteView::region:
		return "Shortcut: R. Keep region properties close to the selected region instead of in a separate dialog flow.";
	case SidebarPaletteView::none:
		return "";
	}

	return "";
}

QString HiveWE::sidebar_feedback_label(const SidebarPaletteView view) const {
	switch (view) {
	case SidebarPaletteView::terrain:
		return "Terrain sidebar";
	case SidebarPaletteView::doodad:
		return "Doodad sidebar";
	case SidebarPaletteView::unit:
		return "Unit sidebar";
	case SidebarPaletteView::pathing:
		return "Pathing sidebar";
	case SidebarPaletteView::region:
		return "Region sidebar";
	case SidebarPaletteView::none:
		return "Sidebar";
	}

	return "Sidebar";
}

namespace {
/// Puts a palette into the scroll area of its host (see create_host in the sidebar setup).
void put_palette_in_host(QWidget* host, Palette* palette) {
	// The content follows the width of the scroll area instead of pushing the palette wider than the column.
	palette->fit_to_column();
	host->findChild<QScrollArea*>("paletteScroll")->setWidget(palette);
}
}

void HiveWE::ensure_palette_view(const SidebarPaletteView view) {
	if ((view == SidebarPaletteView::terrain || view == SidebarPaletteView::pathing) && !terrain_palette) {
		terrain_palette = new TerrainPalette(terrain_host);
		put_palette_in_host(terrain_host, terrain_palette);
		connect(terrain_palette, &Palette::activated, this, [this]() { activate_palette(terrain_palette); });
		connect(terrain_palette, &Palette::ribbon_tab_requested, this, &HiveWE::set_current_custom_tab);
		connect(terrain_palette, &Palette::tool_page_activated, this, &HiveWE::on_tool_page_activated);
		connect(this, &HiveWE::palette_changed, terrain_palette, &Palette::deactivate);
	}

	if ((view == SidebarPaletteView::terrain || view == SidebarPaletteView::pathing) && !pathing_palette) {
		pathing_palette = new PathingPalette(pathing_host);
		put_palette_in_host(pathing_host, pathing_palette);
		connect(pathing_palette, &Palette::activated, this, [this]() { activate_palette(pathing_palette); });
		connect(pathing_palette, &Palette::ribbon_tab_requested, this, &HiveWE::set_current_custom_tab);
		connect(pathing_palette, &Palette::tool_page_activated, this, &HiveWE::on_tool_page_activated);
		connect(this, &HiveWE::palette_changed, pathing_palette, &Palette::deactivate);
		connect(&pathing_palette->current_brush(), &Brush::size_changed, this, [this](glm::ivec2) {
			if (terrain_palette) {
				const QSignalBlocker blocker(terrain_palette->current_brush());
				terrain_palette->current_brush().set_size(pathing_palette->current_brush().get_size());
				terrain_palette->sync_brush_controls(map->brush);
			}
		});
		if (terrain_palette) {
			connect(&terrain_palette->current_brush(), &Brush::size_changed, this, [this](glm::ivec2) {
				if (pathing_palette) {
					const QSignalBlocker blocker(pathing_palette->current_brush());
					pathing_palette->current_brush().set_size(terrain_palette->current_brush().get_size());
					terrain_palette->sync_brush_controls(map->brush);
				}
			});
		}
	}

	if (view == SidebarPaletteView::doodad && !doodad_palette) {
		doodad_palette = new DoodadPalette(doodad_host);
		put_palette_in_host(doodad_host, doodad_palette);
		connect(doodad_palette, &Palette::activated, this, [this]() { activate_palette(doodad_palette); });
		connect(doodad_palette, &Palette::ribbon_tab_requested, this, &HiveWE::set_current_custom_tab);
		connect(doodad_palette, &Palette::tool_page_activated, this, &HiveWE::on_tool_page_activated);
		connect(doodad_palette, &DoodadPalette::preview_doodad_changed, this, &HiveWE::update_doodad_model_preview);
		connect(&doodad_palette->current_brush(), &Brush::selection_changed, this, [this]() {
			if (!selection_panel) {
				return;
			}
			const DoodadBrush& brush = doodad_palette->current_brush();
			if (!brush.selections.empty() || selection_panel->owned_by(&brush)) {
				selection_panel->show_selection(brush);
			}
		});
		connect(this, &HiveWE::palette_changed, doodad_palette, &Palette::deactivate);
	}

	if (view == SidebarPaletteView::unit && !unit_palette) {
		unit_palette = new UnitPalette(unit_host);
		put_palette_in_host(unit_host, unit_palette);
		connect(unit_palette, &Palette::activated, this, [this]() { activate_palette(unit_palette); });
		connect(unit_palette, &Palette::ribbon_tab_requested, this, &HiveWE::set_current_custom_tab);
		connect(unit_palette, &Palette::tool_page_activated, this, &HiveWE::on_tool_page_activated);
		connect(unit_palette, &UnitPalette::preview_unit_changed, this, &HiveWE::update_unit_model_preview);
		connect(&unit_palette->current_brush(), &Brush::selection_changed, this, [this]() {
			if (!selection_panel) {
				return;
			}
			const UnitBrush& brush = unit_palette->current_brush();
			if (!brush.selections.empty() || selection_panel->owned_by(&brush)) {
				selection_panel->show_selection(brush);
			}
		});
		connect(this, &HiveWE::palette_changed, unit_palette, &Palette::deactivate);
	}

	if (view == SidebarPaletteView::region && !region_palette) {
		region_palette = new RegionPalette(region_host);
		put_palette_in_host(region_host, region_palette);
		connect(region_palette, &Palette::activated, this, [this]() { activate_palette(region_palette); });
		connect(region_palette, &Palette::ribbon_tab_requested, this, &HiveWE::set_current_custom_tab);
		connect(region_palette, &Palette::tool_page_activated, this, &HiveWE::on_tool_page_activated);
		connect(this, &HiveWE::palette_changed, region_palette, &Palette::deactivate);
	}
}

void HiveWE::reset_palettes() {
	if (!terrain_palette && !pathing_palette && !doodad_palette && !unit_palette && !region_palette) {
		return;
	}

	hide_palette_sidebar();
	// The panel holds no pointers to the selected objects, but its rows describe a brush that is about to go away.
	if (selection_panel) {
		selection_panel->clear_selection();
	}
	delete terrain_palette;
	terrain_palette = nullptr;
	delete pathing_palette;
	pathing_palette = nullptr;
	delete doodad_palette;
	doodad_palette = nullptr;
	delete unit_palette;
	unit_palette = nullptr;
	delete region_palette;
	region_palette = nullptr;
	current_flat_tab = nullptr;
}

void HiveWE::reset_map_session() {
	if (selection_panel) {
		selection_panel->clear_selection();
	}
	delete terrain_palette;
	terrain_palette = nullptr;
	delete pathing_palette;
	pathing_palette = nullptr;
	delete doodad_palette;
	doodad_palette = nullptr;
	delete unit_palette;
	unit_palette = nullptr;
	delete region_palette;
	region_palette = nullptr;
	remove_custom_tab();
	current_flat_tab = nullptr;
	current_sidebar_view = SidebarPaletteView::none;
	set_sidebar_shown(false);
	update_palette_model_preview(nullptr, "Model Preview");
	ui->widget->makeCurrent();
	ui->ribbon->terrain_palette->setChecked(false);
	ui->ribbon->doodad_palette->setChecked(false);
	ui->ribbon->unit_palette->setChecked(false);
	ui->ribbon->pathing_palette->setChecked(false);
	ui->ribbon->region_palette->setChecked(false);
	update_active_palette_visuals();

	delete map;
	resource_manager.clear();
	skinned_mesh_globals.reset();
	map = new Map();

	connect(&map->terrain, &Terrain::minimap_changed, minimap, &Minimap::set_minimap);
	connect(&map->terrain, &Terrain::tileset_changed, [&]() {
		if (terrain_palette) {
			terrain_palette->refresh();
		}
	});
}

void HiveWE::apply_trigger_editing_state(const bool notify) {
	const bool editable = !map || !map->keeps_script;
	const QString off_text = QString::fromUtf8(trigger_editing_off_text.data(), trigger_editing_off_text.size());
	// Every way into the Trigger Editor goes through this button or the global search (which checks keeps_script itself), and every
	// trigger edit happens inside that window, so disabling the button turns all of them off.
	ui->ribbon->trigger_editor->setEnabled(editable);
	ui->ribbon->trigger_editor->setToolTip(editable ? QString() : off_text);
	if (trigger_editor_action) {
		trigger_editor_action->setEnabled(editable);
		trigger_editor_action->setToolTip(editable ? QString("Open the Trigger Editor") : off_text);
		trigger_editor_action->setStatusTip(trigger_editor_action->toolTip());
	}
	if (editable || !notify) {
		return;
	}

	const QString text = QString::fromUtf8(kept_script_notice.data(), kept_script_notice.size());
	if (qa_dialog_log) {
		*qa_dialog_log << text;
	} else if (qa_offscreen && !qa_kept_script_check) {
		// Map::load printed it to stderr already. An automated run has nobody to answer a dialog.
	} else {
		QMessageBox::information(this, "Map Script Kept", text);
	}
}

void HiveWE::update_map_context(const QString& status, const QString& detail_override) {
	QString detail = detail_override;
	QString map_title = QString::fromStdString(map->name);

	if (!map->filesystem_path.empty()) {
		const QFileInfo map_info(display_path(map->filesystem_path));
		setWindowTitle("HiveWE 0.21 - " + display_path(map->filesystem_path));
		map_title = map_info.fileName().isEmpty() ? map_title : map_info.fileName();
		if (detail.isEmpty()) {
			detail = map_info.absoluteFilePath();
		}
	} else {
		setWindowTitle("HiveWE 0.21 - " + map_title + " (Unsaved)");
		if (detail.isEmpty()) {
			detail = "Unsaved map";
		}
	}

	ui->ribbon->set_map_context(map_title, detail, status);
}

bool HiveWE::has_unsaved_work() {
	// The cheap checks first. The last one serializes the whole map.
	if (map->filesystem_path.empty() || map->world_undo.has_changes_since_save()) {
		return true;
	}
	if (const auto editor = window_handler.get_open<TriggerEditor>(); editor && (*editor)->has_pending_changes()) {
		return true;
	}
	return map->differs_from_saved_state();
}

bool HiveWE::confirm_map_replacement(const QString& action) {
	if (!map || !map->loaded) {
		return true;
	}

	// A field that applies when it loses focus (a region name, an Object Editor cell) takes its typed text now, as a click
	// elsewhere would. Ctrl+Q style paths and the title bar's close button leave the focus where it is.
	QWidget* focus = QApplication::focusWidget();
	if (qobject_cast<QLineEdit*>(focus) || qobject_cast<QAbstractSpinBox*>(focus)) {
		focus->clearFocus();
	}

	if (!has_unsaved_work()) {
		return true;
	}

	// An automated offscreen run has nobody to answer, so it is never asked.
	if (qa_offscreen && qa_prompt_answer == 0 && !qa_prompt_real) {
		std::println("Offscreen run: unsaved changes were not asked about ({})", action.toStdString());
		return true;
	}

	QMessageBox message_box(this);
	message_box.setIcon(QMessageBox::Question);
	message_box.setWindowTitle("Unsaved changes");
	message_box.setText("The current map has unsaved changes.");
	message_box.setInformativeText("Do you want to save before you " + action + "?");
	message_box.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
	message_box.setDefaultButton(QMessageBox::Save);

	int answer = qa_prompt_answer;
	if (answer != 0) {
		++qa_prompt_count;
	} else {
		answer = message_box.exec();
	}

	switch (answer) {
		case QMessageBox::Save:
			return save_current_map();
		case QMessageBox::Discard:
			return true;
		default:
			return false;
	}
}

QString HiveWE::sanitized_folder_name(const QString& map_name) {
	QString sanitized = map_name.trimmed();
	if (sanitized.isEmpty()) {
		sanitized = "New Map";
	}

	sanitized.replace(QRegularExpression(R"([<>:"/\\|?*\x00-\x1F])"), "_");
	sanitized.replace(QRegularExpression(R"(\s+)"), " ");
	sanitized = sanitized.trimmed();
	if (sanitized.isEmpty()) {
		sanitized = "New Map";
	}

	return sanitized;
}

void HiveWE::update_active_palette_visuals() {
	const bool sidebar_visible = sidebar_shown() && current_sidebar_view != SidebarPaletteView::none;
	const bool preview_visible = sidebar_visible && (current_sidebar_view == SidebarPaletteView::doodad || current_sidebar_view == SidebarPaletteView::unit);

	auto set_checked = [](QAbstractButton* button, const bool checked) {
		if (!button) {
			return;
		}

		const QSignalBlocker blocker(button);
		button->setChecked(checked);
	};

	set_checked(ui->ribbon->terrain_palette, sidebar_visible && current_sidebar_view == SidebarPaletteView::terrain);
	set_checked(ui->ribbon->doodad_palette, sidebar_visible && current_sidebar_view == SidebarPaletteView::doodad);
	set_checked(ui->ribbon->unit_palette, sidebar_visible && current_sidebar_view == SidebarPaletteView::unit);
	set_checked(ui->ribbon->pathing_palette, sidebar_visible && current_sidebar_view == SidebarPaletteView::pathing);
	set_checked(ui->ribbon->region_palette, sidebar_visible && current_sidebar_view == SidebarPaletteView::region);
	set_checked(terrain_mode_button, sidebar_visible && current_sidebar_view == SidebarPaletteView::terrain);
	set_checked(doodad_mode_button, sidebar_visible && current_sidebar_view == SidebarPaletteView::doodad);
	set_checked(unit_mode_button, sidebar_visible && current_sidebar_view == SidebarPaletteView::unit);
	set_checked(pathing_mode_button, sidebar_visible && current_sidebar_view == SidebarPaletteView::pathing);
	set_checked(region_mode_button, sidebar_visible && current_sidebar_view == SidebarPaletteView::region);

	if (sidebar_preview_frame) {
		sidebar_preview_frame->setVisible(flat_layout_active || preview_visible);
	}

	if (!sidebar_visible) {
		sidebar_title->clear();
		sidebar_description->clear();
		sidebar_hint->clear();
		return;
	}

	sidebar_title->setText(flat_layout_active ? sidebar_flat_title_for_view(current_sidebar_view) : sidebar_title_for_view(current_sidebar_view));
	sidebar_description->setText(sidebar_description_for_view(current_sidebar_view));
	sidebar_hint->setText(sidebar_hint_for_view(current_sidebar_view));
}

void HiveWE::hide_palette_sidebar() {
	const SidebarPaletteView previous_view = current_sidebar_view;
	current_sidebar_view = SidebarPaletteView::none;
	set_sidebar_shown(false);

	if (auto* palette = palette_for_view(previous_view)) {
		if ((terrain_palette && palette == terrain_palette && map->brush == &terrain_palette->current_brush())
			|| (pathing_palette && palette == pathing_palette && map->brush == &pathing_palette->current_brush())
			|| (doodad_palette && palette == doodad_palette && map->brush == &doodad_palette->current_brush())
			|| (unit_palette && palette == unit_palette && map->brush == &unit_palette->current_brush())
			|| (region_palette && palette == region_palette && map->brush == &region_palette->current_brush())) {
			map->brush = nullptr;
		}
	}

	remove_custom_tab();
	update_active_palette_visuals();
	update_ribbon_visibility();
}

void HiveWE::show_palette_view(const SidebarPaletteView view, const bool show_feedback) {
	ensure_palette_view(view);

	auto* host = host_for_view(view);
	auto* palette = palette_for_view(view);
	if (!host || !palette) {
		return;
	}

	current_sidebar_view = view;
	sidebar_stack->setCurrentWidget(host);
	set_sidebar_shown(true);
	switch (view) {
	case SidebarPaletteView::terrain:
		ui->ribbon->show_tab(MainRibbon::HeaderTab::terrain);
		break;
	case SidebarPaletteView::doodad:
		ui->ribbon->show_tab(MainRibbon::HeaderTab::doodads);
		break;
	case SidebarPaletteView::unit:
		ui->ribbon->show_tab(MainRibbon::HeaderTab::units);
		break;
	case SidebarPaletteView::pathing:
		ui->ribbon->show_tab(MainRibbon::HeaderTab::pathing);
		break;
	case SidebarPaletteView::region:
		ui->ribbon->show_tab(MainRibbon::HeaderTab::regions);
		break;
	case SidebarPaletteView::none:
		break;
	}
	update_active_palette_visuals();
	activate_palette(palette, false);
	update_ribbon_visibility();

	if (show_feedback) {
		show_transient_notice(sidebar_feedback_label(view), 850);
	}
}

void HiveWE::activate_palette(Palette* palette, const bool show_feedback) {
	bool already_active = false;
	if (terrain_palette && palette == terrain_palette) {
		already_active = map->brush == &terrain_palette->current_brush();
	} else if (pathing_palette && palette == pathing_palette) {
		already_active = map->brush == &pathing_palette->current_brush();
	} else if (doodad_palette && palette == doodad_palette) {
		already_active = map->brush == &doodad_palette->current_brush();
	} else if (unit_palette && palette == unit_palette) {
		already_active = map->brush == &unit_palette->current_brush();
	} else if (region_palette && palette == region_palette) {
		already_active = map->brush == &region_palette->current_brush();
	}

	if (terrain_palette && pathing_palette) {
		if (auto* terrain = dynamic_cast<TerrainPalette*>(palette)) {
			if (dynamic_cast<PathingBrush*>(map->brush)) {
				const QSignalBlocker blocker(terrain->current_brush());
				terrain->current_brush().set_size(pathing_palette->current_brush().get_size());
			}
		} else if (auto* pathing = dynamic_cast<PathingPalette*>(palette)) {
			if (dynamic_cast<TerrainBrush*>(map->brush)) {
				const QSignalBlocker blocker(pathing->current_brush());
				pathing->current_brush().set_size(terrain_palette->current_brush().get_size());
			}
		}
	}

	if (auto* terrain = dynamic_cast<TerrainPalette*>(palette)) {
		terrain->activate_palette();
	} else if (auto* pathing = dynamic_cast<PathingPalette*>(palette)) {
		pathing->activate_palette();
	} else if (auto* doodad = dynamic_cast<DoodadPalette*>(palette)) {
		doodad->activate_palette();
	} else if (auto* unit = dynamic_cast<UnitPalette*>(palette)) {
		unit->activate_palette();
	} else if (auto* region = dynamic_cast<RegionPalette*>(palette)) {
		region->activate_palette();
	}

	if (terrain_palette) {
		terrain_palette->sync_brush_controls(map->brush);
	}

	update_active_palette_visuals();

	if (!show_feedback || already_active) {
		return;
	}

	QString label = "Palette active";
	if (dynamic_cast<TerrainPalette*>(palette)) {
		label = "Terrain mode";
	} else if (dynamic_cast<PathingPalette*>(palette)) {
		label = "Pathing mode";
	} else if (dynamic_cast<DoodadPalette*>(palette)) {
		label = "Doodad mode";
	} else if (dynamic_cast<UnitPalette*>(palette)) {
		label = "Unit mode";
	} else if (dynamic_cast<RegionPalette*>(palette)) {
		label = "Region mode";
	}

	show_transient_notice(label, 850);
}

void HiveWE::open_qa_palette() {
	if (qa_show_pathing && qa_offscreen) {
		// Through the ribbon button, so its checked state and map->render_pathing stay in step as with Ctrl+P.
		ui->ribbon->pathing_visible->setChecked(true);
	}

	// The same slots the mode buttons and shortcuts run, so no UI automation is needed to open a palette.
	if (qa_palette == "terrain") {
		toggle_terrain_sidebar();
	} else if (qa_palette == "doodads") {
		toggle_doodad_palette();
	} else if (qa_palette == "units") {
		toggle_unit_palette();
	} else if (qa_palette == "pathing") {
		toggle_pathing_sidebar();
	} else if (qa_palette == "regions") {
		toggle_region_palette();
	}

	// Selects the first object of the open palette without a mouse, so the Properties panel can be captured.
	if (!qa_select_first || !qa_offscreen) {
		return;
	}
	// data/test map has only a start location and no doodads, so a sample object is added to the in-memory map (never saved: the QA run is
	// offscreen and only captured).
	if (qa_palette == "units" && unit_palette) {
		auto& brush = unit_palette->current_brush();
		auto unit = std::ranges::find_if(map->units.units, [](const Unit& u) { return u.id != "sloc"; });
		if (unit == map->units.units.end()) {
			Unit& added = map->units.add_unit("hfoo", glm::vec3(30.f, 30.f, 0.f));
			added.angle = glm::radians(270.f);
			added.update_terrain_height(map->terrain);
			unit = map->units.units.end() - 1;
		}
		brush.selections.insert(&*unit);
		emit brush.selection_changed();
	} else if (qa_palette == "doodads" && doodad_palette) {
		auto& brush = doodad_palette->current_brush();
		if (map->doodads.doodads.empty()) {
			Doodad& added = map->doodads.add_doodad("LTlt", 0, glm::vec3(30.f, 30.f, 0.f), map->terrain);
			added.angle = glm::radians(90.f);
		}
		brush.selections.insert(&map->doodads.doodads.front());
		emit brush.selection_changed();
	}
}

void HiveWE::open_qa_dialog() {
	if (!qa_offscreen || qa_open_dialog.isEmpty()) {
		return;
	}

	// The --qa-offscreen filter in main.cpp keeps every window outside all monitors. Nothing below accepts a dialog, so nothing is written
	// to the map or the settings.
	QWidget* shown = nullptr;
	const QString& name = qa_open_dialog;
	if (name == "settings") {
		shown = new SettingsEditor(this);
	} else if (name == "unit-properties" || name == "item-drop") {
		// data/test map has only a start location: add a sample hero with two item sets, in memory only.
		auto unit = std::ranges::find_if(map->units.units, [](const Unit& u) { return u.id != "sloc"; });
		if (unit == map->units.units.end()) {
			Unit& added = map->units.add_unit("Hamg", glm::vec3(30.f, 30.f, 0.f));
			added.angle = glm::radians(270.f);
			added.item_sets = { ItemSet { .items = { { 60, "phea" }, { 40, "stwp" } } }, ItemSet { .items = { { 35, "YiI2" } } } };
			unit = map->units.units.end() - 1;
		}
		if (name == "item-drop") {
			// The editor runs its own event loop, so open it from the loop instead of from startup.
			QTimer::singleShot(0, this, [this]() {
				std::pair<int, std::string> entry { 60, "phea" };
				edit_item_drop_entry(entry, this);
			});
			return;
		}
		shown = new UnitPropertiesDialog(&*unit, this);
		shown->show();
	} else if (name == "map-settings") {
		shown = new MapInfoEditor(this);
	} else if (name == "scenario") {
		shown = new ScenarioPropertiesEditor(this);
	} else if (name == "gameplay-constants") {
		shown = new GameplayConstantsEditor(this);
	} else if (name == "new-map") {
		shown = new NewMapDialog(this);
		shown->show();
	} else if (name == "object-editor") {
		auto* editor = new ObjectEditor(nullptr);
		shown = editor;
		// Opens the details panel of an object (--qa-object, default the stock unit hfoo) so the field table, summary and bookmarks can be
		// captured. With --qa-script-out the visible field rows are written to that file and HiveWE quits.
		static const std::array<std::pair<QStringView, ObjectEditor::Category>, 7> categories = { {
			{ u"unit", ObjectEditor::Category::unit },
			{ u"item", ObjectEditor::Category::item },
			{ u"doodad", ObjectEditor::Category::doodad },
			{ u"destructible", ObjectEditor::Category::destructible },
			{ u"ability", ObjectEditor::Category::ability },
			{ u"upgrade", ObjectEditor::Category::upgrade },
			{ u"buff", ObjectEditor::Category::buff },
		} };
		const QString category_name = qa_object.section(':', 0, 0);
		const std::string id = qa_object.section(':', 1).toStdString();
		const auto category = std::ranges::find_if(categories, [&](const auto& entry) { return entry.first == category_name; });
		if (category == categories.end()) {
			std::println(stderr, "--qa-object: unknown category {}.", category_name.toStdString());
			QTimer::singleShot(0, this, []() { QApplication::exit(2); });
			return;
		}
		QTimer::singleShot(2000, editor, [editor, category = category->second, id]() {
			editor->select_id(category, id);
			if (qa_script_out.isEmpty()) {
				return;
			}
			const QStringList lines = editor->qa_visible_fields(id);
			QFile file(qa_script_out);
			const bool written = file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate);
			if (written) {
				QTextStream stream(&file);
				stream.setEncoding(QStringConverter::Utf8);
				stream << "OBJECT " << qa_object << "\n";
				for (const QString& line : lines) {
					stream << line << "\n";
				}
				stream << "DONE\n";
			}
			QTimer::singleShot(0, editor, [ok = written && !lines.isEmpty()]() { QApplication::exit(ok ? 0 : 3); });
		});
	} else if (name == "asset-manager") {
		auto* asset_manager = new AssetManager(this);
		shown = asset_manager;
		if (!qa_script_out.isEmpty()) {
			QFile file(qa_script_out);
			const bool written = file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate);
			if (written) {
				QTextStream stream(&file);
				for (const QString& line : asset_manager->qa_report()) {
					stream << line << "\n";
				}
				stream << "DONE\n";
			}
			QTimer::singleShot(0, this, [written]() { QApplication::exit(written ? 0 : 3); });
		}
	} else if (name == "map-protector") {
		shown = new MapProtector(nullptr);
		shown->show();
	} else if (name == "item-tables") {
		shown = new ItemTablesEditor(this);
	} else if (name == "config") {
		shown = new ShortcutConfigDialog(this);
	} else if (name == "model-editor") {
		// Opens the Model Editor with a stock model, from the event loop so the editor's own GL widget is created after the window exists.
		bool created = false;
		auto* editor = window_handler.create_or_raise<ModelEditor>(nullptr, created);
		shown = editor;
		QTimer::singleShot(0, editor, [editor]() {
			// No message box here: a modal box in an offscreen run would hang it. Report the failure to the result file and exit instead.
			std::string error;
			// An absolute path to a file on disk opens like File > Open (its folder's textures count), anything else is a game or map path.
			const std::filesystem::path model(qa_model.toStdWString());
			std::error_code ec;
			const bool local_file = model.is_absolute() && std::filesystem::is_regular_file(model, ec);
			if (!editor->show_model(model, local_file, nullptr, &error)) {
				std::println(stderr, "--qa-open-dialog model-editor: cannot open {}: {}", qa_model.toStdString(), error);
				if (!qa_script_out.isEmpty()) {
					QFile file(qa_script_out);
					if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
						QTextStream(&file) << "FAIL [open model] " << qa_model << ": " << QString::fromStdString(error) << "\nDONE\n";
					}
				}
				QApplication::exit(3);
				return;
			}
			if (!qa_model_editor_script.isEmpty()) {
				editor->run_qa_script(qa_model_editor_script, qa_script_out);
			}
		});
	} else if (name == "browse-models") {
		// Never accepted, so no model is opened. The dialog writes the result file and quits the application itself.
		auto* browser = new BrowseModelsDialog(this);
		shown = browser;
		browser->show();
		if (!qa_script_out.isEmpty()) {
			QTimer::singleShot(0, browser, [browser]() { browser->run_qa(qa_script_out); });
		}
	} else if (name == "tile-setter") {
		shown = new TileSetter(this);
	} else if (name == "tile-setter-changes") {
		// Same dialog with one tile added and one removed, never accepted.
		auto* setter = new TileSetter(this);
		shown = setter;
		setter->qa_apply_sample_changes();
	} else if (name == "tile-setter-limits") {
		// Reduces the list to one tile and to none, prints the OK and Remove button states (to stdout and to --qa-script-out), never accepted.
		auto* setter = new TileSetter(this);
		shown = setter;
		setter->qa_report_limits(qa_script_out);
	} else if (name == "tile-pather") {
		shown = new TilePather(this);
	} else if (name == "import-object-data") {
		// An empty package: the dialog shows its tabs and buttons with empty tables. Never accepted, so nothing is imported.
		ObjectDataImportPackage package;
		package.format = "qa-sample";
		shown = new ObjectDataImportDialog(std::move(package), this);
		shown->show();
	} else {
		std::println(stderr, "--qa-open-dialog {}: unknown dialog name.", name.toStdString());
		return;
	}

	if (auto* tabs = shown->findChild<QTabWidget*>(); tabs && qa_dialog_tab >= 0 && qa_dialog_tab < tabs->count()) {
		tabs->setCurrentIndex(qa_dialog_tab);
	}
}

void HiveWE::run_qa_doodad_undo_check() {
	if (!qa_offscreen || !qa_doodad_undo_check) {
		return;
	}

	// From the event loop, like the other hooks, so QApplication::exit works.
	QTimer::singleShot(0, this, [this]() {
		QStringList lines;
		bool all_passed = true;
		const auto check = [&](const bool passed, const QString& text) {
			lines << (passed ? "PASS " : "FAIL ") + text;
			all_passed = all_passed && passed;
		};
		const auto finish = [&]() {
			for (const QString& line : lines) {
				std::println("{}", line.toStdString());
			}
			if (!qa_script_out.isEmpty()) {
				QFile file(qa_script_out);
				if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
					file.write(lines.join(QChar('\n')).toUtf8() + "\nDONE\n");
				}
			}
			QApplication::exit(all_passed ? 0 : 1);
		};

		if (!doodad_palette) {
			check(false, "the doodad palette is not open (use --palette doodads)");
			return finish();
		}
		DoodadBrush& brush = doodad_palette->current_brush();
		auto& doodads = map->doodads.doodads;

		struct Snapshot {
			std::string id;
			int creation_number;
			glm::vec3 position;
		};
		const auto undo_or_redo = [&](const bool undo) {
			auto context = WorldEditContext {
				.terrain = map->terrain,
				.units = map->units,
				.doodads = map->doodads,
				.regions = map->regions,
				.brush = &brush,
				.pathing_map = map->pathing_map,
			};
			if (undo) {
				map->world_undo.undo(context);
			} else {
				map->world_undo.redo(context);
			}
		};
		// True when every remembered doodad is in the map exactly once with the same id and position, found by creation number.
		const auto all_present = [&](const std::vector<Snapshot>& expected) {
			for (const Snapshot& want : expected) {
				const auto found = std::ranges::count_if(doodads, [&](const Doodad& d) { return d.creation_number == want.creation_number; });
				const auto it = std::ranges::find_if(doodads, [&](const Doodad& d) { return d.creation_number == want.creation_number; });
				if (found != 1 || it->id != want.id || it->position != want.position) {
					return false;
				}
			}
			return true;
		};
		const auto none_present = [&](const std::vector<Snapshot>& expected) {
			return std::ranges::none_of(expected, [&](const Snapshot& want) {
				return std::ranges::any_of(doodads, [&](const Doodad& d) { return d.creation_number == want.creation_number; });
			});
		};
		// Re-selects the remembered doodads: removing and re-inserting moves them, so earlier pointers are stale.
		const auto select_all = [&](const std::vector<Snapshot>& expected) {
			brush.selections.clear();
			for (const Snapshot& want : expected) {
				for (Doodad& d : doodads) {
					if (d.creation_number == want.creation_number) {
						brush.selections.insert(&d);
					}
				}
			}
		};

		// Three doodads with distinct creation numbers and positions, added after the map has loaded. Never saved.
		const std::size_t before = doodads.size();
		std::vector<Snapshot> added;
		for (int i = 0; i < 3; i++) {
			const Doodad& d = map->doodads.add_doodad("LTlt", 0, glm::vec3(30.f + 10.f * i, 40.f + 6.f * i, 0.f), map->terrain);
			added.push_back({ d.id, d.creation_number, d.position });
		}
		check(
			added[0].creation_number != added[1].creation_number && added[1].creation_number != added[2].creation_number
				&& added[0].creation_number != added[2].creation_number,
			"the three added doodads have distinct creation numbers"
		);
		check(doodads.size() == before + 3 && all_present(added), "the doodads were added");

		// Delete selection.
		select_all(added);
		check(brush.selections.size() == 3, "three doodads are selected");
		brush.delete_selection();
		check(doodads.size() == before && none_present(added), "delete: the count is back to the pre-add count and the doodads are gone");
		check(brush.selections.empty(), "delete: the selection is empty");

		undo_or_redo(true);
		check(doodads.size() == before + 3 && all_present(added), "delete, undo: the three doodads are back with the same ids, positions and creation numbers");
		undo_or_redo(false);
		check(doodads.size() == before && none_present(added), "delete, redo: the doodads are gone again");
		undo_or_redo(true);
		check(doodads.size() == before + 3 && all_present(added), "delete, second undo: the three doodads are back again");

		// Cut selection: copies to the clipboard, then deletes through delete_selection.
		select_all(added);
		brush.cut_selection();
		check(doodads.size() == before && none_present(added) && brush.selections.empty(), "cut: the doodads are gone and the selection is empty");
		undo_or_redo(true);
		check(doodads.size() == before + 3 && all_present(added), "cut, undo: the three doodads are back with the same ids, positions and creation numbers");

		finish();
	});
}

void HiveWE::run_qa_merge_export_check() {
	if (!qa_offscreen || qa_merge_export_check.isEmpty()) {
		return;
	}

	// Nothing here opens a dialog, but a hang must still end the run.
	QTimer::singleShot(180000, this, []() {
		std::println("FAIL the merge export check did not finish within 180 s");
		std::fflush(stdout);
		std::_Exit(3);
	});

	QTimer::singleShot(0, this, [this]() {
		QStringList lines;
		bool all_passed = true;
		// Lines are printed and flushed as they come, so a run that hangs or is killed still shows how far it got.
		const auto add_line = [&](const QString& line) {
			lines << line;
			std::println("{}", line.toStdString());
			std::fflush(stdout);		};
		const auto check = [&](const bool passed, const QString& text) {
			add_line((passed ? "PASS " : "FAIL ") + text);
			all_passed = all_passed && passed;
			return passed;
		};
		const auto note = [&](const QString& text) { add_line("INFO " + text); };
		note("merge export check started");
		const auto finish = [&]() {
			if (!qa_script_out.isEmpty()) {
				QFile file(qa_script_out);
				if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
					file.write(lines.join(QChar('\n')).toUtf8() + "\nDONE\n");
				}
			}
			QApplication::exit(all_passed ? 0 : 1);
		};

		// Everything is written into the out folder: the map copy and the merged models. The source map is only read.
		std::error_code ec;
		const fs::path out = qa_merge_export_check.toStdWString();
		const fs::path source = qa_merge_map.isEmpty() ? fs::path("data") / "test map" : fs::path(qa_merge_map.toStdWString());
		// The out folder receives a copy of the map, so it must not be the map folder or inside it (paths compared without case)
		const auto folded = [](const fs::path& path) {
			std::error_code error;
			std::wstring text = fs::weakly_canonical(fs::absolute(path, error), error).generic_wstring();
			std::ranges::transform(text, text.begin(), [](const wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
			while (text.ends_with(L'/')) {
				text.pop_back();
			}
			return text;
		};
		const std::wstring source_text = folded(source);
		const std::wstring out_text = folded(out);
		if (!check(out_text != source_text && !out_text.starts_with(source_text + L'/'), "the out folder is not the map folder or inside it")) {
			return finish();
		}
		if (!check(!fs::exists(out, ec) || fs::is_empty(out, ec), "the out folder is absent or empty")) {
			return finish();
		}
		fs::create_directories(out, ec);
		const fs::path copy = out / "map copy";
		fs::copy(source, copy, fs::copy_options::recursive, ec);
		if (!check(!ec && fs::exists(copy / "war3map.w3i"), "the map was copied into the out folder: " + QString::fromStdWString(source.wstring()))) {
			return finish();
		}
		if (const auto opening = check_map_for_opening(copy); !opening) {
			check(false, "the map copy is refused at open: " + QString::fromStdString(opening.error()));
			return finish();
		}
		try {
			window_handler.close_all();
			ui->widget->makeCurrent();
			reset_map_session();
			map->load(copy);
			map->headless = true;
			map->render_manager.resize_framebuffers(ui->widget->width(), ui->widget->height());
		} catch (const std::exception& error) {
			check(false, QString("the map copy failed to load: %1").arg(QString::fromLocal8Bit(error.what())));
			return finish();
		}
		check(fs::equivalent(map->filesystem_path, copy, ec), "the map copy is the open map");

		const auto read_mdx = [](const std::vector<char>& bytes) {
			BinaryReader reader(decltype(BinaryReader::buffer)(bytes.begin(), bytes.end()));
			return mdx::MDX(reader);
		};
		struct Totals {
			size_t geosets = 0;
			size_t vertices = 0;
			size_t face_indices = 0;
			size_t global_sequences = 0;
			size_t global_tracks = 0;
			size_t file_geosets = 0; // before geosets hidden at Stand start are left out
		};
		// The global sequence id of every track kind the merge export keeps (-1 included; an absent track reads -1)
		const auto global_track_ids = [](const mdx::MDX& model) {
			std::vector<int32_t> ids;
			for (const auto& animation : model.animations) {
				ids.push_back(animation.KGAO.global_sequence_ID);
				ids.push_back(animation.KGAC.global_sequence_ID);
			}
			for (const auto& material : model.materials) {
				for (const auto& layer : material.layers) {
					for (const int32_t id : { layer.KMTA.global_sequence_ID, layer.KMTE.global_sequence_ID, layer.KFC3.global_sequence_ID,
											  layer.KFCA.global_sequence_ID, layer.KFTC.global_sequence_ID }) {
						ids.push_back(id);
					}
					for (const auto& slot : layer.textures) {
						ids.push_back(slot.KMTF.global_sequence_ID);
					}
				}
			}
			for (const auto& animation : model.texture_animations) {
				ids.push_back(animation.KTAT.global_sequence_ID);
				ids.push_back(animation.KTAR.global_sequence_ID);
				ids.push_back(animation.KTAS.global_sequence_ID);
			}
			return ids;
		};
		const auto totals_of = [&](const mdx::MDX& model) {
			Totals result;
			result.geosets = model.geosets.size();
			result.global_sequences = model.global_sequences.size();
			result.global_tracks = std::ranges::count_if(global_track_ids(model), [](const int32_t id) { return id >= 0; });
			for (const auto& geoset : model.geosets) {
				result.vertices += geoset.vertices.size();
				result.face_indices += geoset.faces.size();
			}
			return result;
		};

		// Doodads grouped by model file, in map order. A model is read only when it is a candidate.
		std::vector<std::pair<fs::path, std::vector<Doodad*>>> by_model;
		const auto group_doodads = [&]() {
			by_model.clear();
			for (Doodad& doodad : map->doodads.doodads) {
				if (!doodad.mesh) {
					continue;
				}
				auto group = std::ranges::find_if(by_model, [&](const auto& g) { return g.first == doodad.mesh->path; });
				if (group == by_model.end()) {
					by_model.emplace_back(doodad.mesh->path, std::vector<Doodad*>());
					group = std::prev(by_model.end());
				}
				group->second.push_back(&doodad);
			}
		};
		group_doodads();
		std::map<fs::path, Totals> source_totals;
		const auto has_geometry = [&](const fs::path& path) {
			if (!source_totals.contains(path)) {
				Totals totals;
				try {
					if (auto file = hierarchy.open_file(path)) {
						// What the export takes from the model: its geometry shown at the start of Stand
						const mdx::MDX model(*file);
						totals = totals_of(mdx::frozen_to_stand(model));
						totals.file_geosets = model.geosets.size();
					}
				} catch (const std::exception&) {
				}
				source_totals[path] = totals;
			}
			return source_totals[path].vertices > 0 && source_totals[path].face_indices > 0;
		};
		// The placeholder HiveWE shows for a doodad whose model is missing is not a real model
		const auto is_placeholder = [](const fs::path& path) {
			std::wstring name = path.filename().wstring();
			std::ranges::transform(name, name.begin(), [](const wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
			return name == L"invalidmodel.mdx";
		};
		const auto usable = [&](const fs::path& path) { return !is_placeholder(path) && has_geometry(path); };
		const auto find_first = [&]() {
			return std::ranges::find_if(by_model, [&](const auto& g) { return g.second.size() >= 2 && usable(g.first); });
		};
		const auto find_second = [&](const fs::path& first_path) {
			return std::ranges::find_if(by_model, [&](const auto& g) { return g.first != first_path && usable(g.first); });
		};
		auto first = find_first();
		if (first == by_model.end() || find_second(first->first) == by_model.end()) {
			// The map lacks the doodads (data/test map has none): place stock ones in memory, never saved.
			note("the map has no two doodads of one model plus one of another: placing stock doodads in memory");
			const auto place = [&](const std::string& id, const glm::vec3 position) {
				try {
					const Doodad& placed = map->doodads.add_doodad(id, 0, position, map->terrain);
					return placed.mesh ? placed.mesh->path : fs::path();
				} catch (const std::exception& error) {
					note(QString("could not place %1: %2").arg(QString::fromStdString(id), QString::fromLocal8Bit(error.what())));
					return fs::path();
				}
			};
			place("LTlt", glm::vec3(30.f, 40.f, 0.f));
			const fs::path tree = place("LTlt", glm::vec3(33.f, 41.f, 0.f));
			// The second model: the first destructible in table order with a real model other than the tree's
			for (size_t row = 0; row < destructibles_slk.index_to_row.size() && row < 200; row++) {
				const auto id = destructibles_slk.index_to_row.find(row);
				if (id == destructibles_slk.index_to_row.end() || id->second == "LTlt") {
					continue;
				}
				if (const fs::path placed = place(id->second, glm::vec3(36.f, 38.f, 0.f)); !placed.empty() && placed != tree && usable(placed)) {
					break;
				}
			}
			group_doodads();
			first = find_first();
		}
		if (!check(first != by_model.end(), "the map has two doodads of one model with geometry")) {
			return finish();
		}
		const auto second = find_second(first->first);
		if (!check(second != by_model.end(), "the map has a doodad of a second model with geometry")) {
			return finish();
		}

		std::unordered_set<Doodad*> selection = { first->second[0], first->second[1], second->second[0] };
		Totals expected;
		for (const Doodad* doodad : selection) {
			const Totals& t = source_totals.at(doodad->mesh->path);
			note(QString("selected %1 (%2): %3 geosets (%4 shown at Stand start), %5 vertices, %6 faces, %7 global sequences, %8 tracks on them")
					 .arg(QString::fromStdString(doodad->id), QString::fromStdWString(doodad->mesh->path.wstring()))
					 .arg(t.file_geosets)
					 .arg(t.geosets)
					 .arg(t.vertices)
					 .arg(t.face_indices / 3)
					 .arg(t.global_sequences)
					 .arg(t.global_tracks));
			expected.vertices += t.vertices;
			expected.face_indices += t.face_indices;
			expected.global_sequences += t.global_sequences;
			expected.global_tracks += t.global_tracks;
		}

		// The same function the Merge Selection button runs after its file dialog
		const fs::path target = out / "merged.mdx";
		bool threw = false;
		std::expected<void, std::string> exported;
		try {
			exported = DoodadPalette::export_merged_selection(selection, target);
		} catch (...) {
			threw = true;
		}
		if (!check(!threw && exported.has_value(), "the merge export succeeded: " + QString::fromStdString(exported ? std::string("written") : exported.error()))) {
			return finish();
		}

		std::ifstream file(target, std::ios::binary);
		const std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		mdx::MDX merged;
		try {
			merged = read_mdx(bytes);
		} catch (const std::exception& error) {
			check(false, QString("the merged .mdx is readable: %1").arg(QString::fromLocal8Bit(error.what())));
			return finish();
		}
		check(true, QString("the merged .mdx (%1 bytes) is readable").arg(bytes.size()));
		const Totals result = totals_of(merged);
		note(QString("merged: %1 geosets, %2 vertices, %3 faces").arg(result.geosets).arg(result.vertices).arg(result.face_indices / 3));
		check(result.vertices == expected.vertices, QString("vertex total %1, the sources have %2").arg(result.vertices).arg(expected.vertices));
		check(result.face_indices == expected.face_indices,
			  QString("face total %1, the sources have %2").arg(result.face_indices / 3).arg(expected.face_indices / 3));

		size_t out_of_range = 0;
		size_t unused_vertices = 0;
		size_t bad_groups = 0;
		size_t bad_materials = 0;
		for (const auto& geoset : merged.geosets) {
			std::vector<bool> used(geoset.vertices.size(), false);
			for (const uint16_t face : geoset.faces) {
				if (face >= geoset.vertices.size()) {
					out_of_range += 1;
				} else {
					used[face] = true;
				}
			}
			unused_vertices += std::ranges::count(used, false);
			const uint64_t grouped = std::accumulate(geoset.face_groups.begin(), geoset.face_groups.end(), uint64_t { 0 });
			bad_groups += geoset.face_groups.size() != geoset.face_type_groups.size() || grouped != geoset.faces.size();
			bad_materials += geoset.material_id >= merged.materials.size();
		}
		check(out_of_range == 0, QString("face indices outside their geoset: %1").arg(out_of_range));
		check(bad_groups == 0, QString("geosets whose face groups disagree with their faces: %1").arg(bad_groups));
		check(bad_materials == 0, QString("geosets with a material id outside the list: %1").arg(bad_materials));

		// Every track that reaches the merged file names one of its global sequences or none
		check(merged.global_sequences.size() == expected.global_sequences,
			  QString("global sequences %1, the sources have %2").arg(merged.global_sequences.size()).arg(expected.global_sequences));
		const auto global_ids = global_track_ids(merged);
		const size_t on_global = std::ranges::count_if(global_ids, [](const int32_t id) { return id >= 0; });
		const size_t bad_global = std::ranges::count_if(global_ids, [&](const int32_t id) {
			return id != -1 && (id < 0 || static_cast<size_t>(id) >= merged.global_sequences.size());
		});
		note(QString("tracks on a global sequence: %1 (the sources have %2)").arg(on_global).arg(expected.global_tracks));
		check(bad_global == 0, QString("tracks naming a global sequence the file does not have: %1").arg(bad_global));
		note(QString("vertices no face uses: %1 (the sources may have some too)").arg(unused_vertices));

		// Failure path: the out folder itself is not a writable file. An error result, no throw, no dialog.
		threw = false;
		std::expected<void, std::string> refused;
		try {
			refused = DoodadPalette::export_merged_selection(selection, out);
		} catch (...) {
			threw = true;
		}
		check(!threw && !refused.has_value(),
			  "an unwritable target gives an error result: " + QString::fromStdString(refused ? std::string("no error") : refused.error()));
		threw = false;
		try {
			refused = DoodadPalette::export_merged_selection({}, out / "empty.mdx");
		} catch (...) {
			threw = true;
		}
		check(!threw && !refused.has_value() && !fs::exists(out / "empty.mdx"), "an empty selection gives an error result and writes nothing");

		finish();
	});
}

void HiveWE::run_qa_object_new_check() {
	if (!qa_offscreen || !qa_object_new_check) {
		return;
	}

	// A dialog that blocks (exec, message box) would hang the run: end it instead.
	QTimer::singleShot(60000, this, []() {
		std::println("FAIL the object new check did not finish within 60 s: a dialog is probably blocking");
		std::fflush(stdout);
		std::_Exit(3);
	});

	QTimer::singleShot(0, this, [this]() {
		QStringList lines;
		auto* editor = new ObjectEditor(nullptr); // the constructor calls show(); safe only because --qa-offscreen is required
		const bool passed = editor->run_qa_new_object_check(lines);
		for (const QString& line : lines) {
			std::println("{}", line.toStdString());
		}
		if (!qa_script_out.isEmpty()) {
			QFile file(qa_script_out);
			if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
				file.write(lines.join(QChar('\n')).toUtf8() + "\nDONE\n");
			}
		}
		QApplication::exit(passed ? 0 : 1);
	});
}

void HiveWE::run_qa_region_palette_check() {
	if (!qa_offscreen || !qa_region_palette_check) {
		return;
	}

	// From the event loop, like the other hooks, so QApplication::exit works. Opens no dialog and writes nothing.
	QTimer::singleShot(0, this, [this]() {
		QStringList lines;
		bool all_passed = true;
		const auto check = [&](const bool passed, const QString& text) {
			lines << (passed ? "PASS " : "FAIL ") + text;
			all_passed = all_passed && passed;
			return passed;
		};
		const auto finish = [&]() {
			for (const QString& line : lines) {
				std::println("{}", line.toStdString());
			}
			if (!qa_script_out.isEmpty()) {
				QFile file(qa_script_out);
				if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
					file.write(lines.join(QChar('\n')).toUtf8() + "\nDONE\n");
				}
			}
			QApplication::exit(all_passed ? 0 : 1);
		};

		// Each step runs only when the one before it passed.
		if (!check(region_palette != nullptr, "the Regions palette is open (use --palette regions)")) {
			return finish();
		}
		const auto& regions = map->regions.regions;
		// A map without regions would pass the comparison below without proving anything.
		if (!check(!regions.empty(), QString("the loaded map has regions (%1)").arg(regions.size()))) {
			return finish();
		}
		const QStringList listed = region_palette->listed_region_names();
		if (!check(listed.size() == static_cast<int>(regions.size()), QString("the palette lists %1 rows, the map has %2 regions").arg(listed.size()).arg(regions.size()))) {
			return finish();
		}
		for (int i = 0; i < listed.size(); ++i) {
			const QString expected = QString::fromStdString(regions[i].name);
			check(listed[i] == expected, QString("row %1 shows \"%2\", the map's region is \"%3\"").arg(i).arg(listed[i], expected));
		}
		finish();
	});
}

void HiveWE::run_qa_refusal_checks() {
	if (!qa_offscreen || !qa_refusal_checks) {
		return;
	}

	// A refusal that stops working would open a folder picker or a message box that nobody can close offscreen: end the run instead of hanging.
	// Everything below works on copies in a temporary folder: the startup map folder is never written.
	const auto temp = std::make_shared<QTemporaryDir>();
	QTimer::singleShot(120000, this, [temp]() {
		std::println("FAIL the refusal checks did not finish within 120 s: a dialog is probably blocking (sandbox was {})", temp->path().toStdString());
		std::fflush(stdout);
		temp->remove(); // _Exit runs no destructors
		std::_Exit(3);
	});

	QTimer::singleShot(0, this, [this, temp]() {
		QStringList lines;
		bool all_passed = true;
		const auto check = [&](const bool passed, const QString& text) {
			lines << (passed ? "PASS " : "FAIL ") + text;
			all_passed = all_passed && passed;
		};
		const auto finish = [&]() {
			for (const QString& line : lines) {
				std::println("{}", line.toStdString());
			}
			if (!qa_script_out.isEmpty()) {
				QFile file(qa_script_out);
				if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
					file.write(lines.join(QChar('\n')).toUtf8() + "\nDONE\n");
				}
			}
			QApplication::exit(all_passed ? 0 : 1);
		};

		if (!temp->isValid()) {
			check(false, "could not create a temporary folder");
			return finish();
		}
		std::error_code ec;
		const fs::path root = temp->path().toStdWString();
		const fs::path good = root / "good map";
		const fs::path damaged = root / "damaged map";
		fs::copy(fs::path("data") / "test map", good, fs::copy_options::recursive, ec);
		check(!ec, "data/test map copied to the sandbox");
		fs::copy(good, damaged, fs::copy_options::recursive, ec);
		check(!ec, "the sandbox copy copied again as the damaged map");
		if (!all_passed) {
			return finish();
		}
		{
			// The terrain file's magic is wrong, which the opening check refuses.
			std::fstream file(damaged / "war3map.w3e", std::ios::in | std::ios::out | std::ios::binary);
			file.write("XXXX", 4);
		}
		check(!check_map_for_opening(damaged), "the damaged map is refused by the opening check");

		// Load the good copy like Open does, without the recent-maps registry write.
		if (const auto opening = check_map_for_opening(good); !opening) {
			check(false, "the sandbox map is refused at open: " + QString::fromStdString(opening.error()));
			return finish();
		}
		window_handler.close_all();
		ui->widget->makeCurrent();
		reset_map_session();
		map->load(good);
		map->headless = true; // refusals print to stderr instead of opening a message box
		map->render_manager.resize_framebuffers(ui->widget->width(), ui->widget->height());
		check(fs::equivalent(map->filesystem_path, good, ec) && fs::equivalent(hierarchy.map_directory, good, ec), "the sandbox map is the open map");

		// Folder contents as (relative path, size, FNV-1a hash of the bytes).
		using Listing = std::map<std::wstring, std::pair<std::uintmax_t, std::uint64_t>>;
		bool snapshot_failed = false;
		const auto snapshot = [&snapshot_failed](const fs::path& directory) {
			Listing result;
			std::error_code error;
			fs::recursive_directory_iterator it(directory, error);
			for (; !error && it != fs::recursive_directory_iterator(); it.increment(error)) {
				const auto& entry = *it;
				if (!entry.is_regular_file()) {
					continue;
				}
				std::ifstream stream(entry.path(), std::ios::binary);
				std::uint64_t hash = 1469598103934665603ull;
				for (std::istreambuf_iterator<char> it(stream), end; it != end; ++it) {
					hash = (hash ^ static_cast<unsigned char>(*it)) * 1099511628211ull;
				}
				result[entry.path().lexically_relative(directory).wstring()] = { entry.file_size(), hash };
			}
			if (error) {
				snapshot_failed = true;
			}
			return result;
		};

		// Item 1: a placed unit with a 5-byte id.
		Unit& bad_unit = map->units.add_unit("hfoo", glm::vec3(64.f, 64.f, 0.f));
		bad_unit.id = "hfoo5";
		check(map->units.find_id_over_4_bytes().has_value(), "a unit with the 5-byte id hfoo5 is placed");

		const Listing before = snapshot(good);
		check(!snapshot_failed && before.contains(L"war3map.w3i"), "the folder listing before the refusals is complete (contains war3map.w3i)");
		if (!all_passed) {
			return finish();
		}
		const fs::path path_before = map->filesystem_path;
		const fs::path hierarchy_before = hierarchy.map_directory;
		const auto unchanged = [&](const QString& what) {
			const Listing after = snapshot(good);
			check(!snapshot_failed && after == before, what + ": no file in the map folder was written, changed or added");
			check(map->filesystem_path == path_before && hierarchy.map_directory == hierarchy_before, what + ": the map's paths are unchanged");
		};

		// A broken refusal must never reach a picker or a launch: stop at the first call that was not refused. Save As would open a
		// folder picker, Test Map would start Warcraft III.exe and Export MPQ would open a file picker.
		const bool save_refused = !save_current_map();
		check(save_refused, "Save is refused");
		unchanged("Save");
		if (!save_refused) {
			return finish();
		}
		const bool save_as_refused = !save_current_map_as();
		check(save_as_refused, "Save As is refused before the folder picker");
		unchanged("Save As");
		if (!save_as_refused) {
			return finish();
		}
		// Both save entry points refuse, and play_test and export_mpq begin with save_current_map(), so they return before any launch or picker.
		try {
			play_test();
		} catch (const std::exception& e) {
			check(false, QString("Test Map threw: ") + e.what());
		}
		unchanged("Test Map");
		export_mpq();
		unchanged("Export MPQ");

		// Control: without the bad unit the same Save works, so the refusals above came from the id.
		std::erase_if(map->units.units, [](const Unit& unit) { return unit.id == "hfoo5"; });
		check(!map->units.find_id_over_4_bytes().has_value(), "the 5-byte unit is removed again");
		check(save_current_map(), "control: Save succeeds without the 5-byte unit");

		// Item 4: undo survives a refused Open.
		const std::size_t units_before = map->units.units.size();
		map->world_undo.new_undo_group();
		auto action = std::make_unique<UnitAddAction>();
		const Unit& edit = map->units.add_unit("hfoo", glm::vec3(96.f, 96.f, 0.f));
		const int edit_number = edit.creation_number;
		action->units.push_back(edit);
		map->world_undo.add_undo_action(std::move(action));
		const auto has_edit = [&]() {
			return std::ranges::any_of(map->units.units, [&](const Unit& unit) { return unit.creation_number == edit_number; });
		};
		check(map->units.units.size() == units_before + 1 && has_edit(), "an edit is made: a unit is added with an undo entry");

		const Map* const map_before = map;
		QStringList dialogs;
		qa_dialog_log = &dialogs;
		load_map(damaged);
		qa_dialog_log = nullptr;
		check(dialogs.size() == 1, "Open of the damaged map was refused with one message");
		check(map == map_before && fs::equivalent(map->filesystem_path, good, ec), "the open map is the same Map object after the refusal");
		check(map->units.units.size() == units_before + 1 && has_edit(), "the edit is still in the map after the refusal");

		auto context = WorldEditContext {
			.terrain = map->terrain,
			.units = map->units,
			.doodads = map->doodads,
			.regions = map->regions,
			.brush = nullptr,
			.pathing_map = map->pathing_map,
		};
		map->world_undo.undo(context);
		check(map->units.units.size() == units_before && !has_edit(), "undo after the refusal removes the edit");
		map->world_undo.redo(context);
		check(map->units.units.size() == units_before + 1 && has_edit(), "redo after the refusal brings the edit back");
		map->world_undo.undo(context);
		check(map->units.units.size() == units_before && !has_edit(), "a second undo removes the edit again");

		// Item 5: war3map.w3u holds a custom unit h0zz based on Zzzz, which no table has; the opening check reads only the file's
		// layout. (1220330336212700 made Map::load throw here; 1220369536243160 made it skip the unit instead.)
		const fs::path baseless = root / "baseless map";
		fs::copy(good, baseless, fs::copy_options::recursive, ec);
		check(!ec, "the sandbox copy copied again as the map with an unknown base id");
		{
			std::ofstream file(baseless / "war3map.w3u", std::ios::binary | std::ios::trunc);
			const auto u32 = [&file](const std::uint32_t value) {
				file.write(reinterpret_cast<const char*>(&value), sizeof(value));
			};
			u32(2); // version
			u32(0); // original table: no objects
			u32(1); // custom table: one object
			file.write("Zzzz", 4);
			file.write("h0zz", 4);
			u32(0); // no modifications
		}
		check(check_map_for_opening(baseless).has_value(), "control: the opening check accepts the map with the unknown base id");
		dialogs.clear();
		qa_dialog_log = &dialogs;
		load_map(baseless);
		qa_dialog_log = nullptr;
		// Asana 1220369536243160: an unknown base id no longer fails the load; the custom unit is skipped (and reported on stderr).
		check(dialogs.size() == 1, QString("Open of the map with the unknown base id gave one warning (%1)").arg(dialogs.size()));
		if (!dialogs.isEmpty()) {
			check(dialogs.front().contains("war3map.w3u: h0zz (based on Zzzz)") && dialogs.front().contains("dropped"),
				  "the warning names the file, the custom unit and its base, and says it is dropped on save: " + dialogs.front());
		}
		check(map->skipped_objects.size() == 1 && map->skipped_placed_units == 0, "the map records the one skipped object");
		check(map->loaded && !map->filesystem_path.empty() && fs::equivalent(map->filesystem_path, baseless, ec), "the map with the unknown base id is open");
		check(fs::equivalent(hierarchy.map_directory, baseless, ec), "the hierarchy points at the opened map");
		check(!units_slk.row_headers.contains("h0zz") && !units_slk.shadow_data.contains("h0zz"), "the custom unit with the unknown base is not in the unit table");

		finish();
	});
}

namespace {
	/// A world edit that does nothing, so the hook can see whether the undo history survives.
	class QaNoopCommand final : public WorldCommand {
	  public:
		void undo(WorldEditContext&) override {}
		void redo(WorldEditContext&) override {}
	};
}

void HiveWE::run_qa_tileset_clipboard_check() {
	if (!qa_offscreen || !qa_tileset_clipboard_check) {
		return;
	}

	QTimer::singleShot(0, this, [this]() {
		QStringList lines;
		bool all_passed = true;
		const auto check = [&](const bool passed, const QString& text) {
			lines << (passed ? "PASS " : "FAIL ") + text;
			all_passed = all_passed && passed;
		};
		const auto finish = [&]() {
			for (const QString& line : lines) {
				std::println("{}", line.toStdString());
			}
			if (!qa_script_out.isEmpty()) {
				QFile file(qa_script_out);
				if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
					file.write(lines.join(QChar('\n')).toUtf8() + "\nDONE\n");
				}
			}
			QApplication::exit(all_passed ? 0 : 1);
		};

		if (!terrain_palette) {
			check(false, "the terrain palette is not open (use --palette terrain)");
			return finish();
		}
		std::error_code ec;
		const fs::path texture = fs::absolute(fs::path(qa_tile_texture.toStdWString()));
		if (qa_tile_texture.isEmpty() || !fs::is_regular_file(texture, ec)) {
			check(false, "--qa-tile-texture <blp|tga> must name an existing file");
			return finish();
		}
		map->headless = true; // no message boxes: they would hang an offscreen run
		// The texture override is written into the asset lookup's map folder: point it at a fresh temporary folder, never at the startup
		// map (data/test map is a symlink into the repository).
		QTemporaryDir scratch;
		if (!scratch.isValid()) {
			check(false, "could not create a temporary folder for the texture override");
			return finish();
		}
		hierarchy.map_directory = fs::path(scratch.path().toStdWString());
		TerrainBrush& brush = terrain_palette->current_brush();

		const auto add_history_and_clipboard = [&]() {
			map->world_undo.new_undo_group();
			map->world_undo.add_undo_action(std::make_unique<QaNoopCommand>());
			brush.qa_fill_clipboard();
		};

		// The first tile that no cliff type uses as its ground tile can be removed without the cliffs losing their texture.
		std::vector<std::string> removed_list = map->terrain.tileset_ids;
		for (auto i = removed_list.rbegin(); i != removed_list.rend(); ++i) {
			const bool used_by_cliff = std::ranges::any_of(map->terrain.cliffset_ids, [&](const std::string& cliff_id) {
				return map->terrain.cliff_slk.data("groundtile", cliff_id) == *i;
			});
			if (!used_by_cliff) {
				removed_list.erase(std::next(i).base());
				break;
			}
		}
		const bool found_removable = removed_list.size() + 1 == map->terrain.tileset_ids.size();
		check(found_removable, "one tile found that can be removed");
		if (!found_removable) {
			return finish();
		}

		// 1. A removed tile: undo history and clipboard are dropped, and the map still counts as changed.
		add_history_and_clipboard();
		map->world_undo.mark_saved();
		check(map->world_undo.has_pending_changes() && !brush.clipboard_is_empty(), "control: undo history and clipboard are filled before the change");
		QString error;
		{
			auto* setter = new TileSetter(this);
			const bool applied = setter->qa_apply_list(removed_list, {}, error);
			check(applied, "Change Tileset with one tile removed accepted" + (error.isEmpty() ? QString() : ": " + error));
		}
		check(map->terrain.tile_list_changed, "Terrain reports that the tile list changed");
		check(brush.clipboard_is_empty(), "removing a tile empties the terrain clipboard");
		check(!map->world_undo.has_pending_changes(), "removing a tile empties the undo history");
		check(map->world_undo.has_changes_since_save(), "removing a tile still counts as unsaved work");

		// 2. Only a texture replaced, same list: history and clipboard stay, and the map counts as changed.
		add_history_and_clipboard();
		map->world_undo.mark_saved();
		check(!map->world_undo.has_changes_since_save(), "control: the map counts as saved before the texture change");
		const std::vector<std::string> same_list = map->terrain.tileset_ids;
		error.clear();
		{
			auto* setter = new TileSetter(this);
			const bool applied = setter->qa_apply_list(same_list, texture, error);
			check(applied, "Change Tileset with only a texture replaced accepted" + (error.isEmpty() ? QString() : ": " + error));
		}
		check(!map->terrain.tile_list_changed, "Terrain reports that the tile list did not change");
		check(!brush.clipboard_is_empty(), "a texture-only change keeps the terrain clipboard");
		check(map->world_undo.has_pending_changes(), "a texture-only change keeps the undo history");
		check(map->world_undo.has_changes_since_save(), "a texture-only change counts as unsaved work");

		finish();
	});
}

void HiveWE::run_qa_change_tileset() {
	if (!qa_offscreen || qa_change_tileset.isEmpty()) {
		return;
	}

	// From the event loop: the dialog and the map reload need it, and QApplication::exit only works once it runs.
	QTimer::singleShot(0, this, [this]() {
		QStringList lines;
		bool all_passed = true;
		const auto check = [&](const bool passed, const QString& text) {
			lines << (passed ? "PASS " : "FAIL ") + text;
			all_passed = all_passed && passed;
		};
		const auto finish = [&]() {
			for (const QString& line : lines) {
				std::println("{}", line.toStdString());
			}
			if (!qa_script_out.isEmpty()) {
				QFile file(qa_script_out);
				if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
					file.write(lines.join(QChar('\n')).toUtf8() + "\nDONE\n");
				}
			}
			QApplication::exit(all_passed ? 0 : 1);
		};
		// The ids of a tile's texture file, probed like GroundTexture does: .tga, .blp, .dds. Empty when none is found.
		const auto find_tile_file = [](const std::string& tile_id, std::size_t& size) {
			const slk::SLK& slk = map->terrain.terrain_slk;
			const std::string base = slk.data("dir", tile_id) + "/" + slk.data("file", tile_id);
			for (const char* extension : { ".tga", ".blp", ".dds" }) {
				if (auto reader = hierarchy.open_file(fs::path(base + extension))) {
					size = reader->buffer.size();
					return std::string(extension);
				}
			}
			size = 0;
			return std::string();
		};

		const fs::path texture = fs::absolute(fs::path(qa_tile_texture.toStdWString()));
		const fs::path out = fs::absolute(fs::path(qa_change_tileset.toStdWString()));
		std::error_code ec;
		if (!fs::is_regular_file(texture, ec)) {
			check(false, "the --qa-tile-texture file does not exist");
			return finish();
		}
		if (fs::exists(out, ec) && !fs::is_empty(out, ec)) {
			check(false, "the output folder must be absent or empty");
			return finish();
		}

		// Save As into the output folder FIRST and point the hierarchy at it, so the texture overrides that Change Tileset writes next go
		// into the copy and never into the startup map folder (data/test map, a symlink into the repository).
		fs::create_directories(out, ec);
		hierarchy.map_directory = out;
		map->headless = true; // no message boxes: they would hang an offscreen run
		const auto save_to_out = [&](const QString& what) {
			bool saved = false;
			try {
				saved = map->save(out);
			} catch (const std::exception& e) {
				check(false, what + " threw: " + e.what());
			}
			check(saved, what);
			return saved;
		};
		if (!save_to_out("map copied to the output folder")) {
			return finish();
		}
		check(hierarchy.map_directory == out && fs::equivalent(map->filesystem_path, out, ec), "the map and its asset lookup now point at the output folder");
		if (!all_passed) {
			return finish();
		}

		// Change Tileset exactly as the dialog's OK does it: import, write_pending_overrides, Terrain::change_tileset.
		const char tileset_letter = map->terrain.tileset;
		const std::size_t old_count = map->terrain.tileset_ids.size();
		QString error;
		std::vector<std::string> custom_tiles;
		auto* setter = new TileSetter(this);
		const bool applied = setter->qa_accept_with_custom_tiles(texture, 20, error, custom_tiles);
		check(applied, "Change Tileset accepted" + (error.isEmpty() ? QString() : ": " + error));
		if (!applied) {
			return finish();
		}
		const std::vector<std::string> expected_ids = map->terrain.tileset_ids;
		check(old_count < 20 && expected_ids.size() == 20, QString("tile list grew from %1 to %2 tiles").arg(old_count).arg(expected_ids.size()));
		check(map->terrain.tileset == tileset_letter, "tileset letter kept in memory");
		check(custom_tiles.size() == 2, "two tiles got the custom texture");

		if (!save_to_out("map saved again after Change Tileset")) {
			return finish();
		}

		std::size_t texture_size = 0;
		{
			std::error_code size_error;
			texture_size = static_cast<std::size_t>(fs::file_size(texture, size_error));
		}
		std::string texture_extension;
		for (const wchar_t c : texture.extension().wstring()) {
			texture_extension.push_back(static_cast<char>(std::tolower(c < 128 ? c : '?')));
		}
		for (const std::string& id : custom_tiles) {
			const slk::SLK& slk = map->terrain.terrain_slk;
			const fs::path file = out / (slk.data("dir", id) + "/" + slk.data("file", id) + texture_extension);
			std::error_code file_error;
			check(
				fs::is_regular_file(file, file_error) && fs::file_size(file, file_error) == texture_size,
				QString("override file of tile %1 is in the saved map").arg(QString::fromStdString(id))
			);
		}
		check(fs::is_regular_file(out / "war3map.w3e", ec), "war3map.w3e is in the saved map");

		// Reload the saved map like Open Map does, but without the recent-maps registry write.
		if (const auto opening = check_map_for_opening(out); !opening) {
			check(false, "saved map refused at open: " + QString::fromStdString(opening.error()));
			return finish();
		}
		window_handler.close_all();
		ui->widget->makeCurrent();
		reset_map_session();
		map->load(out);
		map->headless = true;
		map->render_manager.resize_framebuffers(ui->widget->width(), ui->widget->height());

		const auto& terrain = map->terrain;
		check(terrain.tileset == tileset_letter, "tileset letter kept after reload");
		check(terrain.tileset_ids == expected_ids, "tileset ids after reload are the saved list");
		check(terrain.ground_texture_to_id.size() == terrain.tileset_ids.size() + 1, "one ground texture slot per tile plus blight");
		bool ids_valid = true;
		for (const std::string& id : terrain.tileset_ids) {
			ids_valid = ids_valid && terrain.terrain_slk.row_headers.contains(id);
		}
		check(ids_valid, "every tile id of the w3e exists in Terrain.slk");
		check(
			std::ranges::all_of(terrain.corner_ground_texture, [&](const std::uint8_t texture) { return texture < terrain.tileset_ids.size(); }),
			"every corner ground texture index is a valid tile slot"
		);
		for (std::size_t i = 0; i < terrain.tileset_ids.size(); i++) {
			const std::string& id = terrain.tileset_ids[i];
			std::size_t size = 0;
			const std::string extension = find_tile_file(id, size);
			const bool custom = std::ranges::contains(custom_tiles, id);
			// A custom tile must resolve to the map's file (same size as the imported one), not the stock texture.
			const bool good = !extension.empty() && (!custom || size == texture_size);
			lines << QString("INFO tile %1 %2 file=%3 bytes=%4").arg(i).arg(QString::fromStdString(id)).arg(QString::fromStdString(extension)).arg(size);
			check(good, QString("tile %1 (%2) loads %3").arg(i).arg(QString::fromStdString(id)).arg(custom ? "the map's custom texture" : "a real texture, not the placeholder"));
		}
		finish();
	});
}

void HiveWE::run_qa_unsaved_check() {
	if (!qa_offscreen || !qa_unsaved_check) {
		return;
	}

	// Everything below works on copies in a temporary folder: the startup map folder is never written.
	const auto temp = std::make_shared<QTemporaryDir>();
	QTimer::singleShot(180000, this, [temp]() {
		std::println("FAIL the unsaved check did not finish within 180 s: a dialog is probably blocking (sandbox was {})", temp->path().toStdString());
		std::fflush(stdout);
		temp->remove(); // _Exit runs no destructors
		std::_Exit(3);
	});

	QTimer::singleShot(0, this, [this, temp]() {
		QStringList lines;
		bool all_passed = true;
		// Written after every line, so a crash keeps the results so far.
		const auto write_lines = [&](const bool done) {
			if (qa_script_out.isEmpty()) {
				return;
			}
			QFile file(qa_script_out);
			if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
				file.write(lines.join(QChar('\n')).toUtf8() + (done ? "\nDONE\n" : "\n"));
			}
		};
		const auto check = [&](const bool passed, const QString& text) {
			lines << (passed ? "PASS " : "FAIL ") + text;
			all_passed = all_passed && passed;
			std::println("{}", lines.back().toStdString());
			std::fflush(stdout);
			write_lines(false);
			return passed;
		};
		const auto finish = [&]() {
			qa_prompt_answer = 0;
			write_lines(true);
			QApplication::exit(all_passed ? 0 : 1);
		};

		if (!check(temp->isValid(), "a temporary folder was created")) {
			return finish();
		}
		std::error_code ec;
		const fs::path root = temp->path().toStdWString();
		const fs::path folder = root / "map";
		fs::copy(fs::path("data") / "test map", folder, fs::copy_options::recursive, ec);
		if (!check(!ec, "data/test map was copied into the temporary folder")) {
			return finish();
		}

		const auto process_events = []() {
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
			QApplication::processEvents();
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		};
		const auto close_window = [&](QWidget* window) {
			window->close();
			process_events();
		};
		// Open Map without the recent-maps registry write.
		const auto open_map = [&](const fs::path& path) {
			window_handler.close_all();
			process_events();
			ui->widget->makeCurrent();
			reset_map_session();
			map->load(path);
			map->headless = true; // save refusals print instead of opening a dialog
			map->render_manager.resize_framebuffers(ui->widget->width(), ui->widget->height());
		};
		const auto read_file = [](const fs::path& path) {
			std::ifstream in(path, std::ios::binary);
			return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		};

		open_map(folder);

		// Trigger data the editor widgets cannot show unchanged (CRLF, an uninitialised variable with a value), saved into the copy.
		const std::string crlf_header = "// header one\r\n// header two";
		const std::string crlf_script = "// script one\r\n// script two";
		const std::string crlf_comment = "comment one\r\ncomment two";
		const int category_id = map->triggers.new_element_id();
		map->triggers.categories.push_back(make_new_trigger_category("QaCategory", category_id, 0));
		const int script_id = map->triggers.new_element_id();
		map->triggers.triggers.push_back(make_new_trigger(Classifier::script, "QaScript", script_id, category_id));
		map->triggers.triggers.back().custom_text = crlf_script;
		const int comment_id = map->triggers.new_element_id();
		map->triggers.triggers.push_back(make_new_trigger(Classifier::comment, "QaComment", comment_id, category_id));
		map->triggers.triggers.back().description = crlf_comment;
		const int variable_id = map->triggers.new_element_id();
		TriggerVariable variable = make_new_trigger_variable("QaVariable", variable_id, category_id);
		variable.initial_value = "5";
		map->triggers.variables.push_back(variable);
		map->triggers.global_jass = crlf_header;
		map->triggers.global_jass_comment = crlf_comment;
		if (!check(save_current_map(), "sample trigger data saved into the copy")) {
			return finish();
		}
		open_map(folder);

		const auto script_trigger = [&]() -> Trigger* {
			const auto found = std::ranges::find_if(map->triggers.triggers, [&](const Trigger& trigger) { return trigger.id == script_id; });
			return found == map->triggers.triggers.end() ? nullptr : &*found;
		};
		const auto trigger_data_as_saved = [&]() {
			return script_trigger() && script_trigger()->custom_text == crlf_script && map->triggers.global_jass == crlf_header
				&& map->triggers.global_jass_comment == crlf_comment && !map->triggers.variables.empty()
				&& !map->triggers.variables.back().is_initialized;
		};
		check(trigger_data_as_saved(), "control: the reloaded copy holds the sample trigger data byte for byte");
		check(!map->regions.regions.empty() && !map->units.units.empty(), "control: the copy has a region and a unit");

		const auto expect_clean = [&](const QString& what) {
			if (!check(!has_unsaved_work(), what + ": no unsaved changes")) {
				save_current_map(); // so one failure does not fail every later check
			}
		};
		const auto expect_dirty = [&](const QString& what) {
			check(has_unsaved_work(), what + ": counts as unsaved");
			check(save_current_map(), what + ": saved");
			check(!has_unsaved_work(), what + ": no unsaved changes after the save");
		};

		expect_clean("right after opening the map");
		for (int i = 0; i < 3; ++i) {
			ui->widget->grabFramebuffer();
			process_events();
		}
		expect_clean("after rendering frames");

		// --- Opening and closing the windows ---------------------------------------------------------------------------------------------
		bool created = false;
		const auto open_and_close = [&](QWidget* window, const QString& name) {
			process_events();
			expect_clean(name + " open");
			close_window(window);
			expect_clean(name + " closed");
		};
		open_and_close(window_handler.create_or_raise<ObjectEditor>(nullptr, created), "Object Editor");
		open_and_close(new MapInfoEditor(this), "Map Info");
		open_and_close(window_handler.create_or_raise<ScenarioPropertiesEditor>(nullptr, created), "Scenario Properties");
		open_and_close(window_handler.create_or_raise<GameplayConstantsEditor>(nullptr, created), "Gameplay Constants");
		open_and_close(window_handler.create_or_raise<ItemTablesEditor>(nullptr, created), "Item Tables");
		open_and_close(window_handler.create_or_raise<AssetManager>(nullptr, created), "Asset Manager");
		open_and_close(new TileSetter(this), "Change Tileset");
		open_and_close(new TilePather(this), "Tile Pathing");

		{
			auto* tooltips = window_handler.create_or_raise<TooltipEditor>(nullptr, created);
			process_events();
			int browsed = 0;
			for (QListView* list : tooltips->findChildren<QListView*>()) {
				if (!list->model() || !list->selectionModel() || list->model()->rowCount() < 3) {
					continue;
				}
				for (int row = 0; row < 3; ++row) {
					list->setCurrentIndex(list->model()->index(row, 0));
					++browsed;
				}
			}
			for (QSpinBox* level : tooltips->findChildren<QSpinBox*>()) {
				level->setValue(2);
				level->setValue(1);
			}
			check(browsed >= 9, QString("control: browsed %1 Tooltip Editor entries").arg(browsed));
			open_and_close(tooltips, "Tooltip Editor after browsing");
		}

		toggle_terrain_sidebar();
		toggle_doodad_palette();
		toggle_unit_palette();
		toggle_pathing_sidebar();
		toggle_region_palette();
		process_events();
		hide_palette_sidebar();
		expect_clean("opening every palette");

		// --- Trigger Editor --------------------------------------------------------------------------------------------------------------
		TriggerEditor* editor = nullptr;
		TriggerExplorer* explorer = nullptr;
		const auto open_trigger_editor = [&]() {
			editor = window_handler.create_or_raise<TriggerEditor>(nullptr, created);
			explorer = editor->findChild<TriggerExplorer*>();
			process_events();
		};
		const std::function<QModelIndex(const QModelIndex&, int)> find_index = [&](const QModelIndex& parent, const int id) -> QModelIndex {
			for (int row = 0; row < explorer->model()->rowCount(parent); ++row) {
				const QModelIndex index = explorer->model()->index(row, 0, parent);
				if (static_cast<TreeItem*>(index.internalPointer())->id == id) {
					return index;
				}
				if (const QModelIndex found = find_index(index, id); found.isValid()) {
					return found;
				}
			}
			return {};
		};
		const auto open_tab = [&](const int id) -> QWidget* {
			emit explorer->doubleClicked(find_index({}, id));
			process_events();
			return editor->findChild<QWidget*>(QString::number(id));
		};
		const auto set_code = [](QWidget* tab, const QString& text) {
			QWidget* code = tab ? tab->findChild<QWidget*>("jass_editor") : nullptr;
			return code && QMetaObject::invokeMethod(code, "setText", Q_ARG(QString, text));
		};

		open_trigger_editor();
		QWidget* header_tab = open_tab(0);
		QWidget* script_tab = open_tab(script_id);
		QWidget* comment_tab = open_tab(comment_id);
		QWidget* variable_tab = open_tab(variable_id);
		if (!check(header_tab && script_tab && comment_tab && variable_tab, "control: four Trigger Editor tabs opened")) {
			return finish();
		}
		expect_clean("Trigger Editor with four tabs open");
		close_window(editor);
		expect_clean("Trigger Editor closed with four tabs");
		check(trigger_data_as_saved(), "opening and closing the Trigger Editor left the trigger data byte for byte");

		open_trigger_editor();
		header_tab = open_tab(0);
		check(set_code(header_tab, "// QA typed header"), "control: text typed into the Map Header tab");
		check(has_unsaved_work(), "text only in the open Map Header tab counts as unsaved");
		check(map->triggers.global_jass == crlf_header, "the check itself did not write the typed text into the map");
		expect_dirty("typed Map Header text");
		check(map->triggers.global_jass == "// QA typed header", "Save took the typed header text from the open tab");

		check(set_code(header_tab, "// QA other") && set_code(header_tab, "// QA typed header"), "control: header text changed and put back");
		expect_clean("header text typed and put back before any check");

		script_tab = open_tab(script_id);
		check(set_code(script_tab, "// QA_SAVE_FLUSH"), "control: text typed into a script tab");
		check(save_current_map(), "Save with an open script tab");
		check(read_file(folder / "war3map.wct").find("QA_SAVE_FLUSH") != std::string::npos, "Save wrote the text of the open script tab");
		expect_clean("after saving the open script tab");

		comment_tab = open_tab(comment_id);
		if (auto* comments = comment_tab ? comment_tab->findChild<QPlainTextEdit*>("comments") : nullptr) {
			comments->setPlainText("QA new comment");
		}
		expect_dirty("a comment edited in its tab");

		variable_tab = open_tab(variable_id);
		if (auto* name = variable_tab ? variable_tab->findChild<QLineEdit*>("name") : nullptr) {
			name->setText("QaRenamedVariable");
		}
		expect_dirty("a variable renamed in its tab");

		explorer->createCategory();
		const int new_category_id = map->triggers.categories.back().id;
		expect_dirty("new trigger category");
		explorer->createGuiTrigger();
		expect_dirty("new GUI trigger");
		explorer->createJassTrigger();
		expect_dirty("new script trigger");
		explorer->createComment();
		expect_dirty("new trigger comment");
		explorer->createVariable();
		const int new_variable_id = map->triggers.variables.back().id;
		expect_dirty("new variable");
		check(explorer->model()->setData(find_index({}, script_id), "QaScriptRenamed", Qt::EditRole), "control: trigger renamed in the explorer");
		expect_dirty("trigger renamed");
		close_window(editor);

		script_trigger()->is_enabled = false; // what the Enabled menu action does
		expect_dirty("trigger disabled");
		map->triggers.set_element_parent(Classifier::script, script_id, new_category_id);
		expect_dirty("trigger moved to another category");
		map->triggers.remove_element(Classifier::variable, new_variable_id);
		expect_dirty("variable deleted");

		// --- Object data -----------------------------------------------------------------------------------------------------------------
		auto* object_editor = window_handler.create_or_raise<ObjectEditor>(nullptr, created);
		process_events();
		units_table->copyRow("hfoo", "h0qa");
		expect_dirty("new custom unit");
		const QModelIndex name_index = units_table->index(units_slk.row_headers.at("h0qa"), units_slk.column_headers.at("name"));
		units_table->setData(name_index, "QA unit name", Qt::EditRole);
		expect_dirty("custom unit field edited");
		units_table->setData(name_index, "QA unit name", Qt::EditRole);
		expect_clean("committing the same field value again");
		units_table->deleteRow("h0qa");
		expect_dirty("custom unit deleted");
		abilities_table->copyRow("AHbz", "A0qa");
		expect_dirty("new custom ability");
		close_window(object_editor);

		{
			auto* tooltips = window_handler.create_or_raise<TooltipEditor>(nullptr, created);
			process_events();
			for (QListView* list : tooltips->findChildren<QListView*>()) {
				if (list->model() && list->selectionModel() && list->model()->rowCount() > 0) {
					list->setCurrentIndex(list->model()->index(0, 0));
				}
			}
			bool typed = false;
			for (QTextEdit* edit : tooltips->findChildren<QTextEdit*>()) {
				if (!typed && !qobject_cast<QTextBrowser*>(edit) && edit->isEnabled()) {
					edit->insertPlainText("QA ");
					typed = true;
				}
			}
			check(typed, "control: text typed into the Tooltip Editor");
			expect_dirty("tooltip edited");
			close_window(tooltips);
		}

		{
			auto* constants = window_handler.create_or_raise<GameplayConstantsEditor>(nullptr, created);
			process_events();
			auto* view = constants->findChild<QTableView*>();
			check(view && view->model()->setData(view->model()->index(0, 0), "7", Qt::EditRole), "control: first gameplay constant set");
			expect_dirty("gameplay constant edited");
			close_window(constants);
		}

		// --- Map Info and Scenario -------------------------------------------------------------------------------------------------------
		const auto map_info = [&](const bool accept, const std::function<void(MapInfoEditor&)>& edit) {
			auto* info = new MapInfoEditor(this);
			process_events();
			edit(*info);
			info->ui.buttonBox->button(accept ? QDialogButtonBox::Ok : QDialogButtonBox::Cancel)->click();
			process_events();
		};
		map_info(true, [](MapInfoEditor&) {});
		expect_clean("Map Info OK with nothing changed");
		map_info(false, [](MapInfoEditor& info) { info.ui.name->setText("QA cancelled name"); });
		expect_clean("Map Info Cancel after typing");
		map_info(true, [](MapInfoEditor& info) { info.ui.name->setText("QA map name"); });
		expect_dirty("map name changed");
		map_info(true, [](MapInfoEditor& info) { info.ui.meleeMap->toggle(); });
		expect_dirty("map option changed");

		{
			auto* scenario = window_handler.create_or_raise<ScenarioPropertiesEditor>(nullptr, created);
			process_events();
			bool renamed = false;
			for (QTableWidget* table : scenario->findChildren<QTableWidget*>()) {
				if (table->horizontalHeaderItem(1) && table->horizontalHeaderItem(1)->text() == "Player Name" && table->item(0, 1)) {
					table->item(0, 1)->setText("QA player");
					renamed = true;
				}
			}
			check(renamed, "control: first player renamed in Scenario Properties");
			expect_dirty("player renamed");
			bool toggled = false;
			for (QCheckBox* box : scenario->findChildren<QCheckBox*>()) {
				if (box->text() == "Use Custom Forces") {
					box->toggle();
					toggled = true;
				}
			}
			check(toggled, "control: Use Custom Forces toggled");
			expect_dirty("force setting changed");
			close_window(scenario);
		}

		// --- World data changed without an undo step -------------------------------------------------------------------------------------
		map->regions.regions.front().name = "QaRegion";
		expect_dirty("region renamed");
		// The only unit is a start location, so its angle: a new owner would leave a player without one and refuse the save.
		map->units.units.front().angle += 1.f;
		expect_dirty("unit turned");
		{
			std::ofstream imported(folder / "qa_import.txt", std::ios::binary);
			imported << "qa";
		}
		expect_dirty("file added to the map folder (war3map.imp lists it)");

		// --- Big maps (optional, read only: each folder is copied first) -----------------------------------------------------------------
		int big_index = 0;
		for (const QString& source : qEnvironmentVariable("HIVEWE_QA_BIG_MAPS").split(';', Qt::SkipEmptyParts)) {
			const fs::path big = root / ("big" + std::to_string(big_index++));
			fs::copy(fs::path(source.toStdWString()), big, fs::copy_options::recursive, ec);
			if (!check(!ec, "big map copied: " + source)) {
				continue;
			}
			open_map(big);
			for (int i = 0; i < 3; ++i) {
				const auto start = std::chrono::steady_clock::now();
				const bool unsaved = has_unsaved_work();
				const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
				check(!unsaved, QString("big map %1: no unsaved changes, check took %2 ms (%3 doodads, %4 units, %5 x %6 terrain)")
								  .arg(source)
								  .arg(ms, 0, 'f', 1)
								  .arg(map->doodads.doodads.size())
								  .arg(map->units.units.size() + map->units.items.size())
								  .arg(map->terrain.width)
								  .arg(map->terrain.height));
			}
			for (int i = 0; i < 3; ++i) {
				ui->widget->grabFramebuffer();
				process_events();
			}
			open_and_close(window_handler.create_or_raise<ObjectEditor>(nullptr, created), "big map Object Editor");
			open_and_close(window_handler.create_or_raise<ScenarioPropertiesEditor>(nullptr, created), "big map Scenario Properties");
			open_and_close(window_handler.create_or_raise<GameplayConstantsEditor>(nullptr, created), "big map Gameplay Constants");
			open_and_close(window_handler.create_or_raise<ItemTablesEditor>(nullptr, created), "big map Item Tables");
			open_and_close(new MapInfoEditor(this), "big map Map Info");
			open_trigger_editor();
			open_tab(0);
			int tabs = 0;
			for (const Trigger& trigger : map->triggers.triggers) {
				if (tabs++ < 15) {
					open_tab(trigger.id);
				}
			}
			for (std::size_t i = 0; i < map->triggers.variables.size() && i < 15; ++i) {
				open_tab(map->triggers.variables[i].id);
			}
			expect_clean("big map Trigger Editor with tabs open");
			close_window(editor);
			expect_clean("big map Trigger Editor closed");
		}
		open_map(folder);
		save_current_map();

		// --- The prompt ------------------------------------------------------------------------------------------------------------------
		units_table->copyRow("hfoo", "h0qb");
		open_trigger_editor();
		qa_prompt_answer = QMessageBox::Cancel;
		qa_prompt_count = 0;
		const bool refused = !confirm_map_replacement("quit");
		check(refused && qa_prompt_count == 1, "quit with an unsaved custom unit asks, and Cancel refuses");
		if (refused) {
			const bool closed = close();
			const auto open_editor = window_handler.get_open<TriggerEditor>();
			check(!closed && isVisible() && open_editor && (*open_editor)->isVisible(), "Cancel keeps the main window and the Trigger Editor open");
			const Map* before = map;
			create_new_map();
			check(map == before && units_slk.row_headers.contains("h0qb"), "Cancel keeps the map for New Map");
			qa_prompt_count = 0;
			ui->ribbon->exit->click();
			process_events();
			check(qa_prompt_count == 1 && isVisible(), "the Exit button asks too, and Cancel keeps the window");
		}
		close_window(editor);

		{
			const std::string id = map->units.units.front().id;
			map->units.units.front().id = "h0zzz"; // a save is refused for an id over 4 bytes
			qa_prompt_answer = QMessageBox::Save;
			check(!confirm_map_replacement("quit"), "Save that fails does not let the quit continue");
			map->units.units.front().id = id;
		}
		check(confirm_map_replacement("open another map") && !has_unsaved_work(), "Save saves and lets the action continue");
		qa_prompt_count = 0;
		check(confirm_map_replacement("quit") && qa_prompt_count == 0, "a saved map is not asked about");
		units_table->copyRow("hfoo", "h0qc");
		qa_prompt_answer = QMessageBox::Discard;
		check(confirm_map_replacement("quit") && qa_prompt_count == 1 && has_unsaved_work(), "Discard continues without saving");
		qa_prompt_answer = 0;
		check(save_current_map(), "saved before the map switch checks");

		// --- Open editors and another map ------------------------------------------------------------------------------------------------
		const fs::path second = root / "second";
		fs::copy(folder, second, fs::copy_options::recursive, ec);
		open_trigger_editor();
		set_code(open_tab(0), "// QA_STALE_HEADER");
		window_handler.close_all();
		ui->widget->makeCurrent();
		reset_map_session();
		map->load(second);
		map->headless = true;
		map->render_manager.resize_framebuffers(ui->widget->width(), ui->widget->height());
		process_events();
		check(map->triggers.global_jass.find("QA_STALE_HEADER") == std::string::npos, "a closed Trigger Editor does not write into the next map");
		expect_clean("the next map after closing the editors");

		open_trigger_editor();
		set_code(open_tab(script_id), "// QA deleted while open");
		map->triggers.remove_element(Classifier::script, script_id);
		check(has_unsaved_work(), "a deleted trigger with an open tab counts as unsaved and does not crash the check");
		close_window(editor);

		// --- The real dialogs ------------------------------------------------------------------------------------------------------------
		struct ModalAnswer {
			bool answered = false;
			bool off_screen = false;
			QString text;
		};
		// Answers the next dialog with this title from inside its event loop, by clicking a button of its button box. After 10 s it
		// closes whatever is modal instead, so a missed dialog cannot hang the run.
		const auto answer_next_dialog = [this](const QString& title, const std::function<QAbstractButton*(QWidget*)>& button_of) {
			auto result = std::make_shared<ModalAnswer>();
			auto* timer = new QTimer(this);
			auto waited = std::make_shared<QElapsedTimer>();
			waited->start();
			connect(timer, &QTimer::timeout, this, [timer, waited, result, title, button_of]() {
				QWidget* modal = QApplication::activeModalWidget();
				if (modal && modal->isVisible() && modal->windowTitle() == title) {
					timer->stop();
					timer->deleteLater();
					result->off_screen = std::ranges::none_of(QGuiApplication::screens(), [&](const QScreen* screen) {
						return screen->geometry().intersects(modal->frameGeometry());
					});
					if (const auto* box = qobject_cast<QMessageBox*>(modal)) {
						result->text = box->informativeText();
					}
					if (QAbstractButton* button = button_of(modal)) {
						result->answered = true;
						button->click();
					} else {
						modal->close();
					}
				} else if (waited->elapsed() > 10000) {
					timer->stop();
					timer->deleteLater();
					if (modal) {
						modal->close();
					}
				}
			});
			timer->start(20);
			return result;
		};
		const auto message_box_button = [](const QMessageBox::StandardButton which) {
			return [which](QWidget* modal) -> QAbstractButton* {
				auto* box = qobject_cast<QMessageBox*>(modal);
				return box ? box->button(which) : nullptr;
			};
		};
		const auto check_answered = [&](const std::shared_ptr<ModalAnswer>& answer, const QString& what) {
			check(answer->answered, what + ": the dialog appeared and its button was clicked");
			check(answer->off_screen, what + ": the dialog was outside every screen");
		};

		open_map(folder);
		save_current_map();
		qa_prompt_answer = 0;
		qa_prompt_real = true;
		const fs::path object_file = folder / "war3map.w3u";
		{
			units_table->copyRow("hfoo", "h0qd");
			const std::string on_disk = read_file(object_file);

			auto answer = answer_next_dialog("Unsaved changes", message_box_button(QMessageBox::Cancel));
			const bool closed = close();
			check_answered(answer, "real prompt, Cancel");
			check(answer->text.contains("before you quit"), "real prompt: the text names the action");
			check(!closed && isVisible() && has_unsaved_work() && read_file(object_file) == on_disk,
				  "real prompt, Cancel: the window stays, nothing is written, the change is still unsaved");

			answer = answer_next_dialog("Unsaved changes", message_box_button(QMessageBox::Discard));
			const bool discarded = confirm_map_replacement("open another map");
			check_answered(answer, "real prompt, Discard");
			check(discarded && has_unsaved_work() && read_file(object_file) == on_disk, "real prompt, Discard: continues without writing");

			answer = answer_next_dialog("Unsaved changes", message_box_button(QMessageBox::Save));
			const bool saved = confirm_map_replacement("open another map");
			check_answered(answer, "real prompt, Save");
			check(saved && !has_unsaved_work() && read_file(object_file) != on_disk && read_file(object_file).find("h0qd") != std::string::npos,
				  "real prompt, Save: writes the change and continues");
		}

		// --- A half typed region name --------------------------------------------------------------------------------------------------
		{
			toggle_region_palette();
			process_events();
			auto* list = region_palette ? region_palette->findChild<QListWidget*>("regionList") : nullptr;
			auto* name = region_palette ? region_palette->findChild<QLineEdit*>("regionNameField") : nullptr;
			if (check(list && name && list->count() > 0, "control: the Regions palette lists a region and has a name field")) {
				list->setCurrentRow(0);
				process_events();
				// Offscreen windows never become active, so Qt is told which window is. This changes Qt's own state only: no window is
				// raised and the user's focus stays where it is.
				QT_WARNING_PUSH
				QT_WARNING_DISABLE_DEPRECATED
				QApplication::setActiveWindow(this);
				QT_WARNING_POP
				name->setFocus();
				process_events();
				check(QApplication::focusWidget() == name, "control: the region name field has the focus");
				expect_clean("region selected, name field focused");

				const std::string old_name = map->regions.regions.front().name;
				name->selectAll();
				for (const QChar character : QString("QaTyped")) {
					QKeyEvent press(QEvent::KeyPress, 0, Qt::NoModifier, QString(character));
					QApplication::sendEvent(name, &press);
				}
				check(name->text() == "QaTyped" && map->regions.regions.front().name == old_name,
					  "control: the typed name is only in the field");
				check(!has_unsaved_work(), "control: without the focus commit the typed name would not count");

				const auto answer = answer_next_dialog("Unsaved changes", message_box_button(QMessageBox::Cancel));
				const bool closed = close();
				check_answered(answer, "close with a half typed region name");
				check(!closed && isVisible() && map->regions.regions.front().name == "QaTyped" && has_unsaved_work(),
					  "close with a half typed region name: the name is taken, the prompt appears, Cancel keeps the window");
			}
		}

		// --- New Map accepted ------------------------------------------------------------------------------------------------------------
		{
			open_map(folder);
			save_current_map();
			const std::string on_disk = read_file(object_file);
			units_table->copyRow("hfoo", "h0qe");
			window_handler.create_or_raise<ObjectEditor>(nullptr, created);
			open_trigger_editor();
			check(set_code(open_tab(0), "// QA_NEW_MAP_HEADER"), "control: text typed into an open tab before New Map");
			process_events();
			const QPointer<TriggerEditor> old_trigger_editor = editor;
			const QPointer<ObjectEditor> old_object_editor = window_handler.get_open<ObjectEditor>().value_or(nullptr);
			const QPointer<Map> old_map = map;

			const auto prompt = answer_next_dialog("Unsaved changes", message_box_button(QMessageBox::Discard));
			std::shared_ptr<ModalAnswer> dialog;
			// The second watcher starts once the prompt is answered, as both poll for the dialog on top.
			QTimer wait_for_prompt;
			connect(&wait_for_prompt, &QTimer::timeout, this, [&]() {
				if (prompt->answered && !dialog) {
					wait_for_prompt.stop();
					dialog = answer_next_dialog("Create New Map", [](QWidget* modal) -> QAbstractButton* {
						auto* buttons = modal->findChild<QDialogButtonBox*>();
						return buttons ? buttons->button(QDialogButtonBox::Ok) : nullptr;
					});
				}
			});
			wait_for_prompt.start(10);
			// If the second dialog were never answered, this still ends it.
			QTimer last_resort;
			last_resort.setSingleShot(true);
			connect(&last_resort, &QTimer::timeout, this, []() {
				if (QWidget* modal = QApplication::activeModalWidget()) {
					modal->close();
				}
			});
			last_resort.start(30000);
			create_new_map();
			wait_for_prompt.stop();
			last_resort.stop();
			process_events();
			for (int i = 0; i < 3; ++i) {
				ui->widget->grabFramebuffer();
				process_events();
			}

			check_answered(prompt, "New Map, prompt Discard");
			if (check(dialog != nullptr, "New Map: the New Map dialog followed the prompt")) {
				check_answered(dialog, "New Map, dialog accepted");
			}
			check(old_map.isNull() && map->filesystem_path.empty(), "New Map: a new map without a folder replaced the old one");
			check(old_trigger_editor.isNull() && old_object_editor.isNull() && !window_handler.get_open<TriggerEditor>()
					  && !window_handler.get_open<ObjectEditor>(),
				  "New Map: the Trigger Editor and the Object Editor were closed");
			check(map->triggers.global_jass.find("QA_NEW_MAP_HEADER") == std::string::npos, "New Map: nothing from the old editor is in the new map");
			check(!units_slk.row_headers.contains("h0qe") && read_file(object_file) == on_disk, "New Map: the discarded unit is gone and was not written");
			check(has_unsaved_work(), "New Map: the new map counts as unsaved");
		}

		qa_prompt_real = false;
		finish();
	});
}

void HiveWE::run_qa_asset_delete_check() {
	if (!qa_offscreen || !qa_asset_delete_check) {
		return;
	}

	// Everything below works on copies in a temporary folder: the startup map folder is never written.
	const auto temp = std::make_shared<QTemporaryDir>();
	QTimer::singleShot(180000, this, [temp]() {
		std::println("FAIL the asset delete check did not finish within 180 s: a dialog is probably blocking (sandbox was {})", temp->path().toStdString());
		std::fflush(stdout);
		temp->remove(); // _Exit runs no destructors
		std::_Exit(3);
	});

	QTimer::singleShot(0, this, [this, temp]() {
		QStringList lines;
		bool all_passed = true;
		const auto write_lines = [&](const bool done) {
			if (qa_script_out.isEmpty()) {
				return;
			}
			QFile file(qa_script_out);
			if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
				file.write(lines.join(QChar('\n')).toUtf8() + (done ? "\nDONE\n" : "\n"));
			}
		};
		const auto check = [&](const bool passed, const QString& text) {
			lines << (passed ? "PASS " : "FAIL ") + text;
			all_passed = all_passed && passed;
			std::println("{}", lines.back().toStdString());
			std::fflush(stdout);
			write_lines(false);
			return passed;
		};
		const auto finish = [&]() {
			write_lines(true);
			QApplication::exit(all_passed ? 0 : 1);
		};
		const auto process_events = []() {
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
			QApplication::processEvents();
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		};
		// Replaces the map without closing any window, unlike load_map.
		const auto replace_map = [&](const fs::path& path) {
			ui->widget->makeCurrent();
			reset_map_session();
			map->load(path);
			map->headless = true;
			map->render_manager.resize_framebuffers(ui->widget->width(), ui->widget->height());
		};
		const auto files_on_disk = [](const fs::path& folder) {
			std::set<std::wstring> files;
			for (const auto& entry : fs::recursive_directory_iterator(folder)) {
				if (entry.is_regular_file()) {
					files.insert(entry.path().lexically_relative(folder).generic_wstring());
				}
			}
			return files;
		};

		if (!check(temp->isValid(), "a temporary folder was created")) {
			return finish();
		}
		std::error_code ec;
		const fs::path root = temp->path().toStdWString();
		// Cyrillic and CJK names for the map folder, a subfolder and the files.
		const fs::path folder = root / L"\u043A\u0430\u0440\u0442\u0430_\u5730\u56FE";
		const fs::path other = root / L"\u0432\u0442\u043E\u0440\u0430\u044F_\u7B2C\u4E8C";
		fs::copy(fs::path("data") / "test map", folder, fs::copy_options::recursive, ec);
		if (!check(!ec, "data/test map was copied into the temporary folder")) {
			return finish();
		}

		const fs::path sub = fs::path(L"war3mapImported") / L"\u041F\u0430\u043F\u043A\u0430_\u6587\u4EF6\u5939";
		const fs::path doomed = sub / L"\u0443\u0434\u0430\u043B\u0438\u0442\u044C_\u5220\u9664.blp";
		const fs::path kept = sub / L"\u043E\u0441\u0442\u0430\u0432\u0438\u0442\u044C_\u4FDD\u7559.blp";
		const fs::path locked = sub / L"\u0437\u0430\u043F\u0435\u0440\u0442_\u9501\u5B9A.blp";
		const fs::path used_file = fs::path("war3mapImported") / "qa_used.blp";
		const fs::path typed = fs::path("war3mapImported") / "qa_typed.mdx";
		const std::array<fs::path, 3> dot_files = { fs::path(".git") / "config", fs::path(".gitignore"), fs::path("war3mapImported") / ".hidden.blp" };
		fs::create_directories(folder / sub);
		fs::create_directories(folder / ".git");
		for (const fs::path& file : { doomed, kept, locked, used_file, typed, dot_files[0], dot_files[1], dot_files[2] }) {
			std::ofstream(folder / file, std::ios::binary) << "qa";
		}
		std::ofstream(folder / "war3mapSkin.txt", std::ios::binary) << "[CustomSkin]\r\nQaIcon=war3mapImported/qa_used.blp\r\n";

		window_handler.close_all();
		process_events();
		replace_map(folder);

		const auto name_of = [](const fs::path& file) { return QString::fromStdWString(file.generic_wstring()); };
		const auto rows_of = [](const AssetManager* assets) {
			return assets->findChild<QStandardItemModel*>(QString(), Qt::FindDirectChildrenOnly);
		};
		const auto row_of = [&](const AssetManager* assets, const fs::path& file) -> QStandardItem* {
			const QStandardItemModel* rows = rows_of(assets);
			for (int row = 0; rows && row < rows->rowCount(); ++row) {
				if (rows->item(row, asset::FileColumn)->text() == name_of(file)) {
					return rows->item(row, asset::FileColumn);
				}
			}
			return nullptr;
		};
		const auto is_unused = [&](const AssetManager* assets, const fs::path& file) {
			const QStandardItem* item = row_of(assets, file);
			return item && item->data(asset::IsUnusedRole).toBool();
		};
		const auto used_by = [&](const AssetManager* assets, const fs::path& file, const QString& source) {
			const QStandardItem* item = row_of(assets, file);
			for (int child = 0; item && child < item->rowCount(); ++child) {
				if (item->child(child)->text() == source) {
					return true;
				}
			}
			return false;
		};
		const auto click = [](const AssetManager* assets, const char* button_name) {
			if (auto* button = assets->findChild<QPushButton*>(button_name)) {
				button->click();
			}
		};

		// Answers every dialog from inside its event loop: Yes to a confirm dialog (after `before_yes`), OK to a notice.
		struct Dialogs {
			QStringList titles;
			QStringList texts;
			bool off_screen = true;
		};
		const auto delete_checked = [&](const AssetManager* assets, const fs::path& file, const std::function<void()>& before_yes = {}) {
			auto dialogs = std::make_shared<Dialogs>();
			QStandardItem* item = row_of(assets, file);
			if (!check(item != nullptr, "control: a row to check for " + name_of(file))) {
				return dialogs;
			}
			item->setCheckState(Qt::Checked);

			QTimer watcher;
			connect(&watcher, &QTimer::timeout, this, [dialogs, before_yes]() {
				QWidget* modal = QApplication::activeModalWidget();
				if (!modal || !modal->isVisible()) {
					return;
				}
				auto* box = qobject_cast<QMessageBox*>(modal);
				dialogs->titles << modal->windowTitle();
				dialogs->texts << (box ? box->text() : QString());
				dialogs->off_screen = dialogs->off_screen && std::ranges::none_of(QGuiApplication::screens(), [&](const QScreen* screen) {
					return screen->geometry().intersects(modal->frameGeometry());
				});
				if (box && box->button(QMessageBox::Yes) && dialogs->titles.size() == 1) {
					if (before_yes) {
						before_yes();
					}
					box->button(QMessageBox::Yes)->click();
				} else if (box && box->button(QMessageBox::Ok)) {
					box->button(QMessageBox::Ok)->click();
				} else {
					modal->close();
				}
			});
			watcher.start(20);
			click(assets, "assetDeleteButton");
			watcher.stop();
			process_events();
			return dialogs;
		};

		// --- The list ----------------------------------------------------------------------------------------------------------------------
		bool created = false;
		auto* assets = window_handler.create_or_raise<AssetManager>(nullptr, created);
		process_events();
		if (!check(rows_of(assets) && rows_of(assets)->rowCount() > 0, "control: the Asset Manager lists files")) {
			return finish();
		}
		bool dot_rows = false;
		for (int row = 0; row < rows_of(assets)->rowCount(); ++row) {
			const QString path = rows_of(assets)->item(row, asset::FileColumn)->text();
			dot_rows = dot_rows || path.startsWith('.') || path.contains("/.");
		}
		check(!dot_rows && std::ranges::all_of(dot_files, [&](const fs::path& file) { return fs::exists(folder / file); }),
			  "dot files and folders exist and are no rows");
		check(is_unused(assets, doomed) && is_unused(assets, kept) && is_unused(assets, locked) && is_unused(assets, typed),
			  "the unreferenced non-ASCII files and the model are listed as unused");
		check(used_by(assets, used_file, "war3mapSkin.txt"), "the file named in war3mapSkin.txt is used by it");
		check(!has_unsaved_work(), "opening the Asset Manager leaves the map without unsaved changes");

		// --- Text only in an open Trigger Editor tab -------------------------------------------------------------------------------------
		{
			auto* editor = window_handler.create_or_raise<TriggerEditor>(nullptr, created);
			auto* explorer = editor->findChild<TriggerExplorer*>();
			process_events();
			const std::function<QModelIndex(const QModelIndex&)> find_header = [&](const QModelIndex& parent) -> QModelIndex {
				for (int row = 0; row < explorer->model()->rowCount(parent); ++row) {
					const QModelIndex index = explorer->model()->index(row, 0, parent);
					if (static_cast<TreeItem*>(index.internalPointer())->id == 0) {
						return index;
					}
					if (const QModelIndex found = find_header(index); found.isValid()) {
						return found;
					}
				}
				return {};
			};
			emit explorer->doubleClicked(find_header({}));
			process_events();
			QWidget* tab = editor->findChild<QWidget*>("0");
			QWidget* code = tab ? tab->findChild<QWidget*>("jass_editor") : nullptr;
			const std::string header = map->triggers.global_jass;
			const bool typed_in = code
				&& QMetaObject::invokeMethod(code, "setText", Q_ARG(QString, QString(R"(call Preload("war3mapImported\\qa_typed.mdl"))")));
			if (check(typed_in, "control: a path typed into the Map Header tab")) {
				click(assets, "assetRescanButton");
				check(map->triggers.global_jass == header, "the scan did not write the typed text into the map");
				check(!is_unused(assets, typed) && used_by(assets, typed, "Open editor: Map Header"),
					  "the file named only in the open tab counts as used");
				QMetaObject::invokeMethod(code, "setText", Q_ARG(QString, QString::fromStdString(header)));
				check(!has_unsaved_work(), "with the typed text taken back the map has no unsaved changes");
			}
			editor->close();
			process_events();
			click(assets, "assetRescanButton");
			check(is_unused(assets, typed), "control: after the tab is gone the file is unused again");
		}

		// --- Delete one file -------------------------------------------------------------------------------------------------------------
		{
			const auto before = files_on_disk(folder);
			const int rows_before = rows_of(assets)->rowCount();
			const auto dialogs = delete_checked(assets, doomed);
			check(dialogs->titles == QStringList { "Delete files" } && dialogs->texts.value(0).contains("1 file(s)") && dialogs->off_screen,
				  "delete: only the confirm dialog appeared, off screen, for one file, and was answered Yes (no failure reported)");
			auto expected = before;
			expected.erase(doomed.generic_wstring());
			check(expected.size() + 1 == before.size() && files_on_disk(folder) == expected, "delete: exactly that file left the disk");
			check(!row_of(assets, doomed) && rows_of(assets)->rowCount() == rows_before - 1 && row_of(assets, kept) && row_of(assets, locked),
				  "delete: exactly that row left the list");
		}

		// --- A file another program holds open -------------------------------------------------------------------------------------------
		{
			const auto before = files_on_disk(folder);
			std::ofstream lock(folder / locked, std::ios::binary | std::ios::app);
			const auto dialogs = delete_checked(assets, locked);
			check(dialogs->titles == QStringList { "Delete files", "Delete failed" } && dialogs->texts.value(1).contains(name_of(locked)),
				  "locked file: the failure dialog names it");
			check(files_on_disk(folder) == before && row_of(assets, locked), "locked file: it stays on disk and in the list");
		}
		click(assets, "assetRescanButton");

		// --- Save As ---------------------------------------------------------------------------------------------------------------------
		fs::create_directories(other);
		hierarchy.map_directory = other;
		if (!check(map->save(other) && fs::equivalent(map->filesystem_path, other), "control: the map was saved to another folder")) {
			return finish();
		}
		{
			const auto first = files_on_disk(folder);
			const auto second = files_on_disk(other);
			check(second.contains(kept.generic_wstring()), "control: the other folder holds the same file");
			auto dialogs = delete_checked(assets, kept);
			check(dialogs->titles == QStringList { "List out of date" }, "after Save As: delete refuses with a notice and no confirm dialog");
			check(files_on_disk(folder) == first && files_on_disk(other) == second, "after Save As: nothing was deleted in either folder");

			dialogs = delete_checked(assets, kept);
			auto expected = second;
			expected.erase(kept.generic_wstring());
			check(dialogs->titles == QStringList { "Delete files" } && files_on_disk(other) == expected && files_on_disk(folder) == first,
				  "after the rescan: delete removes the file from the folder the map is in now, and only there");
		}

		// --- The folder changes while the confirm dialog is open -------------------------------------------------------------------------
		{
			const auto first = files_on_disk(folder);
			const auto second = files_on_disk(other);
			const fs::path current = map->filesystem_path;
			const auto dialogs = delete_checked(assets, locked, [&]() { map->filesystem_path = fs::absolute(folder) / ""; });
			map->filesystem_path = current;
			check(dialogs->titles == QStringList { "Delete files", "Delete failed" } && files_on_disk(folder) == first
					  && files_on_disk(other) == second,
				  "folder changed during the confirm dialog: nothing was deleted");
		}

		// --- Another map -----------------------------------------------------------------------------------------------------------------
		{
			click(assets, "assetRescanButton");
			const auto first = files_on_disk(folder);
			replace_map(folder);
			const auto dialogs = delete_checked(assets, locked);
			check(dialogs->titles == QStringList { "List out of date" } && files_on_disk(folder) == first,
				  "map replaced with the window left open: delete refuses and the same named file stays");

			const QPointer<AssetManager> open_assets = assets;
			load_map(other);
			process_events();
			check(open_assets.isNull() && !window_handler.get_open<AssetManager>(), "Open Map closes the Asset Manager");
		}

		finish();
	});
}

void HiveWE::run_qa_kept_script_check() {
	if (!qa_offscreen || !qa_kept_script_check) {
		return;
	}

	// Everything below works on copies in a temporary folder: the startup map folder is never written.
	const auto temp = std::make_shared<QTemporaryDir>();
	QTimer::singleShot(180000, this, [temp]() {
		std::println("FAIL the kept script check did not finish within 180 s: a dialog is probably blocking (sandbox was {})", temp->path().toStdString());
		std::fflush(stdout);
		temp->remove(); // _Exit runs no destructors
		std::_Exit(3);
	});

	QTimer::singleShot(0, this, [this, temp]() {
		QStringList lines;
		bool all_passed = true;
		const auto write_lines = [&](const bool done) {
			if (qa_script_out.isEmpty()) {
				return;
			}
			QFile file(qa_script_out);
			if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
				file.write(lines.join(QChar('\n')).toUtf8() + (done ? "\nDONE\n" : "\n"));
			}
		};
		const auto check = [&](const bool passed, const QString& text) {
			lines << (passed ? "PASS " : "FAIL ") + text;
			all_passed = all_passed && passed;
			std::println("{}", lines.back().toStdString());
			std::fflush(stdout);
			write_lines(false);
			return passed;
		};
		const auto finish = [&]() {
			write_lines(true);
			QApplication::exit(all_passed ? 0 : 1);
		};
		const auto read_file = [](const fs::path& path) {
			std::ifstream in(path, std::ios::binary);
			return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		};
		const auto process_events = []() {
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
			QApplication::processEvents();
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		};
		// Open Map without the recent-maps registry write; the notice comes from apply_trigger_editing_state, as in load_map.
		const auto open_map = [&](const fs::path& path) {
			window_handler.close_all();
			process_events();
			ui->widget->makeCurrent();
			reset_map_session();
			map->load(path);
			map->render_manager.resize_framebuffers(ui->widget->width(), ui->widget->height());
		};

		// Watches for modal dialogs from now on: clicks OK on the first one (recording its title and text), closes any later one.
		struct Watch {
			QStringList titles;
			QString text;
			bool off_screen = true;
			QPointer<QTimer> timer;
		};
		const auto watch_dialogs = [this]() {
			auto watch = std::make_shared<Watch>();
			auto* timer = new QTimer(this);
			watch->timer = timer;
			connect(timer, &QTimer::timeout, this, [watch]() {
				QWidget* modal = QApplication::activeModalWidget();
				if (!modal || !modal->isVisible()) {
					return;
				}
				watch->titles << modal->windowTitle();
				watch->off_screen = watch->off_screen && std::ranges::none_of(QGuiApplication::screens(), [&](const QScreen* screen) {
					return screen->geometry().intersects(modal->frameGeometry());
				});
				auto* box = qobject_cast<QMessageBox*>(modal);
				if (box && watch->titles.size() == 1 && box->button(QMessageBox::Ok)) {
					watch->text = box->text();
					box->button(QMessageBox::Ok)->click();
				} else {
					modal->close();
				}
			});
			timer->start(20);
			return watch;
		};
		const auto stop_watch = [](const std::shared_ptr<Watch>& watch) {
			if (watch->timer) {
				watch->timer->stop();
				watch->timer->deleteLater();
			}
		};

		if (!check(temp->isValid(), "a temporary folder was created")) {
			return finish();
		}
		std::error_code ec;
		const fs::path root = temp->path().toStdWString();
		const std::string marker = "// QA kept script: hand-written code that must survive\r\n";

		// root: war3map.j in the folder root. scripts: only scripts\war3map.j. Both without war3map.wtg and war3map.wct.
		const auto make_kept_copy = [&](const std::string& name, const bool under_scripts) {
			const fs::path folder = root / name;
			fs::copy(fs::path("data") / "test map", folder, fs::copy_options::recursive, ec);
			fs::remove(folder / "war3map.wtg", ec);
			fs::remove(folder / "war3map.wct", ec);
			// A trigger string literal, so Strip trigger strings has something to resolve in the kept script.
			const std::string script = marker + "function QaKeptName takes nothing returns string\r\n return \"TRIGSTR_001\"\r\nendfunction\r\n"
				+ read_file(folder / "war3map.j");
			fs::path script_path = folder / "war3map.j";
			if (under_scripts) {
				fs::remove(script_path, ec);
				fs::create_directories(folder / "scripts", ec);
				script_path = folder / "scripts" / "war3map.j";
			}
			std::ofstream(script_path, std::ios::binary) << script;
			return std::pair { folder, script };
		};
		const auto [root_folder, root_script] = make_kept_copy("kept_root", false);
		const auto [scripts_folder, scripts_script] = make_kept_copy("kept_scripts", true);
		const fs::path normal_folder = root / "normal";
		fs::copy(fs::path("data") / "test map", normal_folder, fs::copy_options::recursive, ec);
		if (!check(!ec && root_script.starts_with(marker) && fs::exists(scripts_folder / "scripts" / "war3map.j"), "sandbox copies made")) {
			return finish();
		}

		const auto no_trigger_files = [&](const fs::path& folder) {
			return !fs::exists(folder / "war3map.wtg") && !fs::exists(folder / "war3map.wct") && !fs::exists(folder / "war3map.lua");
		};
		const auto editing_off = [&]() {
			return !ui->ribbon->trigger_editor->isEnabled() && (!trigger_editor_action || !trigger_editor_action->isEnabled());
		};
		const auto editing_on = [&]() {
			return ui->ribbon->trigger_editor->isEnabled() && (!trigger_editor_action || trigger_editor_action->isEnabled());
		};

		// --- Open: the notice, editing off, no unsaved state ---------------------------------------------------------------------------
		open_map(root_folder);
		{
			const auto watch = watch_dialogs();
			apply_trigger_editing_state(true);
			stop_watch(watch);
			check(map->keeps_script, "a map with war3map.j and no war3map.wtg keeps its script");
			check(watch->titles == QStringList { "Map Script Kept" } && watch->text.contains("Trigger editing is turned off"),
				  "open: one notice, clicked OK (dialogs: " + watch->titles.join(", ") + ")");
			check(watch->off_screen, "open: the notice was outside every screen");
		}
		check(editing_off(), "open: the Trigger Editor button and menu action are disabled");
		check(!ui->ribbon->trigger_editor->toolTip().isEmpty(), "open: the disabled button says why");
		ui->ribbon->trigger_editor->click();
		if (trigger_editor_action) {
			trigger_editor_action->trigger();
		}
		process_events();
		check(!window_handler.get_open<TriggerEditor>(), "open: clicking the button or the menu action opens no Trigger Editor");
		check(!has_unsaved_work(), "open: the map does not count as unsaved");

		// --- Save without changes: no warning, script untouched, no trigger files ------------------------------------------------------
		{
			const auto watch = watch_dialogs();
			const bool saved = save_current_map();
			stop_watch(watch);
			check(saved && watch->titles.isEmpty(), "Save without changes: saved with no dialog (dialogs: " + watch->titles.join(", ") + ")");
			check(read_file(root_folder / "war3map.j") == root_script, "Save: war3map.j is byte for byte the same");
			check(no_trigger_files(root_folder), "Save: no war3map.wtg, war3map.wct or war3map.lua was written");
			check(!has_unsaved_work(), "Save: the map counts as saved afterwards");
		}

		// --- A moved unit: unsaved, one warning on Save, still saved -------------------------------------------------------------------
		if (check(!map->units.units.empty(), "the map has a placed unit to move")) {
			map->units.units.front().position.x += 1.f;
			check(has_unsaved_work(), "a moved unit counts as unsaved");
			const auto watch = watch_dialogs();
			const bool saved = save_current_map();
			stop_watch(watch);
			check(watch->titles == QStringList { "Script Not Updated" } && watch->text.contains("placed units"),
				  "Save after a moved unit: one Script Not Updated warning naming placed units, clicked OK (dialogs: " + watch->titles.join(", ") + ")");
			check(watch->off_screen, "Save warning: the dialog was outside every screen");
			check(saved && read_file(root_folder / "war3map.j") == root_script && no_trigger_files(root_folder),
				  "Save after the warning: saved, script unchanged, no trigger files");
			check(!has_unsaved_work(), "Save after the warning: the map counts as saved");

			const auto again = watch_dialogs();
			save_current_map();
			stop_watch(again);
			check(again->titles.isEmpty(), "a second Save without new changes shows no warning");

			// Players/Forces fills in missing slots when it opens. That alone is no change, but it must not hide one made before.
			bool created = false;
			const auto open_and_close_scenario = [&]() {
				auto* scenario = window_handler.create_or_raise<ScenarioPropertiesEditor>(nullptr, created);
				scenario->close();
				process_events();
			};
			open_and_close_scenario();
			check(!has_unsaved_work(), "Players/Forces opened on a saved map: still saved");
			const auto clean = watch_dialogs();
			save_current_map();
			stop_watch(clean);
			check(clean->titles.isEmpty(), "Players/Forces opened on a saved map: Save shows no warning (dialogs: " + clean->titles.join(", ") + ")");

			if (check(!map->info.players.empty(), "the map has a player to change")) {
				map->info.players.front().race = map->info.players.front().race == PlayerRace::orc ? PlayerRace::human : PlayerRace::orc;
				open_and_close_scenario();
				check(has_unsaved_work(), "a changed player race stays unsaved after opening Players/Forces");
				const auto changed = watch_dialogs();
				save_current_map();
				stop_watch(changed);
				check(changed->titles == QStringList { "Script Not Updated" } && changed->text.contains("players"),
					  "a changed player race, then Players/Forces opened: Save still warns about players (dialogs: " + changed->titles.join(", ") + ")");
			}
		}

		// --- Save As into a new folder (the copy step of save_current_map_as, without its folder picker) ---------------------------------
		{
			const fs::path target = root / "save_as" / "kept_root";
			fs::create_directories(target, ec);
			hierarchy.map_directory = target;
			const bool saved = save_map_reporting_errors(target);
			check(saved && read_file(target / "war3map.j") == root_script && no_trigger_files(target),
				  "Save As: the new folder has the script byte for byte and no trigger files");
			check(fs::exists(target / "war3map.w3e") && fs::exists(target / "war3mapUnits.doo"), "Save As: the map files were written");
		}

		// --- Map Protector, current map: its save keeps the script ----------------------------------------------------------------------
		{
			const fs::path protector_temp = root / "protector_temp";
			fs::create_directories(protector_temp, ec);
			ProtectionOptions options;
			options.clear_author = true;
			const auto watch = watch_dialogs();
			const SyncSaveResult result = run_sync_save_and_restore(protector_temp, options);
			stop_watch(watch);
			check(watch->titles.isEmpty(), "Map Protector save: no dialog for an author change (dialogs: " + watch->titles.join(", ") + ")");
			check(result.success,"Map Protector save of the current map succeeded: " + QString::fromStdString(result.error));
			check(read_file(protector_temp / "war3map.j") == root_script && no_trigger_files(protector_temp),
				  "Map Protector save: script byte for byte, no trigger files");
		}

		// Map Protector's pack step (run_async_pack, here on this thread) with Strip trigger strings: the kept script's trigger string is
		// resolved, the rest of it is kept, and war3map.wts is gone.
		ProtectionOptions pack_options;
		pack_options.remove_gui_triggers = true;
		pack_options.strip_trigger_strings = true;
		pack_options.obfuscate_asset_paths = false;
		pack_options.inject_junk_files = false;
		pack_options.encrypt_files = false;
		pack_options.remove_listfile = true;
		const auto read_archived = [](const fs::path& archive, const char* name) -> std::optional<std::string> {
			mpq::MPQ opened;
			if (!opened.open(archive, MPQ_OPEN_READ_ONLY) || !opened.file_exists(name)) {
				return std::nullopt;
			}
			const std::vector<std::uint8_t> bytes = opened.file_open(name).read();
			return std::string(bytes.begin(), bytes.end());
		};
		const auto resolved = [&](const std::string& script) {
			std::string text = script;
			text.replace(text.find("\"TRIGSTR_001\""), 13, "\"Player 1\"");
			return text;
		};
		{
			const fs::path archive = root / "protected_root.w3x";
			const PackResult packed = run_async_pack(root / "protector_temp", archive, pack_options);
			check(packed.success, "Map Protector pack of the kept map succeeded: " + QString::fromStdString(packed.error));
			check(read_archived(archive, "war3map.j") == resolved(root_script),
				  "Map Protector pack: war3map.j is the kept script with only its trigger string resolved");
			check(!read_archived(archive, "war3map.wts") && !read_archived(archive, "war3map.wtg") && !read_archived(archive, "war3map.wct"),
				  "Map Protector pack: no war3map.wts, war3map.wtg or war3map.wct");
		}

		// --- scripts\war3map.j only: Save As and the Export MPQ archive carry it -------------------------------------------------------
		open_map(scripts_folder);
		apply_trigger_editing_state(false);
		check(map->keeps_script && editing_off(), "a map with only scripts\\war3map.j keeps its script, editing off");
		{
			const fs::path target = root / "save_as" / "kept_scripts";
			fs::create_directories(target, ec);
			hierarchy.map_directory = target;
			const bool saved = save_map_reporting_errors(target);
			check(saved && read_file(target / "scripts" / "war3map.j") == scripts_script && !fs::exists(target / "war3map.j") && no_trigger_files(target),
				  "Save As (scripts\\): scripts\\war3map.j byte for byte, no root script, no trigger files");

			const fs::path archive = root / "kept_scripts.w3x";
			const map_archive::Result written = map_archive::write(map->filesystem_path, archive);
			check(written.ok && written.added > 0 && written.failed.empty(),
				  QString("Export MPQ: the archive was written (step %1, error %2, %3 files)")
					  .arg(static_cast<int>(written.step))
					  .arg(written.error)
					  .arg(written.added));
			mpq::MPQ exported;
			std::string archived;
			if (exported.open(archive) && exported.file_exists("scripts\\war3map.j")) {
				const std::vector<std::uint8_t> bytes = exported.file_open("scripts\\war3map.j").read();
				archived.assign(bytes.begin(), bytes.end());
			}
			check(archived == scripts_script, "Export MPQ: the archive holds scripts\\war3map.j byte for byte");
			check(!exported.file_exists("war3map.wtg") && !exported.file_exists("war3map.j"), "Export MPQ: no war3map.wtg and no root script");
		}

		// Map Protector on the scripts\-only map: its save, then the pack with Strip trigger strings and without a (listfile).
		const fs::path protected_scripts = root / "protected_scripts.w3x";
		{
			const fs::path protector_temp = root / "protector_scripts_temp";
			fs::create_directories(protector_temp, ec);
			const SyncSaveResult saved = run_sync_save_and_restore(protector_temp, ProtectionOptions {});
			check(saved.success && read_file(protector_temp / "scripts" / "war3map.j") == scripts_script && !fs::exists(protector_temp / "war3map.j"),
				  "Map Protector save (scripts\\): scripts\\war3map.j byte for byte, no root script: " + QString::fromStdString(saved.error));
			const PackResult packed = run_async_pack(protector_temp, protected_scripts, pack_options);
			check(packed.success, "Map Protector pack (scripts\\) succeeded: " + QString::fromStdString(packed.error));
			check(read_archived(protected_scripts, "scripts\\war3map.j") == resolved(scripts_script),
				  "Map Protector pack (scripts\\): scripts\\war3map.j has its trigger string resolved, the rest kept");
			check(!read_archived(protected_scripts, "war3map.wts") && !read_archived(protected_scripts, "war3map.j")
					  && !read_archived(protected_scripts, "(listfile)"),
				  "Map Protector pack (scripts\\): no war3map.wts, no root script, no (listfile)");
		}

		// Open Map (MPQ) on that protected archive, with the unpack folder given instead of picked: the script under scripts\ is
		// extracted by name although the archive has no (listfile), the map keeps it, and the notice comes from load_map.
		{
			const fs::path unpack_location = root / "unpacked";
			fs::create_directories(unpack_location, ec);
			const auto watch = watch_dialogs();
			open_mpq(protected_scripts, unpack_location);
			stop_watch(watch);
			const fs::path unpacked = unpack_location / "protected_scripts";
			check(fs::exists(unpacked / "war3map.w3i") && map->filesystem_path == fs::absolute(unpacked) / "",
				  "Open Map (MPQ): the protected archive was unpacked and opened");
			check(read_file(unpacked / "scripts" / "war3map.j") == resolved(scripts_script),
				  "Open Map (MPQ): scripts\\war3map.j was extracted by name from an archive without a (listfile)");
			check(map->keeps_script && editing_off(), "Open Map (MPQ): the map keeps its script, editing off");
			check(watch->titles == QStringList { "Map Script Kept" }, "Open Map (MPQ): one notice from load_map (dialogs: " + watch->titles.join(", ") + ")");
			check(watch->off_screen, "Open Map (MPQ): the notice was outside every screen");
		}

		// --- Switching maps resets: a normal map has editing on, saves trigger files and a generated script -----------------------------
		open_map(normal_folder);
		{
			const auto watch = watch_dialogs();
			apply_trigger_editing_state(true);
			stop_watch(watch);
			check(!map->keeps_script && editing_on() && watch->titles.isEmpty(), "a normal map: editing on again, no notice");
		}
		fs::remove(normal_folder / "war3map.wtg", ec);
		map->headless = true; // a script compile message would be a dialog
		check(save_current_map() && fs::exists(normal_folder / "war3map.wtg") && fs::exists(normal_folder / "war3map.j"),
			  "a normal map: Save writes war3map.wtg and the generated script as before");

		finish();
	});
}

void HiveWE::run_qa_save_safety_check() {
	if (!qa_offscreen || !qa_save_safety_check) {
		return;
	}

	// Everything below works on copies in a temporary folder, and the backups go there too: the startup map folder is never written.
	const auto temp = std::make_shared<QTemporaryDir>();
	QTimer::singleShot(180000, this, [temp]() {
		std::println("FAIL the save safety check did not finish within 180 s: a dialog is probably blocking (sandbox was {})", temp->path().toStdString());
		std::fflush(stdout);
		temp->remove(); // _Exit runs no destructors
		std::_Exit(3);
	});

	QTimer::singleShot(0, this, [this, temp]() {
		QStringList lines;
		bool all_passed = true;
		const auto write_lines = [&](const bool done) {
			if (qa_script_out.isEmpty()) {
				return;
			}
			QFile file(qa_script_out);
			if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
				file.write(lines.join(QChar('\n')).toUtf8() + (done ? "\nDONE\n" : "\n"));
			}
		};
		const auto check = [&](const bool passed, const QString& text) {
			lines << (passed ? "PASS " : "FAIL ") + text;
			all_passed = all_passed && passed;
			std::println("{}", lines.back().toStdString());
			std::fflush(stdout);
			write_lines(false);
			return passed;
		};
		const auto finish = [&]() {
			qa_backup_root.clear();
			qa_save_as_folder.clear();
			write_lines(true);
			QApplication::exit(all_passed ? 0 : 1);
		};
		const auto read_file = [](const fs::path& path) {
			std::ifstream in(path, std::ios::binary);
			return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		};
		const auto process_events = []() {
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
			QApplication::processEvents();
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		};
		// Open Map without the recent-maps registry write.
		const auto open_map = [&](const fs::path& path) {
			window_handler.close_all();
			process_events();
			ui->widget->makeCurrent();
			reset_map_session();
			map->load(path);
			map->render_manager.resize_framebuffers(ui->widget->width(), ui->widget->height());
		};

		// Watches for modal dialogs from now on: clicks OK on every message box (recording title and text), closes anything else.
		struct Watch {
			QStringList titles;
			QStringList texts;
			bool off_screen = true;
			QPointer<QTimer> timer;
		};
		const auto watch_dialogs = [this]() {
			auto watch = std::make_shared<Watch>();
			auto* timer = new QTimer(this);
			watch->timer = timer;
			connect(timer, &QTimer::timeout, this, [watch]() {
				QWidget* modal = QApplication::activeModalWidget();
				if (!modal || !modal->isVisible()) {
					return;
				}
				watch->titles << modal->windowTitle();
				watch->off_screen = watch->off_screen && std::ranges::none_of(QGuiApplication::screens(), [&](const QScreen* screen) {
					return screen->geometry().intersects(modal->frameGeometry());
				});
				auto* box = qobject_cast<QMessageBox*>(modal);
				if (box && box->button(QMessageBox::Ok)) {
					watch->texts << box->text();
					box->button(QMessageBox::Ok)->click();
				} else {
					watch->texts << QString();
					modal->close();
				}
			});
			timer->start(20);
			return watch;
		};
		const auto stop_watch = [](const std::shared_ptr<Watch>& watch) {
			if (watch->timer) {
				watch->timer->stop();
				watch->timer->deleteLater();
			}
		};
		const auto dialogs = [](const std::shared_ptr<Watch>& watch) {
			return " (dialogs: " + watch->titles.join(", ") + ")";
		};
		// Every file and folder under `folder` with its size, to show that a refused Save As wrote nothing.
		const auto listing = [](const fs::path& folder) {
			std::vector<std::pair<std::wstring, std::uintmax_t>> entries;
			std::error_code ec;
			for (auto it = fs::recursive_directory_iterator(folder, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
				std::error_code size_ec;
				const auto size = it->is_regular_file(size_ec) ? it->file_size(size_ec) : 0;
				entries.emplace_back(it->path().lexically_relative(folder).wstring(), size);
			}
			std::ranges::sort(entries);
			return entries;
		};
		const auto state = [&]() {
			return std::tuple { hierarchy.map_directory, map->filesystem_path, map->name };
		};
		const auto move_unit = [&]() {
			map->units.units.front().position.x += 1.f;
		};

		if (!check(temp->isValid(), "a temporary folder was created")) {
			return finish();
		}
		std::error_code ec;
		const fs::path root = fs::path(temp->path().toStdWString()).make_preferred();
		const fs::path map_folder = root / "map";
		fs::copy(fs::path("data") / "test map", map_folder, fs::copy_options::recursive, ec);
		if (!check(!ec && fs::exists(map_folder / "war3map.w3i"), "sandbox copy made")) {
			return finish();
		}
		const fs::path backups_root = root / "backups";
		qa_backup_root = QString::fromStdWString(backups_root.wstring());
		const fs::path map_backups = map_backup::backups_folder_for(backups_root, map_folder);

		open_map(map_folder);
		if (!check(map->loaded && !map->units.units.empty(), "the sandbox map is open and has a placed unit")) {
			return finish();
		}

		// --- Save (Ctrl+S) backs up the files it replaces --------------------------------------------------------------------------------
		{
			const std::string units_before = read_file(map_folder / "war3mapUnits.doo");
			move_unit();
			const auto watch = watch_dialogs();
			const bool saved = save_current_map();
			stop_watch(watch);
			const auto sets = map_backup::list_sets(map_backups);
			check(saved && sets.size() == 1, "Save: saved and one backup set was made" + dialogs(watch));
			check(!sets.empty() && read_file(sets[0].path / "war3mapUnits.doo") == units_before && read_file(map_folder / "war3mapUnits.doo") != units_before,
				  "Save: the backup holds war3mapUnits.doo from before the save, the map folder the new one");
			check(!sets.empty() && fs::exists(sets[0].path / "war3map.w3i") && fs::exists(sets[0].path / "source.txt")
					  && !fs::exists(sets[0].path / "war3mapImported"),
				  "Save: the backup has the map files and source.txt, no import folders");
			check(!watch->titles.contains("Backup Failed") && transient_notice->text() == "Map saved", "Save: no backup warning, notice \"Map saved\"");

			// The second Save backs up the first one's output (it differs from the original files); a third without changes finds the
			// files identical to that backup and makes none.
			const auto again = watch_dialogs();
			save_current_map();
			save_current_map();
			stop_watch(again);
			const auto later_sets = map_backup::list_sets(map_backups);
			check(later_sets.size() == 2 && read_file(later_sets[0].path / "war3mapUnits.doo") == read_file(map_folder / "war3mapUnits.doo"),
				  "two more Saves without changes add one backup (of the first Save's output), not two");
		}

		// --- A failed backup warns once and the map is still saved ------------------------------------------------------------------------
		{
			const fs::path file_root = root / "backup_root_is_a_file";
			std::ofstream(file_root, std::ios::binary) << "not a folder";
			qa_backup_root = QString::fromStdWString(file_root.wstring());
			const auto map_listing = listing(map_folder);

			const std::string units_before = read_file(map_folder / "war3mapUnits.doo");
			move_unit();
			const auto watch = watch_dialogs();
			const bool saved = save_current_map();
			stop_watch(watch);
			check(saved && read_file(map_folder / "war3mapUnits.doo") != units_before, "failed backup: the map was still saved");
			check(watch->titles.count("Backup Failed") == 1, "failed backup: one Backup Failed warning, OK clicked" + dialogs(watch));
			const qsizetype warning = watch->titles.indexOf("Backup Failed");
			check(warning >= 0 && watch->texts[warning].contains(QString::fromStdWString(file_root.wstring())),
				  "failed backup: the warning names the backup folder");
			check(watch->off_screen, "failed backup: the warning was outside every screen");
			check(transient_notice->text() == "Saved without a backup", "failed backup: notice \"Saved without a backup\"");
			check(fs::is_regular_file(file_root) && listing(map_folder).size() == map_listing.size(), "failed backup: nothing was added to the map folder");

			move_unit();
			const auto second = watch_dialogs();
			const bool saved_again = save_current_map();
			stop_watch(second);
			check(saved_again && second->titles.isEmpty() && transient_notice->text() == "Saved without a backup",
				  "second failed backup: saved, no dialog, only the notice" + dialogs(second));
			qa_backup_root = QString::fromStdWString(backups_root.wstring());
		}

		const QString folder_name = sanitized_folder_name(QString::fromStdString(map->name));
		const fs::path set_count_folder = map_backups;
		const size_t sets_before_refusals = map_backup::list_sets(set_count_folder).size();

		// --- Save As refuses a folder that already holds files (existing map) ------------------------------------------------------------
		{
			const fs::path pick = root / "pick_full";
			fs::create_directories(pick / folder_name.toStdWString(), ec);
			std::ofstream(pick / folder_name.toStdWString() / "keep.txt", std::ios::binary) << "USER FILE";
			move_unit();
			const auto before = state();
			const auto pick_before = listing(pick);
			const auto map_before = listing(map_folder);
			qa_save_as_folder = QString::fromStdWString(pick.wstring());
			const auto watch = watch_dialogs();
			const bool saved = save_current_map_as();
			stop_watch(watch);
			check(!saved && watch->titles == QStringList { "Save As" } && watch->texts.value(0).contains("not empty")
					  && watch->texts.value(0).contains(QString::fromStdWString((pick / folder_name.toStdWString()).wstring())),
				  "Save As into a folder with files (existing map): refused with a message naming the folder" + dialogs(watch));
			check(listing(pick) == pick_before && listing(map_folder) == map_before && read_file(pick / folder_name.toStdWString() / "keep.txt") == "USER FILE",
				  "refused Save As (existing map): nothing was written");
			check(state() == before && has_unsaved_work(), "refused Save As (existing map): paths, name and unsaved state unchanged");
		}

		// --- Save As refuses a folder inside the map's own folder -------------------------------------------------------------------------
		{
			const fs::path inside = map_folder / "qa_inside";
			fs::create_directories(inside, ec);
			const auto before = state();
			const auto map_before = listing(map_folder);
			qa_save_as_folder = QString::fromStdWString(inside.wstring());
			const auto watch = watch_dialogs();
			const bool saved = save_current_map_as();
			stop_watch(watch);
			check(!saved && watch->titles == QStringList { "Save As" } && watch->texts.value(0).contains("inside"),
				  "Save As inside the map's own folder: refused" + dialogs(watch));
			check(listing(map_folder) == map_before && state() == before, "refused Save As (inside): nothing written, state unchanged");
			fs::remove(inside, ec);
		}

		// --- Save As of a new (never saved) map refuses a folder that already holds files, and saves into an empty one --------------------
		{
			const auto saved_state = state();
			map->filesystem_path.clear();
			map->name = "QA New Map";
			const fs::path pick = root / "pick_new_full";
			fs::create_directories(pick / "QA New Map", ec);
			std::ofstream(pick / "QA New Map" / "war3map.w3e", std::ios::binary) << "SOMEONE ELSE'S TERRAIN";
			const auto before = state();
			const auto pick_before = listing(pick);
			qa_save_as_folder = QString::fromStdWString(pick.wstring());
			const auto watch = watch_dialogs();
			const bool saved = save_current_map_as();
			stop_watch(watch);
			check(!saved && watch->titles == QStringList { "Save As" } && watch->texts.value(0).contains("not empty"),
				  "Save As of a new map into a folder with files: refused" + dialogs(watch));
			check(listing(pick) == pick_before && read_file(pick / "QA New Map" / "war3map.w3e") == "SOMEONE ELSE'S TERRAIN" && state() == before,
				  "refused Save As (new map): the other map's war3map.w3e is untouched, the map still has no folder");

			const fs::path empty_pick = root / "pick_new_empty";
			fs::create_directories(empty_pick, ec);
			qa_save_as_folder = QString::fromStdWString(empty_pick.wstring());
			const auto ok_watch = watch_dialogs();
			const bool saved_new = save_current_map_as();
			stop_watch(ok_watch);
			check(saved_new && fs::exists(empty_pick / "QA New Map" / "war3map.w3i") && fs::equivalent(map->filesystem_path, empty_pick / "QA New Map"),
				  "Save As of a new map into an empty folder: saved there" + dialogs(ok_watch));

			std::tie(hierarchy.map_directory, map->filesystem_path, map->name) = saved_state;
		}
		check(map_backup::list_sets(set_count_folder).size() == sets_before_refusals, "the refused Save As runs made no backup of the open map");

		// --- Save As into an empty folder works, and Save As onto the map's own folder is an ordinary save ---------------------------------
		{
			const fs::path pick = root / "pick_empty";
			fs::create_directories(pick, ec);
			qa_save_as_folder = QString::fromStdWString(pick.wstring());
			const auto watch = watch_dialogs();
			const bool saved = save_current_map_as();
			stop_watch(watch);
			const fs::path target = pick / folder_name.toStdWString();
			check(saved && fs::exists(target / "war3map.w3i") && fs::equivalent(map->filesystem_path, target) && fs::equivalent(hierarchy.map_directory, target),
				  "Save As into an empty folder: saved, the map now lives there" + dialogs(watch));
			check(!has_unsaved_work(), "Save As into an empty folder: the map counts as saved");

			move_unit();
			qa_save_as_folder = QString::fromStdWString(target.wstring());
			const auto same = watch_dialogs();
			const bool saved_same = save_current_map_as();
			stop_watch(same);
			check(saved_same && same->titles.isEmpty() && fs::equivalent(map->filesystem_path, target) && !fs::exists(target / folder_name.toStdWString()),
				  "Save As picking the map's own folder: an ordinary save" + dialogs(same));

			move_unit();
			qa_save_as_folder = QString::fromStdWString(pick.wstring());
			const auto parent = watch_dialogs();
			const bool saved_parent = save_current_map_as();
			stop_watch(parent);
			check(saved_parent && parent->titles.isEmpty() && fs::equivalent(map->filesystem_path, target) && !has_unsaved_work(),
				  "Save As picking the folder the map is in (same name): an ordinary save" + dialogs(parent));
		}

		// --- A Save As that fails midway puts everything back, and the next Save writes into the original folder --------------------------
		open_map(map_folder);
		{
			// The copy carries the read-only attribute, so writing war3map.w3e into the new folder throws after the copy.
			const fs::path terrain = map_folder / "war3map.w3e";
			fs::permissions(terrain, fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write, fs::perm_options::remove, ec);
			move_unit();
			const auto before = state();
			const fs::path pick = root / "pick_fails";
			qa_save_as_folder = QString::fromStdWString(pick.wstring());
			const auto watch = watch_dialogs();
			const bool saved = save_current_map_as();
			stop_watch(watch);
			fs::permissions(terrain, fs::perms::owner_write, fs::perm_options::add, ec);
			const fs::path target = pick / folder_name.toStdWString();
			const qsizetype left = watch->titles.lastIndexOf("Save As");
			check(!saved && watch->titles.contains("Saving failed") && left >= 0 && watch->texts[left].contains("did not finish"),
				  "Save As failing midway: refused with Saving failed and a note about the folder left behind" + dialogs(watch));
			check(state() == before, "Save As failing midway: map folder, path and name are back on the original");
			check(has_unsaved_work(), "Save As failing midway: the map still counts as unsaved");

			const std::string original_units = read_file(map_folder / "war3mapUnits.doo");
			const std::string left_units = read_file(target / "war3mapUnits.doo");
			const auto next = watch_dialogs();
			const bool saved_next = save_current_map();
			stop_watch(next);
			check(saved_next && read_file(map_folder / "war3mapUnits.doo") != original_units && read_file(target / "war3mapUnits.doo") == left_units,
				  "the next Save writes into the original folder, not the half-written one" + dialogs(next));
			check(!has_unsaved_work(), "the next Save: the map counts as saved");
			const std::string imp = read_file(map_folder / "war3map.imp");
			check(imp.find("pick_fails") == std::string::npos, "the next Save: war3map.imp does not list the other folder");
		}

		finish();
	});
}

void HiveWE::qa_silent_accessibility_update(QAccessibleEvent* event) {
	if (event && event->type() == QAccessible::Alert) {
		qa_silenced_alerts++;
	}
}

void HiveWE::run_qa_export_mpq_check() {
	if (!qa_offscreen || !qa_export_mpq_check) {
		return;
	}

	// Everything below works on a copy in a temporary folder, and the backups and archives go there too.
	const auto temp = std::make_shared<QTemporaryDir>();
	QTimer::singleShot(180000, this, [temp]() {
		std::println("FAIL the Export MPQ check did not finish within 180 s: a dialog is probably blocking (sandbox was {})", temp->path().toStdString());
		std::fflush(stdout);
		temp->remove(); // _Exit runs no destructors
		std::_Exit(3);
	});

	QTimer::singleShot(0, this, [this, temp]() {
		QStringList lines;
		bool all_passed = true;
		const auto write_lines = [&](const bool done) {
			if (qa_script_out.isEmpty()) {
				return;
			}
			QFile file(qa_script_out);
			if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
				file.write(lines.join(QChar('\n')).toUtf8() + (done ? "\nDONE\n" : "\n"));
			}
		};
		const auto check = [&](const bool passed, const QString& text) {
			lines << (passed ? "PASS " : "FAIL ") + text;
			all_passed = all_passed && passed;
			std::println("{}", lines.back().toStdString());
			std::fflush(stdout);
			write_lines(false);
			return passed;
		};
		const auto finish = [&]() {
			qa_backup_root.clear();
			qa_export_mpq_file.clear();
			write_lines(true);
			QApplication::exit(all_passed ? 0 : 1);
		};
		const auto read_file = [](const fs::path& path) {
			std::ifstream in(path, std::ios::binary);
			return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		};
		const auto process_events = []() {
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
			QApplication::processEvents();
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		};
		const auto open_map = [&](const fs::path& path) {
			window_handler.close_all();
			process_events();
			ui->widget->makeCurrent();
			reset_map_session();
			map->load(path);
			map->render_manager.resize_framebuffers(ui->widget->width(), ui->widget->height());
		};

		// Watches for modal dialogs: records each message box (title, icon, text, detailed text) and clicks OK; closes anything else.
		struct Watch {
			QStringList titles;
			QStringList texts;
			QStringList details;
			QList<int> icons;
			bool off_screen = true;
			QPointer<QTimer> timer;
		};
		const auto watch_dialogs = [this]() {
			auto watch = std::make_shared<Watch>();
			auto* timer = new QTimer(this);
			watch->timer = timer;
			connect(timer, &QTimer::timeout, this, [watch]() {
				QWidget* modal = QApplication::activeModalWidget();
				if (!modal || !modal->isVisible()) {
					return;
				}
				watch->titles << modal->windowTitle();
				watch->off_screen = watch->off_screen && std::ranges::none_of(QGuiApplication::screens(), [&](const QScreen* screen) {
					return screen->geometry().intersects(modal->frameGeometry());
				});
				auto* box = qobject_cast<QMessageBox*>(modal);
				if (box && box->button(QMessageBox::Ok)) {
					watch->texts << box->text();
					watch->details << box->detailedText();
					watch->icons << static_cast<int>(box->icon());
					box->button(QMessageBox::Ok)->click();
				} else {
					watch->texts << QString();
					watch->details << QString();
					watch->icons << -1;
					modal->close();
				}
			});
			timer->start(20);
			return watch;
		};
		const auto stop_watch = [](const std::shared_ptr<Watch>& watch) {
			if (watch->timer) {
				watch->timer->stop();
				watch->timer->deleteLater();
			}
		};
		const auto dialogs = [](const std::shared_ptr<Watch>& watch) {
			return " (dialogs: " + watch->titles.join(", ") + ")";
		};
		// Export MPQ through the real slot: the save, then the file picker answered with `target`, then the result dialog.
		const auto export_to = [&](const fs::path& target) {
			qa_export_mpq_file = QString::fromStdWString(target.wstring());
			const auto watch = watch_dialogs();
			export_mpq();
			stop_watch(watch);
			qa_export_mpq_file.clear();
			return watch;
		};
		// The last dialog with `title` has `icon` and its text contains every one of `parts`.
		const auto last_box = [](const std::shared_ptr<Watch>& watch, const QString& title, const QMessageBox::Icon icon, const QStringList& parts) {
			const qsizetype index = watch->titles.lastIndexOf(title);
			if (index < 0 || watch->icons.value(index) != static_cast<int>(icon)) {
				return false;
			}
			return std::ranges::all_of(parts, [&](const QString& part) { return watch->texts.value(index).contains(part); });
		};
		const auto no_temp_files = [](const fs::path& folder) {
			std::error_code ec;
			for (auto it = fs::directory_iterator(folder, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
				if (it->path().filename().native().find(L".hivewe-tmp") != std::wstring::npos) {
					return false;
				}
			}
			return !ec;
		};
		const auto archived = [](const fs::path& archive, const std::string& name) -> std::optional<std::string> {
			mpq::MPQ opened;
			if (!opened.open(archive, MPQ_OPEN_READ_ONLY) || !opened.file_exists(name)) {
				return std::nullopt;
			}
			const std::vector<std::uint8_t> bytes = opened.file_open(name).read();
			return std::string(bytes.begin(), bytes.end());
		};
		// Files map_archive::write packs from the folder: regular files except save temp files and root archive internal names.
		const auto packed_count = [](const fs::path& folder) {
			std::size_t count = 0;
			std::error_code ec;
			for (auto it = fs::recursive_directory_iterator(folder, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
				std::error_code type_ec;
				if (!it->is_regular_file(type_ec) || is_map_write_temp_file(it->path())) {
					continue;
				}
				const QString name = QString::fromStdWString(it->path().lexically_relative(folder).wstring()).toLower();
				count += (name == "(listfile)" || name == "(attributes)" || name == "(signature)") ? 0 : 1;
			}
			return count;
		};
		const auto hold = [](const fs::path& path, const DWORD share) {
			return CreateFileW(path.c_str(), GENERIC_READ, share, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		};

		if (!check(temp->isValid(), "a temporary folder was created")) {
			return finish();
		}
		std::error_code ec;
		const fs::path root = fs::path(temp->path().toStdWString()).make_preferred();
		const fs::path map_folder = root / "map";
		const fs::path out = root / "out";
		fs::copy(fs::path("data") / "test map", map_folder, fs::copy_options::recursive, ec);
		fs::create_directories(out, ec);
		if (!check(!ec && fs::exists(map_folder / "war3map.w3i"), "sandbox copy made")) {
			return finish();
		}
		qa_backup_root = QString::fromStdWString((root / "backups").wstring());
		open_map(map_folder);
		if (!check(map->loaded && fs::equivalent(map->filesystem_path, map_folder), "the sandbox map is open")) {
			return finish();
		}

		// Silent run: main() installed the handler that keeps Alert and menu events from the Windows backend's system sounds.
		check(QAccessible::installUpdateHandler(qa_silent_accessibility_update) == qa_silent_accessibility_update,
			  "the silent accessibility handler is installed (no system sounds)");

		// --- Success: path and file count --------------------------------------------------------------------------------------------
		{
			const fs::path target = out / "exported.w3x";
			const int alerts_before = qa_silenced_alerts;
			const auto watch = export_to(target);
			check(qa_silenced_alerts > alerts_before, QString("the message box's Alert event reached the silent handler (%1 alerts)").arg(qa_silenced_alerts - alerts_before));
			const std::size_t count = packed_count(map_folder);
			check(!map->script_compile_failed, "control: the sandbox map's script compiles" + dialogs(watch));
			check(watch->titles == QStringList { "Export MPQ" }
					  && last_box(watch, "Export MPQ", QMessageBox::Information,
								  { "exported to", QString::fromStdWString(target.wstring()), QString::number(count) + " files" }),
				  QString("Export MPQ: one information dialog naming the file and %1 files").arg(count) + dialogs(watch));
			check(watch->off_screen, "Export MPQ: the dialog was outside every screen");
			check(archived(target, "war3map.w3i") == read_file(map_folder / "war3map.w3i") && archived(target, "(listfile)").has_value(),
				  "Export MPQ: the archive holds war3map.w3i byte for byte and a (listfile)");
			check(no_temp_files(out), "Export MPQ: no temporary file left");
		}

		// --- An existing file is replaced --------------------------------------------------------------------------------------------
		{
			const fs::path target = out / "replaced.w3x";
			std::ofstream(target, std::ios::binary) << "OLD ARCHIVE";
			const auto watch = export_to(target);
			check(last_box(watch, "Export MPQ", QMessageBox::Information, { "exported to" }) && archived(target, "war3map.w3i").has_value(),
				  "Export MPQ over an existing file: replaced, information dialog" + dialogs(watch));
			check(no_temp_files(out), "Export MPQ over an existing file: no temporary file left");
		}

		// --- Failure: the old file is held open, so it is kept ------------------------------------------------------------------------
		{
			const fs::path target = out / "held.w3x";
			std::ofstream(target, std::ios::binary) << "OLD ARCHIVE";
			const HANDLE held = hold(target, FILE_SHARE_READ);
			check(held != INVALID_HANDLE_VALUE, "the old archive is held open (read, share read)");
			const auto watch = export_to(target);
			if (held != INVALID_HANDLE_VALUE) {
				CloseHandle(held);
			}
			check(last_box(watch, "Exporting failed", QMessageBox::Critical, { "could not replace the existing file", "Windows error", "kept unchanged" }),
				  "held old archive: Exporting failed, with the Windows error and that the old file was kept" + dialogs(watch));
			check(read_file(target) == "OLD ARCHIVE" && no_temp_files(out), "held old archive: old bytes unchanged, no temporary file left");
		}

		// --- Failure: map files that cannot be read; more than ten, so the list goes into the details -----------------------------------
		{
			std::vector<HANDLE> held;
			for (int i = 0; i < 11; i++) {
				const fs::path file = map_folder / ("qa_locked_" + std::to_string(i) + ".txt");
				std::ofstream(file, std::ios::binary) << "held";
				held.push_back(hold(file, 0));
			}
			check(std::ranges::none_of(held, [](const HANDLE h) { return h == INVALID_HANDLE_VALUE; }), "11 map files are held open (no sharing)");
			const fs::path target = out / "locked.w3x";
			const auto watch = export_to(target);
			for (const HANDLE h : held) {
				if (h != INVALID_HANDLE_VALUE) {
					CloseHandle(h);
				}
			}
			const qsizetype index = watch->titles.lastIndexOf("Exporting failed");
			check(last_box(watch, "Exporting failed", QMessageBox::Critical, { "11 of the map's files could not be added", "No file was written", "and 1 more" })
					  && !watch->texts.value(index).contains("kept unchanged"),
				  "held map files: Exporting failed naming the count, no old file to keep" + dialogs(watch));
			bool all_named = index >= 0;
			for (int i = 0; i < 11; i++) {
				all_named = all_named && watch->details.value(index).contains(QString("qa_locked_%1.txt").arg(i));
			}
			check(all_named, "held map files: the details list all 11 files");
			check(!fs::exists(target) && no_temp_files(out), "held map files: no archive and no temporary file written");
			for (int i = 0; i < 11; i++) {
				fs::remove(map_folder / ("qa_locked_" + std::to_string(i) + ".txt"), ec);
			}
		}

		// --- Refusals: nothing is written ---------------------------------------------------------------------------------------------
		{
			const fs::path inside = map_folder / "inside.w3x";
			const auto watch = export_to(inside);
			check(last_box(watch, "Export MPQ", QMessageBox::Warning, { "not exported", "inside the map's own folder" }) && !fs::exists(inside)
					  && no_temp_files(map_folder),
				  "target inside the map folder: refused, nothing written" + dialogs(watch));

			const fs::path folder_target = out / "a_folder.w3x";
			fs::create_directories(folder_target, ec);
			const auto folder_watch = export_to(folder_target);
			check(last_box(folder_watch, "Export MPQ", QMessageBox::Warning, { "not exported", "is a folder" }) && fs::is_empty(folder_target, ec),
				  "target that is a folder: refused, nothing written" + dialogs(folder_watch));

			const std::size_t length = map_archive::max_path_length - out.native().size() - 1 - 10;
			const fs::path long_target = out / (std::wstring(length - 4, L'x') + L".w3x");
			const auto long_watch = export_to(long_target);
			check(length < 250 && last_box(long_watch, "Export MPQ", QMessageBox::Warning, { "not exported", "too long" }) && !fs::exists(long_target)
					  && no_temp_files(out),
				  "path too long for the temporary files: refused, nothing written" + dialogs(long_watch));

			const fs::path read_only = out / "read_only.w3x";
			std::ofstream(read_only, std::ios::binary) << "OLD ARCHIVE";
			fs::permissions(read_only, fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write, fs::perm_options::remove, ec);
			const auto read_only_watch = export_to(read_only);
			const bool still_read_only = (fs::status(read_only, ec).permissions() & fs::perms::owner_write) == fs::perms::none;
			check(last_box(read_only_watch, "Export MPQ", QMessageBox::Warning, { "not exported", "read-only", "kept unchanged" })
					  && read_file(read_only) == "OLD ARCHIVE" && still_read_only && no_temp_files(out),
				  "read-only existing file: refused, old bytes and read-only attribute kept" + dialogs(read_only_watch));
			fs::permissions(read_only, fs::perms::owner_write, fs::perm_options::add, ec);
		}

		// --- Warning: the script did not compile, so the archive holds the previous script, or none --------------------------------------
		{
			const std::string previous_script = read_file(map_folder / "war3map.j");
			check(!previous_script.empty(), "the sandbox map has a war3map.j");
			const std::string saved_header = map->triggers.global_jass;
			map->triggers.global_jass = "this is not valid JASS\n";

			const fs::path target = out / "script_failed.w3x";
			const auto watch = export_to(target);
			check(map->script_compile_failed, "a broken map header makes the script compile fail" + dialogs(watch));
			check(last_box(watch, "Export MPQ", QMessageBox::Warning, { "exported to", "contains the previous map script" }),
				  "script not compiled: the export is a warning saying the archive has the previous script" + dialogs(watch));
			check(archived(target, "war3map.j") == previous_script, "script not compiled: the archive holds the previous war3map.j byte for byte");

			for (const std::string_view name : map_script_file_names) {
				fs::remove(map_folder / fs::path(name), ec);
			}
			const fs::path no_script = out / "no_script.w3x";
			const auto none_watch = export_to(no_script);
			check(last_box(none_watch, "Export MPQ", QMessageBox::Warning, { "exported to", "contains no map script" })
					  && !watch->texts.value(watch->titles.lastIndexOf("Export MPQ")).contains("contains no map script"),
				  "script not compiled, no script before: the warning says the archive has no map script" + dialogs(none_watch));
			check(!archived(no_script, "war3map.j").has_value() && archived(no_script, "war3map.w3i").has_value(),
				  "script not compiled, no script before: no war3map.j in the archive");

			map->triggers.global_jass = saved_header;
			const auto fixed = export_to(out / "fixed.w3x");
			check(!map->script_compile_failed && last_box(fixed, "Export MPQ", QMessageBox::Information, { "exported to" })
					  && archived(out / "fixed.w3x", "war3map.j").has_value(),
				  "script fixed again: information dialog and a new war3map.j in the archive" + dialogs(fixed));
		}

		finish();
	});
}

void HiveWE::run_qa_lua_script_check() {
	if (!qa_offscreen || !qa_lua_script_check) {
		return;
	}

	// Everything below works on copies in a temporary folder; backups, archives and the generated input.lua go there too.
	const auto temp = std::make_shared<QTemporaryDir>();
	QTimer::singleShot(180000, this, [temp]() {
		std::println("FAIL the Lua script check did not finish within 180 s: a dialog is probably blocking (sandbox was {})", temp->path().toStdString());
		std::fflush(stdout);
		temp->remove(); // _Exit runs no destructors
		std::_Exit(3);
	});

	QTimer::singleShot(0, this, [this, temp]() {
		QStringList lines;
		bool all_passed = true;
		const QByteArray original_tmp = qgetenv("TMP");
		const auto write_lines = [&](const bool done) {
			if (qa_script_out.isEmpty()) {
				return;
			}
			QFile file(qa_script_out);
			if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
				file.write(lines.join(QChar('\n')).toUtf8() + (done ? "\nDONE\n" : "\n"));
			}
		};
		const auto check = [&](const bool passed, const QString& text) {
			lines << (passed ? "PASS " : "FAIL ") + text;
			all_passed = all_passed && passed;
			std::println("{}", lines.back().toStdString());
			std::fflush(stdout);
			write_lines(false);
			return passed;
		};
		const auto finish = [&]() {
			qputenv("TMP", original_tmp);
			qa_backup_root.clear();
			qa_export_mpq_file.clear();
			qa_save_as_folder.clear();
			write_lines(true);
			QApplication::exit(all_passed ? 0 : 1);
		};
		const auto read_file = [](const fs::path& path) {
			std::ifstream in(path, std::ios::binary);
			return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		};
		const auto process_events = []() {
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
			QApplication::processEvents();
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		};
		const auto open_map = [&](const fs::path& path) {
			window_handler.close_all();
			process_events();
			ui->widget->makeCurrent();
			reset_map_session();
			map->load(path);
			map->render_manager.resize_framebuffers(ui->widget->width(), ui->widget->height());
		};

		// Watches for modal dialogs: records each message box (title, icon, text) and clicks OK; closes anything else.
		struct Watch {
			QStringList titles;
			QStringList texts;
			QList<int> icons;
			bool off_screen = true;
			QPointer<QTimer> timer;
		};
		const auto watch_dialogs = [this]() {
			auto watch = std::make_shared<Watch>();
			auto* timer = new QTimer(this);
			watch->timer = timer;
			connect(timer, &QTimer::timeout, this, [watch]() {
				QWidget* modal = QApplication::activeModalWidget();
				if (!modal || !modal->isVisible()) {
					return;
				}
				watch->titles << modal->windowTitle();
				watch->off_screen = watch->off_screen && std::ranges::none_of(QGuiApplication::screens(), [&](const QScreen* screen) {
					return screen->geometry().intersects(modal->frameGeometry());
				});
				auto* box = qobject_cast<QMessageBox*>(modal);
				if (box && box->button(QMessageBox::Ok)) {
					watch->texts << box->text();
					watch->icons << static_cast<int>(box->icon());
					box->button(QMessageBox::Ok)->click();
				} else {
					watch->texts << QString();
					watch->icons << -1;
					modal->close();
				}
			});
			timer->start(20);
			return watch;
		};
		const auto stop_watch = [](const std::shared_ptr<Watch>& watch) {
			if (watch->timer) {
				watch->timer->stop();
				watch->timer->deleteLater();
			}
		};
		const auto dialogs = [](const std::shared_ptr<Watch>& watch) {
			return " (dialogs: " + watch->titles.join(", ") + ")";
		};
		// The last dialog with `title` has `icon` and its text contains every one of `parts`.
		const auto last_box = [](const std::shared_ptr<Watch>& watch, const QString& title, const QMessageBox::Icon icon, const QStringList& parts) {
			const qsizetype index = watch->titles.lastIndexOf(title);
			if (index < 0 || watch->icons.value(index) != static_cast<int>(icon)) {
				return false;
			}
			return std::ranges::all_of(parts, [&](const QString& part) { return watch->texts.value(index).contains(part); });
		};
		const auto box_text = [](const std::shared_ptr<Watch>& watch, const QString& title) {
			return watch->texts.value(watch->titles.lastIndexOf(title));
		};
		const auto save = [&]() {
			const auto watch = watch_dialogs();
			const bool saved = save_current_map();
			stop_watch(watch);
			return std::pair { saved, watch };
		};
		const auto archived = [](const fs::path& archive, const std::string& name) -> std::optional<std::string> {
			mpq::MPQ opened;
			if (!opened.open(archive, MPQ_OPEN_READ_ONLY) || !opened.file_exists(name)) {
				return std::nullopt;
			}
			const std::vector<std::uint8_t> bytes = opened.file_open(name).read();
			return std::string(bytes.begin(), bytes.end());
		};
		const auto move_unit = [&]() {
			map->units.units.front().position.x += 1.f;
		};

		if (!check(temp->isValid(), "a temporary folder was created")) {
			return finish();
		}
		std::error_code ec;
		const fs::path root = fs::path(temp->path().toStdWString()).make_preferred();
		const fs::path map_folder = root / "map";
		const fs::path out = root / "out";
		const fs::path tmp = root / "tmp";
		fs::copy(fs::path("data") / "test map", map_folder, fs::copy_options::recursive, ec);
		fs::create_directories(out, ec);
		fs::create_directories(tmp, ec);
		if (!check(!ec && fs::exists(map_folder / "war3map.w3i"), "sandbox copy made")) {
			return finish();
		}
		// generate_map_script writes QDir::tempPath()/input.lua: point it into the sandbox so the user's temp folder is not touched.
		qputenv("TMP", QString::fromStdWString(tmp.wstring()).toLocal8Bit());
		const fs::path input_lua = fs::path(QDir::tempPath().toStdWString()).make_preferred() / "input.lua";
		if (!check(fs::equivalent(input_lua.parent_path(), tmp, ec), "the generated input.lua goes into the sandbox")) {
			return finish();
		}
		qa_backup_root = QString::fromStdWString((root / "backups").wstring());
		check(QAccessible::installUpdateHandler(qa_silent_accessibility_update) == qa_silent_accessibility_update,
			  "the silent accessibility handler is installed (no system sounds)");

		open_map(map_folder);
		if (!check(map->loaded && !map->units.units.empty(), "the sandbox map is open and has a placed unit")) {
			return finish();
		}
		map->info.lua = true; // data/test map is a JASS map

		// --- Accepted: a valid script is written ---------------------------------------------------------------------------------------
		{
			move_unit();
			const auto [saved, watch] = save();
			const std::string script = read_file(map_folder / "war3map.lua");
			check(saved && !map->script_compile_failed && watch->titles.isEmpty(), "Lua map: Save writes the script, no dialog" + dialogs(watch));
			check(!script.empty() && !check_lua_syntax(script).has_value(), "Lua map: the saved war3map.lua parses");
			check(transient_notice->text() == "Map saved", "Lua map: notice \"Map saved\"");
		}

		// --- Refused: a syntax error in the map header ---------------------------------------------------------------------------------
		const std::string saved_header = map->triggers.global_jass;
		const std::string good_script = read_file(map_folder / "war3map.lua");
		{
			map->triggers.global_jass = "function QA_Header()\n  local broken = = 1\nend\n";
			move_unit();
			const std::string units_before = read_file(map_folder / "war3mapUnits.doo");
			const auto [saved, watch] = save();
			check(saved && map->script_compile_failed, "broken header: Save finishes, the script step failed" + dialogs(watch));
			check(watch->titles == QStringList { "Lua script" }
					  && last_box(watch, "Lua script", QMessageBox::Warning,
								  { "war3map.lua was not updated", QString::fromUtf8(lua_syntax_error_prefix.data(), lua_syntax_error_prefix.size()),
									"(Map header)", "unexpected symbol near '='", "previous war3map.lua was kept" }),
				  "broken header: one Lua script warning naming the syntax error, its line and the map header: " + box_text(watch, "Lua script"));
			check(watch->off_screen, "broken header: the dialog was outside every screen");
			check(read_file(map_folder / "war3map.lua") == good_script, "broken header: war3map.lua keeps the previous script byte for byte");
			check(read_file(map_folder / "war3mapUnits.doo") != units_before, "broken header: the other map files were still saved");
			check(read_file(input_lua).contains("local broken = = 1"), "broken header: input.lua holds the generated script for inspection");
			check(transient_notice->text() == "Saved without a new map script", "broken header: notice \"Saved without a new map script\"");
		}

		// --- Refused: an unclosed function in the header is noticed at the end; the message points back at the header -----------------------
		{
			map->triggers.global_jass = "function QA_Unclosed()\n  local open = 1\n";
			const auto [saved, watch] = save();
			const int header_line = lua_line_of_offset(read_file(input_lua), read_file(input_lua).find("function QA_Unclosed"));
			check(saved && map->script_compile_failed
					  && last_box(watch, "Lua script", QMessageBox::Warning,
								  { "'end' expected", QString("It refers to line %1 (Map header)").arg(header_line), "(HiveWE-generated code)" }),
				  "unclosed header function: the warning names the header line it opened at: " + box_text(watch, "Lua script"));
			check(read_file(map_folder / "war3map.lua") == good_script, "unclosed header function: war3map.lua unchanged");
		}

		// --- Refused: a syntax error in a custom-text trigger names the trigger ------------------------------------------------------------
		{
			map->triggers.global_jass = saved_header;
			Trigger trigger = make_new_trigger(Classifier::script, "QA Broken Trigger", map->triggers.new_element_id(), 0);
			trigger.custom_text = "function QA_Trigger()\n  if 1 != 2 then end\nend";
			map->triggers.triggers.push_back(trigger);
			const auto [saved, watch] = save();
			check(saved && last_box(watch, "Lua script", QMessageBox::Warning, { "(Trigger QA Broken Trigger)" }),
				  "broken trigger: the warning names the trigger: " + box_text(watch, "Lua script"));
			check(read_file(map_folder / "war3map.lua") == good_script, "broken trigger: war3map.lua unchanged");
			map->triggers.triggers.pop_back();
			map->triggers.global_jass = "function QA_Header()\n  local broken = = 1\nend\n";
		}

		// --- Save As: the copy holds the previous script, the warning is shown ----------------------------------------------------------------
		{
			const fs::path pick = root / "pick";
			fs::create_directories(pick, ec);
			qa_save_as_folder = QString::fromStdWString(pick.wstring());
			move_unit();
			const auto watch = watch_dialogs();
			const bool saved = save_current_map_as();
			stop_watch(watch);
			qa_save_as_folder.clear();
			const fs::path target = map->filesystem_path;
			const bool in_pick = fs::path(target).make_preferred().lexically_normal().native().starts_with(pick.native());
			check(saved && map->script_compile_failed && in_pick
					  && last_box(watch, "Lua script", QMessageBox::Warning, { "war3map.lua was not updated", "(Map header)" }),
				  "Save As with a broken header: saved into the new folder with the Lua script warning (map now at "
					  + QString::fromStdWString(target.wstring()) + QString(", saved %1, script failed %2)").arg(saved).arg(map->script_compile_failed)
					  + dialogs(watch));
			check(read_file(target / "war3map.lua") == good_script, "Save As with a broken header: the new folder has the previous war3map.lua");
			check(transient_notice->text() == "Saved without a new map script", "Save As with a broken header: notice \"Saved without a new map script\"");
		}

		// --- Export MPQ: warns that the archive holds the previous script ------------------------------------------------------------------
		{
			const fs::path archive = out / "broken.w3x";
			qa_export_mpq_file = QString::fromStdWString(archive.wstring());
			const auto watch = watch_dialogs();
			export_mpq();
			stop_watch(watch);
			qa_export_mpq_file.clear();
			check(map->script_compile_failed && last_box(watch, "Lua script", QMessageBox::Warning, { "(Map header)" })
					  && last_box(watch, "Export MPQ", QMessageBox::Warning, { "exported to", "contains the previous map script" }),
				  "Export MPQ with a broken header: the Lua warning, then the export warning about the previous script" + dialogs(watch));
			check(archived(archive, "war3map.lua") == good_script, "Export MPQ with a broken header: the archive holds the previous war3map.lua");
		}

		// --- A genuine write failure keeps its own message ---------------------------------------------------------------------------------
		{
			map->triggers.global_jass = saved_header;
			fs::remove(input_lua, ec);
			fs::create_directories(input_lua, ec); // a folder where input.lua should go: the script cannot be written
			const auto [saved, watch] = save();
			const QString text = box_text(watch, "Lua script");
			check(saved && map->script_compile_failed && last_box(watch, "Lua script", QMessageBox::Warning, { "The map script could not be written" })
					  && !text.contains("syntax error"),
				  "input.lua cannot be written: the warning says the script could not be written, not a syntax error: " + text);
			fs::remove(input_lua, ec);
		}

		// --- Fixed again: the new script is written, no dialog ------------------------------------------------------------------------------
		{
			move_unit();
			const auto [saved, watch] = save();
			const std::string script = read_file(map->filesystem_path / "war3map.lua");
			check(saved && !map->script_compile_failed && watch->titles.isEmpty() && !check_lua_syntax(script).has_value(),
				  "header fixed: Save writes a parsing war3map.lua, no dialog" + dialogs(watch));
			check(transient_notice->text() == "Map saved", "header fixed: notice \"Map saved\"");
		}

		finish();
	});
}

void HiveWE::run_qa_trigger_tree_check() {
	if (!qa_offscreen || !qa_trigger_tree_check) {
		return;
	}

	// Everything below works on a copy in a temporary folder; backups and the generated script's input file go there too.
	const auto temp = std::make_shared<QTemporaryDir>();
	QTimer::singleShot(180000, this, [temp]() {
		std::println("FAIL the trigger tree check did not finish within 180 s: a dialog is probably blocking (sandbox was {})", temp->path().toStdString());
		std::fflush(stdout);
		temp->remove(); // _Exit runs no destructors
		std::_Exit(3);
	});

	QTimer::singleShot(0, this, [this, temp]() {
		QStringList lines;
		bool all_passed = true;
		const QByteArray original_tmp = qgetenv("TMP");
		const auto write_lines = [&](const bool done) {
			if (qa_script_out.isEmpty()) {
				return;
			}
			QFile file(qa_script_out);
			if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
				file.write(lines.join(QChar('\n')).toUtf8() + (done ? "\nDONE\n" : "\n"));
			}
		};
		// Written line by line, so a crash keeps the results before it.
		const auto check = [&](const bool passed, const QString& text) {
			lines << (passed ? "PASS " : "FAIL ") + text;
			all_passed = all_passed && passed;
			std::println("{}", lines.back().toStdString());
			std::fflush(stdout);
			write_lines(false);
			return passed;
		};
		const auto finish = [&]() {
			qputenv("TMP", original_tmp);
			qa_backup_root.clear();
			write_lines(true);
			QApplication::exit(all_passed ? 0 : 1);
		};
		const auto read_file = [](const fs::path& path) {
			std::ifstream in(path, std::ios::binary);
			return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		};
		const auto process_events = []() {
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
			QApplication::processEvents();
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		};

		if (!check(temp->isValid(), "a temporary folder was created")) {
			return finish();
		}
		std::error_code ec;
		const fs::path root = fs::path(temp->path().toStdWString()).make_preferred();
		const fs::path folder = root / "map";
		const fs::path tmp = root / "tmp";
		fs::copy(fs::path("data") / "test map", folder, fs::copy_options::recursive, ec);
		fs::create_directories(tmp, ec);
		if (!check(!ec && fs::exists(folder / "war3map.wtg"), "sandbox copy made")) {
			return finish();
		}
		qputenv("TMP", QString::fromStdWString(tmp.wstring()).toLocal8Bit());
		qa_backup_root = QString::fromStdWString((root / "backups").wstring());
		check(QAccessible::installUpdateHandler(qa_silent_accessibility_update) == qa_silent_accessibility_update,
			  "the silent accessibility handler is installed (no system sounds)");

		const auto open_map = [&]() {
			window_handler.close_all();
			process_events();
			ui->widget->makeCurrent();
			reset_map_session();
			map->load(folder);
			map->headless = true; // save refusals print instead of opening a dialog
			map->render_manager.resize_framebuffers(ui->widget->width(), ui->widget->height());
		};

		TriggerEditor* editor = nullptr;
		TreeModel* model = nullptr;
		int rows_moved = 0;
		const auto open_editor = [&]() {
			bool created = false;
			editor = window_handler.create_or_raise<TriggerEditor>(nullptr, created);
			auto* explorer = editor->findChild<TriggerExplorer*>();
			model = explorer ? qobject_cast<TreeModel*>(explorer->model()) : nullptr;
			if (model) {
				connect(model, &QAbstractItemModel::rowsMoved, editor, [&rows_moved]() { ++rows_moved; });
			}
			process_events();
			return model != nullptr;
		};
		const auto close_editor = [&]() {
			editor->close();
			process_events();
			editor = nullptr;
			model = nullptr;
		};

		const std::function<QModelIndex(const QModelIndex&, int)> find_index = [&](const QModelIndex& parent, const int id) -> QModelIndex {
			for (int row = 0; row < model->rowCount(parent); ++row) {
				const QModelIndex index = model->index(row, 0, parent);
				if (static_cast<TreeItem*>(index.internalPointer())->id == id) {
					return index;
				}
				if (const QModelIndex found = find_index(index, id); found.isValid()) {
					return found;
				}
			}
			return {};
		};
		constexpr int not_shown = -1000;
		// The id of the item a tree item is listed under, or not_shown.
		const auto shown_under = [&](const int id) {
			const QModelIndex index = find_index({}, id);
			if (!index.isValid() || !index.parent().isValid()) {
				return not_shown;
			}
			return static_cast<TreeItem*>(index.parent().internalPointer())->id;
		};
		// The ids listed directly under an item, in order, restricted to `ids`.
		const auto listed_under = [&](const int id, const QList<int>& ids) {
			QList<int> result;
			const QModelIndex parent = find_index({}, id);
			for (int row = 0; parent.isValid() && row < model->rowCount(parent); ++row) {
				const int child = static_cast<TreeItem*>(model->index(row, 0, parent).internalPointer())->id;
				if (ids.contains(child)) {
					result << child;
				}
			}
			return result;
		};
		const auto stored_parent = [&](const int id) {
			for (const TriggerCategory& category : map->triggers.categories) {
				if (category.id == id) {
					return category.parent_id;
				}
			}
			for (const Trigger& trigger : map->triggers.triggers) {
				if (trigger.id == id) {
					return trigger.parent_id;
				}
			}
			for (const TriggerVariable& variable : map->triggers.variables) {
				if (variable.id == id) {
					return variable.parent_id;
				}
			}
			return not_shown;
		};
		// A drag of `source` dropped onto `target`, through the model's own mime data and drop handler.
		const auto drop = [&](const int source, const int target) {
			const QModelIndex source_index = find_index({}, source);
			const QModelIndex target_index = find_index({}, target);
			if (!source_index.isValid() || !target_index.isValid()) {
				check(false, QString("drag %1 onto %2: both are shown in the tree").arg(source).arg(target));
				return false;
			}
			const std::unique_ptr<QMimeData> mime(model->mimeData({ source_index }));
			return model->dropMimeData(mime.get(), Qt::MoveAction, -1, -1, target_index);
		};

		// TreeModel::supportedDropActions() is IgnoreAction, so the view itself refuses every drop; the drags here call the model.
		lines << "INFO drag and drop is switched off in the Trigger Explorer view; the drags below call the model's drop handler";
		open_map();
		const auto new_category = [&](const std::string& name, const int parent) {
			const int id = map->triggers.new_element_id();
			map->triggers.categories.push_back(make_new_trigger_category(name, id, parent));
			return id;
		};
		const auto new_trigger = [&](const std::string& name, const int parent) {
			const int id = map->triggers.new_element_id();
			map->triggers.triggers.push_back(make_new_trigger(Classifier::comment, name, id, parent));
			return id;
		};
		const auto new_variable = [&](const std::string& name, const int parent) {
			const int id = map->triggers.new_element_id();
			map->triggers.variables.push_back(make_new_trigger_variable(name, id, parent));
			return id;
		};

		// --- A healthy tree: shown as stored, opening changes nothing -------------------------------------------------------------------
		const int a = new_category("QaA", 0);
		const int b = new_category("QaB", a);
		const int c = new_category("QaC", 0);
		const int p = new_category("QaP", 0);
		const int q = new_category("QaQ", p);
		const int r = new_category("QaR", q);
		const int ta = new_trigger("QaTriggerA", a);
		const int tb = new_trigger("QaTriggerB", b);
		const int tc = new_trigger("QaTriggerC", c);
		const int tq = new_trigger("QaTriggerQ", q);
		const int vb = new_variable("QaVariableB", b);
		if (!check(save_current_map(), "healthy sample categories saved into the copy")) {
			return finish();
		}
		open_map();
		const std::string healthy_wtg = read_file(folder / "war3map.wtg");
		{
			const auto state = map->serialized_state();
			if (!check(open_editor(), "the Trigger Editor opens with its tree model")) {
				return finish();
			}
			check(shown_under(a) == 0 && shown_under(b) == a && shown_under(c) == 0 && shown_under(p) == 0 && shown_under(q) == p && shown_under(r) == q
					  && shown_under(ta) == a && shown_under(tb) == b && shown_under(vb) == b && shown_under(tc) == c && shown_under(tq) == q,
				  "healthy: every element is listed under its stored parent");
			check(listed_under(0, { a, c, p }) == QList<int> { a, c, p } && listed_under(a, { b, ta }) == QList<int> { b, ta }
					  && listed_under(b, { tb, vb }) == QList<int> { tb, vb },
				  "healthy: categories keep their order, listed before triggers and variables");
			check(state && map->serialized_state() == state && !has_unsaved_work(), "healthy: opening the window changes no map data");
			close_editor();
			check(map->serialized_state() == state && !has_unsaved_work(), "healthy: closing the window changes no map data");
			check(save_current_map() && read_file(folder / "war3map.wtg") == healthy_wtg, "healthy: war3map.wtg is saved byte for byte");
		}

		// --- Normal drags ---------------------------------------------------------------------------------------------------------------
		{
			open_editor();
			rows_moved = 0;
			check(drop(b, c) && stored_parent(b) == c && shown_under(b) == c && shown_under(tb) == b && rows_moved == 1,
				  "drag a category onto another: moved with its contents");
			check(has_unsaved_work(), "a moved category counts as unsaved");
			check(!drop(b, c) && stored_parent(b) == c, "drag a category onto its own parent: nothing happens");
			check(drop(ta, c) && stored_parent(ta) == c && shown_under(ta) == c, "drag a trigger onto a category: moved");
			check(drop(ta, tb) && stored_parent(ta) == b && shown_under(ta) == b, "drag a trigger onto a trigger: moved into that one's category");
			check(!drop(ta, 0) && stored_parent(ta) == b && shown_under(ta) == b, "drag a trigger onto the map header: refused");

			// --- A category moved under one that is stored later ------------------------------------------------------------------------
			check(drop(a, c) && stored_parent(a) == c && shown_under(a) == c, "drag a category onto one stored after it: moved");
			close_editor();
			check(save_current_map(), "the moves are saved");
			open_map();
			const auto state = map->serialized_state();
			open_editor();
			check(stored_parent(a) == c && stored_parent(b) == c, "reopened: the stored parents are the moved ones");
			check(shown_under(a) == c && shown_under(b) == c && shown_under(tb) == b && shown_under(ta) == b && shown_under(vb) == b,
				  "reopened: the category under a later category and everything in the moved ones are shown");
			check(state && map->serialized_state() == state && !has_unsaved_work(), "reopened: opening the window changes no map data");
			close_editor();
		}

		// --- A stored cycle and missing parents -----------------------------------------------------------------------------------------
		{
			const int x = new_category("QaX", 0);
			const int y = new_category("QaY", x);
			const int z = new_category("QaZ", 999999);
			const int w = new_category("QaW", 0);
			for (TriggerCategory& category : map->triggers.categories) {
				if (category.id == x) {
					category.parent_id = y;
				} else if (category.id == w) {
					category.parent_id = w;
				}
			}
			const int tx = new_trigger("QaTriggerX", x);
			const int ty = new_trigger("QaTriggerY", y);
			const int tz = new_trigger("QaTriggerZ", z);
			const int tw = new_trigger("QaTriggerW", w);
			const int orphan = new_trigger("QaTriggerOrphan", 888888);
			const int orphan_variable = new_variable("QaVariableOrphan", 888888);
			check(save_current_map(), "a cycle, a self parent and missing parents saved into the copy");
			open_map();
			const std::string broken_wtg = read_file(folder / "war3map.wtg");
			const auto state = map->serialized_state();
			open_editor();
			check(stored_parent(x) == y && stored_parent(y) == x && stored_parent(z) == 999999 && stored_parent(w) == w,
				  "broken: the stored parents are kept as read");
			check(shown_under(x) == 0 && shown_under(y) == x && shown_under(tx) == x && shown_under(ty) == y,
				  "broken: the categories of a cycle and their triggers are shown, the first one under the map header");
			check(shown_under(z) == 0 && shown_under(tz) == z && shown_under(w) == 0 && shown_under(tw) == w,
				  "broken: categories with a missing parent or themselves as parent are shown under the map header");
			check(shown_under(orphan) == 0 && shown_under(orphan_variable) == 0,
				  "broken: a trigger and a variable with a missing parent are shown under the map header");
			check(state && map->serialized_state() == state && !has_unsaved_work(), "broken: opening the window changes no map data");
			close_editor();
			check(map->serialized_state() == state && !has_unsaved_work(), "broken: closing the window changes no map data");
			check(save_current_map() && read_file(folder / "war3map.wtg") == broken_wtg, "broken: war3map.wtg is saved byte for byte");

			open_editor();
			if (find_index({}, orphan).isValid()) {
				check(!drop(tc, orphan) && stored_parent(tc) == c, "broken: drag a trigger onto a root level trigger: refused like the map header");
			}
			if (find_index({}, y).isValid()) {
				check(drop(y, 0) && stored_parent(y) == 0 && shown_under(y) == 0, "broken: dragging a cycle's category to the map header repairs it");
			}
			close_editor();
			check(save_current_map(), "the repaired map is saved");
		}

		// --- Onto itself or its own subcategory (last: the old code corrupted the tree here) ---------------------------------------------
		{
			open_map();
			open_editor();
			rows_moved = 0;
			const auto refused = [&](const int target, const QString& what) {
				const bool moved = drop(p, target);
				check(!moved && stored_parent(p) == 0 && stored_parent(q) == p && stored_parent(r) == q && stored_parent(tq) == q && shown_under(p) == 0
						  && shown_under(q) == p && shown_under(r) == q && shown_under(tq) == q && rows_moved == 0 && !has_unsaved_work(),
					  "drag a category onto " + what + ": refused, nothing written, tree unchanged");
			};
			refused(p, "itself");
			refused(q, "its subcategory");
			refused(r, "a subcategory of its subcategory");
			refused(tq, "a trigger inside its subcategory");
			check(drop(r, p) && stored_parent(r) == p && shown_under(r) == p, "drag a subcategory further up: moved");
			close_editor();
		}

		finish();
	});
}

void HiveWE::run_qa_test_map_check() {
	if (!qa_offscreen || !qa_test_map_check) {
		return;
	}

	// Everything below works on copies in a temporary folder; backups and the generated input.lua go there too.
	const auto temp = std::make_shared<QTemporaryDir>();
	QTimer::singleShot(180000, this, [temp]() {
		std::println("FAIL the Test Map check did not finish within 180 s: a dialog is probably blocking (sandbox was {})", temp->path().toStdString());
		std::fflush(stdout);
		temp->remove(); // _Exit runs no destructors
		std::_Exit(3);
	});

	QTimer::singleShot(0, this, [this, temp]() {
		QStringList lines;
		bool all_passed = true;
		const QByteArray original_tmp = qgetenv("TMP");
		const auto write_lines = [&](const bool done) {
			if (qa_script_out.isEmpty()) {
				return;
			}
			QFile file(qa_script_out);
			if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
				file.write(lines.join(QChar('\n')).toUtf8() + (done ? "\nDONE\n" : "\n"));
			}
		};
		const auto check = [&](const bool passed, const QString& text) {
			lines << (passed ? "PASS " : "FAIL ") + text;
			all_passed = all_passed && passed;
			std::println("{}", lines.back().toStdString());
			std::fflush(stdout);
			write_lines(false);
			return passed;
		};
		const auto finish = [&]() {
			qputenv("TMP", original_tmp);
			qa_backup_root.clear();
			qa_save_as_folder.clear();
			write_lines(true);
			QApplication::exit(all_passed ? 0 : 1);
		};
		const auto read_file = [](const fs::path& path) {
			std::ifstream in(path, std::ios::binary);
			return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		};
		const auto process_events = []() {
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
			QApplication::processEvents();
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		};
		const auto open_map = [&](const fs::path& path) {
			window_handler.close_all();
			process_events();
			ui->widget->makeCurrent();
			reset_map_session();
			map->load(path);
			map->render_manager.resize_framebuffers(ui->widget->width(), ui->widget->height());
		};

		// Watches for modal dialogs: records each message box (title, icon, text) and clicks OK; closes anything else.
		struct Watch {
			QStringList titles;
			QStringList texts;
			QList<int> icons;
			bool off_screen = true;
			QPointer<QTimer> timer;
		};
		const auto watch_dialogs = [this]() {
			auto watch = std::make_shared<Watch>();
			auto* timer = new QTimer(this);
			watch->timer = timer;
			connect(timer, &QTimer::timeout, this, [watch]() {
				QWidget* modal = QApplication::activeModalWidget();
				if (!modal || !modal->isVisible()) {
					return;
				}
				watch->titles << modal->windowTitle();
				watch->off_screen = watch->off_screen && std::ranges::none_of(QGuiApplication::screens(), [&](const QScreen* screen) {
					return screen->geometry().intersects(modal->frameGeometry());
				});
				auto* box = qobject_cast<QMessageBox*>(modal);
				if (box && box->button(QMessageBox::Ok)) {
					watch->texts << box->text();
					watch->icons << static_cast<int>(box->icon());
					box->button(QMessageBox::Ok)->click();
				} else {
					watch->texts << QString();
					watch->icons << -1;
					modal->close();
				}
			});
			timer->start(20);
			return watch;
		};
		const auto stop_watch = [](const std::shared_ptr<Watch>& watch) {
			if (watch->timer) {
				watch->timer->stop();
				watch->timer->deleteLater();
			}
		};
		const auto dialogs = [](const std::shared_ptr<Watch>& watch) {
			return " (dialogs: " + watch->titles.join(", ") + (watch->texts.isEmpty() ? QString() : ": " + watch->texts.join(" | ")) + ")";
		};

		if (!check(temp->isValid(), "a temporary folder was created")) {
			return finish();
		}
		std::error_code ec;
		const fs::path root = fs::path(temp->path().toStdWString()).make_preferred();
		const fs::path map_folder = root / "map";
		const fs::path kept_folder = root / "kept";
		const fs::path pick = root / "pick";
		const fs::path tmp = root / "tmp";
		const fs::path fake_game = root / "game";
		fs::copy(fs::path("data") / "test map", map_folder, fs::copy_options::recursive, ec);
		fs::copy(fs::path("data") / "test map", kept_folder, fs::copy_options::recursive, ec);
		fs::remove(kept_folder / "war3map.wtg", ec);
		fs::remove(kept_folder / "war3map.wct", ec);
		fs::create_directories(pick, ec);
		fs::create_directories(tmp, ec);
		fs::create_directories(fake_game / "x86_64", ec);
		// An empty file stands in for the game while play_test runs, so no code path of this hook can start the real one.
		std::ofstream(fake_game / "x86_64" / "Warcraft III.exe", std::ios::binary).flush();
		if (!check(!ec && fs::exists(map_folder / "war3map.w3i") && fs::exists(kept_folder / "war3map.j")
					   && fs::exists(fake_game / "x86_64" / "Warcraft III.exe"),
				   "sandbox copies made")) {
			return finish();
		}
		qputenv("TMP", QString::fromStdWString(tmp.wstring()).toLocal8Bit());
		const fs::path input_lua = fs::path(QDir::tempPath().toStdWString()).make_preferred() / "input.lua";
		if (!check(fs::equivalent(input_lua.parent_path(), tmp, ec), "the generated input.lua goes into the sandbox")) {
			return finish();
		}
		qa_backup_root = QString::fromStdWString((root / "backups").wstring());
		check(QAccessible::installUpdateHandler(qa_silent_accessibility_update) == qa_silent_accessibility_update,
			  "the silent accessibility handler is installed (no system sounds)");

		struct Run {
			std::shared_ptr<Watch> watch;
			QStringList launches;
		};
		// Test Map through the real slot, with its dialogs clicked and the commands it would have started.
		const auto test_map = [&]() {
			const fs::path game_directory = hierarchy.root_directory;
			hierarchy.root_directory = fake_game;
			qa_game_launches.clear();
			const auto watch = watch_dialogs();
			play_test();
			stop_watch(watch);
			hierarchy.root_directory = game_directory;
			return Run { watch, qa_game_launches };
		};
		const QString not_started = "Warcraft III was not started";
		// One launch of the sandbox's stand-in with the open map, and no dialog.
		const auto launched = [&](const Run& run) {
			return run.watch->titles.isEmpty() && run.launches.size() == 1 && run.launches.front().contains("-launch -loadfile")
				&& run.launches.front().contains("game") && run.launches.front().contains(QString::fromStdWString(fs::canonical(map->filesystem_path).wstring()));
		};
		// No launch, and exactly one dialog `title` with `icon` whose text says so and contains `part`.
		const auto stopped = [&](const Run& run, const QString& title, const QMessageBox::Icon icon, const QString& part) {
			return run.launches.isEmpty() && run.watch->titles == QStringList { title } && run.watch->icons.value(0) == static_cast<int>(icon)
				&& run.watch->texts.value(0).contains(not_started) && run.watch->texts.value(0).contains(part) && run.watch->off_screen;
		};

		// --- JASS -------------------------------------------------------------------------------------------------------------------------
		open_map(map_folder);
		if (!check(map->loaded && !map->info.lua, "the sandbox JASS map is open")) {
			return finish();
		}
		{
			const Run run = test_map();
			check(!map->script_compile_failed && launched(run), "JASS, script compiles: the game is started, no dialog" + dialogs(run.watch));

			const std::string saved_header = map->triggers.global_jass;
			const std::string previous_script = read_file(map_folder / "war3map.j");
			map->triggers.global_jass = "this is not valid JASS\n";
			const Run broken = test_map();
			check(map->script_compile_failed, "control: the broken header makes the JASS compile fail");
			check(stopped(broken, "vJass output", QMessageBox::Information, "There were compilation errors"),
				  "JASS compile error: not started, one dialog with the errors that says so" + dialogs(broken.watch));
			check(read_file(map_folder / "war3map.j") == previous_script && transient_notice->text() == "Saved without a new map script",
				  "JASS compile error: the previous war3map.j is kept and the notice says the script is old");

			const auto watch = watch_dialogs();
			const bool saved = save_current_map();
			stop_watch(watch);
			check(saved && watch->titles == QStringList { "vJass output" } && !watch->texts.value(0).contains(not_started),
				  "a plain Save afterwards: the script dialog without the Test Map line" + dialogs(watch));

			map->triggers.global_jass = saved_header;
			const Run fixed = test_map();
			check(!map->script_compile_failed && launched(fixed), "JASS fixed: the game is started again" + dialogs(fixed.watch));
		}

		// --- Lua --------------------------------------------------------------------------------------------------------------------------
		map->info.lua = true;
		{
			const std::string saved_header = map->triggers.global_jass;
			const Run run = test_map();
			check(!map->script_compile_failed && launched(run), "Lua, script parses: the game is started, no dialog" + dialogs(run.watch));

			map->triggers.global_jass = "function QA_Header()\n  local broken = = 1\nend\n";
			const Run broken = test_map();
			check(stopped(broken, "Lua script", QMessageBox::Warning, "war3map.lua was not updated"),
				  "Lua syntax error: not started, one warning with the error that says so" + dialogs(broken.watch));

			map->triggers.global_jass = saved_header;
			fs::remove(input_lua, ec);
			fs::create_directories(input_lua, ec); // a folder where input.lua should go: the script cannot be written
			const Run unwritable = test_map();
			check(stopped(unwritable, "Lua script", QMessageBox::Warning, "The map script could not be written"),
				  "Lua script cannot be written: not started, one warning that says so" + dialogs(unwritable.watch));
			fs::remove(input_lua, ec);

			// A map without a folder yet: Test Map saves it through Save As (the picker is answered by qa_save_as_folder).
			map->triggers.global_jass = "function QA_Header()\n  local broken = = 1\nend\n";
			map->filesystem_path.clear();
			qa_save_as_folder = QString::fromStdWString(pick.wstring());
			const Run save_as_broken = test_map();
			qa_save_as_folder.clear();
			check(!map->filesystem_path.empty() && stopped(save_as_broken, "Lua script", QMessageBox::Warning, "war3map.lua was not updated"),
				  "unsaved map with a syntax error: saved through Save As, not started, one warning" + dialogs(save_as_broken.watch));

			map->triggers.global_jass = saved_header;
			const Run fixed = test_map();
			check(!map->script_compile_failed && launched(fixed), "Lua fixed: the game is started again" + dialogs(fixed.watch));
		}

		// --- A refused save never starts the game -----------------------------------------------------------------------------------------
		{
			Unit& bad_unit = map->units.add_unit("hfoo", glm::vec3(64.f, 64.f, 0.f));
			bad_unit.id = "hfoo5";
			const Run run = test_map();
			check(run.launches.isEmpty() && run.watch->titles.size() == 1, "refused save: not started, one dialog" + dialogs(run.watch));
		}

		// --- A kept script is not generated, so it cannot fail: the game is started -------------------------------------------------------
		open_map(kept_folder);
		{
			const auto watch = watch_dialogs();
			apply_trigger_editing_state(false);
			stop_watch(watch);
			const std::string kept_script = read_file(kept_folder / "war3map.j");
			check(map->keeps_script, "control: the map without war3map.wtg keeps its script");
			const Run run = test_map();
			check(!map->script_compile_failed && launched(run), "kept script: the game is started, no dialog" + dialogs(run.watch));
			check(read_file(kept_folder / "war3map.j") == kept_script, "kept script: war3map.j is unchanged");
		}

		finish();
	});
}

void HiveWE::dump_qa_layout() const {
	QFile file(qa_dump_layout_file);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
		std::println(stderr, "--qa-dump-layout: cannot write {}", qa_dump_layout_file.toStdString());
		return;
	}
	QTextStream out(&file);

	const auto policy_text = [](const QSizePolicy::Policy policy) {
		switch (policy) {
			case QSizePolicy::Fixed: return "Fixed";
			case QSizePolicy::Minimum: return "Minimum";
			case QSizePolicy::Maximum: return "Maximum";
			case QSizePolicy::Preferred: return "Preferred";
			case QSizePolicy::MinimumExpanding: return "MinExpanding";
			case QSizePolicy::Expanding: return "Expanding";
			case QSizePolicy::Ignored: return "Ignored";
		}
		return "?";
	};
	const auto size_text = [](const QSize& size) { return QString("%1x%2").arg(size.width()).arg(size.height()); };

	auto* host = host_for_view(current_sidebar_view);
	auto* scroll = host ? host->findChild<QScrollArea*>("paletteScroll") : nullptr;
	const int viewport_width = scroll ? scroll->viewport()->width() : 0;

	// The effective minimum width is what a layout reserves for the widget (an explicit minimumSize wins, an Ignored policy means none).
	const auto effective_min_width = [](const QWidget* widget) {
		if (widget->minimumWidth() > 0) {
			return widget->minimumWidth();
		}
		return widget->sizePolicy().horizontalPolicy() == QSizePolicy::Ignored ? 0 : widget->minimumSizeHint().width();
	};
	const auto visible_children = [](QWidget* widget) {
		QList<QWidget*> result;
		for (auto* child : widget->children()) {
			if (auto* child_widget = qobject_cast<QWidget*>(child); child_widget && !child_widget->isWindow() && !child_widget->isHidden()) {
				result.push_back(child_widget);
			}
		}
		return result;
	};

	// OVER_VIEWPORT: the minimum width is larger than the scroll area's viewport (not than the parent's geometry, which is already stretched).
	// DEEPEST_OFFENDER: over the viewport, and no visible child is: its layout (margins, spacing, a row or grid of children) makes the width.
	const std::function<void(QWidget*, int)> dump = [&](QWidget* widget, const int depth) {
		const QSize minimum_hint = widget->minimumSizeHint();
		const int min_width = effective_min_width(widget);
		const bool over = viewport_width > 0 && min_width > viewport_width;
		bool child_over = false;
		for (auto* child : visible_children(widget)) {
			child_over = child_over || effective_min_width(child) > viewport_width;
		}
		out << QString(depth * 2, ' ') << widget->metaObject()->className() << " '" << widget->objectName() << "' geo=" << size_text(widget->size())
			<< " minHint=" << size_text(minimum_hint) << " min=" << size_text(widget->minimumSize()) << " hint=" << size_text(widget->sizeHint())
			<< " policy=" << policy_text(widget->sizePolicy().horizontalPolicy()) << "/" << policy_text(widget->sizePolicy().verticalPolicy())
			<< (widget->isVisibleTo(window()) ? "" : " HIDDEN") << (over ? " OVER_VIEWPORT" : "")
			<< (over && !child_over ? " DEEPEST_OFFENDER" : "") << "\n";
		for (auto* child : visible_children(widget)) {
			dump(child, depth + 1);
		}
	};

	if (host) {
		out << "palette view widget tree (host -> scroll area -> palette); viewport width " << viewport_width << "\n";
		dump(host, 0);
		if (scroll) {
			out << "horizontal scrollbar visible: " << (scroll->horizontalScrollBar()->isVisibleTo(scroll) ? "YES" : "no") << "\n";
		}
	} else {
		out << "no palette open (use --palette)\n";
	}
}

void HiveWE::toggle_terrain_sidebar() {
	if (sidebar_shown() && current_sidebar_view == SidebarPaletteView::terrain) {
		hide_palette_sidebar();
		show_transient_notice("Terrain sidebar hidden", 700);
		return;
	}

	show_palette_view(SidebarPaletteView::terrain, false);
	show_transient_notice("Terrain sidebar shown", 700);
}

void HiveWE::toggle_pathing_sidebar() {
	if (sidebar_shown() && current_sidebar_view == SidebarPaletteView::pathing) {
		hide_palette_sidebar();
		show_transient_notice("Pathing sidebar hidden", 700);
		return;
	}

	show_palette_view(SidebarPaletteView::pathing, false);
	show_transient_notice("Pathing sidebar shown", 700);
}

void HiveWE::toggle_doodad_palette() {
	if (sidebar_shown() && current_sidebar_view == SidebarPaletteView::doodad) {
		hide_palette_sidebar();
		show_transient_notice("Doodad sidebar hidden", 700);
		return;
	}

	show_palette_view(SidebarPaletteView::doodad, false);
	show_transient_notice("Doodad sidebar shown", 700);
}

void HiveWE::toggle_unit_palette() {
	if (sidebar_shown() && current_sidebar_view == SidebarPaletteView::unit) {
		hide_palette_sidebar();
		show_transient_notice("Unit sidebar hidden", 700);
		return;
	}

	show_palette_view(SidebarPaletteView::unit, false);
	show_transient_notice("Unit sidebar shown", 700);
}

void HiveWE::toggle_region_palette() {
	if (sidebar_shown() && current_sidebar_view == SidebarPaletteView::region) {
		hide_palette_sidebar();
		show_transient_notice("Region sidebar hidden", 700);
		return;
	}

	show_palette_view(SidebarPaletteView::region, false);
	show_transient_notice("Region sidebar shown", 700);
}

void HiveWE::update_palette_model_preview(std::shared_ptr<mdx::MDX> model, const QString& title) {
	if (!sidebar_preview_title || !sidebar_preview_placeholder) {
		return;
	}

	sidebar_preview_title->setText(title.isEmpty() ? "Model Preview" : title);
	if (!sidebar_preview && model) {
		sidebar_preview = new ModelEditorGLWidget(sidebar_preview_frame, nullptr, true);
		sidebar_preview->setMinimumHeight(150);
		sidebar_preview->setMaximumHeight(220);
		sidebar_preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		if (auto* preview_layout = qobject_cast<QVBoxLayout*>(sidebar_preview_frame->layout())) {
			preview_layout->insertWidget(1, sidebar_preview);
		}
	}

	if (!sidebar_preview) {
		sidebar_preview_placeholder->setVisible(true);
		sidebar_preview_placeholder->setText("No preview model available.");
		return;
	}

	sidebar_preview->set_model(std::move(model));

	const bool has_model = sidebar_preview->mdx != nullptr;
	sidebar_preview->setVisible(has_model);
	sidebar_preview_placeholder->setVisible(!has_model);
	if (!has_model) {
		sidebar_preview_placeholder->setText("No preview model available.");
	}
}

void HiveWE::update_doodad_model_preview(const QString& id, const int variation, const QString& title) {
	QTimer::singleShot(300, this, [this, id, variation, title]() {
		if (QApplication::activeModalWidget()) {
			QTimer::singleShot(300, this, [this, id, variation, title]() {
				update_doodad_model_preview(id, variation, title);
			});
			return;
		}

		if (id.isEmpty()) {
			update_palette_model_preview(nullptr, title);
			return;
		}

		const auto mesh = map->doodads.get_mesh(id.toStdString(), variation);
		update_palette_model_preview(load_preview_mdx(mesh->path), title);
	});
}

void HiveWE::update_unit_model_preview(const QString& id, const QString& title) {
	QTimer::singleShot(300, this, [this, id, title]() {
		if (QApplication::activeModalWidget()) {
			QTimer::singleShot(300, this, [this, id, title]() {
				update_unit_model_preview(id, title);
			});
			return;
		}

		if (id.isEmpty()) {
			update_palette_model_preview(nullptr, title);
			return;
		}

		const auto mesh = map->units.get_mesh(id.toStdString());
		update_palette_model_preview(load_preview_mdx(mesh->path), title);
	});
}

void HiveWE::show_transient_notice(const QString& text, int timeout_ms) {
	transient_notice->setText(text);
	transient_notice->adjustSize();
	transient_notice->resize(transient_notice->width() + 24, transient_notice->height() + 16);
	position_transient_notice();
	transient_notice->show();
	transient_notice->raise();
	transient_notice_timer.start(timeout_ms);
}

void HiveWE::position_transient_notice() {
	if (!transient_notice) {
		return;
	}

	const int x = (width() - transient_notice->width()) / 2;
	const int y = (height() - transient_notice->height()) / 2;
	transient_notice->move(std::max(0, x), std::max(0, y));
}
