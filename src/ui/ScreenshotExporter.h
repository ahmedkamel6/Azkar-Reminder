#pragma once
#include <string>
#include "../core/Settings.h"

namespace dhikr {

class MainWindow;
class PopupWindow;

class ScreenshotExporter {
public:
    // Exports all requested tabs and themes to pristine high-res PNG files
    static bool ExportAll(const std::wstring& outputDir);

private:
    static bool RenderMainWindow(MainWindow& mainWin, int pageId, ColorTheme theme, const std::wstring& lang, const std::wstring& outPath, float scale = 1.35f);
    static bool RenderPopup(PopupWindow& popupWin, ColorTheme theme, const std::wstring& outPath, bool withBackdrop = false, float scale = 1.5f);
};

} // namespace dhikr
