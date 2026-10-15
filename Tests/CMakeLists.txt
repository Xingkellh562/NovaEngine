cmake_minimum_required(VERSION 3.15)

set (GTEST_ROOT ${CMAKE_SOURCE_DIR}/Linking)

add_library(GTestLocal STATIC
	${GTEST_ROOT}/src/googletext/gtest-all.cc
	#${GTEST_ROOT}/src/googletext/gtest_main.cc
)

target_include_directories(GTestLocal PUBLIC 
	${GTEST_ROOT}/include/googletext
	${GTEST_ROOT}/src/googletext/src
)

set_target_properties(GTestLocal PROPERTIES
	VS_GLOBAL_DisablePrecompileHeaders true
)

if(MSVC)
	target_compile_options(GTestLocal PRIVATE /wd4530 /wd4577 /wd4800)
endif()

enable_testing()

set(TEST_SOURCES
    main_test.cpp          
)

add_executable(EngineCoreTests ${TEST_SOURCES})

target_link_libraries(EngineCoreTests PRIVATE 
	NovaEngineCore
	GTestLocal
)

#include(GoogleTest)
#gtest_discover_tests(EngineCoreTests)