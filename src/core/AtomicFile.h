#pragma once
#include <filesystem>
#include <fstream>
#include <string_view>
#ifdef _WIN32
#include <windows.h>
#endif

namespace dhikr {

inline bool WriteFileAtomically(const std::filesystem::path& path, std::string_view data) {
    auto temporary = path;
    temporary += ".tmp";
    std::error_code error;
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file) return false;
        file.write(data.data(), static_cast<std::streamsize>(data.size()));
        file.flush();
        file.close();
        if (file.fail()) {
            std::filesystem::remove(temporary, error);
            return false;
        }
    }
#ifdef _WIN32
    const bool replaced = MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::filesystem::rename(temporary, path, error);
    const bool replaced = !error;
#endif
    if (!replaced) std::filesystem::remove(temporary, error);
    return replaced;
}

} // namespace dhikr
