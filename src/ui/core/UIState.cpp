#include "UIState.h"
#include <windows.h>
#include "../MainWindow.h"

namespace dhikr {

void UIStateManager::SetPage(PageId page) {
    m_state.currentPage = page;
    MainWindow::Instance().NavigateTo(static_cast<int>(page));
}

void UIStateManager::CommitState(const AppUIState& newState) {
    m_state = newState;
    if (MainWindow::Instance().GetHwnd()) {
        InvalidateRect(MainWindow::Instance().GetHwnd(), nullptr, FALSE);
    }
}

void UIStateManager::RefreshStateFromBackendAsync() {
    // Spin up a background thread to read from DB and Settings without blocking UI
    CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
        AppUIState newState;
        newState.currentPage = UIStateManager::Instance().GetState().currentPage;
        newState.settings = Settings::Instance().GetCopy();
        
        auto& db = Database::Instance();
        newState.todayDelivered = db.GetTodayDeliveredCount();
        newState.streakDays = db.GetCurrentStreakDays();
        newState.totalDelivered = db.GetTotalCount();
        newState.thisWeek = db.GetWeekCount();
        newState.dailyStats = db.GetDailyStats(30);

        // We must dispatch the result back to the main thread or safely set it
        // Since we are just writing to m_state and calling InvalidateRect, we can post a custom message
        // Or simpler for now, just copy it since it's a simple struct (though vector copy should ideally be on main thread)
        // Let's use a PostMessage to the main window to apply it on the UI thread
        AppUIState* ptr = new AppUIState(newState);
        PostMessageW(MainWindow::Instance().GetHwnd(), WM_APP + 101, (WPARAM)ptr, 0);
        
        return 0;
    }, nullptr, 0, nullptr);
}

}
