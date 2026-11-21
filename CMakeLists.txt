cmake_minimum_required(VERSION 3.20)
project(DhikrReminder VERSION 1.2.4 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
    src/core
    src/platform
    src
    third_party
)
target_link_libraries(azkar_core PUBLIC
    sqlite3
)
if(WIN32)
    target_link_libraries(azkar_core PUBLIC shell32 user32)
elseif(APPLE)
    target_link_libraries(azkar_core PUBLIC
        "-framework CoreGraphics"
        "-framework CoreFoundation"
        "-framework AudioToolbox"
        "-framework CoreAudio"
    )
elseif(UNIX)
    target_link_libraries(azkar_core PUBLIC ${CMAKE_DL_LIBS} pthread)
endif()

# ─── Unit Tests: azkar_core_tests ─────────────────────────────────────────
enable_testing()

add_executable(azkar_core_tests
    tests/core/test_main.cpp
)
target_include_directories(azkar_core_tests PRIVATE
    src
    third_party
)
target_link_libraries(azkar_core_tests PRIVATE
    azkar_core
)
add_test(NAME CoreTests COMMAND azkar_core_tests)

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
    src/ui/ScreenshotExporter.cpp
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
    DhikrReminder.exe.manifest
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
option(BUILD_QT_APP "Build Qt 6 cross-platform frontend" OFF)
if(UNIX OR APPLE)
    set(BUILD_QT_APP ON)
endif()

if(BUILD_QT_APP)
    find_package(Qt6 COMPONENTS Core Gui Widgets QUIET)
    if(Qt6_FOUND)
        set(CMAKE_AUTOMOC ON)
        set(CMAKE_AUTORCC ON)
        set(CMAKE_AUTOUIC ON)

        set(QT_UI_SOURCES
            src/ui/qt/ThemeManager.cpp
            src/ui/qt/PopupWindow.cpp
            src/ui/qt/MainWindow.cpp
            src/ui/qt/TrayController.cpp
        )

        add_executable(azkar_reminder
            src/main_qt.cpp
            ${QT_UI_SOURCES}
        )

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
        )

        if(UNIX AND NOT APPLE)
            target_link_libraries(azkar_reminder PRIVATE dl pthread)
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
                MACOSX_BUNDLE_GUI_IDENTIFIER "com.azkar.reminder"
                MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
                MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
                MACOSX_BUNDLE_ICON_FILE "AzkarReminder.icns"
                MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/assets/Info.plist.in"
            )
        endif()
        message(STATUS "Qt 6 frontend target 'azkar_reminder' enabled")
    else()
        message(STATUS "Qt 6 not found in build environment. Qt UI target azkar_reminder skipped.")
    endif()
endif()

