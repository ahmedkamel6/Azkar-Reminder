#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QTimer>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QPointer>
#include <QElapsedTimer>
#include <QScrollArea>
#include <QScreen>
#include "../../core/ContentEngine.h"
#include "../../core/Settings.h"

namespace dhikr::qt {

class PopupWindow : public QWidget {
    Q_OBJECT

public:
    explicit PopupWindow(QWidget* parent = nullptr);
    ~PopupWindow() override;

    // Show reminder popup with specified content item
    void ShowReminder(const ContentItem& item, bool recordDelivery = true);
    void SetTargetScreen(QScreen* screen) { m_screen = screen; }

    // Dismiss with exit animation
    void Dismiss();

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void OnTimerTick();
    void OnCopyClicked();
    void OnPauseClicked();
    void OnThemeChanged();

private:
    void SetupUi();
    void StopActiveAnimations();
    void PositionWindow();
    void StartEntranceAnimation();
    void StartExitAnimation();
    int GetAnimationDurationMs() const;

    // UI elements
    QLabel*       m_lblCategory = nullptr;
    QLabel*       m_lblSource = nullptr;
    QLabel*       m_lblArabic = nullptr;
    QLabel*       m_lblTransliteration = nullptr;
    QPushButton*  m_btnCopy = nullptr;
    QPushButton*  m_btnPause = nullptr;
    QPushButton*  m_btnClose = nullptr;
    QProgressBar* m_progress = nullptr;
    QScrollArea* m_bodyScroll = nullptr;
    QPointer<QScreen> m_screen;
    QElapsedTimer m_elapsedTimer;
    qint64 m_hoverStarted = 0;
    qint64 m_hoverDuration = 0;

    // Animation & State
    QPointer<QParallelAnimationGroup> m_animGroup;
    QPointer<QPropertyAnimation>      m_exitAnim;
    QRect                             m_targetGeometry;

    QTimer* m_dismissTimer = nullptr;
    int     m_elapsedSec = 0;
    int     m_totalDurationSec = 15;
    bool    m_isPausedByHover = false;

    // Current item data (stored by value to prevent dangling pointers if deleted)
    ContentItem m_currentItem;
};

} // namespace dhikr::qt
