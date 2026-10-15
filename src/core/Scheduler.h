#pragma once
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <ctime>
#include "Settings.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace dhikr {

// ── Schedule state ────────────────────────────────────────────────────────────
enum class ScheduleState {
    Active,       // Reminders are running normally
    Paused,       // User-initiated pause (with end time)
    OutOfWindow,  // Current time is outside the configured window
    QuietHours,   // Currently in a quiet period
    Disabled      // Reminders disabled entirely
};

// ── Scheduler ─────────────────────────────────────────────────────────────────
// Cross-platform event-driven scheduler: uses a background thread that sleeps via
// std::condition_variable on standard C++20 clocks. Zero polling.
class Scheduler {
public:
    static Scheduler& Instance();

    using TriggerCallback = std::function<void()>;

    // Start scheduler with callback (cross-platform)
    void Start(TriggerCallback onTrigger);

#ifdef _WIN32
    // Windows overload: posts WM_REMINDER_TRIGGER to hwnd
    void Start(HWND hwnd);
#endif

    void Stop();

    // Recalculate next reminder immediately (call after settings change)
    void Recalculate();

    // Trigger a reminder right now (for testing / "Next Reminder" menu)
    void TriggerNow();

    // Pause control
    void Pause(int durationMinutes); // 0 = indefinite (until tomorrow)
    void PauseUntilTomorrow();
    void Resume();
    bool IsPaused() const;

    // Current state & next fire time
    ScheduleState GetState() const;
    time_t        GetNextReminderTime() const;

    // Called on system wake from sleep/suspend
    void OnSystemWake();

    static time_t NextReminder(const AppSettings& settings, time_t now);

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

private:
    Scheduler() = default;
    ~Scheduler() { Stop(); }

    void WorkerThread();

    // Returns time_t of next reminder, or 0 if none scheduled in range
    time_t ComputeNextReminderTime() const;

    static bool IsQuietTime(const AppSettings& settings, const struct tm& localTime);

    TriggerCallback         m_onTrigger;
#ifdef _WIN32
    HWND                    m_hwnd            = nullptr;
#endif
    std::thread             m_thread;
    std::atomic<bool>       m_running         { false };
    bool                    m_recalcRequested = false;

    mutable std::mutex      m_mutex;
    std::condition_variable m_cv;
    time_t                  m_nextFire        = 0;
    ScheduleState           m_state           = ScheduleState::Disabled;

    time_t                  m_pauseUntil      = 0; // 0 = not paused
    std::atomic<time_t>     m_lastFiredTime   { 0 };
};

} // namespace dhikr
