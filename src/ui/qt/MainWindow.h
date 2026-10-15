#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QTimeEdit>
#include <QLineEdit>
#include <QTimer>
#include <memory>

#include "PopupWindow.h"
#include "../../platform/IAutoStart.h"
#include "../../platform/IAudioEngine.h"
#include "../../platform/IFullScreenDetector.h"

namespace dhikr::qt {

class TrayController;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void SetTrayController(TrayController* tray);

public slots:
    void TriggerReminder();
    void TriggerPreview();
    void RefreshDashboardStats();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void OnNavigationChanged(int index);
    void OnThemeChanged();
    void OnCountdownTick();
    void OnQuickPauseClicked(int minutes);
    void OnResumeClicked();
    void OnLanguageToggle();
    void OnAddCustomDhikr();
    void OnDeleteSelectedDhikr();

private:
    bool HasActiveReminder() const;
    void SetupUi();
    void SetupSidebar();
    void PopulateLibraryList();
    QWidget* CreateHomePage();
    QWidget* CreateRemindersPage();
    QWidget* CreateLibraryPage();
    QWidget* CreateSettingsPage();
    QWidget* CreateAboutPage();

    void UpdateLanguageTexts();

    // Core references & sub-controllers
    TrayController*  m_trayController = nullptr;
    PopupWindow*     m_popupWindow = nullptr;
    std::unique_ptr<IAutoStart>          m_autoStart;
    std::unique_ptr<IAudioEngine>        m_audioEngine;
    std::unique_ptr<IFullScreenDetector> m_fsDetector;
    std::vector<std::unique_ptr<PopupWindow>> m_secondaryPopups;
    bool m_pendingFullscreen = false;
    bool m_notificationPending = false;

    // UI Structure
    QListWidget*    m_navRail = nullptr;
    QStackedWidget* m_pageStack = nullptr;

    // Home Page Widgets
    QLabel*      m_lblNextReminder = nullptr;
    QLabel*      m_lblTodayCount = nullptr;
    QLabel*      m_lblStreakCount = nullptr;
    QLabel*      m_lblTotalCount = nullptr;
    QPushButton* m_btnRemindNow = nullptr;
    QPushButton* m_btnQuickResume = nullptr;
    QTimer*      m_statTimer = nullptr;

    // Reminders Page Widgets
    QCheckBox*  m_chkQuietHours = nullptr;
    QTimeEdit*  m_timeQuietStart = nullptr;
    QTimeEdit*  m_timeQuietEnd = nullptr;
    QCheckBox*  m_chkSound = nullptr;
    QSlider*    m_sliderVolume = nullptr;
    QCheckBox*  m_chkSuppressFullscreen = nullptr;

    // Library Page Widgets
    QLineEdit*   m_txtSearch = nullptr;
    QListWidget* m_listContent = nullptr;
    QComboBox*   m_comboCategory = nullptr;
    QPushButton* m_btnAddDhikr = nullptr;
    QPushButton* m_btnDeleteDhikr = nullptr;

    // Settings Page Widgets
    QComboBox*   m_comboTheme = nullptr;
    QComboBox*   m_comboAnimation = nullptr;
    QComboBox*   m_comboAnimSpeed = nullptr;
    QSlider*     m_sliderDuration = nullptr;
    QLabel*      m_lblDurationValue = nullptr;
    QCheckBox*   m_chkAutostart = nullptr;
    QPushButton* m_btnLanguage = nullptr;
};

} // namespace dhikr::qt
