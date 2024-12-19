add_test(NAME 930_6cyl-run
    COMMAND parseedmlog -o 930_6cyl.out
    ${CMAKE_CURRENT_SOURCE_DIR}/930_6cyl.jpi)


add_test(930_6cyl-cmp
    ${CMAKE_COMMAND} -E compare_files
    930_6cyl.out
    ${CMAKE_CURRENT_SOURCE_DIR}/930_6cyl.expected)


add_test(tool_build
    "${CMAKE_COMMAND}"
    --build "${CMAKE_BINARY_DIR}"
    --target parseedmlog
)

set_tests_properties(tool_build
    PROPERTIES FIXTURES_SETUP tool_fixture)
set_tests_properties(930_6cyl-run
    PROPERTIES FIXTURES_REQUIRED tool_fixture)

set_tests_properties(930_6cyl-run
    PROPERTIES FIXTURES_SETUP 930_6cyl_fixture)
set_tests_properties(930_6cyl-cmp
    PROPERTIES FIXTURES_REQUIRED 930_6cyl_fixture)
