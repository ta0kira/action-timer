cmake_minimum_required(VERSION 2.6)

include_directories(
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/common
  ${CMAKE_SOURCE_DIR}/locking-container/include)

set(CMAKE_CXX_FLAGS "-Wall -pedantic -std=c++11 -O2 -g")

add_executable(
  poisson-printer
  example/poisson-printer.cpp
  src/action.cpp
  src/timer.cpp
  common/helpers.cpp
  common/locking-container.cpp)
target_link_libraries(poisson-printer pthread)

add_executable(
  sensor-recorder
  example/sensor-recorder.cpp
  src/action.cpp
  src/timer.cpp
  common/helpers.cpp
  common/locking-container.cpp)
target_link_libraries(sensor-recorder pthread)

add_executable(
  timer-test-data
  test/timer-test-data.cpp
  src/action.cpp
  src/timer.cpp
  common/locking-container.cpp)
target_link_libraries(timer-test-data pthread)

add_executable(
  poisson-queue-test
  test/poisson-queue-test.cpp
  src/action.cpp
  src/timer.cpp
  common/helpers.cpp
  common/locking-container.cpp)
target_link_libraries(poisson-queue-test pthread)


find_package(GTest)
if(GTEST_LIBRARIES)

  include_directories(${GTEST_INCLUDE_DIRS})

  add_executable(
    category-tree-test
    test/category-tree-test.cpp)
  target_link_libraries(category-tree-test ${GTEST_LIBRARIES} pthread)

endif()



find_package(OpenCV)
if (OpenCV_LIBS)

  add_executable(
    motion-capture
    example/motion-capture.cpp
    src/action.cpp
    src/timer.cpp
    common/locking-container.cpp)
  target_link_libraries(motion-capture pthread ${OpenCV_LIBS})

endif()
