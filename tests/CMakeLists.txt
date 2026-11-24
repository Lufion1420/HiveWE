find_package(doctest CONFIG REQUIRED)

add_executable(HiveWE_tests
	test_main.cpp
	modification_tables_test.cpp
	object_data_io_test.cpp
	binary_writer_test.cpp
	mdl_reader_test.cpp
	mdx_camera_test.cpp
	mdx_skin_test.cpp
	asset_obfuscation_test.cpp
	protection_pipeline_test.cpp
	map_format_check_test.cpp
	map_body_check_test.cpp
	start_location_index_test.cpp
	map_settings_script_test.cpp
	preset_value_test.cpp
	item_globals_test.cpp
	unit_script_test.cpp
	sounds_test.cpp
	object_data_save_check_test.cpp
	ally_priorities_script_test.cpp
	map_info_weather_test.cpp
	trigger_variable_test.cpp
	map_save_failure_test.cpp
	theme_tokens_test.cpp
	selection_hit_points_test.cpp
	model_editor_logic_test.cpp
	tile_animation_test.cpp
	tileset_change_test.cpp
	imports_test.cpp
	world_undo_unsaved_test.cpp
	trigger_strings_literal_test.cpp
)

target_link_libraries(HiveWE_tests PRIVATE
	HiveWE_lib
	doctest::doctest
)

target_compile_definitions(HiveWE_tests PRIVATE
	MDL_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/mdl"
	THEMES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../data/themes"
	TEST_MAP_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../data/test map"
)

# MSVC's LTCG code generator hits an internal compiler error (C1001 in blp.ixx / LNK1000
# IMAGE::BuildImage) when this target is built with whole-program optimization. Tests gain
# nothing from IPO, so disable it here instead of for the whole project.
set_target_properties(HiveWE_tests PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)

include(doctest)
doctest_discover_tests(HiveWE_tests)

# Smoke check of `HiveWE --resave` (src/headless_resave.cpp): saves a copy of data/test map into the build folder, checks the
# output and the refusals (bad arguments, non-empty output, output inside the input). Reported as skipped when there is no
# data folder next to the exe, no Warcraft III CASC or no OpenGL 4.5 (exit 4), for example on CI.
add_test(NAME resave_smoke COMMAND ${CMAKE_COMMAND}
	-DHIVEWE_EXE=$<TARGET_FILE:HiveWE>
	"-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/../data/test map"
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/resave_smoke_out
	"-DUNWRITABLE_W3A=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/resave/unwritable_field.w3a"
	"-DSKINNED_DOO=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/resave/skinned_doodads.doo"
	-P ${CMAKE_CURRENT_SOURCE_DIR}/resave_smoke.cmake
)
set_tests_properties(resave_smoke PROPERTIES SKIP_REGULAR_EXPRESSION "RESAVE_SMOKE_SKIPPED" TIMEOUT 300)
