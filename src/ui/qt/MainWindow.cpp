#include "MainWindow.h"
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
#include <QScrollArea>
#include <QGroupBox>
#include <QMessageBox>
#include <QInputDialog>
#include <QSignalBlocker>
#include <QCloseEvent>
#include <QDateTime>
#include <QGuiApplication>

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
    m_navRail->setFocusPolicy(Qt::NoFocus);

    auto addItem = [this](const QString& text, const QString& icon) {
        auto* item = new QListWidgetItem(icon + "  " + text, m_navRail);
        item->setSizeHint(QSize(180, 44));
    };

    addItem(QString::fromStdWString(Localization::Instance().Get(L"nav.home", L"الرئيسية")), "🏠");
    addItem(QString::fromStdWString(Localization::Instance().Get(L"nav.reminders", L"التذكيرات")), "⏰");
    addItem(QString::fromStdWString(Localization::Instance().Get(L"nav.library", L"مكتبة الأذكار")), "📖");
    addItem(QString::fromStdWString(Localization::Instance().Get(L"nav.settings", L"الإعدادات")), "⚙️");
    addItem(QString::fromStdWString(Localization::Instance().Get(L"nav.about", L"حول البرنامج")), "ℹ️");

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

QWidget* MainWindow::CreateRemindersPage() {
    auto* page = new QWidget();
    auto* scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget();
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(20);

    // Interval setting
    auto* grpInterval = new QGroupBox(QString::fromStdWString(Localization::Instance().Get(L"reminders.frequency", L"تكرار التذكير")));
    auto* lInterval = new QHBoxLayout(grpInterval);
    auto* lblInt = new QLabel(QString::fromStdWString(Localization::Instance().Get(L"reminders.every", L"إظهار ذكر كل:")));
    m_comboInterval = new QComboBox();
    m_comboInterval->addItems({"5 دقائق", "10 دقائق", "15 دقيقة", "20 دقيقة", "30 دقيقة", "45 دقيقة", "60 دقيقة", "90 دقيقة", "120 دقيقة"});

    int currentMins = Settings::Instance().GetCopy().weeklySchedule[0].frequencyMinutes;
    if (currentMins <= 0) currentMins = 15;
    int idx = 2; // default 15
    if (currentMins == 5) idx = 0;
    else if (currentMins == 10) idx = 1;
    else if (currentMins == 15) idx = 2;
    else if (currentMins == 20) idx = 3;
    else if (currentMins == 30) idx = 4;
    else if (currentMins == 45) idx = 5;
    else if (currentMins == 60) idx = 6;
    else if (currentMins == 90) idx = 7;
    else if (currentMins == 120) idx = 8;
    m_comboInterval->setCurrentIndex(idx);

    connect(m_comboInterval, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        int minutes = 15;
        switch (index) {
            case 0: minutes = 5; break;
            case 1: minutes = 10; break;
            case 2: minutes = 15; break;
            case 3: minutes = 20; break;
            case 4: minutes = 30; break;
            case 5: minutes = 45; break;
            case 6: minutes = 60; break;
            case 7: minutes = 90; break;
            case 8: minutes = 120; break;
        }
        Settings::Instance().Modify([minutes](AppSettings& s) {
            for (int i = 0; i < 7; ++i) {
                s.weeklySchedule[i].frequencyMinutes = minutes;
            }
        });
        Settings::Instance().Save();
        Scheduler::Instance().Recalculate();
    });

    lInterval->addWidget(lblInt);
    lInterval->addWidget(m_comboInterval);
    lInterval->addStretch();
    layout->addWidget(grpInterval);

    // Quiet hours
    auto* grpQuiet = new QGroupBox(QString::fromStdWString(Localization::Instance().Get(L"reminders.quiet_hours", L"ساعات الهدوء")));
    auto* lQuiet = new QVBoxLayout(grpQuiet);
    const auto quietSnap = Settings::Instance().GetCopy();
    bool hasQuiet = !quietSnap.quietPeriods.empty();
    m_chkQuietHours = new QCheckBox(QString::fromStdWString(Localization::Instance().Get(L"reminders.quiet_enable", L"تفعيل ساعات الهدوء (إيقاف مؤقت للتذكيرات ليلاً)")));
    m_chkQuietHours->setChecked(hasQuiet);

    int qStartH = 22, qStartM = 0, qEndH = 6, qEndM = 0;
    if (hasQuiet) {
        qStartH = quietSnap.quietPeriods[0].startHour;
        qStartM = quietSnap.quietPeriods[0].startMin;
        qEndH = quietSnap.quietPeriods[0].endHour;
        qEndM = quietSnap.quietPeriods[0].endMin;
    }

    auto* timeRow = new QHBoxLayout();
    timeRow->addWidget(new QLabel(QString::fromStdWString(Localization::Instance().Get(L"reminders.quiet_start", L"من:"))));
    m_timeQuietStart = new QTimeEdit(QTime(qStartH, qStartM));
    timeRow->addWidget(m_timeQuietStart);
    timeRow->addWidget(new QLabel(QString::fromStdWString(Localization::Instance().Get(L"reminders.quiet_end", L"إلى:"))));
    m_timeQuietEnd = new QTimeEdit(QTime(qEndH, qEndM));
    timeRow->addWidget(m_timeQuietEnd);
    timeRow->addStretch();

    connect(m_chkQuietHours, &QCheckBox::toggled, this, [this](bool checked) {
        Settings::Instance().Modify([this, checked](AppSettings& s) {
            s.quietPeriods.clear();
            if (checked) {
                QuietPeriod qp;
                qp.startHour = m_timeQuietStart->time().hour();
                qp.startMin = m_timeQuietStart->time().minute();
                qp.endHour = m_timeQuietEnd->time().hour();
                qp.endMin = m_timeQuietEnd->time().minute();
                s.quietPeriods.push_back(qp);
            }
        });
        Settings::Instance().Save();
        Scheduler::Instance().Recalculate();
    });

    lQuiet->addWidget(m_chkQuietHours);
    lQuiet->addLayout(timeRow);
    layout->addWidget(grpQuiet);

    // Audio & Fullscreen
    auto* grpAudio = new QGroupBox(QString::fromStdWString(Localization::Instance().Get(L"reminders.audio_section", L"الصوت وملء الشاشة")));
    auto* lAudio = new QVBoxLayout(grpAudio);

    m_chkSound = new QCheckBox(QString::fromStdWString(Localization::Instance().Get(L"reminders.sound_enable", L"تشغيل نغمة هادئة عند ظهور التذكير")));
    m_chkSound->setChecked(quietSnap.soundEnabled);
    connect(m_chkSound, &QCheckBox::toggled, this, [](bool checked) {
        Settings::Instance().Modify([checked](AppSettings& s) { s.soundEnabled = checked; });
        Settings::Instance().Save();
    });

    auto* volRow = new QHBoxLayout();
    volRow->addWidget(new QLabel(QString::fromStdWString(Localization::Instance().Get(L"reminders.volume", L"مستوى الصوت:"))));
    m_sliderVolume = new QSlider(Qt::Horizontal);
    m_sliderVolume->setRange(0, 100);
    m_sliderVolume->setValue(quietSnap.soundVolume);
    connect(m_sliderVolume, &QSlider::valueChanged, this, [](int val) {
        Settings::Instance().Modify([val](AppSettings& s) { s.soundVolume = val; });
        Settings::Instance().Save();
    });
    auto* btnTestAudio = new QPushButton(QString::fromStdWString(Localization::Instance().Get(L"reminders.test_audio", L"تجربة النغمة")));
    connect(btnTestAudio, &QPushButton::clicked, this, [this]() {
        if (m_audioEngine) m_audioEngine->PlayReminder();
    });

    volRow->addWidget(m_sliderVolume);
    volRow->addWidget(btnTestAudio);

    m_chkSuppressFullscreen = new QCheckBox(QString::fromStdWString(Localization::Instance().Get(L"reminders.fullscreen", L"إيقاف التذكيرات تلقائياً أثناء وضع ملء الشاشة (الألعاب والعروض)")));
    m_chkSuppressFullscreen->setChecked(quietSnap.suppressDuringFullscreen);
    connect(m_chkSuppressFullscreen, &QCheckBox::toggled, this, [](bool checked) {
        Settings::Instance().Modify([checked](AppSettings& s) { s.suppressDuringFullscreen = checked; });
        Settings::Instance().Save();
    });

    lAudio->addWidget(m_chkSound);
    lAudio->addLayout(volRow);
    lAudio->addWidget(m_chkSuppressFullscreen);
    layout->addWidget(grpAudio);

    layout->addStretch();
    scroll->setWidget(container);

    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->addWidget(scroll);
    return page;
}

void MainWindow::PopulateLibraryList() {
    if (!m_listContent || !m_txtSearch || !m_comboCategory) return;

    QSignalBlocker blocker(m_listContent);
    m_listContent->clear();

    QString filter = m_txtSearch->text().trimmed();
    QString cat = m_comboCategory->currentText();
    int catIdx = m_comboCategory->currentIndex();

    const auto& items = ContentEngine::Instance().GetItems();
    for (const auto& item : items) {
        QString txt = QString::fromStdWString(item.text);
        QString c = QString::fromStdString(item.category);
        if (!filter.isEmpty() && !txt.contains(filter, Qt::CaseInsensitive)) continue;
        if (catIdx > 0 && c != cat) continue;

        auto* listItem = new QListWidgetItem(QString("[%1]  %2").arg(c, txt), m_listContent);
        listItem->setData(Qt::UserRole, QString::fromStdString(item.id));
        listItem->setSizeHint(QSize(200, 40));
    }
}

void MainWindow::OnAddCustomDhikr() {
    bool ok = false;
    QString text = QInputDialog::getMultiLineText(
        this,
        QString::fromStdWString(Localization::Instance().Get(L"library.dialog_title", L"إضافة ذكر مخصص")),
        QString::fromStdWString(Localization::Instance().Get(L"library.enter_dhikr", L"اكتب نص الذكر:")),
        QString(),
        &ok
    ).trimmed();

    if (!ok || text.isEmpty()) return;

    ContentItem newItem;
    newItem.id = "custom_" + std::to_string(QDateTime::currentMSecsSinceEpoch()) + "_" + std::to_string(rand());
    newItem.text = text.toStdWString();
    newItem.category = "custom";
    newItem.priority = 1;
    newItem.repetitions = 1;
    newItem.enabled = true;

    ContentEngine::Instance().AddItem(newItem);

    // Ensure "custom" category is in the filter combo if not already present
    if (m_comboCategory && m_comboCategory->findText("custom") == -1) {
        m_comboCategory->addItem("custom");
    }

    PopulateLibraryList();
}

void MainWindow::OnDeleteSelectedDhikr() {
    if (!m_listContent) return;
    QListWidgetItem* current = m_listContent->currentItem();
    if (!current) return;

    // Copy ID string safely before any dialog or list mutation to prevent dangling pointer crashes
    QString targetId = current->data(Qt::UserRole).toString();
    if (targetId.isEmpty()) return;

    auto reply = QMessageBox::question(
        this,
        QString::fromStdWString(Localization::Instance().Get(L"library.delete_title", L"حذف الذكر")),
        QString::fromStdWString(Localization::Instance().Get(L"library.delete_item_confirm", L"هل أنت متأكد من حذف هذا الذكر؟")),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        ContentEngine::Instance().DeleteItemById(targetId.toStdString());
        PopulateLibraryList();
    }
}

QWidget* MainWindow::CreateLibraryPage() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);

    // Search, Filter & Custom Dhikr Actions Row
    auto* filterRow = new QHBoxLayout();
    m_txtSearch = new QLineEdit();
    m_txtSearch->setPlaceholderText(QString::fromStdWString(Localization::Instance().Get(L"library.search", L"بحث في الأذكار...")));

    m_comboCategory = new QComboBox();
    m_comboCategory->addItem(QString::fromStdWString(Localization::Instance().Get(L"library.all_categories", L"جميع الأقسام")));
    auto cats = ContentEngine::Instance().GetCategories();
    for (const auto& c : cats) {
        m_comboCategory->addItem(QString::fromStdString(c));
    }

    m_btnAddDhikr = new QPushButton(QString::fromStdWString(Localization::Instance().Get(L"library.add_dhikr", L"+ إضافة ذكر")));
    m_btnAddDhikr->setObjectName("PrimaryButton");
    connect(m_btnAddDhikr, &QPushButton::clicked, this, &MainWindow::OnAddCustomDhikr);

    m_btnDeleteDhikr = new QPushButton(QString::fromUtf8("🗑 حذف المحدد"));
    connect(m_btnDeleteDhikr, &QPushButton::clicked, this, &MainWindow::OnDeleteSelectedDhikr);

    filterRow->addWidget(m_txtSearch, 1);
    filterRow->addWidget(m_comboCategory);
    filterRow->addWidget(m_btnAddDhikr);
    filterRow->addWidget(m_btnDeleteDhikr);
    layout->addLayout(filterRow);

    // Content List
    m_listContent = new QListWidget();
    m_listContent->setStyleSheet("QListWidget { background-color: #101310; border-radius: 8px; padding: 6px; }");

    connect(m_txtSearch, &QLineEdit::textChanged, this, &MainWindow::PopulateLibraryList);
    connect(m_comboCategory, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::PopulateLibraryList);

    layout->addWidget(m_listContent, 1);
    PopulateLibraryList();

    return page;
}

QWidget* MainWindow::CreateSettingsPage() {
    auto* page = new QWidget();
    auto* scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget();
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(20);

    // Themes (Aligned 1-to-1 with ColorTheme enum: Green, Yellow, Blue, Red, Silver, Gold)
    auto* grpTheme = new QGroupBox(QString::fromStdWString(Localization::Instance().Get(L"settings.theme_group", L"المظهر والألوان")));
    auto* lTheme = new QHBoxLayout(grpTheme);
    lTheme->addWidget(new QLabel(QString::fromStdWString(Localization::Instance().Get(L"settings.theme_color", L"لون الثيم:"))));
    m_comboTheme = new QComboBox();
    m_comboTheme->addItems({
        "Green (الأخضر الزمردي)",
        "Yellow (الأصفر المشع)",
        "Blue (السماوي)",
        "Red (الياقوتي)",
        "Silver (الفضي البلاتيني)",
        "Gold (الذهبي الملكي)"
    });
    const auto settingsSnap = Settings::Instance().GetCopy();
    m_comboTheme->setCurrentIndex(std::clamp((int)settingsSnap.colorTheme, 0, 5));

    connect(m_comboTheme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [](int index) {
        Settings::Instance().Modify([index](AppSettings& s) { s.colorTheme = (ColorTheme)index; });
        Settings::Instance().Save();
        ThemeManager::Instance().RefreshTheme();
    });
    lTheme->addWidget(m_comboTheme);
    lTheme->addStretch();
    layout->addWidget(grpTheme);

    // Animations (Aligned 1-to-1 with PopupAnimation & AnimationSpeed enums)
    auto* grpAnim = new QGroupBox(QString::fromStdWString(Localization::Instance().Get(L"settings.animation_group", L"أنماط الحركة والأنيميشن")));
    auto* lAnim = new QVBoxLayout(grpAnim);

    auto* animRow1 = new QHBoxLayout();
    animRow1->addWidget(new QLabel(QString::fromStdWString(Localization::Instance().Get(L"settings.anim_style", L"نمط ظهور التذكير:"))));
    m_comboAnimation = new QComboBox();
    m_comboAnimation->addItems({
        "Classic (انزلاق كلاسيكي ناعم)",
        "Spring Bloom (انبثاق حيوي نابض)",
        "Horizontal Drift (انسياب جانبي هادئ)",
        "Liquid Float (طفو مائي متموج)",
        "Origami Unfurl (انفتاح المخطوطة)",
        "Ethereal Sweep (بريق الهالة النورانية)",
        "Zen Breathing (تنفس السكينة المتناغم)",
        "Ripple Genesis (تموج الواحة الهادئ)"
    });
    m_comboAnimation->setCurrentIndex(std::clamp((int)settingsSnap.popupAnimation, 0, 7));
    connect(m_comboAnimation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        Settings::Instance().Modify([index](AppSettings& s) { s.popupAnimation = (PopupAnimation)index; });
        Settings::Instance().Save();
        TriggerPreview();
    });

    auto* btnPreviewAnim = new QPushButton(QString::fromStdWString(Localization::Instance().Get(L"settings.preview_anim", L"▶ معاينة الحركة")));
    connect(btnPreviewAnim, &QPushButton::clicked, this, &MainWindow::TriggerPreview);

    animRow1->addWidget(m_comboAnimation);
    animRow1->addWidget(btnPreviewAnim);
    animRow1->addStretch();

    auto* animRow2 = new QHBoxLayout();
    animRow2->addWidget(new QLabel(QString::fromStdWString(Localization::Instance().Get(L"settings.anim_speed", L"سرعة الحركة:"))));
    m_comboAnimSpeed = new QComboBox();
    m_comboAnimSpeed->addItems({
        "Normal (متزن وطبيعي)",
        "Cinematic (سينمائي هادئ)",
        "Snappy (سريع وخاطف)"
    });
    m_comboAnimSpeed->setCurrentIndex(std::clamp((int)settingsSnap.animationSpeed, 0, 2));
    connect(m_comboAnimSpeed, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [](int index) {
        Settings::Instance().Modify([index](AppSettings& s) { s.animationSpeed = (AnimationSpeed)index; });
        Settings::Instance().Save();
    });
    animRow2->addWidget(m_comboAnimSpeed);
    animRow2->addStretch();

    lAnim->addLayout(animRow1);
    lAnim->addLayout(animRow2);
    layout->addWidget(grpAnim);

    // Duration & Autostart
    auto* grpGeneral = new QGroupBox(QString::fromStdWString(Localization::Instance().Get(L"settings.general_group", L"عام والتكامل")));
    auto* lGen = new QVBoxLayout(grpGeneral);

    auto* durRow = new QHBoxLayout();
    durRow->addWidget(new QLabel(QString::fromStdWString(Localization::Instance().Get(L"settings.duration", L"مدة بقاء التذكير على الشاشة:"))));
    m_sliderDuration = new QSlider(Qt::Horizontal);
    m_sliderDuration->setRange(2, 60);
    m_sliderDuration->setValue(settingsSnap.popupDurationSec);
    m_lblDurationValue = new QLabel(QString::number(settingsSnap.popupDurationSec) + " ثانية");

    connect(m_sliderDuration, &QSlider::valueChanged, this, [this](int val) {
        Settings::Instance().Modify([val](AppSettings& s) { s.popupDurationSec = val; });
        m_lblDurationValue->setText(QString::number(val) + " ثانية");
        Settings::Instance().Save();
    });
    durRow->addWidget(m_sliderDuration);
    durRow->addWidget(m_lblDurationValue);
    lGen->addLayout(durRow);

    m_chkAutostart = new QCheckBox(QString::fromStdWString(Localization::Instance().Get(L"settings.autostart", L"التشغيل التلقائي مع بدء تشغيل النظام")));
    if (m_autoStart) {
        m_chkAutostart->setChecked(m_autoStart->IsAutoStartEnabled());
    }
    connect(m_chkAutostart, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_autoStart) {
            m_autoStart->SetAutoStartEnabled(checked);
        }
    });
    lGen->addWidget(m_chkAutostart);

    auto* langRow = new QHBoxLayout();
    langRow->addWidget(new QLabel(QString::fromStdWString(Localization::Instance().Get(L"settings.language", L"اللغة (Language):"))));
    m_btnLanguage = new QPushButton(Localization::Instance().IsRTL() ? "English" : "العربية");
    connect(m_btnLanguage, &QPushButton::clicked, this, &MainWindow::OnLanguageToggle);
    langRow->addWidget(m_btnLanguage);
    langRow->addStretch();
    lGen->addLayout(langRow);

    layout->addWidget(grpGeneral);
    layout->addStretch();

    scroll->setWidget(container);

    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->addWidget(scroll);
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

    auto* lblVersion = new QLabel("الإصدار: 1.2.0 (Linux & macOS Cross-Platform)");
    lblVersion->setStyleSheet("color: #94A3B8;");

    auto* lblDesc = new QLabel(
        "برنامج أذكار إسلامي صُمم ليعمل بهدوء في خلفية النظام ويُذكّرك بذكر الله عز وجل.\n\n"
        "• مجاني بالكامل وبدون أي إعلانات لوجه الله تعالى.\n"
        "• خصوصية كاملة بنسبة 100% (يعمل أوفلاين دون إنترنت وبدون أي تعقب أو تتبع نهائياً).\n"
        "• متطابق بالكامل في المزايا مع نسخة الويندوز الأصلية.\n\n"
        "قال تعالى: ﴿أَلَا بِذِكْرِ اللَّهِ تَطْمَئِنُّ الْقُلُوبُ﴾ [الرعد: 28]"
    );
    lblDesc->setFont(ThemeManager::Instance().ArabicFont(12, false));
    lblDesc->setWordWrap(true);
    lblDesc->setStyleSheet("line-height: 1.6; color: #CBD5E1;");

    layout->addWidget(lblTitle);
    layout->addWidget(lblVersion);
    layout->addWidget(lblDesc);
    layout->addStretch();

    return page;
}

void MainWindow::OnCountdownTick() {
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
        }
    }
}

void MainWindow::RefreshDashboardStats() {
    auto& db = Database::Instance();
    m_lblTodayCount->setText(QString::number(db.GetTodayDeliveredCount()));
    m_lblStreakCount->setText(QString::number(db.GetCurrentStreakDays()));
    m_lblTotalCount->setText(QString::number(db.GetTotalCount()));
    if (m_trayController) m_trayController->UpdateStatusTooltip();
}

void MainWindow::TriggerReminder() {
    // Check fullscreen suppression for automatic scheduled reminders
    if (Settings::Instance().GetCopy().suppressDuringFullscreen && m_fsDetector) {
        if (m_fsDetector->IsFullScreenActive()) {
            LOG_INFO("Reminder suppressed due to active fullscreen application");
            return;
        }
    }

    TriggerPreview();
}

void MainWindow::TriggerPreview() {
    if (ContentEngine::Instance().IsEmpty()) {
        ContentEngine::Instance().LoadFromResource();
    }

    auto item = ContentEngine::Instance().GetNext();
    if (!item) return;

    // Copy item by value immediately before dispatching to UI
    ContentItem itemCopy = *item;

    if (m_audioEngine) {
        m_audioEngine->PlayReminder();
    }

    if (m_popupWindow) {
        m_popupWindow->ShowReminder(itemCopy);
    }

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
    Localization::Instance().SetLanguage(isRtl ? L"en" : L"ar");
    QGuiApplication::setLayoutDirection(Localization::Instance().IsRTL() ? Qt::RightToLeft : Qt::LeftToRight);
    UpdateLanguageTexts();
}

void MainWindow::UpdateLanguageTexts() {
    m_btnLanguage->setText(Localization::Instance().IsRTL() ? "English" : "العربية");
    OnThemeChanged();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Minimize to tray instead of terminating
    if (m_trayController) {
        hide();
        event->ignore();
    } else {
        event->accept();
    }
}

} // namespace dhikr::qt
