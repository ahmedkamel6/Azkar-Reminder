#include "Scheduler.h"
#include "Settings.h"
#include "Logger.h"
#include <algorithm>
#include <cstring>
#include <chrono>

#ifdef _WIN32
#include "../resource.h"
#endif

namespace dhikr {

static inline void SafeLocalTime(struct tm* tm, const time_t* time) {
#ifdef _WIN32
    localtime_s(tm, time);
#else
    localtime_r(time, tm);
#endif
}

Scheduler& Scheduler::Instance() {
    static Scheduler instance;
    return instance;
}

void Scheduler::Start(TriggerCallback onTrigger) {
    if (m_running) return;
    const auto settings = Settings::Instance().GetCopy();
    m_pauseUntil = settings.paused && settings.pauseUntilUnix > time(nullptr) ? static_cast<time_t>(settings.pauseUntilUnix) : 0;
    if (settings.paused && m_pauseUntil == 0) {
        Settings::Instance().Modify([](AppSettings& current) { current.paused = false; current.pauseUntilUnix = 0; });
        Settings::Instance().Save();
    }
    m_onTrigger = std::move(onTrigger);
    m_recalcRequested = false;
    m_running = true;
    m_thread = std::thread(&Scheduler::WorkerThread, this);
    LOG_INFO("Scheduler started");
}

#ifdef _WIN32
void Scheduler::Start(HWND hwnd) {
    m_hwnd = hwnd;
    Start([hwnd]() {
        if (hwnd) {
            PostMessageW(hwnd, WM_REMINDER_TRIGGER, 0, 0);
        }
    });
}
#endif

void Scheduler::Stop() {
    if (!m_running) return;
    m_running = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_recalcRequested = true;
    }
    m_cv.notify_all();

    if (m_thread.joinable()) {
        m_thread.join();
    }
    LOG_INFO("Scheduler stopped");
}

void Scheduler::Recalculate() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_recalcRequested = true;
    }
    m_cv.notify_all();
}

void Scheduler::TriggerNow() {
    m_lastFiredTime = time(nullptr);
#ifdef _WIN32
    if (m_hwnd) {
        PostMessageW(m_hwnd, WM_REMINDER_TRIGGER, 1, 0);
        return;
    }
#endif
    if (m_onTrigger) {
        m_onTrigger();
    }
}

void Scheduler::Pause(int durationMinutes) {
    time_t until;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (durationMinutes <= 0) {
            // Pause until end of day
            time_t now = time(nullptr);
            struct tm lt;
            SafeLocalTime(&lt, &now);
            ++lt.tm_mday;
            lt.tm_hour = 0; lt.tm_min = 0; lt.tm_sec = 0; lt.tm_isdst = -1;
            m_pauseUntil = mktime(&lt);
        } else {
            m_pauseUntil = time(nullptr) + static_cast<time_t>(std::clamp(durationMinutes, 1, 10080)) * 60;
        }
        m_state = ScheduleState::Paused;
        until = m_pauseUntil;
    }

    // Persist pause state
    Settings::Instance().Modify([until](AppSettings& settings) {
        settings.paused = true; settings.pauseUntilUnix = static_cast<int64_t>(until);
    });
    Settings::Instance().Save();

    Recalculate();
    LOG_INFO("Reminders paused for " + std::to_string(durationMinutes) + " minutes");
}

void Scheduler::PauseUntilTomorrow() { Pause(0); }

void Scheduler::Resume() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pauseUntil = 0;
        m_state = ScheduleState::Active;
    }
    Settings::Instance().Modify([](AppSettings& settings) { settings.paused = false; settings.pauseUntilUnix = 0; });
    Settings::Instance().Save();
    Recalculate();
    LOG_INFO("Reminders resumed");
}

bool Scheduler::IsPaused() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_pauseUntil == 0) return false;
    return time(nullptr) < m_pauseUntil;
}

ScheduleState Scheduler::GetState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

time_t Scheduler::GetNextReminderTime() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_nextFire;
}

void Scheduler::OnSystemWake() {
    LOG_INFO("System wake detected — recalculating scheduler");
    Recalculate();
}

// ── Worker thread (C++20 condition_variable) ──────────────────────────────────
void Scheduler::WorkerThread() {
    while (m_running) {
        try {
            const auto now = time(nullptr);
            bool pauseExpired = false;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_pauseUntil != 0 && m_pauseUntil <= now) {
                    m_pauseUntil = 0;
                    pauseExpired = true;
                }
            }
            if (pauseExpired) {
                Settings::Instance().Modify([now](AppSettings& settings) {
                    if (settings.pauseUntilUnix <= now) { settings.paused = false; settings.pauseUntilUnix = 0; }
                });
                Settings::Instance().Save();
            }
            time_t nextFire = ComputeNextReminderTime();
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_nextFire = nextFire;
            }

            std::unique_lock<std::mutex> lock(m_mutex);
            if (nextFire == 0) {
                // No reminder scheduled in current window — sleep until wake or recalc
                m_cv.wait_for(lock, std::chrono::seconds(30), [this]() { return !m_running || m_recalcRequested; });
                m_recalcRequested = false;
                if (!m_running) break;
                continue;
            }

            auto targetPoint = std::chrono::system_clock::from_time_t(std::min(nextFire, time(nullptr) + 30));
            bool timeoutReached = !m_cv.wait_until(lock, targetPoint, [this]() {
                return !m_running || m_recalcRequested;
            });

            if (!m_running) break;

            if (m_recalcRequested) {
                m_recalcRequested = false;
                continue;
            }

            if (timeoutReached && time(nullptr) >= nextFire) {
                const bool pauseWake = m_pauseUntil != 0 && nextFire == m_pauseUntil;
                bool isPaused = (m_pauseUntil != 0 && time(nullptr) < m_pauseUntil);
                auto callback = m_onTrigger;
                lock.unlock(); // Release mutex before calling Settings or dispatching UI callback

                // Verify reminder can trigger
                const auto s = Settings::Instance().GetCopy();
                const auto firedAt = time(nullptr);
                struct tm fireLocal{}; SafeLocalTime(&fireLocal, &firedAt);
                if (!pauseWake && s.remindersEnabled && !isPaused && firedAt - nextFire < 30 && !IsQuietTime(s, fireLocal)) {
                    time_t nowT = time(nullptr);
                    if (nowT - m_lastFiredTime.load() >= 20) {
                        m_lastFiredTime = nowT;
                        if (callback) {
                            try {
                                callback();
                            } catch (const std::exception& e) {
                                LOG_ERROR("Scheduler callback error: " + std::string(e.what()));
                            } catch (...) {
                                LOG_ERROR("Scheduler callback unknown error");
                            }
                        }
                    }
                }
            }
        } catch (const std::exception& ex) {
            LOG_ERROR("Scheduler worker thread exception: " + std::string(ex.what()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        } catch (...) {
            LOG_ERROR("Scheduler worker thread unknown exception");
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

// ── Schedule calculation ─────────────────────────────────────────────────────
namespace {
std::pair<time_t, time_t> DayWindow(const DaySchedule& day, tm today, int offset) {
    today.tm_mday += offset;
    today.tm_hour = day.startHour; today.tm_min = day.startMin; today.tm_sec = 0; today.tm_isdst = -1;
    const auto start = mktime(&today);
    if (day.endHour * 60 + day.endMin <= day.startHour * 60 + day.startMin) ++today.tm_mday;
    today.tm_hour = day.endHour; today.tm_min = day.endMin; today.tm_isdst = -1;
    return {start, mktime(&today)};
}
}

time_t Scheduler::NextReminder(const AppSettings& input, time_t now) {
    auto settings = input;
    Settings::Validate(settings);
    if (!settings.remindersEnabled) return 0;
    if (settings.paused && settings.pauseUntilUnix > now) return static_cast<time_t>(settings.pauseUntilUnix);
    tm local{}; SafeLocalTime(&local, &now);
    time_t earliest = 0;
    for (int offset = -1; offset <= 7; ++offset) {
        const auto& day = settings.weeklySchedule[(local.tm_wday + offset + 7) % 7];
        if (!day.enabled) continue;
        const auto [start, end] = DayWindow(day, local, offset);
        const auto interval = static_cast<time_t>(day.frequencyMinutes) * 60;
        auto candidate = start > now ? start : start + ((now - start) / interval + 1) * interval;
        for (; candidate < end; candidate += interval) {
            tm fireLocal{}; SafeLocalTime(&fireLocal, &candidate);
            if (!IsQuietTime(settings, fireLocal)) {
                if (earliest == 0 || candidate < earliest) earliest = candidate;
                break;
            }
        }
    }
    return earliest;
}

time_t Scheduler::ComputeNextReminderTime() const {
    const auto settings = Settings::Instance().GetCopy();
    const auto now = time(nullptr);
    tm local{}; SafeLocalTime(&local, &now);
    auto state = ScheduleState::OutOfWindow;
    if (!settings.remindersEnabled) state = ScheduleState::Disabled;
    else if (IsQuietTime(settings, local)) state = ScheduleState::QuietHours;
    else {
        for (int offset = -1; offset <= 0; ++offset) {
            const auto& day = settings.weeklySchedule[(local.tm_wday + offset + 7) % 7];
            const auto [start, end] = DayWindow(day, local, offset);
            if (day.enabled && now >= start && now < end) { state = ScheduleState::Active; break; }
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        // Pause/Resume can run after the settings snapshot was taken. Publish
        // the current pause state while holding the same lock as those calls.
        if (settings.remindersEnabled && m_pauseUntil > now) state = ScheduleState::Paused;
        const_cast<Scheduler*>(this)->m_state = state;
    }
    return NextReminder(settings, now);
}

bool Scheduler::IsQuietTime(const dhikr::AppSettings& s, const struct tm& lt) {
    int nowMinutes = lt.tm_hour * 60 + lt.tm_min;
    for (auto& qp : s.quietPeriods) {
        int startMin = qp.startHour * 60 + qp.startMin;
        int endMin   = qp.endHour   * 60 + qp.endMin;
        if (startMin <= endMin) {
            if (nowMinutes >= startMin && nowMinutes < endMin) return true;
        } else {
            // Spans midnight
            if (nowMinutes >= startMin || nowMinutes < endMin) return true;
        }
    }
    return false;
}

} // namespace dhikr
