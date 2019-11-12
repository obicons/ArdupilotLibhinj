cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

find_package(gazebo REQUIRED)

include_directories(${GAZEBO_INCLUDE_DIRS})
link_directories(${GAZEBO_LIBRARY_DIRS})
list(APPEND CMAKE_CXX_FLAGS "${GAZEBO_CXX_FLAGS}")

add_library(time SHARED time_simulator.cpp)
add_executable(time_reader time_reader.cpp)
target_link_libraries(time ${GAZEBO_LIBRARIES} pthread)
target_link_libraries(time_reader ${GAZEBO_LIBRARIES} pthread)
