cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(parseedmlog C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)


add_executable(parseedmlog
    main.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

if(WIN32)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	target_sources(parseedmlog PRIVATE
    		getopt.cpp
	)
	target_link_libraries(${PROJECT_NAME}
		PUBLIC Ws2_32.lib PRIVATE jpiedm
	)
else()
	target_link_libraries(${PROJECT_NAME}
		PRIVATE jpiedm
	)
endif()
