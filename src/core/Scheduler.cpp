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
    int64_t pauseUntil = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (durationMinutes <= 0) {
            // Pause until end of day
            time_t now = time(nullptr);
            struct tm lt;
            SafeLocalTime(&lt, &now);
            lt.tm_hour = 23; lt.tm_min = 59; lt.tm_sec = 59;
            lt.tm_isdst = -1;
            pauseUntil = (int64_t)mktime(&lt);
        } else {
            pauseUntil = (int64_t)(time(nullptr) + durationMinutes * 60);
        }
        m_pauseUntil.store(pauseUntil);
        m_state = ScheduleState::Paused;
    }

    // Persist pause state
    Settings::Instance().Modify([pauseUntil](AppSettings& s) {
        s.paused = true;
        s.pauseUntilUnix = (int)pauseUntil;
    });
    Settings::Instance().Save();

    Recalculate();
    LOG_INFO("Reminders paused for " + std::to_string(durationMinutes) + " minutes");
}

void Scheduler::PauseUntilTomorrow() {
    time_t now = time(nullptr);
    struct tm lt;
    SafeLocalTime(&lt, &now);
    // Midnight tomorrow
    lt.tm_mday += 1;
    lt.tm_hour = 0; lt.tm_min = 0; lt.tm_sec = 0;
    lt.tm_isdst = -1;
    int mins = (int)(mktime(&lt) - now) / 60;
    Pause(mins);
}

void Scheduler::Resume() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pauseUntil.store(0);
        m_state = ScheduleState::Active;
    }
    Settings::Instance().Modify([](AppSettings& s) {
        s.paused = false;
        s.pauseUntilUnix = 0;
    });
    Settings::Instance().Save();
    Recalculate();
    LOG_INFO("Reminders resumed");
}

bool Scheduler::IsPaused() const {
    int64_t pauseUntil = m_pauseUntil.load();
    if (pauseUntil == 0) return false;
    return (int64_t)time(nullptr) < pauseUntil;
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
            time_t nextFire = ComputeNextReminderTime();
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_nextFire = nextFire;
            }

            std::unique_lock<std::mutex> lock(m_mutex);
            if (nextFire == 0) {
                // No reminder scheduled in current window — sleep until wake or recalc
                m_cv.wait(lock, [this]() { return !m_running || m_recalcRequested; });
                m_recalcRequested = false;
                if (!m_running) break;
                continue;
            }

            auto targetPoint = std::chrono::system_clock::from_time_t(nextFire);
            bool timeoutReached = !m_cv.wait_until(lock, targetPoint, [this]() {
                return !m_running || m_recalcRequested;
            });

            if (!m_running) break;

            if (m_recalcRequested) {
                m_recalcRequested = false;
                continue;
            }

            if (timeoutReached) {
                bool pauseExpired = false;
                int64_t pauseUntil = m_pauseUntil.load();
                if (pauseUntil != 0 && (int64_t)time(nullptr) >= pauseUntil) {
                    m_pauseUntil.store(0);
                    m_state = ScheduleState::Active;
                    pauseExpired = true;
                }

                bool isPaused = (m_pauseUntil.load() != 0 && (int64_t)time(nullptr) < m_pauseUntil.load());
                auto callback = m_onTrigger;
                lock.unlock(); // Release mutex before calling Settings or dispatching UI callback

                if (pauseExpired) {
                    Settings::Instance().Modify([](AppSettings& s) {
                        s.paused = false;
                        s.pauseUntilUnix = 0;
                    });
                    Settings::Instance().Save();
                    Settings::Instance().NotifyChanged();
                    // Recompute the next periodic trigger time instead of firing immediately at expiry
                    continue;
                }

                // Verify reminder can trigger
                auto s = Settings::Instance().GetCopy();
                if (s.remindersEnabled && !isPaused) {
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
time_t Scheduler::ComputeNextReminderTime() const {
    auto s = Settings::Instance().GetCopy(); // thread-safe

    if (!s.remindersEnabled) {
        std::lock_guard<std::mutex> lock(m_mutex);
        const_cast<Scheduler*>(this)->m_state = ScheduleState::Disabled;
        return 0;
    }

    // Check pause
    int64_t pauseUntil = m_pauseUntil.load();
    if (pauseUntil != 0) {
        time_t now = time(nullptr);
        if ((int64_t)now < pauseUntil) {
            std::lock_guard<std::mutex> lock(m_mutex);
            const_cast<Scheduler*>(this)->m_state = ScheduleState::Paused;
            return (time_t)pauseUntil;
        }
    }

    time_t now = time(nullptr);
    struct tm lt;
    SafeLocalTime(&lt, &now);

    time_t nextFireTime = 0;

    // Check from yesterday (overnight schedules) up to 7 days ahead
    for (int daysAhead = -1; daysAhead <= 7; ++daysAhead) {
        int searchDow = (lt.tm_wday + daysAhead) % 7;
        if (searchDow < 0) searchDow += 7;

        auto& ds = s.weeklySchedule[searchDow];
        if (!ds.enabled) continue;

        struct tm midnight = lt;
        midnight.tm_mday += daysAhead;
        midnight.tm_hour = 0; midnight.tm_min = 0; midnight.tm_sec = 0;
        midnight.tm_isdst = -1;
        mktime(&midnight); // Normalize

        int startMin = ds.startHour * 60 + ds.startMin;
        int endMin = ds.endHour * 60 + ds.endMin;
        int duration = (endMin > startMin) ? (endMin - startMin) : (24 * 60 - startMin + endMin);
        if (duration == 0) duration = 24 * 60; // 00:00 to 00:00 means 24h

        int freq = ds.frequencyMinutes;
        if (freq <= 0) freq = 30;

        for (int k = 0; ; ++k) {
            int t = startMin + k * freq;
            if (t >= startMin + duration) break; // Out of this day's window

            struct tm fireTm = midnight;
            fireTm.tm_min += t;
            fireTm.tm_isdst = -1;
            time_t fireTime = mktime(&fireTm);

            if (fireTime > now) {
                struct tm fireLt;
                SafeLocalTime(&fireLt, &fireTime);
                if (!IsQuietTime(s, fireLt)) {
                    nextFireTime = fireTime;
                    break;
                }
            }
        }
        if (nextFireTime != 0) break;
    }

    // Determine current state based on "now"
    ScheduleState currentState = ScheduleState::OutOfWindow;
    bool inWindow = false;
    for (int daysAhead = -1; daysAhead <= 0; ++daysAhead) {
        int searchDow = (lt.tm_wday + daysAhead) % 7;
        if (searchDow < 0) searchDow += 7;
        auto& ds = s.weeklySchedule[searchDow];
        if (!ds.enabled) continue;

        struct tm midnight = lt;
        midnight.tm_mday += daysAhead;
        midnight.tm_hour = 0; midnight.tm_min = 0; midnight.tm_sec = 0;
        midnight.tm_isdst = -1;
        mktime(&midnight);

        int startMin = ds.startHour * 60 + ds.startMin;
        int endMin = ds.endHour * 60 + ds.endMin;
        int duration = (endMin > startMin) ? (endMin - startMin) : (24 * 60 - startMin + endMin);
        if (duration == 0) duration = 24 * 60;

        struct tm startTm = midnight;
        startTm.tm_min += startMin;
        startTm.tm_isdst = -1;
        time_t absStart = mktime(&startTm);
        time_t absEnd = absStart + duration * 60;

        if (now >= absStart && now < absEnd) {
            inWindow = true;
            break;
        }
    }

    if (inWindow) {
        if (IsQuietTime(s, lt)) currentState = ScheduleState::QuietHours;
        else currentState = ScheduleState::Active;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const_cast<Scheduler*>(this)->m_state = currentState;
    }

    return nextFireTime;
}

bool Scheduler::IsQuietTime(const dhikr::AppSettings& s, const struct tm& lt) const {
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
