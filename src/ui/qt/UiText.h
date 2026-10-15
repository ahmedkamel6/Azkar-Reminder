#pragma once
#include "../../core/Localization.h"
#include "../../core/Settings.h"
#include <QMessageBox>

namespace dhikr::qt {
inline QString Text(const wchar_t* key) { return QString::fromStdWString(Localization::Instance().Get(key)); }
inline void SaveSettings(QWidget* owner) {
    if (!Settings::Instance().Save()) QMessageBox::warning(owner, "Azkar Reminder", Text(L"qt.save_failed"));
}
} // namespace dhikr::qt
