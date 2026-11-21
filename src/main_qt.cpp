#include <QApplication>
#include <QCoreApplication>
#include <QFontDatabase>
#include <QDir>
#include <iostream>

#include "core/PlatformPaths.h"
#include "core/Logger.h"
#include "core/Settings.h"
#include "core/Database.h"
#include "core/ContentEngine.h"
#include "core/Scheduler.h"
#include "core/Localization.h"

#include "ui/qt/MainWindow.h"
#include "ui/qt/TrayController.h"
#include "ui/qt/ThemeManager.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Azkar Reminder");
    app.setApplicationVersion("1.2.4");
    app.setOrganizationName("Azkar");
    app.setQuitOnLastWindowClosed(false);

    bool startInBackground = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--background" || arg == "-b") {
            startInBackground = true;
        }
    }

    // 1. Initialize Directories & Logging
    dhikr::PlatformPaths::EnsureDirectoriesExist();
    dhikr::Logger::Instance().Initialize(dhikr::PlatformPaths::GetLogsDir());
    LOG_INFO("Starting Azkar Reminder v1.2.4 (Qt 6 Cross-Platform)");

    // 1.5 Load bundled Cairo Arabic font if present
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList fontCandidates = {
        appDir + "/../Resources/fonts/Cairo-Regular.ttf",
        appDir + "/../share/azkar-reminder/fonts/Cairo-Regular.ttf",
        appDir + "/fonts/Cairo-Regular.ttf",
        "assets/fonts/Cairo-Regular.ttf"
    };
    for (const QString& fontPath : fontCandidates) {
        if (QFile::exists(fontPath)) {
            QFontDatabase::addApplicationFont(fontPath);
            break;
        }
    }

    // 2. Load Settings & Localization
    dhikr::Settings::Instance().Load(dhikr::PlatformPaths::GetConfigDir());
    dhikr::Localization::Instance().SetLanguage(
        dhikr::Settings::Instance().GetCopy().language == L"en" ? L"en" : L"ar"
    );

    if (dhikr::Localization::Instance().IsRTL()) {
        QApplication::setLayoutDirection(Qt::RightToLeft);
    } else {
        QApplication::setLayoutDirection(Qt::LeftToRight);
    }

    // 3. Initialize Database
    auto dbPath = dhikr::PlatformPaths::GetDataDir() / "history.db";
    dhikr::Database::Instance().Open(dbPath);

    // 4. Load Dhikr Content (with persistent user content.json and embedded fallback)
    auto userContentJson = dhikr::PlatformPaths::GetDataDir() / "content.json";
    dhikr::ContentEngine::Instance().SetSavePath(userContentJson);

    dhikr::ContentLoadResult loadRes = dhikr::ContentLoadResult::FileNotFound;
    if (std::filesystem::exists(userContentJson)) {
        loadRes = dhikr::ContentEngine::Instance().Load(userContentJson);
    }

    if (loadRes != dhikr::ContentLoadResult::Success || dhikr::ContentEngine::Instance().IsEmpty()) {
        std::filesystem::path appDirPath(appDir.toStdString());
        std::vector<std::filesystem::path> mdCandidates = {
            dhikr::PlatformPaths::GetDataDir() / "content.md",
            appDirPath / ".." / "Resources" / "content.md",
            appDirPath / ".." / "share" / "azkar-reminder" / "content.md",
            appDirPath / "assets" / "content.md",
            appDirPath / "content.md",
            "assets/content.md",
            "content.md"
        };

        for (const auto& candidate : mdCandidates) {
            if (std::filesystem::exists(candidate)) {
                loadRes = dhikr::ContentEngine::Instance().Load(candidate);
                if (loadRes == dhikr::ContentLoadResult::Success && !dhikr::ContentEngine::Instance().IsEmpty()) {
                    break;
                }
            }
        }

        if (loadRes != dhikr::ContentLoadResult::Success || dhikr::ContentEngine::Instance().IsEmpty()) {
            loadRes = dhikr::ContentEngine::Instance().LoadFromResource();
        }

        if (loadRes == dhikr::ContentLoadResult::Success && !dhikr::ContentEngine::Instance().IsEmpty()) {
            dhikr::ContentEngine::Instance().Save();
        }
    }

    // 5. Create UI & Tray
    dhikr::qt::MainWindow mainWindow;
    dhikr::qt::TrayController trayController(&mainWindow);
    mainWindow.SetTrayController(&trayController);

    trayController.Show();

    if (!startInBackground) {
        mainWindow.show();
    }

    LOG_INFO("Application event loop started");
    int exitCode = app.exec();

    // 6. Graceful cleanup
    dhikr::Scheduler::Instance().Stop();
    dhikr::Database::Instance().Close();
    LOG_INFO("Application exited cleanly with code: " + std::to_string(exitCode));

    return exitCode;
}
