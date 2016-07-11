cmake_minimum_required(VERSION 2.6)

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(category-tree-test category-tree-test.cpp)
set_source_files_properties(category-tree-test.cpp
  PROPERTIES
  COMPILE_FLAGS  "-std=c++11")

target_link_libraries(category-tree-test ${GTEST_LIBRARIES} pthread)
