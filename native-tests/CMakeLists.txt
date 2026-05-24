cmake_minimum_required(VERSION 3.18)
project(vocalremover_native_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Headers under test live in the :audio module's cpp source set. They are
# deliberately Android-independent so they build and run on a plain host.
set(ENGINE_CPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../audio/src/main/cpp)

set(TEST_SOURCES
    test_main.cpp
    RingBufferTest.cpp
    AudioFormatTest.cpp)

add_compile_options(-Wall -Wextra)

# --- Standard build -----------------------------------------------------------
add_executable(unit_tests ${TEST_SOURCES})
target_include_directories(unit_tests PRIVATE ${ENGINE_CPP_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(unit_tests PRIVATE Threads::Threads)

enable_testing()
add_test(NAME unit_tests COMMAND unit_tests)

# --- ThreadSanitizer build ----------------------------------------------------
# The ring buffer is lock-free, so a data race would be silent in a normal
# build. TSan instrumentation turns any race into a hard failure.
option(ENABLE_TSAN "Build a ThreadSanitizer variant of the tests" ON)
if(ENABLE_TSAN)
    add_executable(unit_tests_tsan ${TEST_SOURCES})
    target_include_directories(unit_tests_tsan PRIVATE ${ENGINE_CPP_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(unit_tests_tsan PRIVATE -fsanitize=thread -g -O1)
    target_link_options(unit_tests_tsan PRIVATE -fsanitize=thread)
    target_link_libraries(unit_tests_tsan PRIVATE Threads::Threads)
    add_test(NAME unit_tests_tsan COMMAND unit_tests_tsan)
endif()
