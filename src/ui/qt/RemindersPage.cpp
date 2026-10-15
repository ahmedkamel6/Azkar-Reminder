#include "MainWindow.h"
#include "UiText.h"
#include "ThemeManager.h"
#include "../../core/ContentEngine.h"
#include "../../core/Scheduler.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QGroupBox>
#include <QSpinBox>

namespace dhikr::qt {
QWidget* MainWindow::CreateRemindersPage() {
    auto* page = new QWidget();
    auto* scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget();
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(20);

    const auto initial = Settings::Instance().GetCopy();
    auto* enabled = new QCheckBox(Text(L"reminders.enable"));
    enabled->setObjectName("RemindersEnabled");
    enabled->setChecked(initial.remindersEnabled);
    connect(enabled, &QCheckBox::toggled, this, [this](bool checked) {
        Settings::Instance().Modify([checked](AppSettings& settings) { settings.remindersEnabled = checked; });
        SaveSettings(this);
        Scheduler::Instance().Recalculate();
    });
    layout->addWidget(enabled);

    auto* schedule = new QGroupBox(Text(L"schedule.title"));
    auto* grid = new QGridLayout(schedule);
    grid->addWidget(new QLabel(Text(L"reminders.start_time")), 0, 1);
    grid->addWidget(new QLabel(Text(L"reminders.end_time")), 0, 2);
    grid->addWidget(new QLabel(Text(L"reminders.frequency")), 0, 3);
    const wchar_t* days[] = {L"schedule.sunday", L"schedule.monday", L"schedule.tuesday", L"schedule.wednesday", L"schedule.thursday", L"schedule.friday", L"schedule.saturday"};
    for (int day = 0; day < 7; ++day) {
        const auto entry = initial.weeklySchedule[day];
        auto* active = new QCheckBox(Text(days[day]));
        active->setChecked(entry.enabled);
        auto* start = new QTimeEdit(QTime(entry.startHour, entry.startMin));
        auto* end = new QTimeEdit(QTime(entry.endHour, entry.endMin));
        start->setLayoutDirection(Qt::LeftToRight);
        end->setLayoutDirection(Qt::LeftToRight);
        auto* frequency = new QSpinBox();
        start->setDisplayFormat("HH:mm"); end->setDisplayFormat("HH:mm");
        frequency->setRange(1, 1440);
        frequency->setSuffix(Text(L"qt.minute_suffix"));
        frequency->setValue(entry.frequencyMinutes);
        start->setAccessibleName(Text(days[day]) + ", " + Text(L"reminders.start_time"));
        end->setAccessibleName(Text(days[day]) + ", " + Text(L"reminders.end_time"));
        frequency->setAccessibleName(Text(days[day]) + ", " + Text(L"reminders.frequency"));
        start->setObjectName(QString("ScheduleStart%1").arg(day));
        frequency->setObjectName(QString("ScheduleFrequency%1").arg(day));
        auto save = [this, day, active, start, end, frequency]() {
            Settings::Instance().Modify([&](AppSettings& settings) {
                auto& value = settings.weeklySchedule[day];
                value = {active->isChecked(), start->time().hour(), start->time().minute(), end->time().hour(), end->time().minute(), frequency->value()};
            });
            SaveSettings(this);
            Scheduler::Instance().Recalculate();
        };
        connect(active, &QCheckBox::toggled, this, save);
        connect(start, &QTimeEdit::timeChanged, this, save);
        connect(end, &QTimeEdit::timeChanged, this, save);
        connect(frequency, QOverload<int>::of(&QSpinBox::valueChanged), this, save);
        grid->addWidget(active, day + 1, 0); grid->addWidget(start, day + 1, 1);
        grid->addWidget(end, day + 1, 2); grid->addWidget(frequency, day + 1, 3);
    }
    auto* scheduleNote = new QLabel(Text(L"qt.schedule_note"));
    scheduleNote->setWordWrap(true);
    grid->addWidget(scheduleNote, 8, 0, 1, 4);
    layout->addWidget(schedule);

    // Quiet hours
    auto* grpQuiet = new QGroupBox(QString::fromStdWString(Localization::Instance().Get(L"reminders.quiet_hours", L"ساعات الهدوء")));
    auto* lQuiet = new QVBoxLayout(grpQuiet);
    bool hasQuiet = !Settings::Instance().GetCopy().quietPeriods.empty();
    m_chkQuietHours = new QCheckBox(QString::fromStdWString(Localization::Instance().Get(L"reminders.quiet_enable", L"تفعيل ساعات الهدوء (إيقاف مؤقت للتذكيرات ليلاً)")));
    m_chkQuietHours->setChecked(hasQuiet);

    int qStartH = 22, qStartM = 0, qEndH = 6, qEndM = 0;
    if (hasQuiet) {
        qStartH = Settings::Instance().GetCopy().quietPeriods[0].startHour;
        qStartM = Settings::Instance().GetCopy().quietPeriods[0].startMin;
        qEndH = Settings::Instance().GetCopy().quietPeriods[0].endHour;
        qEndM = Settings::Instance().GetCopy().quietPeriods[0].endMin;
    }

    auto* timeRow = new QHBoxLayout();
    timeRow->addWidget(new QLabel(QString::fromStdWString(Localization::Instance().Get(L"reminders.quiet_start", L"من:"))));
    m_timeQuietStart = new QTimeEdit(QTime(qStartH, qStartM));
    m_timeQuietStart->setLayoutDirection(Qt::LeftToRight);
    m_timeQuietStart->setDisplayFormat("HH:mm");
    m_timeQuietStart->setObjectName("QuietStart");
    m_timeQuietStart->setAccessibleName(Text(L"reminders.quiet_hours") + ", " + Text(L"reminders.start_time"));
    timeRow->addWidget(m_timeQuietStart);
    timeRow->addWidget(new QLabel(QString::fromStdWString(Localization::Instance().Get(L"reminders.quiet_end", L"إلى:"))));
    m_timeQuietEnd = new QTimeEdit(QTime(qEndH, qEndM));
    m_timeQuietEnd->setLayoutDirection(Qt::LeftToRight);
    m_timeQuietEnd->setDisplayFormat("HH:mm");
    m_timeQuietEnd->setObjectName("QuietEnd");
    m_timeQuietEnd->setAccessibleName(Text(L"reminders.quiet_hours") + ", " + Text(L"reminders.end_time"));
    timeRow->addWidget(m_timeQuietEnd);
    timeRow->addStretch();

    auto updateQuietHours = [this]() {
        const bool checked = m_chkQuietHours->isChecked();
        auto s = Settings::Instance().GetCopy();
        s.quietPeriods.clear();
        if (checked) {
            QuietPeriod qp;
            qp.startHour = m_timeQuietStart->time().hour();
            qp.startMin = m_timeQuietStart->time().minute();
            qp.endHour = m_timeQuietEnd->time().hour();
            qp.endMin = m_timeQuietEnd->time().minute();
            s.quietPeriods.push_back(qp);
        }
        Settings::Instance().Modify([&](AppSettings& settings) { settings.quietPeriods = s.quietPeriods; });
        SaveSettings(this);
        Scheduler::Instance().Recalculate();
    };
    connect(m_chkQuietHours, &QCheckBox::toggled, this, updateQuietHours);
    connect(m_timeQuietStart, &QTimeEdit::timeChanged, this, updateQuietHours);
    connect(m_timeQuietEnd, &QTimeEdit::timeChanged, this, updateQuietHours);

    lQuiet->addWidget(m_chkQuietHours);
    lQuiet->addLayout(timeRow);
    layout->addWidget(grpQuiet);

    // Audio & Fullscreen
    auto* grpAudio = new QGroupBox(QString::fromStdWString(Localization::Instance().Get(L"reminders.audio_section", L"الصوت وملء الشاشة")));
    auto* lAudio = new QVBoxLayout(grpAudio);

    m_chkSound = new QCheckBox(QString::fromStdWString(Localization::Instance().Get(L"reminders.sound_enable", L"تشغيل نغمة هادئة عند ظهور التذكير")));
    m_chkSound->setObjectName("SoundEnabled");
    m_chkSound->setChecked(Settings::Instance().GetCopy().soundEnabled);
    connect(m_chkSound, &QCheckBox::toggled, this, [this](bool checked) {
        Settings::Instance().Modify([=](AppSettings& settings) { settings.soundEnabled = checked; });
        SaveSettings(this);
    });

    auto* volRow = new QHBoxLayout();
    volRow->addWidget(new QLabel(QString::fromStdWString(Localization::Instance().Get(L"reminders.volume", L"مستوى الصوت:"))));
    m_sliderVolume = new QSlider(Qt::Horizontal);
    m_sliderVolume->setRange(0, 100);
    m_sliderVolume->setValue(Settings::Instance().GetCopy().soundVolume);
    connect(m_sliderVolume, &QSlider::valueChanged, this, [this](int val) {
        Settings::Instance().Modify([=](AppSettings& settings) { settings.soundVolume = val; });
        SaveSettings(this);
    });
    auto* btnTestAudio = new QPushButton(QString::fromStdWString(Localization::Instance().Get(L"reminders.test_audio", L"تجربة النغمة")));
    connect(btnTestAudio, &QPushButton::clicked, this, [this]() {
        if (m_audioEngine) m_audioEngine->PlayReminder();
    });

    volRow->addWidget(m_sliderVolume);
    volRow->addWidget(btnTestAudio);

    m_chkSuppressFullscreen = new QCheckBox(QString::fromStdWString(Localization::Instance().Get(L"reminders.fullscreen", L"إيقاف التذكيرات تلقائياً أثناء وضع ملء الشاشة (الألعاب والعروض)")));
    m_chkSuppressFullscreen->setChecked(Settings::Instance().GetCopy().suppressDuringFullscreen);
    const bool canDetectFullscreen = m_fsDetector && m_fsDetector->CheckFullScreen() != FullScreenState::Unsupported;
    m_chkSuppressFullscreen->setEnabled(canDetectFullscreen);
    if (!canDetectFullscreen) m_chkSuppressFullscreen->setToolTip(Text(L"qt.fullscreen.unsupported"));
    connect(m_chkSuppressFullscreen, &QCheckBox::toggled, this, [this](bool checked) {
        Settings::Instance().Modify([=](AppSettings& settings) { settings.suppressDuringFullscreen = checked; });
        SaveSettings(this);
    });

    lAudio->addWidget(m_chkSound);
    lAudio->addLayout(volRow);
    lAudio->addWidget(m_chkSuppressFullscreen);
    layout->addWidget(grpAudio);
    if (m_audioEngine && !m_audioEngine->IsAvailable()) {
        auto* warning = new QLabel(Text(L"qt.sound.unavailable"));
        warning->setWordWrap(true);
        layout->addWidget(warning);
    }

    layout->addStretch();
    scroll->setWidget(container);

    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->addWidget(scroll);
    return page;
}

} // namespace dhikr::qt
