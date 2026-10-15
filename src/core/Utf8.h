#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace dhikr {

inline std::wstring Utf8ToWide(std::string_view text) {
    std::wstring result;
    for (size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i]);
        uint32_t value = first;
        size_t length = 1;
        uint32_t minimum = 0;
        if (first >= 0xC2 && first <= 0xDF) { length = 2; value = first & 0x1F; minimum = 0x80; }
        else if (first >= 0xE0 && first <= 0xEF) { length = 3; value = first & 0x0F; minimum = 0x800; }
        else if (first >= 0xF0 && first <= 0xF4) { length = 4; value = first & 7; minimum = 0x10000; }
        bool valid = first < 0x80 || length > 1;
        if (i + length > text.size()) valid = false;
        for (size_t k = 1; valid && k < length; ++k) {
            auto byte = static_cast<unsigned char>(text[i + k]);
            if ((byte & 0xC0) != 0x80) valid = false;
            else value = (value << 6) | (byte & 0x3F);
        }
        valid = valid && value >= minimum && value <= 0x10FFFF && !(value >= 0xD800 && value <= 0xDFFF);
        if (!valid) { value = 0xFFFD; length = 1; }
        i += length;
        if constexpr (sizeof(wchar_t) == 2) {
            if (value > 0xFFFF) {
                value -= 0x10000;
                result.push_back(static_cast<wchar_t>(0xD800 + (value >> 10)));
                result.push_back(static_cast<wchar_t>(0xDC00 + (value & 0x3FF)));
                continue;
            }
        }
        result.push_back(static_cast<wchar_t>(value));
    }
    return result;
}

inline std::string WideToUtf8(std::wstring_view text) {
    std::string result;
    for (size_t i = 0; i < text.size(); ++i) {
        uint32_t value = static_cast<uint32_t>(text[i]);
        if constexpr (sizeof(wchar_t) == 2) {
            if (value >= 0xD800 && value <= 0xDBFF && i + 1 < text.size()) {
                const auto low = static_cast<uint32_t>(text[i + 1]);
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    value = 0x10000 + ((value - 0xD800) << 10) + low - 0xDC00;
                    ++i;
                }
            }
        }
        if (value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) value = 0xFFFD;
        if (value < 0x80) result.push_back(static_cast<char>(value));
        else if (value < 0x800) {
            result.push_back(static_cast<char>(0xC0 | (value >> 6)));
            result.push_back(static_cast<char>(0x80 | (value & 0x3F)));
        } else if (value < 0x10000) {
            result.push_back(static_cast<char>(0xE0 | (value >> 12)));
            result.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (value & 0x3F)));
        } else {
            result.push_back(static_cast<char>(0xF0 | (value >> 18)));
            result.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (value & 0x3F)));
        }
    }
    return result;
}

} // namespace dhikr
