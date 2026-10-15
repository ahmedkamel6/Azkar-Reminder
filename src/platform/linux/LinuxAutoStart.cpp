#include "LinuxAutoStart.h"
#include "../../core/Logger.h"
#include "../../core/PlatformPaths.h"
#include <fstream>
#include <cstdlib>
#include <sstream>
#include "../../core/AtomicFile.h"

#ifndef _WIN32
#ifndef __APPLE__

namespace dhikr {

std::unique_ptr<IAutoStart> IAutoStart::Create() {
    return std::make_unique<LinuxAutoStart>();
}

std::filesystem::path LinuxAutoStart::GetAutostartDesktopFilePath() {
    return PlatformPaths::GetConfigDir().parent_path() / "autostart" / "azkar-reminder.desktop";
}

bool LinuxAutoStart::IsAutoStartEnabled() const {
    auto path = GetAutostartDesktopFilePath();
    std::error_code error;
    if (!std::filesystem::exists(path, error) || error) return false;

    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        if (line.find("Hidden=true") != std::string::npos ||
            line.find("X-GNOME-Autostart-enabled=false") != std::string::npos) {
            return false;
        }
    }
    return true;
}

bool LinuxAutoStart::SetAutoStartEnabled(bool enable, const std::filesystem::path& appPath) {
    auto desktopPath = GetAutostartDesktopFilePath();
    std::error_code ec;

    if (!enable) {
        if (std::filesystem::exists(desktopPath)) {
            std::filesystem::remove(desktopPath, ec);
            if (ec) {
                LOG_ERROR("Failed to remove autostart desktop file: " + ec.message());
                return false;
            }
        }
        LOG_INFO("Linux autostart entry removed: " + desktopPath.string());
        return true;
    }

    // Determine binary execution path
    std::filesystem::path execPath = appPath;
    if (execPath.empty()) {
        const char* appImage = std::getenv("APPIMAGE");
        if (appImage && *appImage && std::filesystem::path(appImage).is_absolute()) execPath = appImage;
    }
    if (execPath.empty()) {
        try {
            execPath = std::filesystem::canonical("/proc/self/exe");
        } catch (...) {
            LOG_ERROR("Could not resolve autostart executable");
            return false;
        }
    }

    auto autostartDir = desktopPath.parent_path();
    std::filesystem::create_directories(autostartDir, ec);
    if (ec) {
        LOG_ERROR("Failed to create autostart directory: " + ec.message());
        return false;
    }

    std::string quotedPath;
    for (char character : execPath.string()) {
        if (character == '\n' || character == '\r' || character == '=') return false;
        if (character == '%') quotedPath += "%%";
        else if (character == '\\') quotedPath += "\\\\\\\\";
        else if (character == '"') quotedPath += "\\\\\\\"";
        else if (character == '$' || character == '`') { quotedPath += "\\\\"; quotedPath += character; }
        else quotedPath += character;
    }

    std::ostringstream out;

    out << "[Desktop Entry]\n";
    out << "Type=Application\n";
    out << "Version=1.0\n";
    out << "Name=Azkar Reminder\n";
    out << "GenericName=Islamic Dhikr & Dua Reminder\n";
    out << "Comment=Scheduled desktop dhikr reminders\n";
    out << "Exec=\"" << quotedPath << "\" --background\n";
    out << "Icon=azkar-reminder\n";
    out << "Terminal=false\n";
    out << "Categories=Utility;Clock;Qt;\n";
    out << "StartupNotify=false\n";
    out << "X-GNOME-Autostart-enabled=true\n";
    if (!WriteFileAtomically(desktopPath, out.str())) {
        LOG_ERROR("Failed writing desktop file content");
        return false;
    }

    LOG_INFO("Linux autostart desktop entry created: " + desktopPath.string());
    return true;
}

} // namespace dhikr

#endif // !__APPLE__
#endif // !_WIN32
