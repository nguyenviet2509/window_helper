find_package(OpenCV CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)

add_executable(WindowHelper WIN32
    main.cpp
    capture/wgc-capture.cpp
    vision/vision-pipeline.cpp
    vision/bar-detector.cpp
    vision/waterline.cpp
    vision/hue-sampler.cpp
    input/postmessage-backend.cpp
    input/send-input-backend.cpp
    input/input-scheduler.cpp
    input/mouse-path.cpp
    core/humanizer.cpp
    core/output-gate.cpp
    core/capture-health-fsm.cpp
    core/logger.cpp
    combat/pot-evaluator.cpp
    combat/combat-activity-monitor.cpp
    combat/combat-fsm.cpp
    combat/pot-refill-scheduler.cpp
    dispatch/action-dispatcher.cpp
    config/config-loader.cpp
    ui/main-window.cpp
    ui/calibration-window.cpp
    ui/tray-icon.cpp
    ui/activation-dialog.cpp
    ui/license-info-dialog.cpp
    license/hwid-collector.cpp
    license/license-manager.cpp
    license/ed25519-verify.cpp
    license/license-client.cpp
    license/license-cache.cpp
    resources/version.rc
)

target_include_directories(WindowHelper PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(WindowHelper PRIVATE
    opencv_core opencv_imgproc opencv_imgcodecs
    nlohmann_json::nlohmann_json
    imgui::imgui
    user32 gdi32 d3d11 dxgi dxguid windowsapp runtimeobject wtsapi32 shell32 comctl32
    iphlpapi bcrypt winhttp
    unofficial-sodium::sodium
)
# unofficial-sodium::sodium is the vcpkg target name for libsodium

target_compile_definitions(WindowHelper PRIVATE _WIN32_WINNT=0x0A00)

# OUTPUT_NAME random theo timestamp build để né Xingcode3 file-name signature.
# User có thể override bằng -DWH_OUTPUT_NAME=foo.
if(NOT DEFINED WH_OUTPUT_NAME)
    string(TIMESTAMP _wh_ts "%s")
    string(RANDOM LENGTH 5 ALPHABET "abcdefghijklmnopqrstuvwxyz0123456789"
           RANDOM_SEED ${_wh_ts} _wh_suffix)
    set(WH_OUTPUT_NAME "svc_${_wh_suffix}")
endif()
file(WRITE "${CMAKE_BINARY_DIR}/.last-output-name.txt" "${WH_OUTPUT_NAME}")
message(STATUS "WindowHelper OUTPUT_NAME = ${WH_OUTPUT_NAME}")

set_target_properties(WindowHelper PROPERTIES
    OUTPUT_NAME "${WH_OUTPUT_NAME}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
