set(HELPER_SOURCES
  src/main.cpp
  src/serial_transport.cpp
  src/audio_pipeline.cpp
)

if(APPLE)
  enable_language(OBJCXX)
  list(APPEND HELPER_SOURCES src/macos/core_audio_tap.mm)
elseif(WIN32)
  list(APPEND HELPER_SOURCES src/windows/wasapi_capture.cpp)
else()
  list(APPEND HELPER_SOURCES src/stub_capture.cpp)
endif()

add_executable(loudness-bridge ${HELPER_SOURCES})
target_link_libraries(loudness-bridge PRIVATE lmv1_core)
target_include_directories(loudness-bridge PRIVATE include)
target_compile_options(loudness-bridge PRIVATE -Wall -Wextra -Wpedantic)

if(APPLE)
  target_compile_definitions(loudness-bridge PRIVATE LMV1_MACOS=1)
  target_link_libraries(loudness-bridge PRIVATE
    "-framework CoreAudio" "-framework Foundation")
  set_source_files_properties(src/macos/core_audio_tap.mm
    PROPERTIES COMPILE_FLAGS "-fblocks -Wno-deprecated-declarations")
elseif(WIN32)
  target_compile_definitions(loudness-bridge PRIVATE LMV1_WINDOWS=1)
  target_link_libraries(loudness-bridge PRIVATE ole32 uuid)
endif()

add_test(NAME helper_self_test COMMAND loudness-bridge --self-test)

