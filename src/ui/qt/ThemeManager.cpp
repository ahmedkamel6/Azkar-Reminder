#include "ThemeManager.h"
#include <QFontDatabase>
#include <QFontInfo>
#include <QApplication>
#include <ctime>
#include "../../core/Localization.h"

namespace dhikr::qt {

ThemeManager& ThemeManager::Instance() {
    static ThemeManager instance;
    return instance;
}

ThemeManager::ThemeManager() {
}

bool ThemeManager::IsAppPaused() const {
    const auto s = Settings::Instance().GetCopy();
    if (!s.remindersEnabled || s.paused) return true;
    if (s.pauseUntilUnix > 0 && time(nullptr) < s.pauseUntilUnix) return true;
    return false;
}

ColorTheme ThemeManager::CurrentTheme() const {
    return Settings::Instance().GetCopy().colorTheme;
}

QColor ThemeManager::GetThemePrimary(ColorTheme theme, qreal alpha) const {
    int a = static_cast<int>(alpha * 255.0);
    switch (theme) {
        case ColorTheme::Yellow: return QColor(0xFA, 0xDC, 0x26, a); // Radiant Solar Yellow (#FADC26)
        case ColorTheme::Blue:   return QColor(0x38, 0xBD, 0xF8, a); // Electric Cyan Sky (#38BDF8)
        case ColorTheme::Red:    return QColor(0xF8, 0x71, 0x71, a); // Crimson Ruby (#F87171)
        case ColorTheme::Silver: return QColor(0xE2, 0xE8, 0xF0, a); // Platinum Brushed Silver (#E2E8F0)
        case ColorTheme::Gold:   return QColor(0xD4, 0xAF, 0x37, a); // Regal Antique Metallic Gold (#D4AF37)
        case ColorTheme::Green:
        default:                 return QColor(0xB7, 0xF3, 0x4A, a); // Radiant Emerald Lime (#B7F34A)
    }
}

QColor ThemeManager::PrimaryColor(qreal alpha) const {
    if (IsAppPaused()) {
        int a = static_cast<int>(alpha * 255.0);
        return QColor(0x8E, 0x99, 0x91, a); // Sleek slate gray (#8E9991)
    }
    return GetThemePrimary(CurrentTheme(), alpha);
}

QColor ThemeManager::PrimaryDarkColor(qreal alpha) const {
    QColor p = PrimaryColor(alpha);
    return QColor(static_cast<int>(p.red() * 0.7),
                  static_cast<int>(p.green() * 0.7),
                  static_cast<int>(p.blue() * 0.7),
                  p.alpha());
}

QColor ThemeManager::BackgroundColor() const   { return QColor(0x08, 0x0A, 0x09); }
QColor ThemeManager::SidebarColor() const      { return QColor(0x10, 0x13, 0x10); }
QColor ThemeManager::SurfaceColor() const      { return QColor(0x15, 0x19, 0x15); }
QColor ThemeManager::CardColor() const         { return QColor(0x15, 0x19, 0x15); }
QColor ThemeManager::CardHoverColor() const    { return QColor(0x1B, 0x22, 0x1C); }
QColor ThemeManager::BorderColor() const       { return QColor(0x28, 0x30, 0x29); }
QColor ThemeManager::BorderSubtleColor() const { return QColor(0x1E, 0x25, 0x20); }

QColor ThemeManager::TextPrimaryColor() const   { return QColor(0xFD, 0xFE, 0xFD); }
QColor ThemeManager::TextSecondaryColor() const { return QColor(0xCB, 0xD5, 0xE1); }
QColor ThemeManager::TextMutedColor() const     { return QColor(0x94, 0xA3, 0xB8); }

QColor ThemeManager::PopupBackgroundColor() const { return QColor(0x0F, 0x14, 0x10, 250); }
QColor ThemeManager::PopupBorderColor() const     { return QColor(0x24, 0x2F, 0x26); }

QFont ThemeManager::ArabicFont(int pointSize, bool bold) const {
    QFont font("Cairo", pointSize);
    if (!QFontInfo(font).exactMatch()) {
        font.setFamily("Amiri");
        if (!QFontInfo(font).exactMatch()) {
            font.setFamily("Arial");
        }
    }
    font.setBold(bold);
    return font;
}

QFont ThemeManager::UiFont(int pointSize, bool bold) const {
    QFont font = Localization::Instance().IsRTL() ? ArabicFont(pointSize, bold) : QApplication::font();
    font.setPointSize(pointSize);
    font.setBold(bold);
    return font;
}

void ThemeManager::RefreshTheme() {
    emit ThemeChanged();
}

QString ThemeManager::GenerateApplicationStyleSheet() const {
    QString primaryHex = PrimaryColor().name();
    QString primaryDarkHex = PrimaryDarkColor().name();

    return QString(R"(
        QWidget {
            background-color: #080A09;
            color: #FDFEFD;
            font-family: 'Cairo', 'Segoe UI', 'Ubuntu', sans-serif;
            font-size: 13px;
        }
        QMainWindow, QDialog {
            background-color: #080A09;
        }
        QLabel#Countdown { font-size: 28px; font-weight: bold; }
        QLabel#StatValue { font-size: 22px; font-weight: bold; }
        QLabel, QCheckBox, QGroupBox { background-color: transparent; }
        QListWidget#Sidebar {
            background-color: #101310;
            border-right: 1px solid #1E2520;
            outline: none;
            border: none;
        }
        QListWidget#Sidebar::item { padding: 10px; border-left: 3px solid transparent; }
        QListWidget#Sidebar::item:selected { color: %1; background: #1B221C; border-left: 3px solid %1; }
        QListWidget#Sidebar::item:focus { border: 1px solid %1; }
        QFrame#Card {
            background-color: #151915;
            border: 1px solid #283029;
            border-radius: 6px;
        }
        QFrame#Card:hover {
            border: 1px solid %1;
            background-color: #1B221C;
        }
        QPushButton {
            background-color: #1A211B;
            color: #FDFEFD;
            border: 1px solid #283029;
            border-radius: 8px;
            padding: 8px 16px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #242E26;
            border: 1px solid %1;
        }
        QPushButton:pressed {
            background-color: #161D17;
        }
        QPushButton#PrimaryButton {
            background-color: %1;
            color: #080A09;
            border: none;
            font-weight: bold;
        }
        QPushButton#PrimaryButton:hover {
            background-color: %2;
        }
        QScrollBar:vertical {
            background: #080A09;
            width: 8px;
            margin: 0;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background: #283029;
            min-height: 24px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: %1;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QLineEdit, QTextEdit, QPlainTextEdit {
            background-color: #101310;
            border: 1px solid #283029;
            border-radius: 8px;
            padding: 8px;
            color: #FDFEFD;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
            border: 1px solid %1;
        }
        QComboBox {
            background-color: #101310;
            border: 1px solid #283029;
            border-radius: 8px;
            padding: 6px 12px;
            color: #FDFEFD;
        }
        QComboBox:hover {
            border: 1px solid %1;
        }
        QComboBox::drop-down {
            border: none;
        }
        QSlider::groove:horizontal {
            border: none;
            height: 4px;
            background: #283029;
            border-radius: 2px;
        }
        QSlider::sub-page:horizontal {
            background: %1;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #FDFEFD;
            border: 2px solid %1;
            width: 14px;
            margin-top: -5px;
            margin-bottom: -5px;
            border-radius: 7px;
        }
    )").arg(primaryHex, primaryDarkHex);
}

QString ThemeManager::GeneratePopupStyleSheet() const {
    QString primaryHex = PrimaryColor().name();
    return QString(R"(
        QWidget#PopupBackground {
            background-color: rgba(15, 20, 16, 250);
            border: 1px solid #283029;
            border-radius: 16px;
        }
        QLabel#DhikrArabic {
            color: #FDFEFD;
            font-weight: bold;
        }
        QLabel#DhikrTranslation {
            color: #CBD5E1;
            font-size: 13px;
        }
        QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }
        QLabel { background: transparent; }
        QPushButton { padding: 2px; }
        QLabel#CategoryBadge {
            background-color: rgba(183, 243, 74, 25);
            color: %1;
            border: 1px solid %1;
            border-radius: 10px;
            padding: 2px 8px;
            font-size: 11px;
            font-weight: bold;
        }
    )").arg(primaryHex);
}

} // namespace dhikr::qt
