cmake_minimum_required(VERSION 4.0)

project(ktest2)

option(KTEST2_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})

add_library(ktest2 INTERFACE include/ktest2/ktest2.hpp)
target_compile_features(ktest2 INTERFACE cxx_std_23)
target_include_directories(ktest2 INTERFACE include)

add_library(ktest2_with_main STATIC src/main.cpp)
target_link_libraries(ktest2_with_main PUBLIC ktest2)

if(KTEST2_BUILD_TESTS)
	add_subdirectory(unit)
endif()