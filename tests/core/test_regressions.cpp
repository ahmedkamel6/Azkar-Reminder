#include "core/Settings.h"
#include "core/Scheduler.h"
#include "core/ContentEngine.h"
#include "core/Utf8.h"
#include "core/AtomicFile.h"
#include "platform/IAutoStart.h"
#include <fstream>
#include <functional>
#include <thread>
#include <cstdlib>
#include <chrono>

void RunTest(const std::string&, const std::function<bool()>&);

namespace {
time_t LocalDate(int year, int month, int day, int hour, int minute) {
    tm local{};
    local.tm_year = year - 1900; local.tm_mon = month - 1; local.tm_mday = day;
    local.tm_hour = hour; local.tm_min = minute; local.tm_isdst = -1;
    return mktime(&local);
}

std::filesystem::path TestDirectory(const std::string& name) {
    auto path = std::filesystem::temp_directory_path() / ("azkar_regression_" + name);
    std::filesystem::create_directories(path);
    return path;
}
}

void RunRegressionTests() {
    RunTest("Expired pause clears while disabled before reactivation", [] {
        auto& settings = dhikr::Settings::Instance();
        auto& scheduler = dhikr::Scheduler::Instance();
        const auto before = settings.GetCopy();
        settings.Modify([](dhikr::AppSettings& current) {
            current.remindersEnabled = false;
            current.paused = true;
            current.pauseUntilUnix = time(nullptr) + 1;
        });
        scheduler.Start([] {});
        std::this_thread::sleep_for(std::chrono::milliseconds(1200));
        scheduler.OnSystemWake();
        for (int attempt = 0; attempt < 40 && settings.GetCopy().paused; ++attempt)
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        const bool cleared = !settings.GetCopy().paused && settings.GetCopy().pauseUntilUnix == 0;
        settings.Modify([](dhikr::AppSettings& current) { current.remindersEnabled = true; });
        scheduler.Recalculate();
        scheduler.Stop();
        settings.Update(before);
        return cleared;
    });
    RunTest("Malformed settings backup retains original bytes", [] {
        auto directory = TestDirectory("invalid-backup");
        std::ofstream(directory / "settings.json") << "{broken";
        auto& settings = dhikr::Settings::Instance();
        if (settings.Load(directory) || !settings.BackupInvalidFile()) return false;
        for (const auto& file : std::filesystem::directory_iterator(directory)) {
            if (file.path().filename().string().find("settings-invalid-") != 0) continue;
            std::ifstream stream(file.path());
            std::string bytes((std::istreambuf_iterator<char>(stream)), {});
            return bytes == "{broken" && std::filesystem::exists(directory / "settings.json");
        }
        return false;
    });
    RunTest("Preference updates preserve live pause state", [] {
        auto& settings = dhikr::Settings::Instance();
        auto before = settings.GetCopy();
        auto pending = before;
        pending.soundVolume = 27;
        settings.Modify([](dhikr::AppSettings& value) { value.paused = true; value.pauseUntilUnix = time(nullptr) + 900; });
        const auto deadline = settings.GetCopy().pauseUntilUnix;
        settings.UpdatePreferences(pending);
        const auto after = settings.GetCopy();
        settings.Update(before);
        return after.paused && after.pauseUntilUnix == deadline && after.soundVolume == 27;
    });
    RunTest("Library export retains canonical save path", [] {
        const auto directory = TestDirectory("export");
        auto& content = dhikr::ContentEngine::Instance();
        content.SetSavePath(directory / "content.json");
        content.LoadFromMarkdownString("## test\n- original\n");
        if (!content.Export(directory / "backup.json")) return false;
        content.LoadFromMarkdownString("## test\n- updated\n");
        if (!content.Save() || content.Load(directory / "backup.json") != dhikr::ContentLoadResult::Success) return false;
        if (content.GetItems().at(0).text != L"original") return false;
        return content.Load(directory / "content.json") == dhikr::ContentLoadResult::Success && content.GetItems().at(0).text == L"updated";
    });
    RunTest("Unicode Arabic and non-BMP roundtrip", [] {
        const std::string text = "أَذكار 🌙 📖";
        return dhikr::WideToUtf8(dhikr::Utf8ToWide(text)) == text &&
            dhikr::WideToUtf8(dhikr::Utf8ToWide("\xF0\x80\x80\x80")).find("\xEF\xBF\xBD") == 0;
    });
    RunTest("Disabled items never selected and survive reload", [] {
        auto directory = TestDirectory("disabled");
        auto& content = dhikr::ContentEngine::Instance();
        content.SetSavePath(directory / "content.json");
        content.LoadFromMarkdownString("## test\n- one\n- two\n");
        if (!content.ToggleItemEnabled(0)) return false;
        for (int i = 0; i < 100; ++i) {
            auto next = content.GetNext();
            if (!next || next->text != L"two") return false;
        }
        if (!content.Save() || content.Load(directory / "content.json") != dhikr::ContentLoadResult::Success || content.Count() != 2) return false;
        return !content.GetItems().at(0).enabled;
    });
    RunTest("All disabled and intentional empty library", [] {
        auto& content = dhikr::ContentEngine::Instance();
        content.LoadFromMarkdownString("## test\n- one\n");
        content.ToggleItemEnabled(0);
        if (content.GetNext() || content.GetNextPreview()) return false;
        content.ClearAll();
        auto path = TestDirectory("empty") / "content.json";
        content.SetSavePath(path);
        return content.Save() && content.Load(path) == dhikr::ContentLoadResult::Success && content.IsEmpty();
    });
    RunTest("Content pointer survives deletion", [] {
        auto& content = dhikr::ContentEngine::Instance();
        content.LoadFromMarkdownString("## test\n- محفوظ 🌙\n");
        const auto* item = content.GetNext();
        if (!item) return false;
        auto text = item->text;
        content.DeleteItem(0);
        return item->text == text;
    });
    RunTest("Markdown metadata and invalid import preserve content", [] {
        auto& content = dhikr::ContentEngine::Instance();
        content.LoadFromMarkdownString("## test\n- ذكر | dhikr | meaning | source\n");
        auto items = content.GetItems();
        if (items.size() != 1 || items[0].source != L"source" || items[0].text != L"ذكر") return false;
        auto path = TestDirectory("invalid") / "content.json";
        std::ofstream(path) << "{invalid";
        return content.Load(path) == dhikr::ContentLoadResult::ParseError && content.Count() == 1;
    });
    RunTest("Atomic replacement failure retains original", [] {
        auto directory = TestDirectory("atomic");
        auto path = directory / "data.txt";
        if (!dhikr::WriteFileAtomically(path, "before") || !dhikr::WriteFileAtomically(path, "after")) return false;
        std::ifstream input(path); std::string text; input >> text;
        auto bad = directory / "destination-directory";
        std::filesystem::create_directories(bad);
        return text == "after" && !dhikr::WriteFileAtomically(bad, "lost") && std::filesystem::is_directory(bad);
    });
    RunTest("Settings load rollback, bounds, and save errors", [] {
        auto& settings = dhikr::Settings::Instance();
        auto directory = TestDirectory("settings");
        settings.Load(directory);
        auto expected = settings.GetCopy();
        std::ofstream(directory / "settings.json") << "{\"language\":\"ar\",\"soundVolume\":\"bad\"}";
        if (settings.Load(directory) || settings.GetCopy().language != expected.language) return false;
        auto invalid = expected;
        invalid.popupDurationSec = 2147483647;
        invalid.weeklySchedule[0].frequencyMinutes = -1;
        invalid.pauseUntilUnix = 2200000000LL;
        settings.Update(invalid);
        if (settings.GetCopy().popupDurationSec != 60 || settings.GetCopy().weeklySchedule[0].frequencyMinutes != 1) return false;
        if (!settings.Save() || !settings.Load(directory) || settings.GetCopy().pauseUntilUnix != 2200000000LL) return false;
        auto bad = TestDirectory("save-error");
        std::filesystem::create_directories(bad / "settings.json");
        settings.Load(bad);
        const bool rejected = !settings.Save();
        settings.Update(expected);
        settings.Load(directory);
        return rejected;
    });
    RunTest("Scheduler chooses nearest overlapping window", [] {
        dhikr::AppSettings settings;
        for (auto& day : settings.weeklySchedule) day.enabled = false;
        settings.weeklySchedule[2] = {true, 22, 0, 20, 0, 120};
        settings.weeklySchedule[3] = {true, 0, 0, 23, 59, 5};
        return dhikr::Scheduler::NextReminder(settings, LocalDate(2026, 10, 14, 10, 5)) == LocalDate(2026, 10, 14, 10, 10);
    });
    RunTest("Quiet hours, overnight, pause, and disabled scheduling", [] {
        dhikr::AppSettings settings;
        for (auto& day : settings.weeklySchedule) day = {true, 0, 0, 0, 0, 30};
        settings.quietPeriods = {{22, 0, 6, 0}};
        const auto now = LocalDate(2026, 10, 14, 23, 30);
        if (dhikr::Scheduler::NextReminder(settings, now) != LocalDate(2026, 10, 15, 6, 0)) return false;
        settings.paused = true; settings.pauseUntilUnix = now + 120;
        if (dhikr::Scheduler::NextReminder(settings, now) != now + 120) return false;
        settings.remindersEnabled = false;
        return dhikr::Scheduler::NextReminder(settings, now) == 0;
    });
    RunTest("Scheduler restores persisted pause", [] {
        auto& settings = dhikr::Settings::Instance();
        auto before = settings.GetCopy();
        settings.Modify([](dhikr::AppSettings& current) { current.paused = true; current.pauseUntilUnix = time(nullptr) + 900; });
        auto& scheduler = dhikr::Scheduler::Instance();
        scheduler.Start([] {});
        const bool paused = scheduler.IsPaused();
        scheduler.Stop();
        settings.Update(before);
        return paused;
    });
#ifdef __linux__
    RunTest("AppImage autostart uses permanent path", [] {
        const auto* previous = std::getenv("XDG_CONFIG_HOME");
        const std::string original = previous ? previous : "";
        auto directory = TestDirectory("autostart");
        setenv("XDG_CONFIG_HOME", directory.c_str(), 1);
        setenv("APPIMAGE", "/opt/Azkar Reminder% preview.AppImage", 1);
        auto startup = dhikr::IAutoStart::Create();
        bool result = startup->SetAutoStartEnabled(true);
        std::ifstream file(directory / "autostart/azkar-reminder.desktop");
        std::string text((std::istreambuf_iterator<char>(file)), {});
        result = result && text.find("/opt/Azkar Reminder%% preview.AppImage") != std::string::npos;
        startup->SetAutoStartEnabled(false);
        unsetenv("APPIMAGE");
        if (previous) setenv("XDG_CONFIG_HOME", original.c_str(), 1); else unsetenv("XDG_CONFIG_HOME");
        return result;
    });
#endif
}
