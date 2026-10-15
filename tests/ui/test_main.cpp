#include <QApplication>
#include <QtTest>
#include <QTemporaryDir>
#include <QTimeEdit>
#include <QListWidget>
#include <QScrollBar>
#include <QFontDatabase>
#include <QCloseEvent>
#include <QDir>
#include <QSpinBox>
#include <QCheckBox>
#include <QAccessible>
#include <QScreen>
#include "ui/qt/MainWindow.h"
#include "ui/qt/TrayController.h"
#include "core/Settings.h"
#include "core/Database.h"
#include "core/Localization.h"
#include "core/Scheduler.h"
#ifdef Q_OS_LINUX
#include <QDBusConnection>
#include <QDBusVariant>
class Notifications : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")
public:
    QString lastBody;
    QVariantMap lastHints;
public slots:
    uint Notify(const QString&, uint, const QString&, const QString&, const QString& body, const QStringList&, const QVariantMap& hints, int) {
        lastBody = body; lastHints = hints; return 1;
    }
};
#endif

class UiTests : public QObject {
    Q_OBJECT
    QTemporaryDir m_directory;
private slots:
    void initTestCase() {
        QVERIFY(m_directory.isValid());
        qApp->setApplicationVersion(AZKAR_APP_VERSION);
        QVERIFY(QFontDatabase::addApplicationFont(":/azkar/fonts/cairo.ttf") >= 0);
        QVERIFY(dhikr::Settings::Instance().Load(std::filesystem::u8path(m_directory.path().toUtf8().constData())));
        QVERIFY(dhikr::Database::Instance().Open(std::filesystem::u8path((m_directory.path() + "/history.db").toUtf8().constData())));
        dhikr::ContentEngine::Instance().SetSavePath(std::filesystem::u8path((m_directory.path() + "/content.json").toUtf8().constData()));
        dhikr::ContentEngine::Instance().LoadFromResource();
    }
    void init() {
        dhikr::AppSettings settings;
        settings.language = L"en";
        settings.soundEnabled = false;
        settings.animationEnabled = false;
        settings.quietPeriods = {{22, 0, 6, 0}};
        dhikr::Settings::Instance().Update(settings);
        dhikr::Localization::Instance().SetLanguage(L"en");
        qApp->setLayoutDirection(Qt::LeftToRight);
    }
    void quietTimePersistsWithoutToggling() {
        dhikr::qt::MainWindow window;
        auto* time = window.findChild<QTimeEdit*>("QuietStart");
        QVERIFY(time);
        time->setTime(QTime(21, 15));
        QCOMPARE(dhikr::Settings::Instance().GetCopy().quietPeriods.at(0).startHour, 21);
        QVERIFY(dhikr::Settings::Instance().Load(std::filesystem::u8path(m_directory.path().toUtf8().constData())));
        QCOMPARE(dhikr::Settings::Instance().GetCopy().quietPeriods.at(0).startMin, 15);
    }
    void weeklyScheduleAndMasterSwitchPersist() {
        dhikr::qt::MainWindow window;
        auto* time = window.findChild<QTimeEdit*>("ScheduleStart3");
        auto* frequency = window.findChild<QSpinBox*>("ScheduleFrequency3");
        auto* enabled = window.findChild<QCheckBox*>("RemindersEnabled");
        QVERIFY(time && frequency && enabled);
        auto* accessible = QAccessible::queryAccessibleInterface(time);
        QVERIFY(accessible);
        QCOMPARE(accessible->text(QAccessible::Name), QString("Wednesday, Start Time"));
        time->setTime(QTime(7, 20));
        frequency->setValue(17);
        enabled->setChecked(false);
        QVERIFY(dhikr::Settings::Instance().Load(std::filesystem::u8path(m_directory.path().toUtf8().constData())));
        const auto settings = dhikr::Settings::Instance().GetCopy();
        QCOMPARE(settings.weeklySchedule[3].startMin, 20);
        QCOMPARE(settings.weeklySchedule[3].frequencyMinutes, 17);
        QVERIFY(!settings.remindersEnabled);
    }
    void languageRebuildsAndPersists() {
        dhikr::qt::MainWindow window;
        auto* navigation = window.findChild<QListWidget*>("Sidebar");
        QVERIFY(navigation);
        const auto previous = navigation->item(0)->text();
        QVERIFY(QMetaObject::invokeMethod(&window, "OnLanguageToggle", Qt::DirectConnection));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        navigation = window.findChild<QListWidget*>("Sidebar");
        QVERIFY(navigation && navigation->item(0)->text() != previous);
        QVERIFY(dhikr::Settings::Instance().Load(std::filesystem::u8path(m_directory.path().toUtf8().constData())));
        QVERIFY(dhikr::Settings::Instance().GetCopy().language == L"ar");
        const auto images = qEnvironmentVariable("AZKAR_SCREENSHOTS_DIR");
        if (!images.isEmpty()) {
            QDir().mkpath(images);
            window.show();
            QTest::qWait(1200);
            for (int page = 0; page < 5; ++page) {
                navigation->setCurrentRow(page);
                QApplication::processEvents();
                QVERIFY(window.grab().save(images + QString("/page-%1.png").arg(page)));
            }
        }
    }
    void popupPreservesReadingAndSupportsScrolling() {
        dhikr::Settings::Instance().Modify([](dhikr::AppSettings& settings) { settings.showTashkeel = false; });
        dhikr::qt::PopupWindow popup;
        dhikr::ContentItem item;
        item.id = "long"; item.category = "test";
        for (int i = 0; i < 100; ++i) item.text += L"سُبْحَانَ اللهِ وَبِحَمْدِهِ ";
        popup.ShowReminder(item);
        QApplication::processEvents();
        auto* label = popup.findChild<QLabel*>("DhikrArabic");
        auto* scroll = popup.findChild<QScrollArea*>();
        QVERIFY(label && scroll);
        QVERIFY(!label->text().contains(QChar(0x064F)));
        QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
        QVERIFY(popup.height() <= QGuiApplication::primaryScreen()->availableGeometry().height() / 2);
        const auto first = label->text();
        dhikr::ContentItem replacement; replacement.text = L"replacement";
        popup.ShowReminder(replacement);
        QCOMPARE(label->text(), first);
        auto path = qEnvironmentVariable("AZKAR_SCREENSHOTS_DIR");
        if (!path.isEmpty()) QVERIFY(popup.grab().save(path + "/popup-scroll.png"));
    }
    void closeHasAUsableFallbackWithoutTray() {
        if (QSystemTrayIcon::isSystemTrayAvailable()) QSKIP("A system tray is available");
        dhikr::qt::MainWindow window;
        dhikr::qt::TrayController tray(&window);
        window.SetTrayController(&tray);
        QCloseEvent event;
        QApplication::sendEvent(&window, &event);
        QVERIFY(event.isAccepted());
    }
#ifdef Q_OS_LINUX
    void notificationUsesDesktopPolicy() {
        auto bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) QSKIP("Run under dbus-run-session to test notifications");
        Notifications service;
        QVERIFY(bus.registerService("org.freedesktop.Notifications"));
        QVERIFY(bus.registerObject("/org/freedesktop/Notifications", &service, QDBusConnection::ExportAllSlots));
        dhikr::qt::MainWindow window;
        dhikr::qt::TrayController tray(&window);
        dhikr::ContentItem item; item.text = L"ذكر <literal>";
        bool done = false, successful = false;
        tray.NotifyReminder(item, [&](bool accepted) { successful = accepted; done = true; });
        QTRY_VERIFY_WITH_TIMEOUT(done, 3000);
        QVERIFY(successful);
        QCOMPARE(service.lastBody, QString::fromUtf8("ذكر &lt;literal&gt;"));
        QCOMPARE(service.lastHints.value("suppress-sound").toBool(), true);
        bus.unregisterObject("/org/freedesktop/Notifications");
        bus.unregisterService("org.freedesktop.Notifications");
    }
#endif
    void cleanupTestCase() { dhikr::Scheduler::Instance().Stop(); dhikr::Database::Instance().Close(); }
};

QTEST_MAIN(UiTests)
#include "test_main.moc"
