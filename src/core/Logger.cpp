#include "Logger.h"
#include <array>
#include <algorithm>

namespace dhikr {

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file.is_open()) m_file.close();
}

void Logger::Initialize(const std::filesystem::path& logDirectory) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logDir = logDirectory;

    namespace fs = std::filesystem;
    try {
        fs::create_directories(m_logDir);
    } catch (...) {}

    // Find next available file index
    std::error_code ec;
    for (int i = 0; i < kMaxFiles; ++i) {
        try {
            auto path = m_logDir / ("dhikr_" + std::to_string(i) + ".log");
            bool exists = fs::exists(path, ec);
            if (ec) { ec.clear(); continue; }
            uintmax_t sz = exists ? fs::file_size(path, ec) : 0;
            if (ec) { ec.clear(); continue; }
            if (!exists || sz < kMaxFileSize) {
                m_fileIndex = i;
                m_file.open(path, std::ios::app);
                if (m_file.is_open()) {
                    m_currentSize = exists ? (size_t)sz : 0;
                    break;
                }
            }
        } catch (...) {
            // Skip locked or inaccessible log file
        }
    }

    if (!m_file.is_open()) {
        // All files full or locked — overwrite oldest
        try {
            m_fileIndex = 0;
            auto path = m_logDir / "dhikr_0.log";
            m_file.open(path, std::ios::trunc);
            m_currentSize = 0;
        } catch (...) {}
    }

    // Write startup marker
    if (m_file.is_open()) {
        m_file << "\n── Logger initialized " << CurrentTimestamp() << " ──\n";
        m_file.flush();
    }
}

void Logger::Log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_file.is_open()) return;

    RotateIfNeeded();

    std::string line = "[" + CurrentTimestamp() + "] [" + LevelString(level) + "] " + message + "\n";
    m_file << line;
    m_file.flush();
    m_currentSize += line.size();
}

void Logger::RotateIfNeeded() {
    if (m_currentSize < kMaxFileSize) return;

    m_file.close();
    m_fileIndex = (m_fileIndex + 1) % kMaxFiles;

    auto path = m_logDir / ("dhikr_" + std::to_string(m_fileIndex) + ".log");
    m_file.open(path, std::ios::trunc);
    m_currentSize = 0;

    m_file << "── Log rotated " << CurrentTimestamp() << " ──\n";
    m_file.flush();
}

std::string Logger::CurrentTimestamp() {
    auto now = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string Logger::LevelString(LogLevel level) {
    switch (level) {
        case LogLevel::Info:    return "INFO ";
        case LogLevel::Warning: return "WARN ";
        case LogLevel::Error:   return "ERROR";
        default:                return "?????";
    }
}

} // namespace dhikr
