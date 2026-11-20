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
