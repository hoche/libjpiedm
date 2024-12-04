add_test(NAME golden-1-run
        COMMAND parseedmlog -o golden-1-out
        ${CMAKE_CURRENT_SOURCE_DIR}/930_six_cylinder_multiple_flights.jpi)


add_test(golden-1-cmp
        ${CMAKE_COMMAND} -E compare_files
        golden-1-out
        ${CMAKE_CURRENT_SOURCE_DIR}/930_six_cylinder_multiple_flights.expected)


add_test(tool_build
        "${CMAKE_COMMAND}"
        --build "${CMAKE_BINARY_DIR}"
        --config $<CONFIG>
        --target parseedmlog
        )

set_tests_properties(tool_build
        PROPERTIES FIXTURES_SETUP tool_fixture)
set_tests_properties(golden-1-run
        PROPERTIES FIXTURES_REQUIRED tool_fixture)
set_tests_properties(golden-1-run
        PROPERTIES FIXTURES_SETUP golden-1_fixture)
set_tests_properties(golden-1-cmp
        PROPERTIES FIXTURES_REQUIRED golden-1_fixture)
