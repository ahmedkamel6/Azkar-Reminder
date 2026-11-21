#pragma once
#include <windows.h>
#include <shellapi.h>
#include <vector>

namespace dhikr {

class FullScreenDetector {
public:
    static FullScreenDetector& Instance();

    // Returns true if a true fullscreen / gaming / presentation / video application is active on any monitor
    bool IsFullScreenActive() const;

    // Returns all monitors currently covered by an active fullscreen window
    std::vector<HMONITOR> GetFullScreenMonitors() const;

    // Returns true if the given monitor currently has an active fullscreen window
    bool IsMonitorFullScreen(HMONITOR hMon) const;

private:
    FullScreenDetector() = default;
    ~FullScreenDetector() = default;
};

} // namespace dhikr
