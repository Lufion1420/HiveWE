find_package(doctest CONFIG REQUIRED)

add_executable(HiveWE_tests
	test_main.cpp
	modification_tables_test.cpp
	object_data_io_test.cpp
	binary_writer_test.cpp
	mdl_reader_test.cpp
	asset_obfuscation_test.cpp
)

target_link_libraries(HiveWE_tests PRIVATE
	HiveWE_lib
	doctest::doctest
)

target_compile_definitions(HiveWE_tests PRIVATE
	MDL_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/mdl"
)

# MSVC's LTCG code generator hits an internal compiler error (C1001 in blp.ixx / LNK1000
# IMAGE::BuildImage) when this target is built with whole-program optimization. Tests gain
# nothing from IPO, so disable it here instead of for the whole project.
set_target_properties(HiveWE_tests PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)

include(doctest)
doctest_discover_tests(HiveWE_tests)
