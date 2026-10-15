#include "PopupWindow.h"
#include "ThemeManager.h"
#include "../../core/Scheduler.h"
#include "../../core/Database.h"
#include "../../core/Localization.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QGuiApplication>
#include <QScreen>
#include <QCursor>
#include <QClipboard>
#include <QToolTip>
#include <QEasingCurve>
#include <QStyle>

namespace dhikr::qt {

static constexpr int kPopupWidth  = 420;
static constexpr int kPopupHeight = 195;

PopupWindow::PopupWindow(QWidget* parent)
    : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    SetupUi();

    m_dismissTimer = new QTimer(this);
    connect(m_dismissTimer, &QTimer::timeout, this, &PopupWindow::OnTimerTick);

    connect(&ThemeManager::Instance(), &ThemeManager::ThemeChanged, this, &PopupWindow::OnThemeChanged);
}

PopupWindow::~PopupWindow() {
    StopActiveAnimations();
}

void PopupWindow::SetupUi() {
    resize(kPopupWidth, kPopupHeight);
    setMinimumSize(200, 140);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(18, 14, 18, 14);
    rootLayout->setSpacing(8);

    // ── Header: Badges & Controls ──
    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(8);

    m_lblCategory = new QLabel(this);
    m_lblCategory->setTextFormat(Qt::PlainText);
    m_lblCategory->setObjectName("CategoryBadge");
    m_lblCategory->setFont(ThemeManager::Instance().UiFont(9, true));

    m_lblSource = new QLabel(this);
    m_lblSource->setTextFormat(Qt::PlainText);
    m_lblSource->setFont(ThemeManager::Instance().UiFont(9, false));
    m_lblSource->setStyleSheet("color: #94A3B8;");

    headerLayout->addWidget(m_lblCategory);
    headerLayout->addWidget(m_lblSource);
    headerLayout->addStretch();

    // Copy Button
    m_btnCopy = new QPushButton(this);
    m_btnCopy->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    m_btnCopy->setAccessibleName(QString::fromStdWString(Localization::Instance().Get(L"popup.copy")));
    m_btnCopy->setFixedSize(28, 28);
    m_btnCopy->setToolTip(QString::fromStdWString(Localization::Instance().Get(L"popup.copy", L"نسخ الأذكار")));
    connect(m_btnCopy, &QPushButton::clicked, this, &PopupWindow::OnCopyClicked);
    headerLayout->addWidget(m_btnCopy);

    // Pause Button
    m_btnPause = new QPushButton(this);
    m_btnPause->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    m_btnPause->setAccessibleName(QString::fromStdWString(Localization::Instance().Get(L"popup.pause_15")));
    m_btnPause->setFixedSize(28, 28);
    m_btnPause->setToolTip(QString::fromStdWString(Localization::Instance().Get(L"popup.pause_15", L"إيقاف مؤقت (15 دقيقة)")));
    connect(m_btnPause, &QPushButton::clicked, this, &PopupWindow::OnPauseClicked);
    headerLayout->addWidget(m_btnPause);

    // Close Button
    m_btnClose = new QPushButton(this);
    m_btnClose->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
    m_btnClose->setAccessibleName(QString::fromStdWString(Localization::Instance().Get(L"popup.close")));
    m_btnClose->setFixedSize(28, 28);
    m_btnClose->setToolTip(QString::fromStdWString(Localization::Instance().Get(L"popup.close", L"إغلاق")));
    connect(m_btnClose, &QPushButton::clicked, this, &PopupWindow::Dismiss);
    headerLayout->addWidget(m_btnClose);

    rootLayout->addLayout(headerLayout);

    // ── Dhikr Arabic Text (Centered, Clean, No Counter Pill) ──
    m_lblArabic = new QLabel(this);
    m_lblArabic->setObjectName("DhikrArabic");
    m_lblArabic->setFont(ThemeManager::Instance().ArabicFont(17, true));
    m_lblArabic->setAlignment(Qt::AlignCenter);
    m_lblArabic->setWordWrap(true);
    m_lblArabic->setLayoutDirection(Qt::RightToLeft);
    m_lblArabic->setTextFormat(Qt::PlainText);
    m_lblArabic->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    m_bodyScroll = new QScrollArea(this);
    m_bodyScroll->setFrameShape(QFrame::NoFrame);
    m_bodyScroll->setWidgetResizable(true);
    m_bodyScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_bodyScroll->setWidget(m_lblArabic);
    rootLayout->addWidget(m_bodyScroll, 1);

    // ── Transliteration / Meaning ──
    m_lblTransliteration = new QLabel(this);
    m_lblTransliteration->setObjectName("DhikrTranslation");
    m_lblTransliteration->setFont(ThemeManager::Instance().UiFont(10, false));
    m_lblTransliteration->setAlignment(Qt::AlignCenter);
    m_lblTransliteration->setWordWrap(true);
    m_lblTransliteration->setTextFormat(Qt::PlainText);
    rootLayout->addWidget(m_lblTransliteration);

    // ── Bottom: Slim Auto-Dismiss Progress Bar Only ──
    m_progress = new QProgressBar(this);
    m_progress->setFixedHeight(4);
    m_progress->setTextVisible(false);
    m_progress->setStyleSheet(R"(
        QProgressBar { background: #283029; border: none; border-radius: 2px; }
        QProgressBar::chunk { background: #B7F34A; border-radius: 2px; }
    )");
    rootLayout->addWidget(m_progress);
}

void PopupWindow::StopActiveAnimations() {
    if (m_animGroup) {
        m_animGroup->disconnect(this);
        m_animGroup->stop();
        m_animGroup->deleteLater();
        m_animGroup.clear();
    }
    if (m_exitAnim) {
        m_exitAnim->disconnect(this);
        m_exitAnim->stop();
        m_exitAnim->deleteLater();
        m_exitAnim.clear();
    }
}

void PopupWindow::OnThemeChanged() {
    setStyleSheet(ThemeManager::Instance().GeneratePopupStyleSheet());
    QString primaryHex = ThemeManager::Instance().PrimaryColor().name();
    m_progress->setStyleSheet(QString(R"(
        QProgressBar { background: #283029; border: none; border-radius: 2px; }
        QProgressBar::chunk { background: %1; border-radius: 2px; }
    )").arg(primaryHex));
    update();
}

void PopupWindow::ShowReminder(const ContentItem& item, bool recordDelivery) {
    if (isVisible()) return;
    m_dismissTimer->stop();
    StopActiveAnimations();

    // Store by value so deleting custom dhikr while popup is open is 100% safe
    m_currentItem = item;
    m_isPausedByHover = false;

    const auto s = Settings::Instance().GetCopy();

    m_lblCategory->setText(QString::fromStdString(item.category));
    m_lblSource->setText(s.showSource ? QString::fromStdWString(item.source) : QString());
    auto text = QString::fromStdWString(item.text);
    if (!s.showTashkeel) {
        QString plain;
        for (auto character : text) {
            auto value = character.unicode();
            if (!(value >= 0x064B && value <= 0x065F) && value != 0x0670) plain += character;
        }
        text = plain;
    }
    m_lblArabic->setText(text);
    m_lblArabic->setFont(ThemeManager::Instance().ArabicFont(s.popupFontSize == PopupFontSize::Large ? 22 : s.popupFontSize == PopupFontSize::Small ? 14 : 17, true));

    if (s.showSource && !item.transliteration.empty()) {
        m_lblTransliteration->setText(QString::fromStdWString(item.transliteration));
        m_lblTransliteration->setVisible(true);
    } else if (s.showSource && !item.description.empty()) {
        m_lblTransliteration->setText(QString::fromStdWString(item.description));
        m_lblTransliteration->setVisible(true);
    } else {
        m_lblTransliteration->setVisible(false);
    }

    OnThemeChanged();

    m_totalDurationSec = s.popupDurationSec;
    if (m_totalDurationSec <= 0) m_totalDurationSec = 5;
    m_elapsedSec = 0;
    m_elapsedTimer.start();
    m_hoverDuration = 0;
    m_progress->setRange(0, m_totalDurationSec * 10);
    m_progress->setValue(m_totalDurationSec * 10);

    PositionWindow();
    setWindowOpacity(1.0);
    show();
    raise();
    StartEntranceAnimation();

    m_dismissTimer->start(100);

    // Record delivery in database
    if (recordDelivery) Database::Instance().InsertReminderEvent(item.id, item.category, "delivered");
}

void PopupWindow::PositionWindow() {
    const auto s = Settings::Instance().GetCopy();
    QScreen* screen = m_screen;
    if (!screen && s.popupMonitor == PopupMonitor::Active) {
        screen = QGuiApplication::screenAt(QCursor::pos());
    } else if (!screen && s.popupMonitor == PopupMonitor::Specific) {
        auto screens = QGuiApplication::screens();
        if (s.specificMonitor >= 0 && s.specificMonitor < screens.size()) {
            screen = screens[s.specificMonitor];
        }
    }
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }

    int w = screen ? std::max(200, std::min(kPopupWidth, screen->availableGeometry().width() - 40)) : kPopupWidth;
    m_lblArabic->setMinimumHeight(m_lblArabic->fontMetrics().boundingRect(QRect(0, 0, w - 54, 100000), Qt::TextWordWrap, m_lblArabic->text()).height() + 8);
    int h = std::max(kPopupHeight, m_lblArabic->minimumHeight() + 96);
    if (screen) h = std::min(h, std::max(140, screen->availableGeometry().height() / 2));

    if (screen) {
        QRect avail = screen->availableGeometry();
        int margin = 20;
        int x = avail.left() + (avail.width() - w) / 2;
        int y = avail.top() + margin;

        switch (s.popupPosition) {
            case PopupPosition::TopLeft:      x = avail.left() + margin;                    y = avail.top() + margin; break;
            case PopupPosition::TopCenter:    x = avail.left() + (avail.width() - w) / 2;   y = avail.top() + margin; break;
            case PopupPosition::TopRight:     x = avail.right() - w - margin;               y = avail.top() + margin; break;
            case PopupPosition::CenterLeft:   x = avail.left() + margin;                    y = avail.top() + (avail.height() - h) / 2; break;
            case PopupPosition::Center:       x = avail.left() + (avail.width() - w) / 2;   y = avail.top() + (avail.height() - h) / 2; break;
            case PopupPosition::CenterRight:  x = avail.right() - w - margin;               y = avail.top() + (avail.height() - h) / 2; break;
            case PopupPosition::BottomLeft:   x = avail.left() + margin;                    y = avail.bottom() - h - margin; break;
            case PopupPosition::BottomCenter: x = avail.left() + (avail.width() - w) / 2;   y = avail.bottom() - h - margin; break;
            case PopupPosition::BottomRight:  x = avail.right() - w - margin;               y = avail.bottom() - h - margin; break;
            default:                          x = avail.left() + (avail.width() - w) / 2;   y = avail.top() + margin; break;
        }

        m_targetGeometry = QRect(x, y, w, h);
        setGeometry(m_targetGeometry);
    } else {
        m_targetGeometry = QRect(100, 40, w, h);
        setGeometry(m_targetGeometry);
    }
}

int PopupWindow::GetAnimationDurationMs() const {
    switch (Settings::Instance().GetCopy().animationSpeed) {
        case AnimationSpeed::Snappy:    return 250;
        case AnimationSpeed::Cinematic: return 550;
        case AnimationSpeed::Normal:
        default:                        return 380;
    }
}

void PopupWindow::StartEntranceAnimation() {
    const auto s = Settings::Instance().GetCopy();
    if (!s.animationEnabled) {
        setGeometry(m_targetGeometry);
        setWindowOpacity(1.0);
        return;
    }

    int duration = GetAnimationDurationMs();
    PopupAnimation animType = s.popupAnimation;

    m_animGroup = new QParallelAnimationGroup(this);

    auto* animOpacity = new QPropertyAnimation(this, "windowOpacity", m_animGroup);
    animOpacity->setDuration(duration);
    animOpacity->setStartValue(0.0);
    animOpacity->setEndValue(1.0);
    animOpacity->setEasingCurve(QEasingCurve::OutCubic);
    m_animGroup->addAnimation(animOpacity);

    QRect targetGeo = m_targetGeometry;
    QRect startGeo = targetGeo;
    bool isTop = (s.popupPosition == PopupPosition::TopLeft ||
                  s.popupPosition == PopupPosition::TopCenter ||
                  s.popupPosition == PopupPosition::TopRight);
    int dirY = isTop ? -1 : 1;

    auto* animGeo = new QPropertyAnimation(this, "geometry", m_animGroup);
    animGeo->setDuration(duration);
    animGeo->setEndValue(targetGeo);

    switch (animType) {
        case PopupAnimation::SpringBloom:
            startGeo.adjust(24, 18, -24, -18);
            startGeo.translate(0, dirY * 16);
            animGeo->setDuration(duration + 80);
            animGeo->setEasingCurve(QEasingCurve::OutBack);
            break;

        case PopupAnimation::HorizontalDrift: {
            bool isLeft = (s.popupPosition == PopupPosition::TopLeft ||
                           s.popupPosition == PopupPosition::CenterLeft ||
                           s.popupPosition == PopupPosition::BottomLeft);
            startGeo.translate(isLeft ? -65 : 65, 0);
            animGeo->setEasingCurve(QEasingCurve::OutQuint);
            break;
        }

        case PopupAnimation::LiquidFloat:
            startGeo.translate(0, dirY * 32);
            animGeo->setEasingCurve(QEasingCurve::OutElastic);
            animGeo->setDuration(duration + 120);
            break;

        case PopupAnimation::OrigamiUnfurl:
            startGeo.adjust(10, 45, -10, -45);
            animGeo->setEasingCurve(QEasingCurve::OutQuart);
            break;

        case PopupAnimation::EtherealSweep:
            startGeo.adjust(12, 8, -12, -8);
            startGeo.translate(0, dirY * 18);
            animGeo->setEasingCurve(QEasingCurve::OutCubic);
            break;

        case PopupAnimation::ZenBreathing:
            startGeo.adjust(20, 14, -20, -14);
            animGeo->setDuration(duration + 100);
            animGeo->setEasingCurve(QEasingCurve::InOutSine);
            break;

        case PopupAnimation::RippleGenesis:
            startGeo.adjust(36, 26, -36, -26);
            animGeo->setDuration(duration + 90);
            animGeo->setEasingCurve(QEasingCurve::OutBack);
            break;

        case PopupAnimation::Classic:
        default:
            startGeo.translate(0, dirY * 24);
            animGeo->setEasingCurve(QEasingCurve::OutCubic);
            break;
    }

    setGeometry(startGeo);
    setWindowOpacity(0.0);
    animGeo->setStartValue(startGeo);
    m_animGroup->addAnimation(animGeo);
    m_animGroup->start(QAbstractAnimation::DeleteWhenStopped);
}

void PopupWindow::Dismiss() {
    m_dismissTimer->stop();
    StartExitAnimation();
}

void PopupWindow::StartExitAnimation() {
    StopActiveAnimations();

    if (!Settings::Instance().GetCopy().animationEnabled) {
        hide();
        setWindowOpacity(1.0);
        return;
    }

    int duration = std::max(120, GetAnimationDurationMs() / 2);
    m_exitAnim = new QPropertyAnimation(this, "windowOpacity", this);
    m_exitAnim->setDuration(duration);
    m_exitAnim->setStartValue(windowOpacity());
    m_exitAnim->setEndValue(0.0);
    m_exitAnim->setEasingCurve(QEasingCurve::InQuad);

    connect(m_exitAnim, &QPropertyAnimation::finished, this, [this]() {
        hide();
        setWindowOpacity(1.0);
    });

    m_exitAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void PopupWindow::OnTimerTick() {
    if (m_isPausedByHover) return;

    m_elapsedSec = static_cast<int>((m_elapsedTimer.elapsed() - m_hoverDuration) / 100);
    int remaining = (m_totalDurationSec * 10) - m_elapsedSec;
    if (remaining <= 0) {
        Dismiss();
    } else {
        m_progress->setValue(remaining);
    }
}

void PopupWindow::OnCopyClicked() {
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(QString::fromStdWString(m_currentItem.text));
    QToolTip::showText(m_btnCopy->mapToGlobal(QPoint(0, -25)),
                       QString::fromStdWString(Localization::Instance().Get(L"popup.copied", L"تم النسخ ✓")),
                       m_btnCopy);
}

void PopupWindow::OnPauseClicked() {
    Scheduler::Instance().Pause(15);
    Dismiss();
}

void PopupWindow::enterEvent(QEnterEvent* event) {
    QWidget::enterEvent(event);
    m_isPausedByHover = true;
    m_hoverStarted = m_elapsedTimer.isValid() ? m_elapsedTimer.elapsed() : 0;
}

void PopupWindow::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    m_isPausedByHover = false;
    if (m_elapsedTimer.isValid()) m_hoverDuration += m_elapsedTimer.elapsed() - m_hoverStarted;
}

void PopupWindow::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF r = rect().adjusted(1, 1, -1, -1);
    QPainterPath path;
    path.addRoundedRect(r, 16, 16);

    // Translucent dark glass background
    painter.fillPath(path, ThemeManager::Instance().PopupBackgroundColor());

    // Highlight border with subtle accent tint
    QPen pen(ThemeManager::Instance().PopupBorderColor(), 1.5);
    painter.setPen(pen);
    painter.drawPath(path);
}

} // namespace dhikr::qt
