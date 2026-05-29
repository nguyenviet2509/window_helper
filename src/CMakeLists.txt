find_package(OpenCV CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

add_executable(WindowHelper WIN32
    main.cpp
    capture/wgc-capture.cpp
    vision/vision-pipeline.cpp
    vision/bar-detector.cpp
    vision/waterline.cpp
    input/postmessage-backend.cpp
    input/send-input-backend.cpp
    input/input-scheduler.cpp
    core/humanizer.cpp
    core/output-gate.cpp
    core/capture-health-fsm.cpp
    core/logger.cpp
    combat/pot-evaluator.cpp
    combat/combat-activity-monitor.cpp
    combat/combat-fsm.cpp
    dispatch/action-dispatcher.cpp
    config/config-loader.cpp
    ui/main-window.cpp
    ui/tray-icon.cpp
    resources/version.rc
)

target_include_directories(WindowHelper PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(WindowHelper PRIVATE
    opencv_core opencv_imgproc opencv_imgcodecs
    nlohmann_json::nlohmann_json
    imgui::imgui
    user32 gdi32 d3d11 dxgi dxguid windowsapp runtimeobject wtsapi32 shell32 comctl32
)

target_compile_definitions(WindowHelper PRIVATE _WIN32_WINNT=0x0A00)

set_target_properties(WindowHelper PROPERTIES
    OUTPUT_NAME "WindowHelper"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
