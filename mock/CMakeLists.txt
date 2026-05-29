add_executable(PtMockGame WIN32
    src/main.cpp
)
target_link_libraries(PtMockGame PRIVATE user32 gdi32 comctl32)
target_compile_definitions(PtMockGame PRIVATE _WIN32_WINNT=0x0A00)
set_target_properties(PtMockGame PROPERTIES
    OUTPUT_NAME "PtMockGame"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
