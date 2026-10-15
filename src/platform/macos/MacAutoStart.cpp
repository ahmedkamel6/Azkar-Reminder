#include "MacAutoStart.h"
#include "../../core/Logger.h"
#include <fstream>
#include <cstdlib>
#include <sstream>
#include <vector>
#include "../../core/AtomicFile.h"

#ifdef __APPLE__
#include <mach-o/dyld.h>

namespace dhikr {

std::unique_ptr<IAutoStart> IAutoStart::Create() {
    return std::make_unique<MacAutoStart>();
}

std::filesystem::path MacAutoStart::GetLaunchAgentPlistPath() {
    const char* home = std::getenv("HOME");
    std::filesystem::path homeDir = (home && *home) ? std::filesystem::path(home) : std::filesystem::path("/tmp");
    return homeDir / "Library" / "LaunchAgents" / "com.azkar.reminder.plist";
}

bool MacAutoStart::IsAutoStartEnabled() const {
    auto path = GetLaunchAgentPlistPath();
    return std::filesystem::exists(path);
}

bool MacAutoStart::SetAutoStartEnabled(bool enable, const std::filesystem::path& appPath) {
    auto plistPath = GetLaunchAgentPlistPath();
    std::error_code ec;

    if (!enable) {
        if (std::filesystem::exists(plistPath)) {
            std::filesystem::remove(plistPath, ec);
            if (ec) {
                LOG_ERROR("Failed to remove LaunchAgent plist: " + ec.message());
                return false;
            }
        }
        LOG_INFO("macOS LaunchAgent autostart entry removed: " + plistPath.string());
        return true;
    }

    std::filesystem::path execPath = appPath;
    if (execPath.empty()) {
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        std::vector<char> buffer(size);
        if (_NSGetExecutablePath(buffer.data(), &size) != 0) return false;
        execPath = std::filesystem::weakly_canonical(buffer.data(), ec);
        if (ec) return false;
    }

    auto parentDir = plistPath.parent_path();
    std::filesystem::create_directories(parentDir, ec);
    if (ec) return false;

    std::string xmlPath;
    for (char character : execPath.string()) {
        switch (character) {
            case '&': xmlPath += "&amp;"; break;
            case '<': xmlPath += "&lt;"; break;
            case '>': xmlPath += "&gt;"; break;
            case '"': xmlPath += "&quot;"; break;
            case '\'': xmlPath += "&apos;"; break;
            default: xmlPath += character;
        }
    }
    std::ostringstream out;

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
    out << "<plist version=\"1.0\">\n";
    out << "<dict>\n";
    out << "    <key>Label</key>\n";
    out << "    <string>com.azkar.reminder</string>\n";
    out << "    <key>ProgramArguments</key>\n";
    out << "    <array>\n";
    out << "        <string>" << xmlPath << "</string>\n";
    out << "        <string>--background</string>\n";
    out << "    </array>\n";
    out << "    <key>RunAtLoad</key>\n";
    out << "    <true/>\n";
    out << "    <key>KeepAlive</key>\n";
    out << "    <false/>\n";
    out << "</dict>\n";
    out << "</plist>\n";
    if (!WriteFileAtomically(plistPath, out.str())) {
        LOG_ERROR("Failed writing LaunchAgent plist data");
        return false;
    }

    LOG_INFO("macOS LaunchAgent created successfully: " + plistPath.string());
    return true;
}

} // namespace dhikr

#endif // __APPLE__
