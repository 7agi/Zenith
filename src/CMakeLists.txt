set(SOURCES
    main.cpp
    config/Config.cpp
    hotkey/HotkeyManager.cpp
    recorder/Recorder.cpp
    recorder/CircularBuffer.cpp
    recorder/ClipSaver.cpp
    overlay/OverlayManager.cpp
    overlay/CameraOverlay.cpp
    overlay/KeyOverlay.cpp
    ui/TrayIcon.cpp
    ui/SettingsWindow.cpp
    ui/StatusBar.cpp
)

add_executable(Zenith WIN32 ${SOURCES})

# Need to tell MSVC to compile with Unicode and standard Windows headers
target_compile_definitions(Zenith PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)

target_link_libraries(Zenith PRIVATE
    nlohmann_json::nlohmann_json
    obs
    d2d1
    dwrite
    shell32
    user32
    gdi32
    comctl32
)
