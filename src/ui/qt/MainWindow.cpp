#include "MainWindow.h"
#include "UiText.h"
#include "TrayController.h"
#include "ThemeManager.h"
#include "../../core/Settings.h"
#include "../../core/Database.h"
#include "../../core/ContentEngine.h"
#include "../../core/Scheduler.h"
#include "../../core/Localization.h"
#include "../../core/Logger.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QCloseEvent>
#include <QGuiApplication>
#include <QApplication>
#include <QAction>
#include <QStyle>
#include <QScreen>
#include <QKeySequence>
#include <algorithm>

namespace dhikr::qt {



MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("Azkar Reminder");
    resize(920, 620);
    setMinimumSize(800, 520);

    m_popupWindow = new PopupWindow();
    m_autoStart   = IAutoStart::Create();
    m_audioEngine = IAudioEngine::Create();
    m_fsDetector  = IFullScreenDetector::Create();

    if (m_audioEngine) {
        m_audioEngine->Initialize();
    }

    SetupUi();
    OnThemeChanged();
    auto* exitAction = new QAction(Text(L"qt.quit"), this);
    exitAction->setShortcut(QKeySequence::Quit);
    addAction(exitAction);
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);

    // Scheduler trigger hook
    Scheduler::Instance().Start([this]() {
        QMetaObject::invokeMethod(this, "TriggerReminder", Qt::QueuedConnection);
    });

    m_statTimer = new QTimer(this);
    connect(m_statTimer, &QTimer::timeout, this, &MainWindow::OnCountdownTick);
    m_statTimer->start(1000);

    connect(&ThemeManager::Instance(), &ThemeManager::ThemeChanged, this, &MainWindow::OnThemeChanged);
}

MainWindow::~MainWindow() {
    Scheduler::Instance().Stop();
    if (m_popupWindow) {
        delete m_popupWindow;
        m_popupWindow = nullptr;
    }
}

void MainWindow::SetTrayController(TrayController* tray) {
    m_trayController = tray;
}

void MainWindow::SetupUi() {
    auto* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    SetupSidebar();
    mainLayout->addWidget(m_navRail);

    m_pageStack = new QStackedWidget(this);
    m_pageStack->addWidget(CreateHomePage());
    m_pageStack->addWidget(CreateRemindersPage());
    m_pageStack->addWidget(CreateLibraryPage());
    m_pageStack->addWidget(CreateSettingsPage());
    m_pageStack->addWidget(CreateAboutPage());

    mainLayout->addWidget(m_pageStack, 1);
}

void MainWindow::SetupSidebar() {
    m_navRail = new QListWidget(this);
    m_navRail->setObjectName("Sidebar");
    m_navRail->setFixedWidth(200);
    m_navRail->setFocusPolicy(Qt::StrongFocus);

    auto addItem = [this](const QString& text, QStyle::StandardPixmap icon) {
        auto* item = new QListWidgetItem(style()->standardIcon(icon), text, m_navRail);
        item->setSizeHint(QSize(180, 44));
    };

    addItem(QString::fromStdWString(Localization::Instance().Get(L"nav.home", L"الرئيسية")), QStyle::SP_DirHomeIcon);
    addItem(QString::fromStdWString(Localization::Instance().Get(L"nav.reminders", L"التذكيرات")), QStyle::SP_BrowserReload);
    addItem(QString::fromStdWString(Localization::Instance().Get(L"nav.library", L"مكتبة الأذكار")), QStyle::SP_FileDialogDetailedView);
    addItem(QString::fromStdWString(Localization::Instance().Get(L"nav.settings", L"الإعدادات")), QStyle::SP_FileDialogContentsView);
    addItem(QString::fromStdWString(Localization::Instance().Get(L"nav.about", L"حول البرنامج")), QStyle::SP_MessageBoxInformation);

    m_navRail->setCurrentRow(0);
    connect(m_navRail, &QListWidget::currentRowChanged, this, &MainWindow::OnNavigationChanged);
}

void MainWindow::OnNavigationChanged(int index) {
    m_pageStack->setCurrentIndex(index);
    if (index == 0) RefreshDashboardStats();
    else if (index == 2) PopulateLibraryList();
}

void MainWindow::OnThemeChanged() {
    setStyleSheet(ThemeManager::Instance().GenerateApplicationStyleSheet());
    if (m_trayController) m_trayController->UpdateTrayIcon();
}

QWidget* MainWindow::CreateHomePage() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(20);

    // Next Reminder Card
    auto* nextCard = new QFrame();
    nextCard->setObjectName("Card");
    auto* nextLayout = new QVBoxLayout(nextCard);
    nextLayout->setContentsMargins(20, 20, 20, 20);

    auto* lblNextTitle = new QLabel(QString::fromStdWString(Localization::Instance().Get(L"home.next_reminder", L"التذكير القادم")));
    lblNextTitle->setFont(ThemeManager::Instance().UiFont(12, true));
    lblNextTitle->setStyleSheet("color: #94A3B8;");

    m_lblNextReminder = new QLabel("--:--");
    m_lblNextReminder->setObjectName("Countdown");
    m_lblNextReminder->setFont(ThemeManager::Instance().UiFont(28, true));

    auto* btnRow = new QHBoxLayout();
    m_btnRemindNow = new QPushButton(QString::fromStdWString(Localization::Instance().Get(L"home.remind_now", L"عرض ذكر الآن")));
    m_btnRemindNow->setObjectName("PrimaryButton");
    connect(m_btnRemindNow, &QPushButton::clicked, this, &MainWindow::TriggerPreview);
    btnRow->addWidget(m_btnRemindNow);

    auto* btnPause15 = new QPushButton(QString::fromStdWString(Localization::Instance().Get(L"home.pause_15", L"إيقاف 15 دقيقة")));
    connect(btnPause15, &QPushButton::clicked, this, [this]() { OnQuickPauseClicked(15); });
    btnRow->addWidget(btnPause15);

    auto* btnPause60 = new QPushButton(QString::fromStdWString(Localization::Instance().Get(L"home.pause_60", L"إيقاف ساعة")));
    connect(btnPause60, &QPushButton::clicked, this, [this]() { OnQuickPauseClicked(60); });
    btnRow->addWidget(btnPause60);

    m_btnQuickResume = new QPushButton(QString::fromStdWString(Localization::Instance().Get(L"home.resume", L"استئناف")));
    connect(m_btnQuickResume, &QPushButton::clicked, this, &MainWindow::OnResumeClicked);
    btnRow->addWidget(m_btnQuickResume);

    btnRow->addStretch();

    nextLayout->addWidget(lblNextTitle);
    nextLayout->addWidget(m_lblNextReminder);
    nextLayout->addLayout(btnRow);
    layout->addWidget(nextCard);

    // Statistics Grid (Today, Streak, Total)
    auto* gridLayout = new QGridLayout();
    gridLayout->setSpacing(16);

    auto createStatCard = [](const QString& title, QLabel*& valLabel) -> QFrame* {
        auto* card = new QFrame();
        card->setObjectName("Card");
        auto* l = new QVBoxLayout(card);
        l->setContentsMargins(16, 16, 16, 16);
        auto* lblT = new QLabel(title);
        lblT->setStyleSheet("color: #94A3B8; font-size: 11px;");
        valLabel = new QLabel("0");
        valLabel->setObjectName("StatValue");
        valLabel->setFont(ThemeManager::Instance().UiFont(22, true));
        l->addWidget(lblT);
        l->addWidget(valLabel);
        return card;
    };

    gridLayout->addWidget(createStatCard(QString::fromStdWString(Localization::Instance().Get(L"home.today_count", L"أذكار اليوم")), m_lblTodayCount), 0, 0);
    gridLayout->addWidget(createStatCard(QString::fromStdWString(Localization::Instance().Get(L"home.current_streak", L"أيام المداومة المتتالية")), m_lblStreakCount), 0, 1);
    gridLayout->addWidget(createStatCard(QString::fromStdWString(Localization::Instance().Get(L"home.total_count", L"إجمالي الأذكار")), m_lblTotalCount), 0, 2);

    layout->addLayout(gridLayout);
    layout->addStretch();

    RefreshDashboardStats();
    return page;
}

QWidget* MainWindow::CreateAboutPage() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 28, 28, 28);
    layout->setSpacing(16);

    auto* lblTitle = new QLabel("Azkar Reminder (أذكار)");
    lblTitle->setFont(ThemeManager::Instance().UiFont(20, true));
    lblTitle->setStyleSheet("color: #B7F34A;");

    auto* lblVersion = new QLabel(QCoreApplication::applicationVersion());
    lblVersion->setStyleSheet("color: #94A3B8;");
    auto* lblDesc = new QLabel(Text(L"qt.about"));
    lblDesc->setFont(ThemeManager::Instance().ArabicFont(12, false));
    lblDesc->setWordWrap(true);
    lblDesc->setStyleSheet("line-height: 1.6; color: #CBD5E1;");

    layout->addWidget(lblTitle);
    layout->addWidget(lblVersion);
    layout->addWidget(lblDesc);
    auto* aboutQt = new QPushButton(Text(L"qt.about_qt"));
    connect(aboutQt, &QPushButton::clicked, this, [this]() { QMessageBox::aboutQt(this); });
    layout->addWidget(aboutQt, 0, Qt::AlignLeading);
    layout->addStretch();

    return page;
}

void MainWindow::OnCountdownTick() {
    if (m_pendingFullscreen) {
        if (Scheduler::Instance().IsPaused() || !Settings::Instance().GetCopy().remindersEnabled) m_pendingFullscreen = false;
        else if (m_fsDetector && m_fsDetector->CheckFullScreen() == FullScreenState::NotFullScreen) {
            m_pendingFullscreen = false;
            TriggerReminder();
        }
    }
    auto& sched = Scheduler::Instance();
    if (sched.IsPaused()) {
        m_lblNextReminder->setText(QString::fromStdWString(Localization::Instance().Get(L"home.status_paused", L"التذكيرات متوقفة مؤقتاً")));
        m_lblNextReminder->setStyleSheet("color: #8E9991;");
    } else {
        time_t nextTime = sched.GetNextReminderTime();
        if (nextTime > 0) {
            time_t now = time(nullptr);
            int diffSec = (int)(nextTime - now);
            if (diffSec < 0) diffSec = 0;
            int m = diffSec / 60;
            int s = diffSec % 60;
            m_lblNextReminder->setText(QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0')));
            m_lblNextReminder->setStyleSheet("color: #FDFEFD;");
        } else m_lblNextReminder->setText(Text(L"qt.no_schedule"));
    }
}

void MainWindow::RefreshDashboardStats() {
    auto& db = Database::Instance();
    m_lblTodayCount->setText(QString::number(db.GetTodayDeliveredCount()));
    m_lblStreakCount->setText(QString::number(db.GetCurrentStreakDays()));
    m_lblTotalCount->setText(QString::number(db.GetTotalCount()));
    if (m_trayController) m_trayController->UpdateStatusTooltip();
}

bool MainWindow::HasActiveReminder() const {
    if (m_notificationPending || (m_popupWindow && m_popupWindow->isVisible())) return true;
    return std::any_of(m_secondaryPopups.begin(), m_secondaryPopups.end(), [](const auto& popup) { return popup->isVisible(); });
}

void MainWindow::TriggerReminder() {
    if (HasActiveReminder() || Scheduler::Instance().IsPaused() || !Settings::Instance().GetCopy().remindersEnabled) return;
    const auto settings = Settings::Instance().GetCopy();
    bool useSystemNotification = QGuiApplication::platformName().startsWith("wayland");
    // Check fullscreen suppression for automatic scheduled reminders
    if (Settings::Instance().GetCopy().suppressDuringFullscreen && m_fsDetector) {
        if (m_fsDetector->IsFullScreenActive()) {
            if (settings.fullscreenSuppressionMode == 0) { m_pendingFullscreen = true; return; }
            if (settings.fullscreenSuppressionMode == 1) return;
            useSystemNotification = true;
        }
    }

    if (useSystemNotification && m_trayController) {
        const auto* item = ContentEngine::Instance().GetNext();
        if (!item) return;
        const auto copy = *item;
        m_notificationPending = true;
        m_trayController->NotifyReminder(copy, [this, copy](bool delivered) {
            m_notificationPending = false;
            if (delivered) Database::Instance().InsertReminderEvent(copy.id, copy.category, "delivered");
            else if (m_popupWindow && !HasActiveReminder() && !Scheduler::Instance().IsPaused() && Settings::Instance().GetCopy().remindersEnabled)
                m_popupWindow->ShowReminder(copy);
            RefreshDashboardStats();
        });
        return;
    }

    TriggerPreview();
}

void MainWindow::TriggerPreview() {
    if (HasActiveReminder()) return;
    const auto* item = ContentEngine::Instance().GetNext();
    if (!item) return;

    // Copy item by value immediately before dispatching to UI
    ContentItem itemCopy = *item;

    if (m_popupWindow) {
        m_popupWindow->SetTargetScreen(nullptr);
        m_popupWindow->ShowReminder(itemCopy);
    }
    m_secondaryPopups.clear();
    if (Settings::Instance().GetCopy().popupMonitor == PopupMonitor::All) {
        for (auto* screen : QGuiApplication::screens()) {
            if (screen == QGuiApplication::primaryScreen()) continue;
            auto popup = std::make_unique<PopupWindow>();
            popup->SetTargetScreen(screen);
            popup->ShowReminder(itemCopy, false);
            m_secondaryPopups.push_back(std::move(popup));
        }
    }

    if (m_audioEngine) m_audioEngine->PlayReminder();
    RefreshDashboardStats();
}

void MainWindow::OnQuickPauseClicked(int minutes) {
    Scheduler::Instance().Pause(minutes);
    RefreshDashboardStats();
    ThemeManager::Instance().RefreshTheme();
}

void MainWindow::OnResumeClicked() {
    Scheduler::Instance().Resume();
    RefreshDashboardStats();
    ThemeManager::Instance().RefreshTheme();
}

void MainWindow::OnLanguageToggle() {
    bool isRtl = Localization::Instance().IsRTL();
    Settings::Instance().Modify([isRtl](AppSettings& settings) { settings.language = isRtl ? L"en" : L"ar"; });
    SaveSettings(this);
    Localization::Instance().SetLanguage(isRtl ? L"en" : L"ar");
    QGuiApplication::setLayoutDirection(Localization::Instance().IsRTL() ? Qt::RightToLeft : Qt::LeftToRight);
    UpdateLanguageTexts();
}

void MainWindow::UpdateLanguageTexts() {
    const int currentPage = m_navRail->currentRow();
    auto* previous = takeCentralWidget();
    SetupUi();
    m_navRail->setCurrentRow(currentPage);
    previous->deleteLater();
    if (m_trayController) m_trayController->Retranslate();
    OnThemeChanged();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Minimize to tray instead of terminating
    if (m_trayController && m_trayController->IsAvailable()) {
        hide();
        event->ignore();
    } else {
        event->accept();
        QApplication::quit();
    }
}

} // namespace dhikr::qt
