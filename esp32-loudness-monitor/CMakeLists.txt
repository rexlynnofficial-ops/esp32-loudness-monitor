cmake_minimum_required(VERSION 3.20)
project(loudness_monitor_v1 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LMV1_BUILD_HELPER "Build the desktop Helper" ON)
option(LMV1_BUILD_TESTS "Build host tests" ON)

if(LMV1_BUILD_TESTS)
  enable_testing()
endif()

add_library(lmv1_core STATIC
  firmware/LoudnessMonitor/src/protocol/wire_protocol.cpp
  firmware/LoudnessMonitor/src/dsp/loudness_meter.cpp
  firmware/LoudnessMonitor/src/dsp/third_octave.cpp
)
target_include_directories(lmv1_core PUBLIC firmware/LoudnessMonitor/src)
target_compile_options(lmv1_core PRIVATE -Wall -Wextra -Wpedantic)

if(LMV1_BUILD_HELPER)
  add_subdirectory(helper)
endif()

if(LMV1_BUILD_TESTS)
  add_subdirectory(tests)
endif()
