#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <filesystem>
#include "core/Database.h"
#include "core/Settings.h"
#include "core/ContentEngine.h"
#include "core/Scheduler.h"
#include "core/PlatformPaths.h"
#include "core/Localization.h"
#include "platform/IAutoStart.h"
#include "platform/IFullScreenDetector.h"

int g_passedTests = 0;
int g_failedTests = 0;
void RunRegressionTests();

void RunTest(const std::string& name, const std::function<bool()>& testFunc) {
    std::cout << "[ RUN  ] " << name << " ... ";
    try {
        if (testFunc()) {
            std::cout << "PASSED" << std::endl;
            g_passedTests++;
        } else {
            std::cout << "FAILED" << std::endl;
            g_failedTests++;
        }
    } catch (const std::exception& e) {
        std::cout << "FAILED (Exception: " << e.what() << ")" << std::endl;
        g_failedTests++;
    } catch (...) {
        std::cout << "FAILED (Unknown exception)" << std::endl;
        g_failedTests++;
    }
}

// ── Test 1: PlatformPaths ─────────────────────────────────────────────────────
bool TestPlatformPaths() {
    auto config = dhikr::PlatformPaths::GetConfigDir();
    auto data   = dhikr::PlatformPaths::GetDataDir();
    auto logs   = dhikr::PlatformPaths::GetLogsDir();

    if (config.empty() || data.empty() || logs.empty()) return false;
    if (!dhikr::PlatformPaths::EnsureDirectoriesExist()) return false;
    return std::filesystem::exists(config) && std::filesystem::exists(data) && std::filesystem::exists(logs);
}

// ── Test 2: Database Operations ───────────────────────────────────────────────
bool TestDatabase() {
    auto tmpDir = std::filesystem::temp_directory_path() / "azkar_test_db";
    std::filesystem::create_directories(tmpDir);
    auto dbPath = tmpDir / "test_history.db";

    std::error_code ec;
    std::filesystem::remove(dbPath, ec);

    auto& db = dhikr::Database::Instance();
    if (!db.Open(dbPath)) return false;

    // Insert events
    if (!db.InsertReminderEvent("subhan_allah", "Tasbeeh", "delivered")) return false;
    if (!db.InsertReminderEvent("alhamdulillah", "Tasbeeh", "delivered")) return false;
    if (!db.InsertReminderEvent("allahu_akbar", "Tasbeeh", "dismissed")) return false;

    // Total count reflects delivered reminders
    if (db.GetTotalCount() != 2) return false;
    if (db.GetTodayDeliveredCount() != 2) return false;

    auto events = db.GetRecentEvents(10);
    if (events.size() != 3) return false;

    auto top = db.GetTopDhikrs(5);
    if (top.empty()) return false;

    db.Close();
    std::filesystem::remove_all(tmpDir, ec);
    return true;
}

// ── Test 3: Settings Persistence ──────────────────────────────────────────────
bool TestSettings() {
    auto tmpDir = std::filesystem::temp_directory_path() / "azkar_test_settings";
    std::filesystem::create_directories(tmpDir);

    auto& sMgr = dhikr::Settings::Instance();
    if (!sMgr.Load(tmpDir)) return false;

    // Mutate
    auto s = sMgr.GetCopy();
    s.colorTheme = dhikr::ColorTheme::Gold;
    s.popupDurationSec = 17;
    s.popupAnimation = dhikr::PopupAnimation::RippleGenesis;
    s.animationSpeed = dhikr::AnimationSpeed::Snappy;
    sMgr.Update(s);
    if (!sMgr.Save()) return false;

    // Reload in fresh object state
    s.colorTheme = dhikr::ColorTheme::Green; // overwrite in memory
    sMgr.Update(s);
    if (!sMgr.Load(tmpDir)) return false;

    const auto loaded = sMgr.GetCopy();
    if (loaded.colorTheme != dhikr::ColorTheme::Gold) return false;
    if (loaded.popupDurationSec != 17) return false;
    if (loaded.popupAnimation != dhikr::PopupAnimation::RippleGenesis) return false;
    if (loaded.animationSpeed != dhikr::AnimationSpeed::Snappy) return false;

    std::error_code ec;
    std::filesystem::remove_all(tmpDir, ec);
    return true;
}

// ── Test 4: ContentEngine Parsing & Selection ─────────────────────────────────
bool TestContentEngine() {
    std::string sampleMarkdown =
        "## تسبيح وتحميد\n"
        "- سبحان الله وبحمده | Subhan Allah | فضلها عظيم | صحيح البخاري\n"
        "- الحمد لله رب العالمين | Alhamdulillah | أم الكتاب | القرآن الكريم\n"
        "## دعاء واستغفار\n"
        "- أستغفر الله العظيم وأتوب إليه | Astaghfirullah | سيد الاستغفار | صحيح مسلم\n";

    auto& ce = dhikr::ContentEngine::Instance();
    auto res = ce.LoadFromMarkdownString(sampleMarkdown);
    if (res != dhikr::ContentLoadResult::Success) return false;

    if (ce.Count() != 3) return false;
    auto cats = ce.GetCategories();
    if (cats.size() != 2) return false;

    const auto* nextItem = ce.GetNext();
    if (!nextItem || nextItem->text.empty()) return false;

    return true;
}

// ── Test 5: Scheduler State & Pause/Resume & TriggerNow ───────────────────────
bool TestScheduler() {
    auto& sched = dhikr::Scheduler::Instance();

    // Enable reminders in settings
    dhikr::Settings::Instance().Modify([](dhikr::AppSettings& settings) {
        settings.remindersEnabled = true; settings.paused = false; settings.pauseUntilUnix = 0;
    });

    bool triggered = false;
    sched.Start([&]() {
        triggered = true;
    });

    sched.Resume();
    if (sched.IsPaused()) { sched.Stop(); return false; }

    // Let the worker race with repeated UI pause/resume requests. A stale
    // settings snapshot must never publish Active after Pause has returned.
    for (int attempt = 0; attempt < 100; ++attempt) {
        sched.Pause(15);
        std::this_thread::yield();
        if (!sched.IsPaused()) { sched.Stop(); return false; }
        if (sched.GetState() != dhikr::ScheduleState::Paused) { sched.Stop(); return false; }
        sched.Resume();
        if (sched.IsPaused()) { sched.Stop(); return false; }
        if (sched.GetState() == dhikr::ScheduleState::Paused) { sched.Stop(); return false; }
    }

    // TriggerNow must invoke the callback
    sched.TriggerNow();
    if (!triggered) { sched.Stop(); return false; }

    sched.Stop();
    return true;
}

// ── Test 6: Localization ──────────────────────────────────────────────────────
bool TestLocalization() {
    auto& loc = dhikr::Localization::Instance();
    loc.SetLanguage(L"ar");
    if (!loc.IsRTL()) return false;
    std::wstring homeAr = loc.Get(L"nav.home");
    if (homeAr.empty()) return false;

    loc.SetLanguage(L"en");
    if (loc.IsRTL()) return false;
    std::wstring homeEn = loc.Get(L"nav.home");
    if (homeEn.empty() || homeEn == homeAr) return false;

    return true;
}

// ── Test 7: Platform Adapters ─────────────────────────────────────────────────
bool TestPlatformAdapters() {
    auto autoStart = dhikr::IAutoStart::Create();
    if (!autoStart) return false;
    bool enabled = autoStart->IsAutoStartEnabled();
    (void)enabled;

    auto fsDetector = dhikr::IFullScreenDetector::Create();
    if (!fsDetector) return false;
    dhikr::FullScreenState state = fsDetector->CheckFullScreen();
    (void)state;

    return true;
}

// ── Test 8: ContentEngine Custom Item Operations & Pointer Safety ─────────────
bool TestContentEngineCustomItems() {
    auto& ce = dhikr::ContentEngine::Instance();
    size_t initialCount = ce.Count();

    dhikr::ContentItem item1;
    item1.id = "custom_test_1";
    item1.text = L"ذكر مخصص للتجربة ١";
    item1.category = "custom";
    item1.repetitions = 3;
    item1.enabled = true;
    ce.AddItem(item1);

    if (ce.Count() != initialCount + 1) return false;

    // Check preview and current
    const auto* prev = ce.GetNextPreview();
    if (!prev) return false;

    // Toggle enabled
    if (!ce.ToggleItemEnabled(0)) return false;

    // Delete by ID
    if (!ce.DeleteItemById("custom_test_1")) return false;
    if (ce.Count() != initialCount) return false;

    // Safe delete of non-existent ID should return false, not crash
    if (ce.DeleteItemById("non_existent_id")) return false;

    // Add and delete by index
    ce.AddItem(item1);
    if (!ce.DeleteItem(0)) return false;
    if (ce.DeleteItem(99999)) return false; // out of bounds check

    return true;
}

// ── Test 9: Embedded Default Content Resource (macOS/Linux/Windows Fallback) ──
bool TestEmbeddedContentResource() {
    auto& ce = dhikr::ContentEngine::Instance();
    ce.ClearAll();
    if (!ce.IsEmpty()) return false;

    auto res = ce.LoadFromResource();
    if (res != dhikr::ContentLoadResult::Success) return false;
    if (ce.Count() < 100) return false;

    const auto* item = ce.GetNext();
    if (!item || item->text.empty()) return false;

    return true;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   Azkar Core Unit Tests (C++20)        " << std::endl;
    std::cout << "========================================" << std::endl;

    RunTest("PlatformPaths Directory Resolution", TestPlatformPaths);
    RunTest("Database CRUD & Statistics",         TestDatabase);
    RunTest("Settings Load & Atomic Save",        TestSettings);
    RunTest("ContentEngine Markdown Parsing",      TestContentEngine);
    RunTest("Scheduler Pause & State Tracking",   TestScheduler);
    RunTest("Localization RTL & Dictionaries",    TestLocalization);
    RunTest("Platform Adapters (AutoStart & FullScreen)", TestPlatformAdapters);
    RunTest("ContentEngine Custom Items & Safety", TestContentEngineCustomItems);
    RunTest("Embedded Content Resource Fallback",  TestEmbeddedContentResource);
    RunRegressionTests();

    std::cout << "========================================" << std::endl;
    std::cout << "Tests Passed: " << g_passedTests << " / " << (g_passedTests + g_failedTests) << std::endl;
    std::cout << "========================================" << std::endl;

    return (g_failedTests == 0) ? 0 : 1;
}
