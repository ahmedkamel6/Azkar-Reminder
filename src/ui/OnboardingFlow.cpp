#include "OnboardingFlow.h"
#include "../core/Settings.h"
#include "../core/ContentEngine.h"
#include "../core/Application.h"
#include "../core/Logger.h"
#include "../../src/resource.h"
#include <commdlg.h>

namespace dhikr {

void OnboardingFlow::Show(HINSTANCE hInstance) {
    m_hInst = hInstance;
    // Use a simple modal dialog from resource
    // For a resource-less approach, create a simple Win32 dialog programmatically
    DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_ONBOARDING), nullptr, DlgProc, (LPARAM)this);
}

INT_PTR CALLBACK OnboardingFlow::DlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    OnboardingFlow* self = nullptr;

    if (msg == WM_INITDIALOG) {
        self = reinterpret_cast<OnboardingFlow*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->DrawPage(hwnd, self->m_page);
        return TRUE;
    }

    self = reinterpret_cast<OnboardingFlow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) return FALSE;

    switch (msg) {
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDC_ONBOARD_NEXT:  self->OnNext(hwnd); return TRUE;
                case IDC_ONBOARD_BACK:  self->OnBack(hwnd); return TRUE;
                case IDC_ONBOARD_BROWSE: self->OnBrowseContent(hwnd); return TRUE;
                case IDCANCEL:          EndDialog(hwnd, 0); return TRUE;
            }
            break;
    }
    return FALSE;
}

void OnboardingFlow::DrawPage(HWND hwnd, int page) {
    // Update title and description label controls
    static const wchar_t* kTitles[] = {
        L"Welcome to Azkar Reminder",
        L"Load Your Content File",
        L"Configure Your Schedule",
        L"You're all set!"
    };
    static const wchar_t* kBodies[] = {
        L"This app quietly reminds you throughout the day to remember Allah.\n\nIt runs in the background and shows gentle popup reminders at intervals you choose.",
        L"Azkar Reminder includes a built-in library of authentic dhikr.\n\nYou can also import your own custom JSON content file below if you wish.",
        L"Set the times you'd like to receive reminders. You can always change these in Settings.",
        L"Azkar Reminder is ready. It will start with Windows and remind you quietly throughout the day.\n\nMay Allah accept this effort."
    };

    SetDlgItemTextW(hwnd, IDC_ONBOARD_TITLE, kTitles[page]);
    SetDlgItemTextW(hwnd, IDC_ONBOARD_BODY,  kBodies[page]);

    // Show/hide controls based on page
    ShowWindow(GetDlgItem(hwnd, IDC_ONBOARD_BROWSE), page == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(hwnd, IDC_ONBOARD_BACK),   page > 0  ? SW_SHOW : SW_HIDE);

    // Last page: change "Next" to "Finish"
    SetDlgItemTextW(hwnd, IDC_ONBOARD_NEXT, page == 3 ? L"Finish" : L"Next \u2192");
}

void OnboardingFlow::OnNext(HWND hwnd) {
    if (m_page < 3) {
        m_page++;
        DrawPage(hwnd, m_page);
    } else {
        EndDialog(hwnd, 1); // Done
    }
}

void OnboardingFlow::OnBack(HWND hwnd) {
    if (m_page > 0) {
        m_page--;
        DrawPage(hwnd, m_page);
    }
}

void OnboardingFlow::OnBrowseContent(HWND hwnd) {
    wchar_t path[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hwnd;
    ofn.lpstrFilter = L"JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        auto result = ContentEngine::Instance().Load(path);
        if (result == ContentLoadResult::Success) {
            // Copy to AppData
            auto dest = Application::Instance().ContentFilePath();
            CopyFileW(path, dest.c_str(), FALSE);
            SetDlgItemTextW(hwnd, IDC_ONBOARD_FILE_LABEL,
                (std::to_wstring(ContentEngine::Instance().Count()) +
                 L" items loaded successfully.").c_str());
        } else {
            SetDlgItemTextW(hwnd, IDC_ONBOARD_FILE_LABEL,
                ContentEngine::Instance().LastError().c_str());
        }
    }
}

} // namespace dhikr
