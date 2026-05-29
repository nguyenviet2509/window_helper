add_executable(postmessage_probe probe.cpp)
target_link_libraries(postmessage_probe PRIVATE user32)
set_target_properties(postmessage_probe PROPERTIES
    OUTPUT_NAME "PostMessageProbe"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
