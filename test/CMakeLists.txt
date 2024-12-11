add_test(NAME NewSixCyl-run
    COMMAND parseedmlog -o NewSixCylSample1.out
    ${CMAKE_CURRENT_SOURCE_DIR}/930_six_cylinder_multiple_flights.jpi)


add_test(NewSixCyl-cmp
    ${CMAKE_COMMAND} -E compare_files
    NewSixCylSample1.out
    ${CMAKE_CURRENT_SOURCE_DIR}/930_six_cylinder_multiple_flights.expected)


add_test(tool_build
    "${CMAKE_COMMAND}"
    --build "${CMAKE_BINARY_DIR}"
    --target parseedmlog
)

set_tests_properties(tool_build
    PROPERTIES FIXTURES_SETUP tool_fixture)
set_tests_properties(NewSixCyl-run
    PROPERTIES FIXTURES_REQUIRED tool_fixture)

set_tests_properties(NewSixCyl-run
    PROPERTIES FIXTURES_SETUP NewSixCyl_fixture)
set_tests_properties(NewSixCyl-cmp
    PROPERTIES FIXTURES_REQUIRED NewSixCyl_fixture)
