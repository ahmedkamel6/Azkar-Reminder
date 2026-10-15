#include "ContentEngine.h"
#include "Utf8.h"
#include "AtomicFile.h"
#include <azkar/EmbeddedContent.h>
#include "Logger.h"
#include "Database.h"
#ifdef _WIN32
#include "../resource.h"
#endif
#include "../third_party/json.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <random>
#include <set>
#include <filesystem>

using json = nlohmann::json;

namespace dhikr {

ContentEngine& ContentEngine::Instance() {
    static ContentEngine instance;
    return instance;
}

static inline void SafeLocalTime(struct tm* tm, const time_t* time) {
#ifdef _WIN32
    localtime_s(tm, time);
#else
    localtime_r(time, tm);
#endif
}

// ── Load from file ────────────────────────────────────────────────────────────
ContentLoadResult ContentEngine::Load(const std::filesystem::path& contentFilePath) {
    bool isMarkdown = (contentFilePath.extension() == ".md" || contentFilePath.extension() == ".MD");

    // Peek the first character to detect if a .md file is actually JSON
    if (isMarkdown) {
        std::ifstream peekF(contentFilePath, std::ios::binary);
        if (peekF.is_open()) {
            char bom[3] = {};
            peekF.read(bom, 3);
            if (!(bom[0] == '\xEF' && bom[1] == '\xBB' && bom[2] == '\xBF')) {
                peekF.clear();
                peekF.seekg(0);
            }
            char firstChar = 0;
            while (peekF.get(firstChar)) {
                if (!isspace(static_cast<unsigned char>(firstChar))) break;
            }
            if (firstChar == '[' || firstChar == '{') {
                isMarkdown = false;
            }
        }
    }

    if (isMarkdown) {
        return LoadFromMarkdownFile(contentFilePath);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<ContentItem> parsed;
    std::wstring error;
    ContentLoadResult result = ParseAndValidate(contentFilePath, parsed, error);

    if (result == ContentLoadResult::Success) {
        m_lastFilePath = contentFilePath;
        m_items = std::move(parsed);
        m_current = nullptr;
        m_lastError.clear();

        // Seed recent IDs from database history
        auto recentIds = Database::Instance().GetRecentContentIds(kHistoryWindow);
        m_recentIds.clear();
        for (auto& id : recentIds) m_recentIds.push_back(id);

        LOG_INFO("Content loaded: " + std::to_string(m_items.size()) + " items");
    } else {
        m_lastError = error;
        LOG_WARN(std::string("Content load failed: ") + WideToUtf8(m_lastError));
        // Keep previous content if any; do not replace with empty/invalid set
    }

    return result;
}

ContentLoadResult ContentEngine::LoadFromResource() {
#ifdef _WIN32
    auto res = LoadFromMarkdownResource(nullptr, IDR_CONTENT_MD);
    if (res == ContentLoadResult::Success) return res;
#endif
    return LoadFromMarkdownString(kEmbeddedContentMd);
}

#ifdef _WIN32
ContentLoadResult ContentEngine::LoadFromMarkdownResource(HINSTANCE hInstance, int resourceId) {
    if (!hInstance) hInstance = GetModuleHandleW(nullptr);
    HRSRC hRes = FindResourceW(hInstance, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!hRes) {
        return LoadFromMarkdownString(kEmbeddedContentMd);
    }
    HGLOBAL hData = LoadResource(hInstance, hRes);
    if (!hData) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = L"Embedded content resource failed to load";
        return ContentLoadResult::FileNotFound;
    }
    const char* pBuf = reinterpret_cast<const char*>(LockResource(hData));
    DWORD sz = SizeofResource(hInstance, hRes);
    if (!pBuf || sz == 0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = L"Embedded content resource is empty";
        return ContentLoadResult::EmptyContent;
    }
    std::string mdText(pBuf, sz);
    return LoadFromMarkdownString(mdText);
}
#endif

ContentLoadResult ContentEngine::LoadFromMarkdownString(const std::string& markdownText) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::istringstream stream(markdownText);
    std::string line;
    std::vector<ContentItem> items;
    std::string currentCategory = "General";

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        // Skip UTF-8 BOM if on first line
        if (items.empty() && line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF) {
            line = line.substr(3);
            if (line.empty()) continue;
        }

        // Check for category (e.g. ## Section)
        if (line.size() >= 2 && line[0] == '#' && line[1] == '#') {
            size_t start = 2;
            while (start < line.size() && (line[start] == '#' || line[start] == ' ' || line[start] == '\t')) start++;
            if (start < line.size()) {
                currentCategory = line.substr(start);
                while (!currentCategory.empty() && (currentCategory.back() == ' ' || currentCategory.back() == '\t' || currentCategory.back() == '\r')) {
                    currentCategory.pop_back();
                }
            }
            continue;
        }

        // Check for list item (e.g. 1. text or - text or * text)
        size_t textStart = std::string::npos;
        std::string idPrefix = "";

        size_t dotPos = line.find(". ");
        if (dotPos != std::string::npos && dotPos > 0) {
            bool isNum = true;
            for (size_t i = 0; i < dotPos; ++i) {
                if (!isdigit(static_cast<unsigned char>(line[i]))) { isNum = false; break; }
            }
            if (isNum) {
                textStart = dotPos + 2;
                idPrefix = line.substr(0, dotPos);
            }
        }

        if (textStart == std::string::npos && line.size() > 2 && (line[0] == '-' || line[0] == '*') && line[1] == ' ') {
            textStart = 2;
        }

        if (textStart != std::string::npos) {
            ContentItem item;
            item.id = currentCategory + "_" + std::to_string(items.size() + 1);
            std::istringstream fields(line.substr(textStart));
            std::vector<std::wstring> values;
            std::string field;
            while (std::getline(fields, field, '|')) {
                auto first = field.find_first_not_of(" \t");
                auto last = field.find_last_not_of(" \t");
                values.push_back(first == std::string::npos ? L"" : Utf8ToWide(field.substr(first, last - first + 1)));
            }
            if (values.empty()) continue;
            item.text = values[0];
            if (values.size() > 1) item.transliteration = values[1];
            if (values.size() > 2) item.description = values[2];
            if (values.size() > 3) item.source = values[3];
            item.category = currentCategory;
            item.priority = 1;
            item.enabled = true;
            if (!item.text.empty()) items.push_back(std::move(item));
        }
    }

    if (items.empty()) {
        m_lastError = L"No valid content found in markdown data";
        return ContentLoadResult::EmptyContent;
    }

    m_items = std::move(items);
    m_current = nullptr;
    m_lastError.clear();

    auto recentIds = Database::Instance().GetRecentContentIds(kHistoryWindow);
    m_recentIds.clear();
    for (auto& id : recentIds) m_recentIds.push_back(id);

    LOG_INFO("Content loaded from markdown memory stream: " + std::to_string(m_items.size()) + " items");
    return ContentLoadResult::Success;
}

ContentLoadResult ContentEngine::LoadFromMarkdownFile(const std::filesystem::path& filePath) {
    std::ifstream f(filePath, std::ios::binary);
    if (!f.is_open()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = L"Content file not found";
        return ContentLoadResult::FileNotFound;
    }
    std::string str((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto result = LoadFromMarkdownString(str);
    if (result == ContentLoadResult::Success) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastFilePath = filePath;
    }
    return result;
}

ContentLoadResult ContentEngine::Reload() {
    std::filesystem::path source;
    { std::lock_guard<std::mutex> lock(m_mutex); source = m_lastFilePath; }
    return source.empty() ? LoadFromResource() : Load(source);
}

bool ContentEngine::Save() {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Always save to the dedicated AppData path if set; otherwise fall back to source path
    const auto& saveTo = m_savePath.empty() ? m_lastFilePath : m_savePath;
    return SaveToLocked(saveTo);
}

bool ContentEngine::Export(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return SaveToLocked(path);
}

bool ContentEngine::SaveToLocked(const std::filesystem::path& saveTo) {
    if (saveTo.empty()) { m_lastError = L"No library save path configured"; return false; }
    try {
    json j = json::array();
    for (const auto& item : m_items) {
        json obj;
        obj["id"] = item.id;
        obj["category"] = item.category;
        obj["text"] = WideToUtf8(item.text);
        if (!item.transliteration.empty()) obj["transliteration"] = WideToUtf8(item.transliteration);
        if (!item.description.empty()) obj["description"] = WideToUtf8(item.description);
        if (!item.source.empty()) obj["source"] = WideToUtf8(item.source);
        if (item.priority != 1) obj["priority"] = item.priority;
        if (item.repetitions > 1) obj["repetitions"] = item.repetitions;
        if (!item.enabled) obj["enabled"] = false;
        j.push_back(obj);
    }

    if (!WriteFileAtomically(saveTo, j.dump(4))) {
        m_lastError = L"Could not save the library; previous file retained";
        LOG_ERROR("Could not replace content file");
        return false;
    }
    m_lastError.clear();
    return true;
    } catch (const std::exception& error) {
        m_lastError = L"Could not serialize the library; previous file retained";
        LOG_ERROR(std::string("Could not save content: ") + error.what());
        return false;
    }
}

void ContentEngine::AddItem(const ContentItem& item) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_items.insert(m_items.begin(), item);
        m_current = nullptr;
    }
    Save();
}

bool ContentEngine::DeleteItem(size_t index) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (index >= m_items.size()) return false;
        m_items.erase(m_items.begin() + index);
        m_current = nullptr;
    }
    Save();
    return true;
}

bool ContentEngine::DeleteItemById(const std::string& id) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = std::find_if(m_items.begin(), m_items.end(), [&](const ContentItem& item) {
            return item.id == id;
        });
        if (it == m_items.end()) return false;
        m_items.erase(it);
        m_current = nullptr;
    }
    Save();
    return true;
}

bool ContentEngine::ToggleItemEnabled(size_t index) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (index >= m_items.size()) return false;
        m_items[index].enabled = !m_items[index].enabled;
    }
    Save();
    return true;
}

void ContentEngine::ClearAll() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_items.clear();
        m_current = nullptr;
        m_recentIds.clear();
    }
    Save();
}

const ContentItem* ContentEngine::GetNextPreview() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    thread_local ContentItem snapshot;
    if (m_current && m_current->enabled) { snapshot = *m_current; return &snapshot; }
    for (const auto& item : m_items) {
        if (item.enabled) { snapshot = item; return &snapshot; }
    }
    return nullptr;
}

ContentLoadResult ContentEngine::ParseAndValidate(const std::filesystem::path& filePath,
                                                   std::vector<ContentItem>& out,
                                                   std::wstring& errorOut) {
    // Open file as UTF-8
    std::ifstream f(filePath, std::ios::binary);
    if (!f.is_open()) {
        errorOut = L"Content file not found";
        return ContentLoadResult::FileNotFound;
    }

    // Skip UTF-8 BOM if present
    char bom[3] = {};
    f.read(bom, 3);
    if (!(bom[0] == '\xEF' && bom[1] == '\xBB' && bom[2] == '\xBF')) {
        f.clear();
        f.seekg(0);
    }

    json root;
    try {
        f >> root;
    } catch (const json::exception& e) {
        errorOut = Utf8ToWide(std::string("JSON parse error: ") + e.what());
        return ContentLoadResult::ParseError;
    }

    // Validate structure (support both {"items": [...]} and top-level [...])
    json itemsArray;
    if (root.is_array()) {
        itemsArray = root;
    } else if (root.is_object() && root.contains("items") && root["items"].is_array()) {
        itemsArray = root["items"];
    } else {
        errorOut = L"Invalid schema: missing 'items' array";
        return ContentLoadResult::InvalidSchema;
    }

    std::set<std::string> seenIds;
    std::vector<ContentItem> items;

    for (auto& entry : itemsArray) {
        if (!entry.is_object()) continue;

        // Extract text (supporting standard 'text' as well as third-party Arabic DB formats)
        std::string rawText;
        if (entry.contains("text") && entry["text"].is_string()) rawText = entry["text"].get<std::string>();
        else if (entry.contains("content") && entry["content"].is_string()) rawText = entry["content"].get<std::string>();
        else if (entry.contains("body") && entry["body"].is_string()) rawText = entry["body"].get<std::string>();
        else if (entry.contains("arabic") && entry["arabic"].is_string()) rawText = entry["arabic"].get<std::string>();
        else if (entry.contains("dua") && entry["dua"].is_string()) rawText = entry["dua"].get<std::string>();

        if (rawText.empty()) continue;

        ContentItem item;
        item.text = Utf8ToWide(rawText);
        if (item.text.empty()) continue;

        // Extract ID
        if (entry.contains("id") && entry["id"].is_string()) {
            item.id = entry["id"].get<std::string>();
        } else if (entry.contains("id") && entry["id"].is_number_integer()) {
            item.id = std::to_string(entry["id"].get<int>());
        } else {
            item.id = "item_" + std::to_string(seenIds.size() + 1);
        }

        // Skip duplicate IDs
        if (seenIds.count(item.id)) continue;
        seenIds.insert(item.id);

        // Optional fields (with third-party aliases)
        if (entry.contains("category") && entry["category"].is_string())
            item.category = entry["category"].get<std::string>();
        else if (entry.contains("title") && entry["title"].is_string())
            item.category = entry["title"].get<std::string>();
        else if (entry.contains("section") && entry["section"].is_string())
            item.category = entry["section"].get<std::string>();
        else if (entry.contains("chapter") && entry["chapter"].is_string())
            item.category = entry["chapter"].get<std::string>();

        if (entry.contains("transliteration") && entry["transliteration"].is_string())
            item.transliteration = Utf8ToWide(entry["transliteration"].get<std::string>());
        else if (entry.contains("translation") && entry["translation"].is_string())
            item.transliteration = Utf8ToWide(entry["translation"].get<std::string>());

        if (entry.contains("description") && entry["description"].is_string())
            item.description = Utf8ToWide(entry["description"].get<std::string>());
        else if (entry.contains("fadl") && entry["fadl"].is_string())
            item.description = Utf8ToWide(entry["fadl"].get<std::string>());
        else if (entry.contains("meaning") && entry["meaning"].is_string())
            item.description = Utf8ToWide(entry["meaning"].get<std::string>());

        if (entry.contains("source") && entry["source"].is_string())
            item.source = Utf8ToWide(entry["source"].get<std::string>());
        else if (entry.contains("reference") && entry["reference"].is_string())
            item.source = Utf8ToWide(entry["reference"].get<std::string>());
        else if (entry.contains("hadith") && entry["hadith"].is_string())
            item.source = Utf8ToWide(entry["hadith"].get<std::string>());
        else if (entry.contains("surah") && entry["surah"].is_string())
            item.source = Utf8ToWide(entry["surah"].get<std::string>());

        if (entry.contains("priority") && entry["priority"].is_number_integer())
            item.priority = std::clamp(entry["priority"].get<int>(), 1, 5);

        // Recitation repetition count (1, 3, 33, 100)
        if (entry.contains("repetitions") && entry["repetitions"].is_number_integer())
            item.repetitions = std::max(1, entry["repetitions"].get<int>());
        else if (entry.contains("repeat") && entry["repeat"].is_number_integer())
            item.repetitions = std::max(1, entry["repeat"].get<int>());
        else if (entry.contains("count") && entry["count"].is_number_integer())
            item.repetitions = std::max(1, entry["count"].get<int>());

        if (entry.contains("enabled") && entry["enabled"].is_boolean())
            item.enabled = entry["enabled"].get<bool>();

        items.push_back(std::move(item));
    }

    if (items.empty() && !itemsArray.empty()) {
        errorOut = L"No valid content items found";
        return ContentLoadResult::EmptyContent;
    }

    out = std::move(items);
    return ContentLoadResult::Success;
}

// ── Weighted random selection ─────────────────────────────────────────────────
double ContentEngine::ComputeWeight(const ContentItem& item,
                                     size_t recentlyShownIndex) const {
    // Base weight: clamp priority between 1 and 5 (priority 1 = weight 5.0, priority 5 = weight 1.0)
    int p = std::clamp(item.priority, 1, 5);
    double weight = 6.0 - p;

    // ── Time-of-Day Context Awareness ──────────────────────────────────────────
    time_t nowTime = time(nullptr);
    struct tm lt{};
    SafeLocalTime(&lt, &nowTime);
    int hour = lt.tm_hour; // 0..23

    auto containsKeyword = [&](const std::wstring& kw) {
        return item.text.find(kw) != std::wstring::npos ||
               item.description.find(kw) != std::wstring::npos;
    };
    auto containsCategory = [&](const std::string& catKw) {
        return item.category.find(catKw) != std::string::npos;
    };

    // 1. Morning (الصباح / أذكار النهار): 04:00 - 11:59 (Fajr to Dhuhr)
    if (hour >= 4 && hour < 12) {
        if (containsKeyword(L"أصبحنا") || containsKeyword(L"صباح") || containsCategory("صباح") || containsCategory("morning")) {
            weight *= 5.0; // Strongly prioritize morning adhkar
        } else if (containsKeyword(L"أمسينا") || containsCategory("مساء") || containsCategory("evening") || containsCategory("sleep")) {
            weight *= 0.001; // Suppress evening and sleep adhkar in the morning
        } else {
            weight *= 0.9; // General daily content can appear
        }
    }
    // 2. Midday / Afternoon (الكونتنت العادي لباقي اليوم): 12:00 - 16:59
    else if (hour >= 12 && hour < 17) {
        if (containsCategory("morning") || containsKeyword(L"أصبحنا") || containsCategory("evening") || containsKeyword(L"أمسينا") || containsCategory("sleep")) {
            weight *= 0.001; // Suppress morning, evening, and sleep adhkar during midday
        } else {
            weight *= 3.5; // Strongly prioritize general daily content for rest of the day
        }
    }
    // 3. Evening (المساء / أذكار المساء): 17:00 - 21:59
    else if (hour >= 17 && hour < 22) {
        if (containsKeyword(L"أمسينا") || containsKeyword(L"مساء") || containsCategory("مساء") || containsCategory("evening")) {
            weight *= 5.0; // Strongly prioritize evening adhkar
        } else if (containsKeyword(L"أصبحنا") || containsCategory("صباح") || containsCategory("morning") || containsCategory("sleep")) {
            weight *= 0.001; // Suppress morning and sleep adhkar in the evening
        } else {
            weight *= 0.9; // General daily content can appear
        }
    }
    // 4. Night / Sleep / Early Dawn (الليل والنوم): 22:00 - 03:59
    else {
        if (containsCategory("sleep") || containsCategory("night_prayer") ||
            containsKeyword(L"أموت وأحيا") || containsKeyword(L"نوم") || containsKeyword(L"قيام")) {
            weight *= 4.5; // Prioritize sleep and night adhkar
        } else if (containsCategory("morning") || containsCategory("evening")) {
            weight *= 0.001; // Suppress morning and evening adhkar at night
        } else {
            weight *= 1.0; // General daily dhikr can appear
        }
    }

    // Reduce weight heavily if recently shown
    // Items closer to the front of m_recentIds (= more recently shown) are penalized more
    for (size_t i = 0; i < m_recentIds.size(); ++i) {
        if (m_recentIds[i] == item.id) {
            // Exponential decay based on position (0 = most recent)
            double recency = (double)(m_recentIds.size() - i) / (double)m_recentIds.size();
            weight *= (1.0 - 0.95 * recency); // most recent → down to 5% weight
            break;
        }
    }

    return std::max(weight, 0.01); // never zero
}

const ContentItem* ContentEngine::GetNext() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_items.empty()) return nullptr;

    // Build weights
    std::vector<double> weights(m_items.size());
    for (size_t i = 0; i < m_items.size(); ++i) {
        weights[i] = m_items[i].enabled ? ComputeWeight(m_items[i], i) : 0.0;
    }

    // Weighted random selection
    double total = std::accumulate(weights.begin(), weights.end(), 0.0);
    if (total <= 0.0) return nullptr;
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<double> dist(0.0, total);
    double pick = dist(rng);

    size_t selected = 0;
    double cumulative = 0.0;
    for (size_t i = 0; i < weights.size(); ++i) {
        cumulative += weights[i];
        if (weights[i] > 0.0 && pick < cumulative) {
            selected = i;
            break;
        }
    }

    m_current = &m_items[selected];

    // Update history
    m_recentIds.push_front(m_current->id);
    while (m_recentIds.size() > kHistoryWindow) m_recentIds.pop_back();

    thread_local ContentItem snapshot;
    snapshot = *m_current;
    return &snapshot;
}

const ContentItem* ContentEngine::GetCurrent() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_current) return nullptr;
    thread_local ContentItem snapshot;
    snapshot = *m_current;
    return &snapshot;
}

std::vector<ContentItem> ContentEngine::GetAllItems() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_items;
}

size_t ContentEngine::Count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_items.size();
}

void ContentEngine::SetSavePath(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_savePath = path;
}

std::wstring ContentEngine::LastError() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

std::vector<std::string> ContentEngine::GetCategories() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::set<std::string> cats;
    for (auto& item : m_items) {
        if (!item.category.empty()) cats.insert(item.category);
    }
    return std::vector<std::string>(cats.begin(), cats.end());
}

} // namespace dhikr
