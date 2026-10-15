#include "MainWindow.h"
#include "UiText.h"
#include "ThemeManager.h"
#include "../../core/ContentEngine.h"
#include "../../core/Scheduler.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QScrollArea>
#include <QGroupBox>
#include <QSignalBlocker>
#include <QGuiApplication>
#include <QScreen>

namespace dhikr::qt {
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
    m_comboTheme->addItems({Text(L"qt.color.green"), Text(L"qt.color.yellow"), Text(L"qt.color.blue"), Text(L"qt.color.red"), Text(L"qt.color.silver"), Text(L"qt.color.gold")});
    m_comboTheme->setCurrentIndex(std::clamp((int)Settings::Instance().GetCopy().colorTheme, 0, 5));

    connect(m_comboTheme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        Settings::Instance().Modify([=](AppSettings& settings) { settings.colorTheme = (ColorTheme)index; });
        SaveSettings(this);
        ThemeManager::Instance().RefreshTheme();
    });
    lTheme->addWidget(m_comboTheme);
    lTheme->addStretch();
    layout->addWidget(grpTheme);

    auto* popup = new QGroupBox(Text(L"settings.popup"));
    auto* form = new QFormLayout(popup);
    const auto settings = Settings::Instance().GetCopy();
    auto* position = new QComboBox();
    const wchar_t* positions[] = {L"pos.top_left", L"pos.top_center", L"pos.top_right", L"pos.center_left", L"pos.center", L"pos.center_right", L"pos.bottom_left", L"pos.bottom_center", L"pos.bottom_right"};
    for (auto key : positions) position->addItem(Text(key));
    position->setCurrentIndex(static_cast<int>(settings.popupPosition));
    connect(position, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        Settings::Instance().Modify([index](AppSettings& value) { value.popupPosition = static_cast<PopupPosition>(index); });
        SaveSettings(this);
    });
    form->addRow(Text(L"settings.position"), position);
    auto* monitor = new QComboBox();
    monitor->addItem(Text(L"settings.monitor_primary"), static_cast<int>(PopupMonitor::Primary));
    monitor->addItem(Text(L"settings.monitor_active"), static_cast<int>(PopupMonitor::Active));
    monitor->addItem(Text(L"settings.monitor_all"), static_cast<int>(PopupMonitor::All));
    const auto screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) monitor->addItem(screens[i]->name(), 3 + i);
    monitor->setCurrentIndex(settings.popupMonitor == PopupMonitor::Specific ? std::min(3 + settings.specificMonitor, monitor->count() - 1) : static_cast<int>(settings.popupMonitor));
    connect(monitor, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, monitor](int) {
        const int choice = monitor->currentData().toInt();
        Settings::Instance().Modify([choice](AppSettings& value) {
            value.popupMonitor = choice < 3 ? static_cast<PopupMonitor>(choice) : PopupMonitor::Specific;
            value.specificMonitor = std::max(0, choice - 3);
        });
        SaveSettings(this);
    });
    form->addRow(Text(L"settings.monitor"), monitor);
    auto* font = new QComboBox();
    font->addItems({Text(L"font.small"), Text(L"font.medium"), Text(L"font.large")});
    font->setCurrentIndex(static_cast<int>(settings.popupFontSize));
    connect(font, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        Settings::Instance().Modify([index](AppSettings& value) { value.popupFontSize = static_cast<PopupFontSize>(index); });
        SaveSettings(this);
    });
    form->addRow(Text(L"settings.font_size"), font);
    auto checkbox = [this, form](const wchar_t* key, bool AppSettings::*field) {
        auto* control = new QCheckBox(Text(key));
        control->setChecked(Settings::Instance().GetCopy().*field);
        connect(control, &QCheckBox::toggled, this, [this, field](bool checked) {
            Settings::Instance().Modify([field, checked](AppSettings& value) { value.*field = checked; });
            SaveSettings(this);
        });
        form->addRow(control);
    };
    checkbox(L"settings.show_tashkeel", &AppSettings::showTashkeel);
    checkbox(L"settings.show_source", &AppSettings::showSource);
    checkbox(L"settings.animation_enabled", &AppSettings::animationEnabled);
    layout->addWidget(popup);

    // Animations (Aligned 1-to-1 with PopupAnimation & AnimationSpeed enums)
    auto* grpAnim = new QGroupBox(QString::fromStdWString(Localization::Instance().Get(L"settings.animation_group", L"أنماط الحركة والأنيميشن")));
    auto* lAnim = new QVBoxLayout(grpAnim);

    auto* animRow1 = new QHBoxLayout();
    animRow1->addWidget(new QLabel(QString::fromStdWString(Localization::Instance().Get(L"settings.anim_style", L"نمط ظهور التذكير:"))));
    m_comboAnimation = new QComboBox();
    m_comboAnimation->addItems({Text(L"qt.anim.slide"), Text(L"qt.anim.spring"), Text(L"qt.anim.side"), Text(L"qt.anim.float"), Text(L"qt.anim.unfold"), Text(L"qt.anim.fade"), Text(L"qt.anim.expand"), Text(L"qt.anim.ripple")});
    m_comboAnimation->setCurrentIndex(std::clamp((int)Settings::Instance().GetCopy().popupAnimation, 0, 7));
    connect(m_comboAnimation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        Settings::Instance().Modify([=](AppSettings& settings) { settings.popupAnimation = (PopupAnimation)index; });
        SaveSettings(this);
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
    m_comboAnimSpeed->addItems({Text(L"qt.speed.slow"), Text(L"qt.speed.normal"), Text(L"qt.speed.fast")});
    m_comboAnimSpeed->setCurrentIndex(std::clamp((int)Settings::Instance().GetCopy().animationSpeed, 0, 2));
    connect(m_comboAnimSpeed, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        Settings::Instance().Modify([=](AppSettings& settings) { settings.animationSpeed = (AnimationSpeed)index; });
        SaveSettings(this);
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
    m_sliderDuration->setValue(Settings::Instance().GetCopy().popupDurationSec);
    m_lblDurationValue = new QLabel(Text(L"qt.seconds").arg(Settings::Instance().GetCopy().popupDurationSec));

    connect(m_sliderDuration, &QSlider::valueChanged, this, [this](int val) {
        Settings::Instance().Modify([=](AppSettings& settings) { settings.popupDurationSec = val; });
        m_lblDurationValue->setText(Text(L"qt.seconds").arg(val));
        SaveSettings(this);
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
            if (!m_autoStart->SetAutoStartEnabled(checked)) {
                QSignalBlocker blocker(m_chkAutostart);
                m_chkAutostart->setChecked(m_autoStart->IsAutoStartEnabled());
                QMessageBox::warning(this, "Azkar Reminder", Text(L"qt.save_failed"));
            }
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

} // namespace dhikr::qt
