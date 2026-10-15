#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <functional>
#include "../../core/ContentEngine.h"

namespace dhikr::qt {

class MainWindow;

class TrayController : public QObject {
    Q_OBJECT

public:
    explicit TrayController(MainWindow* mainWindow, QObject* parent = nullptr);
    ~TrayController() override;

    void Show();
    void UpdateStatusTooltip();
    void UpdateTrayIcon();
    bool IsAvailable() const;
    void Retranslate();
    void NotifyReminder(const ContentItem& item, std::function<void(bool)> completed);

signals:
    void TriggerReminderRequested();

private slots:
    void OnTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void OnOpenClicked();
    void OnTriggerNowClicked();
    void OnPause15Clicked();
    void OnPause60Clicked();
    void OnPauseTomorrowClicked();
    void OnResumeClicked();
    void OnExitClicked();

private:
    void BuildContextMenu();

    MainWindow*       m_mainWindow = nullptr;
    QSystemTrayIcon*  m_trayIcon = nullptr;
    QMenu*            m_menu = nullptr;

    QAction* m_actOpen = nullptr;
    QAction* m_actTrigger = nullptr;
    QAction* m_actPause15 = nullptr;
    QAction* m_actPause60 = nullptr;
    QAction* m_actPauseTomorrow = nullptr;
    QAction* m_actResume = nullptr;
    QAction* m_actExit = nullptr;
};

} // namespace dhikr::qt
