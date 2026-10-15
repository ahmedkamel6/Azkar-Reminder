cmake_minimum_required(VERSION 3.20)
project(DhikrReminder VERSION 1.2.5 LANGUAGES CXX C)

if(UNIX)
    set(QT_APP_DEFAULT ON)
else()
    set(QT_APP_DEFAULT OFF)
endif()
option(BUILD_QT_APP "Build Qt 6 cross-platform frontend" ${QT_APP_DEFAULT})

if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CTest)
file(READ "${CMAKE_SOURCE_DIR}/assets/content.md" AZKAR_CONTENT_MD)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/assets/content.md")
configure_file(src/core/EmbeddedContent.h.in "${CMAKE_BINARY_DIR}/generated/azkar/EmbeddedContent.h" @ONLY)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Windows subsystem - GUI app, no console window
if(MSVC)
    add_compile_options(
        /W3 /MP /utf-8 /permissive-
        /GS          # Buffer Security Check
        /guard:cf    # Control Flow Guard
    )
    add_compile_definitions(
        _UNICODE
        UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
    )
endif()

# ─── Third-party: SQLite3 ──────────────────────────────────────────────────
add_library(sqlite3 STATIC
    third_party/sqlite3.c
)
target_include_directories(sqlite3 PUBLIC third_party)
if(MSVC)
    target_compile_options(sqlite3 PRIVATE /W0)
endif()

# ─── Library: azkar_core (C++20 Cross-Platform) ───────────────────────────
set(AZKAR_CORE_SOURCES
    src/core/PlatformPaths.cpp
    src/core/Logger.cpp
    src/core/Settings.cpp
    src/core/Database.cpp
    src/core/ContentEngine.cpp
    src/core/Scheduler.cpp
    src/core/Localization.cpp
)

# ─── Platform Abstraction Sources ──────────────────────────────────────────
if(WIN32)
    set(PLATFORM_SOURCES
        src/platform/windows/WindowsAutoStart.cpp
        src/platform/windows/WindowsFullScreenDetector.cpp
    )
    if(BUILD_QT_APP)
        list(APPEND PLATFORM_SOURCES src/platform/audio/MiniAudioEngine.cpp)
    endif()
elseif(APPLE)
    set(PLATFORM_SOURCES
        src/platform/macos/MacAutoStart.cpp
        src/platform/macos/MacFullScreenDetector.cpp
        src/platform/audio/MiniAudioEngine.cpp
    )
elseif(UNIX)
    set(PLATFORM_SOURCES
        src/platform/linux/LinuxAutoStart.cpp
        src/platform/linux/LinuxFullScreenDetector.cpp
        src/platform/audio/MiniAudioEngine.cpp
    )
endif()

add_library(azkar_core STATIC
    ${AZKAR_CORE_SOURCES}
    ${PLATFORM_SOURCES}
)
target_include_directories(azkar_core PUBLIC
    "${CMAKE_BINARY_DIR}/generated"
    src/core
    src/platform
    src
    third_party
)
target_link_libraries(azkar_core PUBLIC
    sqlite3
)
target_compile_definitions(azkar_core PUBLIC AZKAR_APP_VERSION="${PROJECT_VERSION}")
if(WIN32)
    target_link_libraries(azkar_core PUBLIC shell32 user32 ole32 advapi32)
elseif(APPLE)
    target_link_libraries(azkar_core PUBLIC
        "-framework CoreGraphics"
        "-framework CoreFoundation"
        "-framework AudioToolbox"
        "-framework CoreAudio"
    )
elseif(UNIX)
    find_library(XCB_LIBRARY xcb REQUIRED)
    target_link_libraries(azkar_core PUBLIC ${CMAKE_DL_LIBS} pthread ${XCB_LIBRARY})
endif()

# ─── Unit Tests: azkar_core_tests ─────────────────────────────────────────
if(BUILD_TESTING)
add_executable(azkar_core_tests
    tests/core/test_main.cpp
    tests/core/test_regressions.cpp
)
target_include_directories(azkar_core_tests PRIVATE
    src
    third_party
)
target_link_libraries(azkar_core_tests PRIVATE
    azkar_core
)
add_test(NAME CoreTests COMMAND azkar_core_tests)
set_tests_properties(CoreTests PROPERTIES TIMEOUT 30)
if(UNIX AND NOT APPLE)
    add_executable(azkar_x11_tests tests/platform/test_fullscreen_x11.cpp)
    target_link_libraries(azkar_x11_tests PRIVATE azkar_core)
    add_test(NAME X11FullScreen COMMAND azkar_x11_tests)
    set_tests_properties(X11FullScreen PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 10)
endif()
if(NOT WIN32 OR BUILD_QT_APP)
    add_executable(azkar_audio_tests tests/core/test_audio.cpp)
    target_link_libraries(azkar_audio_tests PRIVATE azkar_core)
    add_test(NAME AudioRetry COMMAND azkar_audio_tests)
    set_tests_properties(AudioRetry PROPERTIES TIMEOUT 10)
endif()
endif()

# ─── Source files ─────────────────────────────────────────────────────────
set(APP_SOURCES
    src/core/Application.cpp
    src/core/FullScreenDetector.cpp
)

set(UI_SOURCES
    src/ui/MainWindow.cpp
    src/ui/PopupWindow.cpp
    src/ui/TrayIcon.cpp
    src/ui/OnboardingFlow.cpp
    src/ui/core/WindowChrome.cpp
    src/ui/core/LogoRenderer.cpp
    src/ui/core/MosqueRenderer.cpp
    src/ui/core/ResourceLoader.cpp
    src/ui/core/UIState.cpp
    src/ui/components/TimePickerPopup.cpp
    src/ui/components/DurationPickerPopup.cpp
    src/ui/components/DropdownPopup.cpp
    src/ui/pages/HomePage.cpp
    src/ui/pages/RemindersPage.cpp
    src/ui/pages/LibraryPage.cpp
    src/ui/pages/SettingsPage.cpp
    src/ui/pages/AboutPage.cpp
)

set(AUDIO_SOURCES
    src/audio/AudioEngine.cpp
)

set(UPDATE_SOURCES
    src/update/Updater.cpp
)

set(ALL_SOURCES
    src/main.cpp
    ${APP_SOURCES}
    ${UI_SOURCES}
    ${AUDIO_SOURCES}
    ${UPDATE_SOURCES}
    DhikrReminder.rc
)

# ─── Windows Native Executable (Direct2D / Win32) ─────────────────────────
if(WIN32)
add_executable(AzkarReminder WIN32 ${ALL_SOURCES})

target_include_directories(AzkarReminder PRIVATE
    src
    third_party
)

# ─── Link libraries ───────────────────────────────────────────────────────
target_link_libraries(AzkarReminder PRIVATE
    azkar_core
    d2d1
    dwrite
    shell32
    user32
    gdi32
    ole32
    oleaut32
    comctl32
    comdlg32
    shlwapi
    xaudio2
    dwmapi
    mfplat
    mfreadwrite
    mfuuid
    uxtheme
    version
)

# ─── Linker settings ──────────────────────────────────────────────────────
if(MSVC)
    set_target_properties(AzkarReminder PROPERTIES
        VS_DPI_AWARE "PerMonitor"
        LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /guard:cf"
    )
endif()

# ─── Output ───────────────────────────────────────────────────────────────
set_target_properties(AzkarReminder PROPERTIES
    OUTPUT_NAME "AzkarReminder"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ─── Copy audio to output ─────────────────────────────────────────────────
add_custom_command(TARGET AzkarReminder POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets/audio"
        "$<TARGET_FILE_DIR:AzkarReminder>/audio"
    COMMENT "Copying audio assets to output directory"
)

# ─── Install rules ────────────────────────────────────────────────────────
install(TARGETS AzkarReminder
    RUNTIME DESTINATION bin
)
install(DIRECTORY assets/audio
    DESTINATION bin
)
endif()

# ─── Qt 6 Frontend Target (Linux, macOS, and optional Windows) ─────────────
if(BUILD_QT_APP)
    find_package(Qt6 6.2 COMPONENTS Core Gui Widgets Network REQUIRED)
    if(UNIX AND NOT APPLE)
        find_package(Qt6 6.2 REQUIRED COMPONENTS DBus)
    endif()
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    set(QT_UI_SOURCES
        src/ui/qt/ThemeManager.cpp
        src/ui/qt/PopupWindow.cpp
        src/ui/qt/MainWindow.cpp
        src/ui/qt/RemindersPage.cpp
        src/ui/qt/LibraryPage.cpp
        src/ui/qt/SettingsPage.cpp
        src/ui/qt/TrayController.cpp
    )

    add_library(azkar_qt_ui OBJECT ${QT_UI_SOURCES} assets/qt.qrc)
    target_include_directories(azkar_qt_ui PUBLIC src src/platform src/ui/qt third_party)
    target_link_libraries(azkar_qt_ui PUBLIC azkar_core Qt6::Core Qt6::Gui Qt6::Widgets)
    if(UNIX AND NOT APPLE)
        target_link_libraries(azkar_qt_ui PUBLIC Qt6::DBus)
    endif()
    add_executable(azkar_reminder src/main_qt.cpp)
    target_link_libraries(azkar_reminder PRIVATE azkar_qt_ui)
    if(BUILD_TESTING)
        find_package(Qt6 REQUIRED COMPONENTS Test)
        add_executable(azkar_ui_tests tests/ui/test_main.cpp)
        target_link_libraries(azkar_ui_tests PRIVATE azkar_qt_ui Qt6::Test)
        add_test(NAME UiTests COMMAND azkar_ui_tests)
        set_tests_properties(UiTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)
    endif()

    target_include_directories(azkar_reminder PRIVATE
        src
        src/platform
        src/ui/qt
        third_party
    )

    target_link_libraries(azkar_reminder PRIVATE
        azkar_core
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
    )
    if(APPLE)
        set(QT_AUDIO_DIR "$<TARGET_BUNDLE_CONTENT_DIR:azkar_reminder>/Resources/audio")
    else()
        set(QT_AUDIO_DIR "$<TARGET_FILE_DIR:azkar_reminder>/audio")
    endif()
    add_custom_command(TARGET azkar_reminder POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets/audio" "${QT_AUDIO_DIR}")

    if(UNIX AND NOT APPLE)
        target_link_libraries(azkar_reminder PRIVATE dl pthread Qt6::DBus)
    endif()

    if(APPLE)
        target_link_libraries(azkar_reminder PRIVATE
            "-framework CoreGraphics"
            "-framework AppKit"
            "-framework AudioToolbox"
            "-framework CoreAudio"
        )
        set_target_properties(azkar_reminder PROPERTIES
            OUTPUT_NAME "Azkar Reminder"
        MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "Azkar Reminder"
        MACOSX_BUNDLE_ICON_FILE "AzkarReminder.icns"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/assets/Info.plist.in"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.azkar.reminder"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        )
    endif()
    install(TARGETS azkar_reminder RUNTIME DESTINATION bin BUNDLE DESTINATION .)
    if(UNIX AND NOT APPLE)
        install(DIRECTORY assets/audio DESTINATION share/azkar-reminder)
        install(FILES assets/content.md DESTINATION share/azkar-reminder)
    endif()
    message(STATUS "Qt 6 frontend target 'azkar_reminder' enabled")
endif()

