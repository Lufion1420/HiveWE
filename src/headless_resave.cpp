#include <glad/glad.h>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QSurfaceFormat>

#include "headless_resave.h"

import std;
import Map;
import MapGlobal;
import MapBodyCheck;
import Hierarchy;
import Timer;

namespace fs = std::filesystem;

int run_headless_resave(const QString& input_folder, const QString& output_folder) {
	Timer timer;
	const fs::path input = fs::absolute(input_folder.toStdWString());
	const fs::path output = fs::absolute(output_folder.toStdWString());

	std::error_code ec;
	if (!fs::is_directory(input, ec)) {
		std::println(stderr, "Resave failed: input folder does not exist: {}", input.string());
		return 1;
	}
	// Map::save copies the input folder recursively into the output, so the output must not be the input or lie inside it.
	// Components compare case-insensitively, as Windows paths do.
	const fs::path canonical_input = fs::weakly_canonical(input, ec);
	const fs::path canonical_output = fs::weakly_canonical(output, ec);
	const auto lower = [](const fs::path& part) {
		std::wstring text = part.wstring();
		std::ranges::transform(text, text.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
		return text;
	};
	auto input_part = canonical_input.begin();
	auto output_part = canonical_output.begin();
	while (input_part != canonical_input.end() && output_part != canonical_output.end() && lower(*input_part) == lower(*output_part)) {
		++input_part;
		++output_part;
	}
	// A trailing separator leaves one empty component at the end.
	if (input_part != canonical_input.end() && input_part->empty()) {
		++input_part;
	}
	if (input_part == canonical_input.end()) {
		std::println(stderr, "Resave failed: the output folder is the input folder or inside it: {}", output.string());
		return 1;
	}

	if (fs::exists(output, ec)) {
		if (!fs::is_directory(output, ec) || !fs::is_empty(output, ec)) {
			std::println(stderr, "Resave failed: the output folder must be absent or empty: {}", output.string());
			return 1;
		}
	}

	// Stands in for the GLWidget context: Terrain, doodads and units create GL objects on the main thread while loading.
	// It shares with the global share context, like the gl_thread_pool worker contexts.
	QOffscreenSurface surface;
	surface.setFormat(QSurfaceFormat::defaultFormat());
	surface.create();

	QOpenGLContext context;
	context.setFormat(QSurfaceFormat::defaultFormat());
	context.setShareContext(QOpenGLContext::globalShareContext());
	if (!context.create() || !context.makeCurrent(&surface)) {
		std::println(stderr, "Resave failed: could not create an OpenGL 4.5 context");
		return 4;
	}
	if (!gladLoadGL()) {
		std::println(stderr, "Resave failed: could not load OpenGL functions");
		return 4;
	}

	GLuint vao = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	if (const auto check = check_map_for_opening(input); !check) {
		std::println(stderr, "Resave refused at open: {}", check.error());
		return 1;
	}

	map = new Map();
	map->headless = true;

	try {
		map->load(input);
	} catch (const std::exception& e) {
		std::println(stderr, "Resave failed while loading: {}", e.what());
		return 1;
	}

	// The same steps as Save As into a new folder: the folder exists and the hierarchy points at it before Map::save copies into it.
	fs::create_directories(output, ec);
	if (ec) {
		std::println(stderr, "Resave failed: could not create the output folder: {}", ec.message());
		return 1;
	}
	hierarchy.map_directory = output;

	bool saved = false;
	try {
		saved = map->save(output);
	} catch (const std::exception& e) {
		std::println(stderr, "Resave failed while saving: {}", e.what());
		return 1;
	}

	if (!saved) {
		return 1;
	}

	glFinish();
	std::println("Resave done: {}ms", timer.elapsed_ms());
	return map->script_compile_failed ? 3 : 0;
}
