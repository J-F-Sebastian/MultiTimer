cmake_minimum_required(VERSION 3.10)

project(MultiTimer LANGUAGES CXX)

add_library(MultiTimer
    MultiTimer.cpp
    MultiTimer.h
)

target_compile_features(MultiTimer PUBLIC cxx_std_14)

if (MSVC)
    target_compile_options(MultiTimer PRIVATE /Wall)
else()
    target_compile_options(MultiTimer PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_include_directories(MultiTimer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(MultiTimerTest
    main.cpp
)

enable_testing()
add_test(NAME MultiTimerTest COMMAND MultiTimerTest)

target_link_libraries(MultiTimerTest PRIVATE MultiTimer)
target_compile_features(MultiTimerTest PRIVATE cxx_std_14)

if (MSVC)
    target_compile_options(MultiTimerTest PRIVATE /Wall)
else()
    target_compile_options(MultiTimerTest PRIVATE -Wall -Wextra -Wpedantic)
endif()
