cmake_minimum_required(VERSION 2.6)

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

SET(CMAKE_CXX_FLAGS "-Wall -pedantic -std=c++11 -O2 -g -I${CMAKE_SOURCE_DIR}/../include")

add_executable(
  category-tree-test
  category-tree-test.cpp)
target_link_libraries(category-tree-test ${GTEST_LIBRARIES} pthread)

add_executable(
  exponential-printer
  exponential-printer.cpp
  action-timer.cpp)
target_link_libraries(exponential-printer pthread)
