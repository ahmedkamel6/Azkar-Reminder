#include "TrayController.h"
#include "MainWindow.h"
#include "ThemeManager.h"
#include "../../core/Scheduler.h"
#include "../../core/Localization.h"
#include <QApplication>
#include <QPainter>
#include <QPixmap>
#include <QDateTime>

namespace dhikr::qt {

TrayController::TrayController(MainWindow* mainWindow, QObject* parent)
    : QObject(parent), m_mainWindow(mainWindow) {
    m_trayIcon = new QSystemTrayIcon(this);
    BuildContextMenu();
    UpdateTrayIcon();
    UpdateStatusTooltip();

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &TrayController::OnTrayActivated);
    connect(&ThemeManager::Instance(), &ThemeManager::ThemeChanged, this, &TrayController::UpdateTrayIcon);
}

TrayController::~TrayController() = default;

void TrayController::Show() {
    if (m_trayIcon) {
        m_trayIcon->show();
    }
}

void TrayController::BuildContextMenu() {
    m_menu = new QMenu(m_mainWindow);
    m_menu->setStyleSheet(ThemeManager::Instance().GenerateApplicationStyleSheet());

    m_actOpen = m_menu->addAction(QString::fromStdWString(Localization::Instance().Get(L"tray.open", L"فتح الأذكار")),
                                 this, &TrayController::OnOpenClicked);
    m_actTrigger = m_menu->addAction(QString::fromStdWString(Localization::Instance().Get(L"tray.trigger_now", L"عرض الذكر التالي الآن")),
                                     this, &TrayController::OnTriggerNowClicked);
    m_menu->addSeparator();

    m_actPause15 = m_menu->addAction(QString::fromStdWString(Localization::Instance().Get(L"tray.pause_15", L"إيقاف مؤقت (15 دقيقة)")),
                                     this, &TrayController::OnPause15Clicked);
    m_actPause60 = m_menu->addAction(QString::fromStdWString(Localization::Instance().Get(L"tray.pause_60", L"إيقاف مؤقت (ساعة)")),
                                     this, &TrayController::OnPause60Clicked);
    m_actPauseTomorrow = m_menu->addAction(QString::fromStdWString(Localization::Instance().Get(L"tray.pause_tomorrow", L"إيقاف حتى الغد")),
                                           this, &TrayController::OnPauseTomorrowClicked);
    m_actResume = m_menu->addAction(QString::fromStdWString(Localization::Instance().Get(L"tray.resume", L"استئناف التذكيرات")),
                                    this, &TrayController::OnResumeClicked);
    m_menu->addSeparator();

    m_actExit = m_menu->addAction(QString::fromStdWString(Localization::Instance().Get(L"tray.exit", L"إنهاء")),
                                  this, &TrayController::OnExitClicked);

    m_trayIcon->setContextMenu(m_menu);
}

void TrayController::UpdateTrayIcon() {
    // Generate crisp vector icon for system tray
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor accent = ThemeManager::Instance().PrimaryColor();

    // Outer circle
    painter.setBrush(accent);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(4, 4, 56, 56);

    // Inner crescent cut
    painter.setBrush(QColor(0x08, 0x0A, 0x09));
    painter.drawEllipse(18, 6, 44, 44);

    m_trayIcon->setIcon(QIcon(pixmap));
}

void TrayController::UpdateStatusTooltip() {
    auto& sched = Scheduler::Instance();
    QString tip = "Azkar Reminder";
    if (sched.IsPaused()) {
        tip += " (" + QString::fromStdWString(Localization::Instance().Get(L"tray.paused", L"متوقف مؤقتاً")) + ")";
    } else {
        time_t nextTime = sched.GetNextReminderTime();
        if (nextTime > 0) {
            QDateTime dt = QDateTime::fromSecsSinceEpoch(nextTime);
            tip += "\n" + QString::fromStdWString(Localization::Instance().Get(L"home.next_reminder", L"التذكير القادم:")) + " " + dt.toString("hh:mm AP");
        }
    }
    m_trayIcon->setToolTip(tip);
}

void TrayController::OnTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        OnOpenClicked();
    }
}

void TrayController::OnOpenClicked() {
    if (m_mainWindow) {
        m_mainWindow->showNormal();
        m_mainWindow->activateWindow();
        m_mainWindow->raise();
    }
}

void TrayController::OnTriggerNowClicked() {
    Scheduler::Instance().TriggerNow();
}

void TrayController::OnPause15Clicked() {
    Scheduler::Instance().Pause(15);
    UpdateStatusTooltip();
    UpdateTrayIcon();
}

void TrayController::OnPause60Clicked() {
    Scheduler::Instance().Pause(60);
    UpdateStatusTooltip();
    UpdateTrayIcon();
}

void TrayController::OnPauseTomorrowClicked() {
    Scheduler::Instance().PauseUntilTomorrow();
    UpdateStatusTooltip();
    UpdateTrayIcon();
}

void TrayController::OnResumeClicked() {
    Scheduler::Instance().Resume();
    UpdateStatusTooltip();
    UpdateTrayIcon();
}

void TrayController::OnExitClicked() {
    QApplication::quit();
}

} // namespace dhikr::qt
