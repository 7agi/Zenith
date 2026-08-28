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
target_compile_definitions(Zenith PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

# Find OBS dependencies (like FFmpeg) and include them
file(GLOB OBS_DEPS_DIRS "${OBS_DIR}/.deps/obs-deps-*-x64")
if(OBS_DEPS_DIRS)
    list(GET OBS_DEPS_DIRS 0 OBS_DEPS_DIR)
    target_include_directories(Zenith PRIVATE "${OBS_DEPS_DIR}/include")
    target_link_directories(Zenith PRIVATE "${OBS_DEPS_DIR}/bin")
    
    # ClipSaver uses FFmpeg directly
    target_link_libraries(Zenith PRIVATE avformat avcodec avutil)
endif()

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
