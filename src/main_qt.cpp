#include <QApplication>
#include <QFontDatabase>
#include <QIcon>
#include <QLockFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QCryptographicHash>
#include <QMessageBox>
#include <QLocale>
#include <QTimer>
#include "core/PlatformPaths.h"
#include "core/Logger.h"
#include "core/Settings.h"
#include "core/Database.h"
#include "core/ContentEngine.h"
#include "core/Scheduler.h"
#include "core/Localization.h"
#include "core/Utf8.h"
#include "ui/qt/MainWindow.h"
#include "ui/qt/TrayController.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Azkar Reminder");
    app.setApplicationVersion(AZKAR_APP_VERSION);
    app.setOrganizationName("Azkar");
    app.setWindowIcon(QIcon(":/azkar/images/logo.png"));
    app.setQuitOnLastWindowClosed(false);
    const auto arguments = app.arguments();
    const bool background = arguments.contains("--background") || arguments.contains("-b");
    const bool quit = arguments.contains("--quit");
    if (!dhikr::PlatformPaths::EnsureDirectoriesExist()) {
        QMessageBox::critical(nullptr, "Azkar Reminder", "Could not create the application's data directories.");
        return 1;
    }
    const auto config = dhikr::PlatformPaths::GetConfigDir();
    const auto configUtf8 = config.u8string();
    const auto configName = QString::fromUtf8(reinterpret_cast<const char*>(configUtf8.data()), static_cast<int>(configUtf8.size()));
    QLockFile lock(configName + "/instance.lock");
    lock.setStaleLockTime(0);
    const auto serverName = "AzkarReminder-" + QCryptographicHash::hash(configName.toUtf8(), QCryptographicHash::Sha256).toHex().left(24);
    if (!lock.tryLock()) {
        if (lock.error() != QLockFile::LockFailedError) return 1;
        QLocalSocket socket;
        socket.connectToServer(QString::fromLatin1(serverName));
        if (!socket.waitForConnected(1500)) return 1;
        const QByteArray request = quit ? "quit\n" : background ? "background\n" : "show\n";
        if (socket.write(request) != request.size()) return 1;
        if (socket.bytesToWrite() && !socket.waitForBytesWritten(1500)) return 1;
        if (!socket.canReadLine() && !socket.waitForReadyRead(1500)) return 1;
        return socket.readLine().trimmed() == "ok" ? 0 : 1;
    }
    if (quit) return 0;
    QLocalServer::removeServer(QString::fromLatin1(serverName));
    QLocalServer server;
    dhikr::Logger::Instance().Initialize(dhikr::PlatformPaths::GetLogsDir());
    LOG_INFO(std::string("Starting Azkar Reminder ") + AZKAR_APP_VERSION);
    if (QFontDatabase::addApplicationFont(":/azkar/fonts/cairo.ttf") < 0) LOG_WARN("Bundled Cairo font could not be loaded");
    if (!dhikr::Settings::Instance().Load(config)) {
        // Preserve malformed settings before an explicit user change creates a replacement.
        if (!dhikr::Settings::Instance().BackupInvalidFile()) { LOG_ERROR("Could not preserve malformed settings"); return 1; }
        LOG_WARN("Malformed settings backed up; defaults are active");
        if (!dhikr::Settings::Instance().Save()) LOG_WARN("Default settings could not be persisted");
    }
    auto language = dhikr::Settings::Instance().GetCopy().language;
    if (language == L"system") language = QLocale::system().language() == QLocale::Arabic ? L"ar" : L"en";
    dhikr::Localization::Instance().SetLanguage(language);
    app.setLayoutDirection(language == L"ar" ? Qt::RightToLeft : Qt::LeftToRight);
    if (!dhikr::Database::Instance().Open(dhikr::PlatformPaths::GetDataDir() / "history.db")) LOG_WARN("History database is unavailable");
    auto& content = dhikr::ContentEngine::Instance();
    const auto contentPath = dhikr::PlatformPaths::GetDataDir() / "content.json";
    content.SetSavePath(contentPath);
    const bool hasContent = std::filesystem::exists(contentPath);
    if (!hasContent || content.Load(contentPath) != dhikr::ContentLoadResult::Success) {
        if (hasContent) { LOG_ERROR("User library could not be loaded; original file retained"); return 1; }
        content.LoadFromResource();
        if (!content.Save()) LOG_WARN("Default library could not be persisted");
    }
    dhikr::qt::MainWindow window;
    dhikr::qt::TrayController tray(&window);
    window.SetTrayController(&tray);
    QObject::connect(&server, &QLocalServer::newConnection, &window, [&] {
        while (server.hasPendingConnections()) {
            auto* socket = server.nextPendingConnection();
            const auto readRequest = [socket, &window] {
                if (!socket->canReadLine()) return;
                const auto request = socket->readLine().trimmed();
                if (request != "quit" && request != "show" && request != "background") { socket->disconnectFromServer(); return; }
                if (request == "show") { window.showNormal(); window.raise(); window.activateWindow(); }
                socket->write("ok\n");
                socket->flush();
                socket->disconnectFromServer();
                // quit() before app.exec() has no effect. Queue shutdown after
                // acknowledging the request, including Cocoa startup events.
                if (request == "quit") QTimer::singleShot(0, qApp, [] { QApplication::quit(); });
            };
            QObject::connect(socket, &QLocalSocket::readyRead, &window, readRequest);
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            // A fast client can send its request before newConnection is handled.
            // readyRead is not emitted again for bytes already in the buffer.
            if (socket->canReadLine()) readRequest();
        }
    });
    tray.Show();
    if (!background || !tray.IsAvailable()) window.show();
    // Publish readiness after construction and Cocoa window/tray startup.
    if (!server.listen(QString::fromLatin1(serverName))) return 1;
    const int result = app.exec();
    dhikr::Scheduler::Instance().Stop();
    dhikr::Database::Instance().Close();
    return result;
}
