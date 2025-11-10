set(PARSEEDM_CASES
    830_6cyl
    930_6cyl
    930_6cyl_turbo
    960_4cyl_twin
)

foreach(case ${PARSEEDM_CASES})
    add_test(NAME ${case}
        COMMAND ${CMAKE_COMMAND}
            -DROOTNAME=${case}
            -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
            -DPARSEEDMLOG_EXECUTABLE=$<TARGET_FILE:parseedmlog>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_parse_and_compare.cmake)
endforeach()


add_test(tool_build
    "${CMAKE_COMMAND}"
    --build "${CMAKE_BINARY_DIR}"
    --target parseedmlog
)

set_tests_properties(tool_build
    PROPERTIES FIXTURES_SETUP tool_fixture)

foreach(case ${PARSEEDM_CASES})
    set_tests_properties(${case}
        PROPERTIES FIXTURES_REQUIRED tool_fixture)
endforeach()
