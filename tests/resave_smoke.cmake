# Smoke test of `HiveWE --resave`, called from tests/CMakeLists.txt with HIVEWE_EXE, INPUT and OUTPUT.
# INPUT is copied to a sandbox first, so nothing is ever written next to it.
get_filename_component(exe_dir "${HIVEWE_EXE}" DIRECTORY)
set(sandbox "${OUTPUT}_input")

function(run_resave out_result out_errors)
	execute_process(
		COMMAND "${HIVEWE_EXE}" --resave ${ARGN}
		WORKING_DIRECTORY "${exe_dir}"
		RESULT_VARIABLE result
		OUTPUT_QUIET
		ERROR_VARIABLE errors
		TIMEOUT 240
	)
	set(${out_result} "${result}" PARENT_SCOPE)
	set(${out_errors} "${errors}" PARENT_SCOPE)
endfunction()

if(NOT EXISTS "${exe_dir}/data")
	message("RESAVE_SMOKE_SKIPPED: no data folder next to ${HIVEWE_EXE}")
	return()
endif()

# Wrong argument count: exit 2, before CASC is opened.
run_resave(result errors "${INPUT}")
if(NOT result EQUAL 2)
	message(FATAL_ERROR "--resave with one argument exited with ${result}, expected 2: ${errors}")
endif()

file(REMOVE_RECURSE "${OUTPUT}" "${sandbox}")
file(COPY "${INPUT}/" DESTINATION "${sandbox}")

run_resave(result errors "${sandbox}" "${OUTPUT}")
if(result EQUAL 4)
	message("RESAVE_SMOKE_SKIPPED: ${errors}")
	return()
endif()
if(NOT result EQUAL 0)
	message(FATAL_ERROR "HiveWE --resave exited with ${result}: ${errors}")
endif()
foreach(name war3map.w3i war3map.w3e war3map.wpm war3map.doo war3mapUnits.doo war3map.wtg war3map.j)
	if(NOT EXISTS "${OUTPUT}/${name}")
		message(FATAL_ERROR "HiveWE --resave did not write ${name}")
	endif()
endforeach()
file(SIZE "${OUTPUT}/war3map.w3i" w3i_size)
if(w3i_size EQUAL 0)
	message(FATAL_ERROR "war3map.w3i is empty")
endif()

# The output folder is now non-empty: refused.
run_resave(result errors "${sandbox}" "${OUTPUT}")
if(NOT result EQUAL 1)
	message(FATAL_ERROR "--resave into a non-empty folder exited with ${result}, expected 1: ${errors}")
endif()

# An output folder inside the input: refused before anything is created.
run_resave(result errors "${sandbox}" "${sandbox}/nested")
if(NOT result EQUAL 1 OR EXISTS "${sandbox}/nested")
	message(FATAL_ERROR "--resave into a folder inside the input exited with ${result}, expected 1: ${errors}")
endif()

# The output folder equal to the input: refused.
run_resave(result errors "${sandbox}" "${sandbox}")
if(NOT result EQUAL 1)
	message(FATAL_ERROR "--resave into the input folder exited with ${result}, expected 1: ${errors}")
endif()

# A doodad whose skin differs from its id keeps the skin through load, save and the script (the post-load Doodad::init used to
# reset it). Fixture: FTtw skinned as LTlt (skin at byte 52, with an item set so the script creates it), then LTlt skinned as
# LTlt (skin at byte 118). Both are fixed-Z so the records do not depend on terrain height.
file(COPY_FILE "${SKINNED_DOO}" "${sandbox}/war3map.doo")
set(skin_output "${OUTPUT}_skin")
file(REMOVE_RECURSE "${skin_output}")
run_resave(result errors "${sandbox}" "${skin_output}")
if(NOT result EQUAL 0)
	message(FATAL_ERROR "--resave with a skinned doodad exited with ${result}: ${errors}")
endif()
file(READ "${SKINNED_DOO}" doo_in HEX)
file(READ "${skin_output}/war3map.doo" doo_out HEX)
string(SUBSTRING "${doo_out}" 104 8 changed_skin)
string(SUBSTRING "${doo_out}" 236 8 same_skin)
if(NOT changed_skin STREQUAL "4c546c74" OR NOT same_skin STREQUAL "4c546c74")
	message(FATAL_ERROR "war3map.doo skins after resave: ${changed_skin} and ${same_skin}, expected 4c546c74 (LTlt) for both")
endif()
if(NOT doo_out STREQUAL doo_in)
	message(FATAL_ERROR "war3map.doo with skinned doodads changed on resave:\n${doo_in}\n${doo_out}")
endif()
file(READ "${skin_output}/war3map.j" skin_script)
string(FIND "${skin_script}" "BlzCreateDestructableZWithSkin('FTtw'" create_call)
string(FIND "${skin_script}" "'LTlt')" skin_argument)
if(create_call EQUAL -1 OR skin_argument EQUAL -1)
	message(FATAL_ERROR "war3map.j does not create FTtw with skin LTlt")
endif()
file(REMOVE_RECURSE "${skin_output}")
file(COPY_FILE "${INPUT}/war3map.doo" "${sandbox}/war3map.doo")

# An ability field the object data writer cannot write (anam with data pointer 1 loads as "namea", which has no meta data id):
# refused before anything is copied or written, so the output folder stays empty.
file(COPY_FILE "${UNWRITABLE_W3A}" "${sandbox}/war3map.w3a")
set(refused_output "${OUTPUT}_refused")
file(REMOVE_RECURSE "${refused_output}")
run_resave(result errors "${sandbox}" "${refused_output}")
if(NOT result EQUAL 1 OR NOT errors MATCHES "Save refused: war3map.w3a object AHbz field namea")
	message(FATAL_ERROR "--resave with an unwritable ability field exited with ${result}, expected 1 and a refusal: ${errors}")
endif()
file(GLOB refused_files "${refused_output}/*")
if(refused_files)
	message(FATAL_ERROR "--resave refused the save but wrote into the output folder: ${refused_files}")
endif()
file(REMOVE_RECURSE "${refused_output}")

file(REMOVE_RECURSE "${sandbox}")
message("resave_smoke: saved ${OUTPUT}, refusals checked")
