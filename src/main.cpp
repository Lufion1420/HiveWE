#define MI_MALLOC_OVERRIDE
#include <mimalloc.h>

#define QT_NO_OPENGL

#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QFont>
#include <QGuiApplication>
#include <QScreen>
#include <QMessageBox>
#include <QPalette>
#include <QSurfaceFormat>
#include <QSettings>
#include <QStyleFactory>
#include <QTimer>

#include "main_window/hivewe.h"
#include "main_window/theme.h"
#include "headless_resave.h"
#include "DockManager.h"

#ifdef WIN32
// To force HiveWE to run on the discrete GPU if available
extern "C" {
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
}
#endif

#include <tracy/Tracy.hpp>

import std;
import Map;
import MapBodyCheck;
import Timer;
import MapGlobal;
import Globals;
import Utilities;
import ThemeTokens;
import Hierarchy;
import BinaryReader;
import GLThreadPool;
namespace fs = std::filesystem;

namespace {
/// --qa-offscreen: every top-level window the app creates (dialogs, editors, message boxes) refuses activation, so an automated run never
/// takes the foreground from the user.
class NoActivateFilter : public QObject {
  public:
	using QObject::QObject;

	bool eventFilter(QObject* watched, QEvent* event) override {
		if (event->type() == QEvent::Polish) {
			auto* widget = qobject_cast<QWidget*>(watched);
			if (widget && (widget->windowType() == Qt::Window || widget->windowType() == Qt::Dialog) && !widget->isVisible()) {
				widget->setAttribute(Qt::WA_ShowWithoutActivating);
				widget->setWindowFlag(Qt::WindowDoesNotAcceptFocus, true);
				// A window without a parent would open on the primary screen. Dialogs with a parent are centred on it, which is already off screen.
				if (!widget->parentWidget() && !widget->testAttribute(Qt::WA_Moved)) {
					widget->move(QGuiApplication::primaryScreen()->virtualGeometry().left() - 4000, 0);
				}
			}
		}
		return false;
	}
};
}

int main(int argc, char* argv[]) {
	ZoneScopedN("main");

	Timer start_timer;

	QSurfaceFormat format;
	format.setDepthBufferSize(24);
	format.setStencilBufferSize(8);
	format.setVersion(4, 5);
	format.setProfile(QSurfaceFormat::CoreProfile);
	format.setOption(QSurfaceFormat::DebugContext);
	format.setSwapInterval(1);
	//format.setColorSpace(QSurfaceFormat::sRGBColorSpace);
	QSurfaceFormat::setDefaultFormat(format);

	QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
	QCoreApplication::setOrganizationName("HiveWE");
	QCoreApplication::setApplicationName("HiveWE");

	QLocale::setDefault(QLocale("en_US"));

	// Create a dark palette
	// For some magically unknown reason Qt draws Qt::white text as black, so we use QColor(255, 254, 255) instead
	QPalette darkPalette;
	darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
	darkPalette.setColor(QPalette::WindowText, QColor(255, 254, 255));
	darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
	darkPalette.setColor(QPalette::Base, QColor(42, 42, 42));
	darkPalette.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
	darkPalette.setColor(QPalette::ToolTipBase, QColor(66, 66, 66));
	darkPalette.setColor(QPalette::ToolTipText, QColor(255, 254, 255));
	darkPalette.setColor(QPalette::Text, QColor(255, 254, 255));
	darkPalette.setColor(QPalette::PlaceholderText, Qt::gray);
	darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
	darkPalette.setColor(QPalette::Dark, QColor(35, 35, 35));
	darkPalette.setColor(QPalette::Shadow, QColor(20, 20, 20));
	darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
	darkPalette.setColor(QPalette::ButtonText, QColor(255, 254, 255));
	darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
	darkPalette.setColor(QPalette::BrightText, Qt::red);
	darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
	darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
	darkPalette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
	darkPalette.setColor(QPalette::HighlightedText, QColor(255, 254, 255));
	darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(127, 127, 127));

	QApplication::setPalette(darkPalette);
	QApplication::setStyle("Fusion");

	QApplication a(argc, argv);

	// HiveWE.exe --resave <input_folder> <output_folder>: load and save one map folder without the editor window.
	const QStringList arguments = QApplication::arguments();
	const bool resave = arguments.size() >= 2 && arguments[1] == "--resave";
	if (resave && arguments.size() != 4) {
		std::println(stderr, "Usage: HiveWE --resave <input_folder> <output_folder>");
		return 2;
	}

	// Options for automated screenshots and harmless in normal use.
	QString theme_override;
	for (int i = 1; i < arguments.size(); ++i) {
		if (arguments[i] == "--help" || arguments[i] == "-h") {
			std::println(
				"Usage: HiveWE [--theme <name>] [--qa-offscreen [--window-size WxH]] [--palette <name>]\n"
				"       HiveWE --resave <input_folder> <output_folder>\n"
				"\n"
				"  --theme <name>       Use this theme for this run only (Dark, Light, Flat Dark or Flat Light). The saved setting is not changed.\n"
				"  --qa-offscreen       Show the main window outside all monitors, without activating or raising it. Its position is not saved.\n"
				"  --window-size WxH    Size in pixels of the --qa-offscreen window, for example 1366x768. Ignored without --qa-offscreen.\n"
				"  --palette <name>     Open a palette after the startup map has loaded: terrain, doodads, units, pathing or regions.\n"
				"                       Needs a loaded map, so it only works with the startup map (data/test map).\n"
				"  --qa-select-first    With --palette units or doodads: select the first unit or doodad of the map, adding a sample one in memory if the map has none (for screenshots).\n"
				"  --qa-open-dialog <name>  Needs --qa-offscreen. Open one dialog or tool window with sample data after the map has loaded (never saved). Names:\n"
				"                       settings, unit-properties, item-drop, map-settings, scenario, gameplay-constants, new-map, object-editor,\n"
				"                       asset-manager, map-protector, import-object-data (empty package), item-tables, config, tile-setter, tile-setter-changes, tile-setter-limits (prints OK/Remove button states to stdout and --qa-script-out), tile-pather,\n"
				"                       model-editor (stock model units/human/footman/footman.mdx), browse-models (the Model Editor's model browser; with --qa-script-out <file>\n"
				"                       it waits for the thumbnails, writes result lines to <file> and a PNG of the dialog next to it).\n"
				"  --qa-model-editor-script \"<steps>\"  With --qa-open-dialog model-editor and --qa-script-out <file>: drive the Model Editor's widgets in-process\n"
				"                       (steps separated by ';', for example \"wait:3000;pause;slider:500;expect:timefrac=0.5\") and write PASS/FAIL/INFO lines to the file, then quit.\n"
				"                       The steps are listed above ModelEditor::run_qa_script in src/model_editor/model_editor.cpp.\n"
				"  --qa-model <path>    With --qa-open-dialog model-editor: the model to open instead of units/human/footman/footman.mdx.\n"
				"  --qa-change-tileset <out folder>  Needs --qa-offscreen, --qa-tile-texture <blp|tga> and --qa-script-out <file>: grows the startup map to 20 tiles with\n"
				"                       the texture on two tiles, runs the Change Tileset OK path, saves to <out folder> (absent or empty), reloads it and writes PASS/FAIL lines.\n"
				"                       Run it from a sandbox working directory: the overrides are written into its data/test map. Exit code 0 only when all pass.\n"
				"  --qa-tileset-clipboard-check  Needs --qa-offscreen, --palette terrain and --qa-tile-texture <blp|tga> (--qa-script-out <file> optional): fills undo history and\n"
				"                       terrain clipboard, runs Change Tileset with a tile removed (both dropped), then with only a texture replaced (both kept, map counts as changed).\n"
				"                       The texture goes into a temporary folder, nothing is written to the map folder. Never saves. Exit code 0 only when all pass.\n"
				"  --qa-doodad-undo-check  Needs --qa-offscreen and --palette doodads (--qa-script-out <file> optional): adds three doodads in memory, checks delete, undo, redo,\n"
				"                       undo again and cut with undo through the doodad brush, writes PASS/FAIL lines and quits. Never saves. Exit code 0 only when all pass.\n"
				"  --qa-refusal-checks  Needs --qa-offscreen (--qa-script-out <file> optional): on sandbox copies of data/test map, checks that Save, Save As, Test Map and\n"
				"                       Export MPQ refuse a unit with a 5-byte id without writing a file, and that undo works after Open refused a damaged map.\n"
				"                       Writes PASS/FAIL lines and quits. Run it from a sandbox working directory (the control Save writes data/tools/war3map.j).\n"
				"                       Exit code 0 only when all pass.\n"
				"  --qa-object-new-check  Needs --qa-offscreen (--qa-script-out <file> optional): drives the Object Editor's Add dialog for units without clicks: a typed\n"
				"                       non-ASCII id (hfo+o-umlaut) must keep OK disabled and a double-click on a base unit must create nothing; a valid id is the control.\n"
				"                       Adds one custom unit in memory, never saves. Writes PASS/FAIL lines and quits. Exit code 0 only when all pass.\n"
				"  --qa-region-palette-check  Needs --qa-offscreen and --palette regions (--qa-script-out <file> optional): checks that the Regions palette lists the regions\n"
				"                       of the startup map (data/test map of the working directory; it has one region) by count and name. Never saves. Writes PASS/FAIL lines\n"
				"                       and quits. Exit code 0 only when all pass.\n"
				"  --qa-dialog-tab <n> With --qa-open-dialog: select tab n (0-based) of the window's first tab widget.\n"
				"  --qa-dump-layout <file>  With --qa-offscreen and --palette: 2.5 s after startup, write the widget tree of the open palette\n"
				"                       (sizes, size hints, policies) to <file>.\n"
				"  --resave <in> <out>  Load one map folder and save it to another folder without opening the editor window.\n"
				"  --help               Show this text."
			);
			return 0;
		}
		if (arguments[i] == "--qa-offscreen") {
			HiveWE::qa_offscreen = true;
		} else if (arguments[i] == "--theme") {
			if (i + 1 >= arguments.size()) {
				std::println(stderr, "--theme needs a theme name. See HiveWE --help.");
				return 2;
			}
			theme_override = arguments[++i];
		} else if (arguments[i] == "--palette") {
			static const QStringList names = { "terrain", "doodads", "units", "pathing", "regions" };
			if (i + 1 >= arguments.size() || !names.contains(arguments[i + 1])) {
				std::println(stderr, "--palette needs one of: terrain, doodads, units, pathing, regions. See HiveWE --help.");
				return 2;
			}
			HiveWE::qa_palette = arguments[++i];
		} else if (arguments[i] == "--qa-select-first") {
			// The hook adds a sample object to the loaded map, so it must never run in a window the user works in.
			if (!HiveWE::qa_offscreen && !arguments.contains("--qa-offscreen")) {
				std::println(stderr, "--qa-select-first needs --qa-offscreen. See HiveWE --help.");
				return 2;
			}
			HiveWE::qa_select_first = true;
		} else if (arguments[i] == "--qa-doodad-undo-check") {
			// The hook adds doodads to the loaded map, so it must never run in a window the user works in.
			if (!arguments.contains("--qa-offscreen") || !arguments.contains("--palette")) {
				std::println(stderr, "--qa-doodad-undo-check needs --qa-offscreen and --palette doodads. See HiveWE --help.");
				return 2;
			}
			HiveWE::qa_doodad_undo_check = true;
		} else if (arguments[i] == "--qa-tileset-clipboard-check") {
			// The hook changes the loaded map's tileset in memory (the texture goes into a temporary folder), so it must never run in a window the user works in.
			const int palette = arguments.indexOf("--palette");
			if (!arguments.contains("--qa-offscreen") || palette < 0 || arguments.value(palette + 1) != "terrain" || !arguments.contains("--qa-tile-texture")) {
				std::println(stderr, "--qa-tileset-clipboard-check needs --qa-offscreen, --palette terrain and --qa-tile-texture <file>. See HiveWE --help.");
				return 2;
			}
			HiveWE::qa_tileset_clipboard_check = true;
		} else if (arguments[i] == "--qa-refusal-checks") {
			if (!arguments.contains("--qa-offscreen")) {
				std::println(stderr, "--qa-refusal-checks needs --qa-offscreen. See HiveWE --help.");
				return 2;
			}
			HiveWE::qa_refusal_checks = true;
		} else if (arguments[i] == "--qa-object-new-check") {
			// The hook adds an object to the loaded map's tables, so it must never run in a window the user works in.
			if (!arguments.contains("--qa-offscreen")) {
				std::println(stderr, "--qa-object-new-check needs --qa-offscreen. See HiveWE --help.");
				return 2;
			}
			HiveWE::qa_object_new_check = true;
		} else if (arguments[i] == "--qa-region-palette-check") {
			if (!arguments.contains("--qa-offscreen") || arguments.indexOf("--palette") < 0 || arguments.value(arguments.indexOf("--palette") + 1) != "regions") {
				std::println(stderr, "--qa-region-palette-check needs --qa-offscreen and --palette regions. See HiveWE --help.");
				return 2;
			}
			HiveWE::qa_region_palette_check = true;
		} else if (arguments[i] == "--qa-open-dialog") {
			static const QStringList names = { "settings", "unit-properties", "item-drop", "map-settings", "scenario", "gameplay-constants", "new-map",
				"object-editor", "asset-manager", "map-protector", "import-object-data", "item-tables", "config", "tile-setter", "tile-setter-changes", "tile-setter-limits", "tile-pather", "model-editor", "browse-models" };
			if (i + 1 >= arguments.size() || !names.contains(arguments[i + 1])) {
				std::println(stderr, "--qa-open-dialog needs one of: {}. See HiveWE --help.", names.join(", ").toStdString());
				return 2;
			}
			// The hook adds sample data to the loaded map, so it must never run in a window the user works in.
			if (!arguments.contains("--qa-offscreen")) {
				std::println(stderr, "--qa-open-dialog needs --qa-offscreen. See HiveWE --help.");
				return 2;
			}
			HiveWE::qa_open_dialog = arguments[++i];
		} else if (arguments[i] == "--qa-model") {
			if (i + 1 >= arguments.size() || !arguments.contains("--qa-offscreen")) {
				std::println(stderr, "--qa-model needs a model path and --qa-offscreen. See HiveWE --help.");
				return 2;
			}
			HiveWE::qa_model = arguments[++i];
		} else if (arguments[i] == "--qa-model-editor-script" || arguments[i] == "--qa-script-out") {
			const bool is_script = arguments[i] == "--qa-model-editor-script";
			if (i + 1 >= arguments.size() || !arguments.contains("--qa-offscreen")) {
				std::println(stderr, "{} needs a value and --qa-offscreen. See HiveWE --help.", arguments[i].toStdString());
				return 2;
			}
			(is_script ? HiveWE::qa_model_editor_script : HiveWE::qa_script_out) = arguments[++i];
		} else if (arguments[i] == "--qa-change-tileset" || arguments[i] == "--qa-tile-texture") {
			const bool is_folder = arguments[i] == "--qa-change-tileset";
			if (i + 1 >= arguments.size() || !arguments.contains("--qa-offscreen")) {
				std::println(stderr, "{} needs a value and --qa-offscreen. See HiveWE --help.", arguments[i].toStdString());
				return 2;
			}
			(is_folder ? HiveWE::qa_change_tileset : HiveWE::qa_tile_texture) = arguments[++i];
		} else if (arguments[i] == "--qa-dialog-tab") {
			bool tab_ok = false;
			const int tab = i + 1 < arguments.size() ? arguments[i + 1].toInt(&tab_ok) : 0;
			if (!tab_ok || tab < 0) {
				std::println(stderr, "--qa-dialog-tab needs a tab index like 1. See HiveWE --help.");
				return 2;
			}
			HiveWE::qa_dialog_tab = tab;
			++i;
		} else if (arguments[i] == "--qa-dump-layout") {
			if (i + 1 >= arguments.size()) {
				std::println(stderr, "--qa-dump-layout needs a file name. See HiveWE --help.");
				return 2;
			}
			HiveWE::qa_dump_layout_file = arguments[++i];
		} else if (arguments[i] == "--window-size") {
			const QStringList parts = i + 1 < arguments.size() ? arguments[i + 1].split('x') : QStringList();
			bool width_ok = false;
			bool height_ok = false;
			const int width = parts.size() == 2 ? parts[0].toInt(&width_ok) : 0;
			const int height = parts.size() == 2 ? parts[1].toInt(&height_ok) : 0;
			if (!width_ok || !height_ok || width < 200 || height < 200) {
				std::println(stderr, "--window-size needs a size like 1366x768. See HiveWE --help.");
				return 2;
			}
			HiveWE::qa_window_size = QSize(width, height);
			++i;
		}
	}

	if (HiveWE::qa_offscreen) {
		a.installEventFilter(new NoActivateFilter(&a));
	}

	ads::CDockManager::setConfigFlag(ads::CDockManager::FocusHighlighting);
	ads::CDockManager::setConfigFlag(ads::CDockManager::AllTabsHaveCloseButton);
	ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaDynamicTabsMenuButtonVisibility);
	ads::CDockManager::setConfigFlag(ads::CDockManager::OpaqueSplitterResize);
	ads::CDockManager::setConfigFlag(ads::CDockManager::MiddleMouseButtonClosesTab);

	QSettings settings;
	// --resave opens no window, so it does not need the theme.
	if (!resave) {
		// A broken theme must never stop HiveWE from starting: fall back to Dark, and to no style sheet if that fails too.
		const QString requested = theme_override.isEmpty() ? settings.value("theme", "Dark").toString() : theme_override;
		const auto startup_theme = theme_tokens::apply_with_fallback(requested.toStdString(), [](const std::string& name) {
			return theme::apply(QString::fromStdString(name)).toStdString();
		});
		if (!startup_theme.error.empty()) {
			qWarning() << "Error: Applying the theme failed:" << QString::fromStdString(startup_theme.error);
			const QString used = startup_theme.applied.empty() ? QString("no theme (the default look)") : QString::fromStdString(startup_theme.applied);
			const QString message = "The theme \"" + requested + "\" could not be applied:\n\n" + QString::fromStdString(startup_theme.error)
				+ "\n\nHiveWE starts with " + used + ". Pick another theme in Settings.";
			if (HiveWE::qa_offscreen) {
				// An automated run must not open a window on the user's screen.
				std::println(stderr, "{}", message.toStdString());
			} else {
				QMessageBox::warning(nullptr, "Theme", message);
			}
		}
	}

	const auto load_files = [] {
		// Place common.j and blizzard.j in the data folder. Required by JassHelper
		BinaryReader common = hierarchy.open_file("scripts/common.j").value();
		std::ofstream output("data/tools/common.j");
		output.write(reinterpret_cast<char*>(common.buffer.data()), common.buffer.size());
		BinaryReader blizzard = hierarchy.open_file("scripts/blizzard.j").value();
		std::ofstream output2("data/tools/blizzard.j");
		output2.write(reinterpret_cast<char*>(blizzard.buffer.data()), blizzard.buffer.size());

		world_edit_strings.load("UI/WorldEditStrings.txt");
		world_edit_game_strings.load("UI/WorldEditGameStrings.txt");
		world_edit_data.load("UI/WorldEditData.txt");

		world_edit_data.substitute(world_edit_game_strings, "WorldEditStrings");
		world_edit_data.substitute(world_edit_strings, "WorldEditStrings");
	};

	bool is_casc_open = false;
	const auto casc_future = std::async(std::launch::async, [&]() {
		const fs::path directory = find_warcraft_directory();

		is_casc_open = hierarchy.open_casc(directory);
		if (is_casc_open) {
			load_files();
		}
	});

	gl_thread_pool.init(8);

	casc_future.wait();

	if (!is_casc_open && resave) {
		std::println(stderr, "Resave failed: could not open the Warcraft III CASC storage");
		return 4;
	}

	if (!is_casc_open) {
		fs::path directory = find_warcraft_directory();

		while (!hierarchy.open_casc(directory)) {
			directory = QFileDialog::getExistingDirectory(nullptr, "Select Warcraft Directory", "/home", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks).toStdWString();
			if (directory == "") {
				exit(EXIT_SUCCESS);
			}
		}
		settings.setValue("warcraftDirectory", QString::fromStdString(directory.string()));

		load_files();
	}

	if (resave) {
		const int exit_code = run_headless_resave(arguments[2], arguments[3]);
		// Skip static destructors: they free GL objects and join the GL worker threads after their contexts are gone.
		std::fflush(stdout);
		std::fflush(stderr);
		std::_Exit(exit_code);
	}

	const fs::path startup_map = "data/test map/";

	// Before the window exists: without a loaded map the object tables stay null while the ribbon is live.
	if (const auto check = check_map_for_opening(startup_map); !check) {
		QMessageBox::critical(nullptr, "Opening map failed", QString::fromStdString(check.error()) + "\n\nThe map was not changed.");
		return 1;
	}

	HiveWE w;

	std::println("Application start: {}ms", start_timer.elapsed_ms());

	// An exception here (for example a missing data file) would otherwise hit std::terminate with no message.
	try {
		map->load(startup_map);
	} catch (const std::exception& e) {
		QMessageBox::critical(
			nullptr,
			"Opening map failed",
			QString("The map could not be loaded: %1\n\nA data file may be missing or damaged.").arg(QString::fromLocal8Bit(e.what()))
		);
		return 1;
	} catch (...) {
		QMessageBox::critical(nullptr, "Opening map failed", "The map could not be loaded (unknown error).\n\nA data file may be missing or damaged.");
		return 1;
	}
	// map->load("C:/Users/User/Desktop/MCFC.w3x");

	w.open_qa_palette();
	w.open_qa_dialog();
	w.run_qa_change_tileset();
	w.run_qa_tileset_clipboard_check();
	w.run_qa_doodad_undo_check();
	w.run_qa_refusal_checks();
	w.run_qa_object_new_check();
	w.run_qa_region_palette_check();
	if (HiveWE::qa_offscreen && !HiveWE::qa_dump_layout_file.isEmpty()) {
		QTimer::singleShot(2500, &w, [&w]() { w.dump_qa_layout(); });
	}

	return QApplication::exec();
}
