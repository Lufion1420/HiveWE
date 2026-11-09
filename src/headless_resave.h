#pragma once

#include <QString>

/// `HiveWE.exe --resave <input_folder> <output_folder>`: loads a map folder and saves it to output_folder without opening the
/// editor window. CASC, the world edit strings and gl_thread_pool must already be set up. The output folder must be absent or
/// empty. Returns the process exit code: 0 saved, 1 refused or failed, 3 saved but the
/// generated script did not compile (the old war3map.j is kept), 4 no CASC or OpenGL. Reasons go to stderr.
int run_headless_resave(const QString& input_folder, const QString& output_folder);
