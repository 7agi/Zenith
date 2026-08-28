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
    ui/RmlUiWindow.cpp
    ui/StatusBar.cpp
)

add_executable(Zenith WIN32 ${SOURCES}
    # RmlUi Win32+DX11 backend (must be compiled into our target)
    "${rmlui_SOURCE_DIR}/Backends/RmlUi_Backend_Win32_DX11.cpp"
    "${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_DX11.cpp"
    "${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_Win32.cpp"
)

target_include_directories(Zenith PRIVATE "${rmlui_SOURCE_DIR}/Backends")

# Copy UI assets next to the exe so RmlUi can find them at runtime
add_custom_command(TARGET Zenith POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets/ui"
        "$<TARGET_FILE_DIR:Zenith>/ui"
    COMMENT "Copying UI assets..."
)

# Need to tell MSVC to compile with Unicode and standard Windows headers
target_compile_definitions(Zenith PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

# Find OBS dependencies (like FFmpeg) and include them
file(GLOB OBS_DEPS_DIRS "${OBS_DIR}/.deps/obs-deps-*-x64")
if(OBS_DEPS_DIRS)
    list(GET OBS_DEPS_DIRS 0 OBS_DEPS_DIR)
    target_include_directories(Zenith PRIVATE "${OBS_DEPS_DIR}/include")
    target_link_directories(Zenith PRIVATE "${OBS_DEPS_DIR}/lib")
    
    # ClipSaver uses FFmpeg directly
    target_link_libraries(Zenith PRIVATE avformat avcodec avutil)
endif()

target_link_libraries(Zenith PRIVATE
    nlohmann_json::nlohmann_json
    RmlUi::RmlUi
    RmlUi::Debugger
    freetype
    obs
    d2d1
    d3d11
    dxgi
    dwrite
    shell32
    user32
    gdi32
    comctl32
)
