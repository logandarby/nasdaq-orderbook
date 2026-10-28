cmake_minimum_required(VERSION 3.25)
project(orderbook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ORDERBOOK_BUILD_TESTS "Build unit and differential tests" ON)
option(ORDERBOOK_BUILD_BENCH "Build benchmarks" ON)
option(ORDERBOOK_BUILD_FUZZ "Build libFuzzer targets (Clang only)" OFF)
option(ORDERBOOK_WERROR "Treat warnings as errors" OFF)
set(ORDERBOOK_SANITIZERS "" CACHE STRING "Value for -fsanitize=, e.g. address,undefined")

# Sanitizers apply to dependencies too, so instrumentation is consistent.
if(ORDERBOOK_SANITIZERS)
  add_compile_options(-fsanitize=${ORDERBOOK_SANITIZERS} -fno-omit-frame-pointer)
  add_link_options(-fsanitize=${ORDERBOOK_SANITIZERS})
endif()

# Warnings apply to our targets only, not to fetched dependencies.
add_library(orderbook_warnings INTERFACE)
target_compile_options(orderbook_warnings INTERFACE
  -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
  $<$<BOOL:${ORDERBOOK_WERROR}>:-Werror>)

# Parser, books and queues. Header-only for now: switch to a STATIC library
# once src/ has .cpp files.
add_library(orderbook_core INTERFACE)
target_include_directories(orderbook_core INTERFACE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(orderbook_core INTERFACE orderbook_warnings)

add_executable(replay apps/replay.cpp)
target_link_libraries(replay PRIVATE orderbook_core)

include(FetchContent)
set(FETCHCONTENT_QUIET ON)

if(ORDERBOOK_BUILD_TESTS)
  enable_testing()
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(googletest)

  add_executable(unit_tests tests/smoke_test.cpp tests/itch_parser_test.cpp)
  target_link_libraries(unit_tests PRIVATE orderbook_core GTest::gtest_main)
  target_compile_definitions(unit_tests PRIVATE
    ORDERBOOK_TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data")
  include(GoogleTest)
  gtest_discover_tests(unit_tests)
endif()

if(ORDERBOOK_BUILD_BENCH)
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(benchmark
    URL https://github.com/google/benchmark/archive/refs/tags/v1.9.1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(benchmark)

  add_executable(book_bench bench/book_bench.cpp)
  target_link_libraries(book_bench PRIVATE orderbook_core benchmark::benchmark)
endif()

if(ORDERBOOK_BUILD_FUZZ)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "ORDERBOOK_BUILD_FUZZ needs Clang (libFuzzer)")
  endif()
  add_executable(itch_fuzz fuzz/itch_fuzz.cpp)
  target_link_libraries(itch_fuzz PRIVATE orderbook_core)
  # Keep EXPECT checks on although RelWithDebInfo defines NDEBUG.
  target_compile_definitions(itch_fuzz PRIVATE ORDERBOOK_ENABLE_EXPECT=1)
  target_compile_options(itch_fuzz PRIVATE -fsanitize=fuzzer)
  target_link_options(itch_fuzz PRIVATE -fsanitize=fuzzer)
endif()
