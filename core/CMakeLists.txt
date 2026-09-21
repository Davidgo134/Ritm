cmake_minimum_required(VERSION 3.10)
project(ritm_core CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(ZLIB REQUIRED)

add_library(ritmcore STATIC
  src/project.cpp
  src/io.cpp
  src/flp.cpp
  src/als.cpp
  src/engine.cpp)
target_include_directories(ritmcore PUBLIC include)
target_link_libraries(ritmcore PUBLIC ZLIB::ZLIB)
if(NOT MSVC)
  target_compile_options(ritmcore PRIVATE -Wall -Wextra)
endif()

option(RITM_BUILD_TOOLS "build tests and cli" ON)
if(RITM_BUILD_TOOLS)
  add_executable(ritm-info ../tools/ritm_info.cpp)
  target_link_libraries(ritm-info ritmcore)

  enable_testing()
  add_executable(ritm-tests tests/tests.cpp)
  target_link_libraries(ritm-tests ritmcore)
  add_test(NAME ritm-tests COMMAND ritm-tests)
endif()
