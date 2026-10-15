#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <functional>
#include <filesystem>
#include <cstdint>

namespace dhikr {

// ── Notification position ────────────────────────────────────────────────────
enum class PopupPosition {
    TopLeft = 0, TopCenter, TopRight,
    CenterLeft, Center, CenterRight,
    BottomLeft, BottomCenter, BottomRight
};

// ── Popup monitor target ────────────────────────────────────────────────────
enum class PopupMonitor { Primary = 0, Active = 1, All = 2, Specific = 3 };

// ── Popup Animation Style ────────────────────────────────────────────────────
enum class PopupAnimation {
    Classic = 0,         // انسيابي كلاسيكي (Classic Spring Slide)
    SpringBloom = 1,     // انبثاق مرن ثلاثي الأبعاد (Spring Bloom)
    HorizontalDrift = 2, // انزلاق أفقي هادئ (Horizontal Drift)
    LiquidFloat = 3,     // طفو مائي انسيابي (Liquid Float)
    OrigamiUnfurl = 4,   // انبساط هادئ كالطيات (Origami Unfurl)
    Origami = 4,
    EtherealSweep = 5,   // همس النور / التدفق الأثيري (Ethereal Aura Sweep)
    ZenBreathing = 6,    // نبض السكينة / التنفس الهادئ (Zen Breathing Ease)
    RippleGenesis = 7    // تموّج الواحة (Ripple Genesis)
};

// ── Animation Speed ──────────────────────────────────────────────────────────
enum class AnimationSpeed {
    Cinematic = 0, // هادئ ومتمهل (Cinematic Pace - 1.35x)
    Normal = 1,    // قياسي متزن (Standard Pace - 1.0x)
    Snappy = 2,    // سريع وخاطف (Snappy Pace - 0.65x)
    Smooth = 0,
    Balanced = 1
};

// ── Popup Font Size Scale ────────────────────────────────────────────────────
enum class PopupFontSize {
    Small = 0,  // صغير مدمج (Compact)
    Medium = 1, // قياسي (Standard - Default)
    Large = 2   // كبير وواضح (Spacious / Large)
};

// ── Popup Surface Material ───────────────────────────────────────────────────
enum class PopupSurfaceStyle {
    FrostedAcrylic = 0, // زجاج متجمد سائل (Frosted Acrylic)
    DeepObsidian = 1,   // أوبسيديان داكن معتم (Deep Obsidian)
    AuroraGlow = 2      // هالة الأورورا المتوهجة (Aurora Glow)
};

// ── Popup Corner Radius Style ────────────────────────────────────────────────
enum class PopupCornerStyle {
    Rounded = 0,  // انسيابي متزن (14px)
    Squircle = 1, // حديث مدمج (8px)
    Capsule = 2   // كبسولي دائري (24px)
};

// ── Sound Library Preset ─────────────────────────────────────────────────────
enum class SoundPreset {
    Chime = 0,        // رنين بلوري (Crystal Chime - Default)
    WaterDrop = 1,    // قطرة ماء هادئة (Pristine Water Droplet)
    AmbientEcho = 2,  // صدى ناي دافئ (Warm Ambient Echo)
    WoodTap = 3,      // نقرة خشبية خافتة (Gentle Wood Tap)
    Custom = 4        // ملف صوتي مخصص (Custom Audio File)
};

// ── Day schedule entry ───────────────────────────────────────────────────────
struct DaySchedule {
    bool    enabled   = true;
    int     startHour = 8,  startMin = 0;
    int     endHour   = 23, endMin   = 0;
    int     frequencyMinutes = 30;
};

// ── Quiet hours entry ────────────────────────────────────────────────────────
struct QuietPeriod {
    int startHour = 12, startMin = 0;
    int endHour   = 14, endMin   = 0;
};

// ── Color Theme (Brand & Accent) ─────────────────────────────────────────────
enum class ColorTheme {
    Green  = 0, // Emerald Green (Default)
    Yellow = 1, // Radiant Amber Gold (Neon Yellow)
    Blue   = 2, // Electric Cyan Sky
    Red    = 3, // Crimson Ruby
    Silver = 4, // Brushed Platinum Silver
    Gold   = 5  // Luxury Brushed Gold
};

// ── Accent color preset ──────────────────────────────────────────────────────
struct Color {
    uint8_t r, g, b;
    Color(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
};

// ── Application settings ─────────────────────────────────────────────────────
struct AppSettings {
    // General
    std::wstring language       = L"en";  // "en", "ar", "system"
    bool         startWithWindows = true;
    std::wstring theme          = L"dark"; // "dark", "light", "system"
    ColorTheme   colorTheme     = ColorTheme::Green;
    Color        accentColor    = { 0xB7, 0xF3, 0x4A };

    // Reminders
    bool         remindersEnabled = true;
    DaySchedule  weeklySchedule[7]; // 0=Sunday
    std::vector<QuietPeriod> quietPeriods;

    // Popup Appearance & Display
    PopupPosition popupPosition       = PopupPosition::TopCenter;
    PopupMonitor  popupMonitor        = PopupMonitor::Primary;
    int           specificMonitor     = 0;
    int           popupDurationSec    = 5;
    bool          animationEnabled    = true;
    PopupAnimation popupAnimation     = PopupAnimation::Classic;
    AnimationSpeed animationSpeed     = AnimationSpeed::Normal;
    PopupSurfaceStyle popupSurface    = PopupSurfaceStyle::FrostedAcrylic;
    PopupCornerStyle  popupCorner     = PopupCornerStyle::Rounded;
    bool          arabesqueOverlay    = true;

    // Typography & Content
    PopupFontSize popupFontSize       = PopupFontSize::Medium;
    bool          showTashkeel        = true;
    bool          showCounterPill     = false;
    bool          showSource          = true;

    // Sound
    bool          soundEnabled        = true;
    SoundPreset   soundPreset         = SoundPreset::Chime;
    int           soundVolume         = 50; // 0–100 (Default 50%)
    std::wstring  customSoundPath     = L""; // Empty = default chime

    // Full-Screen & Distraction Awareness
    bool          suppressDuringFullscreen  = true;
    int           fullscreenSuppressionMode = 0; // 0 = Queue until exit, 1 = Suppress completely, 2 = Windows notification only

    // Privacy
    std::wstring  installationId   = L"";  // anonymous UUID, generated once

    // Internal
    bool          firstLaunch       = true;
    bool          paused            = false;
    int64_t       pauseUntilUnix    = 0;      // 0 = not paused
    long long     lastUpdateCheck   = 0;      // Unix timestamp

    // Update checker — persisted so we can show the prompt after restart
    std::wstring  pendingUpdateVersion = L""; // e.g. L"1.1.0", empty = no pending
    std::wstring  pendingUpdateUrl     = L""; // HTTPS URL to DhikrReminderSetup.exe
};

// ── Settings manager ─────────────────────────────────────────────────────────
class Settings {
public:
    static Settings& Instance();

    // Load from file; creates defaults if not found
    bool Load(const std::filesystem::path& appDataPath);

    // Persist immediately
    bool Save();
    bool BackupInvalidFile();

    // Thread-safe settings access
    AppSettings GetCopy() const;
    void Update(const AppSettings& newSettings);
    void UpdatePreferences(const AppSettings& newSettings);
    void Modify(const std::function<void(AppSettings&)>& change);
    static void Validate(AppSettings& settings);

    // Change notification
    using ChangeCallback = std::function<void()>;
    void OnChanged(ChangeCallback cb) { std::lock_guard<std::mutex> lock(m_mutex); m_changeCallbacks.push_back(std::move(cb)); }
    void NotifyChanged();

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

private:
    Settings() = default;

    std::filesystem::path       m_filePath;
    AppSettings                 m_settings;
    mutable std::mutex          m_mutex;
    std::vector<ChangeCallback> m_changeCallbacks;
};

} // namespace dhikr
