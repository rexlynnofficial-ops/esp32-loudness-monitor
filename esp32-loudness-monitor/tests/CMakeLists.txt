add_executable(test_protocol test_protocol.cpp)
target_link_libraries(test_protocol PRIVATE lmv1_core)
add_test(NAME protocol COMMAND test_protocol)

add_executable(test_dsp test_dsp.cpp)
target_link_libraries(test_dsp PRIVATE lmv1_core)
add_test(NAME dsp COMMAND test_dsp)

add_executable(test_ui_policy test_ui_policy.cpp)
target_include_directories(test_ui_policy PRIVATE
  ${CMAKE_SOURCE_DIR}/firmware/LoudnessMonitor/src)
add_test(NAME ui_render_policy COMMAND test_ui_policy)
