cmake_minimum_required(VERSION 3.15)

foreach(var ROOTNAME SOURCE_DIR BINARY_DIR PARSEEDMLOG_EXECUTABLE)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "${var} variable is required")
    endif()
endforeach()

set(INPUT "${SOURCE_DIR}/${ROOTNAME}.jpi")
set(EXPECTED "${SOURCE_DIR}/${ROOTNAME}.expected")
set(OUTPUT "${BINARY_DIR}/${ROOTNAME}.out")

if(NOT EXISTS "${INPUT}")
    message(FATAL_ERROR "Input flight data file not found: ${INPUT}")
endif()

if(NOT EXISTS "${EXPECTED}")
    message(FATAL_ERROR "Expected output file not found: ${EXPECTED}")
endif()

file(MAKE_DIRECTORY "${BINARY_DIR}")

execute_process(
    COMMAND "${PARSEEDMLOG_EXECUTABLE}" -v -o "${OUTPUT}" "${INPUT}"
    WORKING_DIRECTORY "${BINARY_DIR}"
    RESULT_VARIABLE parse_result
)

if(NOT parse_result EQUAL 0)
    message(FATAL_ERROR "parseedmlog failed for ${ROOTNAME} with exit code ${parse_result}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${OUTPUT}" "${EXPECTED}"
    RESULT_VARIABLE cmp_result
)

if(NOT cmp_result EQUAL 0)
    message(FATAL_ERROR "Output file ${OUTPUT} differs from expected ${EXPECTED}")
endif()

