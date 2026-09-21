cmake_minimum_required(VERSION 3.18)
project(ritmjni CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(RITM_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../core ritmcore_build)

find_package(oboe REQUIRED CONFIG)

add_library(ritmjni SHARED bridge.cpp)
target_link_libraries(ritmjni ritmcore oboe::oboe log)
