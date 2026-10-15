#include "Localization.h"
#include <unordered_map>

namespace dhikr {

// Static string maps for fast in-memory access
static std::unordered_map<std::wstring, std::wstring> g_en = {
    // Navigation
    { L"nav.home", L"Home" },
    { L"nav.reminders", L"Reminders & Schedule" },
    { L"nav.schedule", L"Schedule" },
    { L"nav.library", L"Dhikr Library" },
    { L"nav.statistics", L"Statistics" },
    { L"nav.settings", L"Settings" },
    { L"nav.about", L"About" },

    // Home
    { L"home.greeting", L"Peace be upon you" },
    { L"home.subtitle", L"Make the remembrance of Allah a part of your day" },
    { L"home.status", L"Status:" },
    { L"home.active", L"Active" },
    { L"home.paused", L"Paused" },
    { L"home.reminders_active", L"Reminders Active" },
    { L"home.reminders_paused", L"Reminders Paused" },
    { L"home.next_reminder", L"Next Reminder" },
    { L"home.remaining_time", L"Remaining" },
    { L"home.time_for_dhikr", L"Time for Dhikr" },
    { L"home.mark_done", L"Mark as Done ✓" },
    { L"home.snooze_5m", L"Snooze 5m ⏱" },
    { L"home.browse_azkar", L"Browse Dhikr ➔" },
    { L"home.reps_suffix", L"times ↻" },
    { L"home.rep_singular", L"time ↻" },
    { L"home.today_reminders", L"Today's Reminders" },
    { L"home.today_reminders_sub", L"Completed %d of %d reminders" },
    { L"home.commitment_streak", L"Commitment Streak" },
    { L"home.streak_days_val", L"%d Days" },
    { L"home.streak_sub", L"Consecutive so far" },
    { L"home.today_progress", L"Today's Progress" },
    { L"home.today_progress_sub", L"Based on completed reminders" },
    { L"home.today_log", L"Today's Log" },
    { L"home.view_all", L"View All ➔" },
    { L"home.period_morning", L"Morning Dhikr ☀️" },
    { L"home.period_day", L"Daytime Dhikr 🌤" },
    { L"home.period_sunset", L"Sunset Dhikr 🌇" },
    { L"home.period_night", L"Evening Dhikr 🌙" },
    { L"home.todays_reminders", L"Today's Reminders" },
    { L"home.current_streak", L"Current Streak" },
    { L"home.total_reminders", L"Total Reminders" },
    { L"home.todays_progress", L"Today's Progress" },
    { L"home.days", L"Days" },
    { L"home.this_week", L"This Week" },
    { L"home.completed", L"Completed" },
    { L"home.in_minutes", L"In minutes" },
    { L"home.today", L"Today" },
    { L"home.total", L"Total" },
    { L"home.streak", L"Streak" },
    { L"home.minutes", L"min" },
    { L"home.quick_reminder", L"Test Alert" },
    { L"home.sent", L"Triggered!" },
    { L"home.daily_target", L"Daily Target: " },
    { L"home.streak_motto", L"Consistency in remembrance brings peace" },
    { L"home.journey_title", L"Daily Dhikr Journey" },
    { L"home.milestone_morning", L"Morning" },
    { L"home.milestone_forenoon", L"Forenoon" },
    { L"home.milestone_noon", L"Noon" },
    { L"home.milestone_afternoon", L"Afternoon" },
    { L"home.milestone_evening", L"Evening" },

    // Reminders
    { L"reminders.title", L"Reminders" },
    { L"reminders.enable", L"Enable Reminders" },
    { L"reminders.start_time", L"Start Time" },
    { L"reminders.end_time", L"End Time" },
    { L"reminders.frequency", L"Frequency" },
    { L"reminders.quiet_hours", L"Quiet Hours" },
    { L"reminders.pause_for", L"Pause For" },
    { L"reminders.preview", L"Preview Notification" },
    { L"reminders.save", L"Save Changes" },
    { L"reminders.save_star", L"Save Changes *" },
    { L"reminders.pause_dropdown", L"Pause" },

    // Schedule
    { L"schedule.title", L"Weekly Schedule" },
    { L"schedule.sunday", L"Sunday" },
    { L"schedule.monday", L"Monday" },
    { L"schedule.tuesday", L"Tuesday" },
    { L"schedule.wednesday", L"Wednesday" },
    { L"schedule.thursday", L"Thursday" },
    { L"schedule.friday", L"Friday" },
    { L"schedule.saturday", L"Saturday" },
    { L"schedule.sun", L"Sun" },
    { L"schedule.mon", L"Mon" },
    { L"schedule.tue", L"Tue" },
    { L"schedule.wed", L"Wed" },
    { L"schedule.thu", L"Thu" },
    { L"schedule.fri", L"Fri" },
    { L"schedule.sat", L"Sat" },
    { L"schedule.add_quiet_hours", L"Add Quiet Hours" },
    { L"schedule.hour", L"hour" },
    { L"schedule.hours", L"hours" },

    // Library
    { L"library.title", L"Dhikr Library" },
    { L"library.add_dhikr", L"+ Add Dhikr" },
    { L"library.dialog_title", L"Add Custom Dhikr" },
    { L"library.dhikr_text", L"Dhikr Text" },
    { L"library.category", L"Category" },
    { L"library.repeat_count", L"Repetitions" },
    { L"library.filter_all", L"All" },
    { L"library.filter_morning", L"Morning" },
    { L"library.filter_evening", L"Evening" },
    { L"library.filter_daily", L"Daily Dhikr" },
    { L"library.filter_morning_evening", L"Morning & Evening" },
    { L"library.filter_tasbih", L"Tasbih" },
    { L"library.filter_custom", L"Custom" },
    { L"library.delete_all", L"Delete All" },
    { L"library.import_md", L"Import MD" },
    { L"library.empty", L"Your library is empty. Add a dhikr or import a markdown file." },
    { L"library.delete_confirm", L"Are you sure you want to delete all dhikr items?" },
    { L"library.delete_title", L"Confirm Delete" },
    { L"library.delete_item_confirm", L"Delete this dhikr item?" },
    { L"library.import_success", L"Dhikr items imported successfully!" },
    { L"library.import_error", L"Failed to import markdown file." },
    { L"library.items_loaded", L"Items available" },

    // Statistics
    { L"statistics.title", L"Statistics" },
    { L"statistics.today", L"Today" },
    { L"statistics.this_week", L"This Week" },
    { L"statistics.this_month", L"This Month" },
    { L"statistics.total", L"Total Reminders" },
    { L"statistics.completed", L"Completed" },
    { L"statistics.streak", L"Streak" },
    { L"statistics.progress", L"Progress" },
    { L"stats.daily_chart", L"Daily Activity" },
    { L"stats.top_dhikrs", L"Top Dhikr Items" },

    // Popup
    { L"popup.copy", L"Copy" },
    { L"popup.copied", L"Copied!" },
    { L"popup.snooze", L"Snooze 5m" },
    { L"popup.dismiss", L"Dismiss" },
    { L"home.next_preview_prefix", L"Upcoming Dhikr:" },

    // Settings
    { L"settings.title", L"Settings" },
    { L"settings.general", L"General" },
    { L"settings.popup", L"Popup" },
    { L"settings.sound", L"Sound" },
    { L"settings.system", L"System" },
    { L"settings.language", L"Language" },
    { L"settings.start_with_windows", L"Start with Windows" },
    { L"settings.accent_color", L"Accent Color" },
    { L"settings.color_theme", L"Color Theme" },
    { L"theme.green", L"Emerald Green" },
    { L"theme.yellow", L"Radiant Solar Yellow" },
    { L"theme.blue", L"Electric Blue" },
    { L"theme.red", L"Crimson Red" },
    { L"theme.silver", L"Brushed Silver" },
    { L"theme.gold", L"Regal Antique Gold" },
    { L"settings.reset_all", L"Reset All Settings" },
    { L"settings.position", L"Position" },
    { L"settings.monitor", L"Monitor" },
    { L"settings.monitor_all", L"All Monitors (Simultaneously)" },
    { L"settings.monitor_primary", L"Primary Monitor (Default)" },
    { L"settings.monitor_active", L"Active Monitor (Cursor)" },
    { L"settings.monitor_num", L"Monitor" },
    { L"settings.multi_monitor_title", L"Target Display Monitor" },
    { L"settings.monitors_detected", L"Detected Monitors" },
    { L"settings.duration", L"Duration" },
    { L"settings.animations", L"Animations" },
    { L"settings.animation_style", L"Popup Animation Style" },
    { L"settings.preview_anim", L"Preview Animation" },
    { L"anim.classic", L"Classic Spring Slide (Default)" },
    { L"anim.spring_bloom", L"Spring Bloom (3D Scale)" },
    { L"anim.horizontal_drift", L"Horizontal Drift (Lateral Sheet)" },
    { L"anim.liquid_float", L"Liquid Float (Fluid Wave)" },
    { L"anim.origami_unfurl", L"Origami Unfurl (Scroll Reveal)" },
    { L"anim.ethereal_sweep", L"Ethereal Aura Sweep" },
    { L"anim.zen_breathing", L"Zen Breathing Ease" },
    { L"anim.ripple_genesis", L"Ripple Genesis (Oasis Wave)" },
    { L"settings.animation_speed", L"Motion Pace" },
    { L"speed.cinematic", L"Cinematic (Relaxed 1.35x)" },
    { L"speed.normal", L"Standard (Balanced 1.0x)" },
    { L"speed.snappy", L"Snappy (Fast 0.65x)" },
    { L"settings.popup_surface", L"Glass & Surface Style" },
    { L"surface.frosted", L"Frosted Acrylic Glass" },
    { L"surface.obsidian", L"Deep Obsidian (Max Contrast)" },
    { L"surface.aurora", L"Vibrant Aurora Glow" },
    { L"settings.corner_style", L"Corner Radius" },
    { L"corner.rounded", L"Smooth Rounded (14px)" },
    { L"corner.squircle", L"Modern Squircle (8px)" },
    { L"corner.capsule", L"Curved Capsule (24px)" },
    { L"settings.arabesque", L"Islamic Arabesque Geometry" },
    { L"settings.typography_section", L"Typography & Content" },
    { L"settings.font_size", L"Dhikr Font Size" },
    { L"font.small", L"Compact" },
    { L"font.medium", L"Standard (Default)" },
    { L"font.large", L"Large & Clear" },
    { L"settings.show_tashkeel", L"Show Arabic Diacritics (Tashkeel)" },
    { L"settings.show_counter", L"Interactive Counter Pill" },
    { L"settings.show_source", L"Show Hadith Source & Virtue" },
    { L"settings.enable_sound", L"Enable Sound" },
    { L"settings.sound_preset", L"Sound Melody" },
    { L"sound.chime", L"Crystal Chime (Default)" },
    { L"sound.water", L"Pristine Water Droplet" },
    { L"sound.ambient", L"Warm Ambient Echo" },
    { L"sound.wood", L"Gentle Wood Tap" },
    { L"sound.custom", L"Custom Audio File..." },
    { L"settings.volume", L"Volume" },
    { L"settings.notification_sound", L"Custom Sound File" },
    { L"settings.sound_default", L"Default (Chime)" },
    { L"settings.choose_sound", L"Browse Audio File..." },
    { L"settings.reset_sound", L"Reset to Default" },
    { L"settings.test_sound", L"Test Sound" },
    { L"settings.version", L"Application Version" },
    { L"settings.check_updates", L"Open Releases Page" },
    { L"settings.checking", L"Checking..." },
    { L"settings.up_to_date", L"Up to date" },
    { L"settings.update_available", L"Update Available" },
    { L"settings.update_now", L"Update Now" },
    { L"settings.reset_confirm_title", L"Reset All Settings" },
    { L"settings.reset_confirm_msg", L"Are you sure you want to reset all settings to defaults? This action cannot be undone." },
    { L"settings.fullscreen_suppression", L"Do not interrupt in full-screen mode" },
    { L"settings.fullscreen_behavior", L"When full-screen active" },
    { L"settings.fullscreen_queue", L"Queue until full-screen ends" },
    { L"settings.fullscreen_suppress", L"Suppress reminder completely" },
    { L"settings.fullscreen_win_notify", L"Windows notification only" },

    // TimePicker
    { L"timepicker.title", L"Select Time" },
    { L"timepicker.apply", L"Apply" },
    { L"timepicker.cancel", L"Cancel" },
    { L"durationpicker.title", L"Custom Interval" },
    { L"durationpicker.custom_opt", L"Custom Duration..." },

    // About
    { L"about.title", L"About" },
    { L"about.version", L"Version" },
    { L"about.contact", L"Contact Developer" },
    { L"about.copy", L"Copy" },
    { L"about.copied", L"Copied!" },
    { L"about.support", L"Support the Developer" },
    { L"about.support_sub", L"Your kind support helps keep the application updated and free" },
    { L"about.vodafone", L"Vodafone Cash" },
    { L"about.iban", L"Banque Misr (IBAN)" },
    { L"about.wallet", L"Mobile Wallet" },
    { L"about.bank_transfer", L"Bank Transfer" },
    { L"about.copy_num", L"Copy Number" },
    { L"about.copy_iban", L"Copy IBAN" },
    { L"about.scan_qr", L"Scan QR Code" },
    { L"about.scan_hint", L"Point phone camera to scan directly" },
    { L"about.close_modal", L"Close" },

    // Tray
    { L"tray.show", L"Show Azkar Reminder" },
    { L"tray.pause", L"Pause Reminders (1h)" },
    { L"tray.resume", L"Resume Reminders" },
    { L"tray.settings", L"Settings" },
    { L"tray.exit", L"Exit" },

    // Common
    { L"common.ok", L"OK" },
    { L"common.cancel", L"Cancel" },
    { L"common.close", L"Close" },
    { L"common.yes", L"Yes" },
    { L"common.no", L"No" },
    { L"common.save", L"Save" },
    { L"common.delete", L"Delete" },
    { L"common.warning", L"Warning" },
    { L"common.error", L"Error" },
    { L"common.success", L"Success" },
};

static std::unordered_map<std::wstring, std::wstring> g_ar = {
    // Navigation
    { L"nav.home", L"الرئيسية" },
    { L"nav.reminders", L"التنبيهات والجدول" },
    { L"nav.schedule", L"الجدول" },
    { L"nav.library", L"مكتبة الأذكار" },
    { L"nav.statistics", L"الإحصائيات" },
    { L"nav.settings", L"الإعدادات" },
    { L"nav.about", L"عن التطبيق" },

    // Home
    { L"home.greeting", L"السلام عليكم" },
    { L"home.subtitle", L"اجعل لذكر الله نصيبًا من يومك" },
    { L"home.status", L"الحالة:" },
    { L"home.active", L"نشط" },
    { L"home.paused", L"متوقف مؤقتاً" },
    { L"home.reminders_active", L"التذكيرات مفعلة" },
    { L"home.reminders_paused", L"التذكيرات متوقفة" },
    { L"home.next_reminder", L"التذكير القادم" },
    { L"home.remaining_time", L"متبقي" },
    { L"home.time_for_dhikr", L"حان وقت الذكر" },
    { L"home.mark_done", L"تم الذكر ✓" },
    { L"home.snooze_5m", L"تأجيل 5 دقائق ⏱" },
    { L"home.browse_azkar", L"عرض باقي الأذكار ➔" },
    { L"home.reps_suffix", L"مرات ↻" },
    { L"home.rep_singular", L"مرة ↻" },
    { L"home.today_reminders", L"تذكيرات اليوم" },
    { L"home.today_reminders_sub", L"تم إكمال %d من %d تذكير" },
    { L"home.commitment_streak", L"سلسلة الالتزام" },
    { L"home.streak_days_val", L"%d أيام" },
    { L"home.streak_sub", L"متواصلة حتى الآن" },
    { L"home.today_progress", L"تقدم اليوم" },
    { L"home.today_progress_sub", L"بناءً على التذكيرات التي أكملتها" },
    { L"home.today_log", L"سجل اليوم" },
    { L"home.view_all", L"عرض الكل ➔" },
    { L"home.period_morning", L"أذكار الصباح ☀️" },
    { L"home.period_day", L"أذكار اليوم 🌤" },
    { L"home.period_sunset", L"أذكار المساء 🌇" },
    { L"home.period_night", L"أذكار المساء 🌙" },
    { L"home.todays_reminders", L"تنبيهات اليوم" },
    { L"home.current_streak", L"السلسلة الحالية" },
    { L"home.total_reminders", L"إجمالي التنبيهات" },
    { L"home.todays_progress", L"تقدم اليوم" },
    { L"home.days", L"أيام" },
    { L"home.this_week", L"هذا الأسبوع" },
    { L"home.completed", L"مكتمل" },
    { L"home.in_minutes", L"بالدقائق" },
    { L"home.today", L"اليوم" },
    { L"home.total", L"الإجمالي" },
    { L"home.streak", L"السلسلة" },
    { L"home.minutes", L"دقيقة" },
    { L"home.quick_reminder", L"تنبيه فوري ⚡" },
    { L"home.sent", L"تم التنبيه! ✓" },
    { L"home.daily_target", L"الهدف اليومي: " },
    { L"home.streak_motto", L"أحب الأعمال إلى الله أدومها وإن قلّ" },
    { L"home.journey_title", L"مسار الذكر اليومي" },
    { L"home.milestone_morning", L"الصباح" },
    { L"home.milestone_forenoon", L"الضحى" },
    { L"home.milestone_noon", L"الظهيرة" },
    { L"home.milestone_afternoon", L"العصر" },
    { L"home.milestone_evening", L"المساء" },

    // Reminders
    { L"reminders.title", L"التنبيهات" },
    { L"reminders.enable", L"تفعيل التنبيهات" },
    { L"reminders.start_time", L"وقت البدء" },
    { L"reminders.end_time", L"وقت الانتهاء" },
    { L"reminders.frequency", L"التكرار" },
    { L"reminders.quiet_hours", L"أوقات الهدوء" },
    { L"reminders.pause_for", L"إيقاف مؤقت لمدة" },
    { L"reminders.preview", L"تجربة التنبيه" },
    { L"reminders.save", L"حفظ التغييرات" },
    { L"reminders.save_star", L"حفظ التغييرات *" },
    { L"reminders.pause_dropdown", L"إيقاف مؤقت" },

    // Schedule
    { L"schedule.title", L"الجدول الأسبوعي" },
    { L"schedule.sunday", L"الأحد" },
    { L"schedule.monday", L"الإثنين" },
    { L"schedule.tuesday", L"الثلاثاء" },
    { L"schedule.wednesday", L"الأربعاء" },
    { L"schedule.thursday", L"الخميس" },
    { L"schedule.friday", L"الجمعة" },
    { L"schedule.saturday", L"السبت" },
    { L"schedule.sun", L"الأحد" },
    { L"schedule.mon", L"الإثنين" },
    { L"schedule.tue", L"الثلاثاء" },
    { L"schedule.wed", L"الأربعاء" },
    { L"schedule.thu", L"الخميس" },
    { L"schedule.fri", L"الجمعة" },
    { L"schedule.sat", L"السبت" },
    { L"schedule.add_quiet_hours", L"إضافة وقت هدوء" },
    { L"schedule.hour", L"ساعة" },
    { L"schedule.hours", L"ساعات" },

    // Library
    { L"library.title", L"مكتبة الأذكار" },
    { L"library.add_dhikr", L"+ إضافة ذكر" },
    { L"library.dialog_title", L"إضافة ذكر جديد" },
    { L"library.dhikr_text", L"نص الذكر" },
    { L"library.category", L"التصنيف" },
    { L"library.repeat_count", L"مرات التكرار" },
    { L"library.filter_all", L"الكل" },
    { L"library.filter_morning", L"أذكار الصباح" },
    { L"library.filter_evening", L"أذكار المساء" },
    { L"library.filter_daily", L"باقي اليوم" },
    { L"library.filter_morning_evening", L"الصباح والمساء" },
    { L"library.filter_tasbih", L"التسبيح" },
    { L"library.filter_custom", L"أذكاري" },
    { L"library.delete_all", L"حذف الكل" },
    { L"library.import_md", L"استيراد ملف" },
    { L"library.empty", L"المكتبة فارغة. قم بإضافة ذكر أو استيراد ملف." },
    { L"library.delete_confirm", L"هل أنت متأكد من حذف جميع الأذكار؟" },
    { L"library.delete_title", L"تأكيد الحذف" },
    { L"library.delete_item_confirm", L"حذف هذا الذكر؟" },
    { L"library.import_success", L"تم استيراد الأذكار بنجاح!" },
    { L"library.import_error", L"فشل استيراد ملف الأذكار." },
    { L"library.items_loaded", L"عنصر متوفر" },

    // Statistics
    { L"statistics.title", L"الإحصائيات" },
    { L"statistics.today", L"اليوم" },
    { L"statistics.this_week", L"هذا الأسبوع" },
    { L"statistics.this_month", L"هذا الشهر" },
    { L"statistics.total", L"إجمالي التنبيهات" },
    { L"statistics.completed", L"مكتمل" },
    { L"statistics.streak", L"السلسلة" },
    { L"statistics.progress", L"التقدم" },
    { L"stats.daily_chart", L"النشاط اليومي" },
    { L"stats.top_dhikrs", L"أكثر الأذكار تكراراً" },

    // Popup
    { L"popup.copy", L"نسخ" },
    { L"popup.copied", L"تم النسخ!" },
    { L"popup.snooze", L"تأجيل 5د" },
    { L"popup.dismiss", L"إغلاق" },
    { L"home.next_preview_prefix", L"الذكر القادم:" },

    // Settings
    { L"settings.title", L"الإعدادات" },
    { L"settings.general", L"عام" },
    { L"settings.popup", L"النافذة المنبثقة" },
    { L"settings.sound", L"الصوت" },
    { L"settings.system", L"النظام" },
    { L"settings.language", L"اللغة" },
    { L"settings.start_with_windows", L"التشغيل مع الويندوز" },
    { L"settings.accent_color", L"طابع الألوان" },
    { L"settings.color_theme", L"طابع الألوان" },
    { L"theme.green", L"أخضر زمردي" },
    { L"theme.yellow", L"أصفر شمسي مشرق" },
    { L"theme.blue", L"أزرق سماوي" },
    { L"theme.red", L"أحمر ياقوتي" },
    { L"theme.silver", L"فضي معدني" },
    { L"theme.gold", L"ذهبي ملكي عتيق" },
    { L"settings.reset_all", L"إعادة ضبط المصنع" },
    { L"settings.position", L"الموقع" },
    { L"settings.monitor", L"الشاشة" },
    { L"settings.monitor_all", L"كل الشاشات معاً" },
    { L"settings.monitor_primary", L"الشاشة الرئيسية (الافتراضي)" },
    { L"settings.monitor_active", L"مكان مؤشر الماوس" },
    { L"settings.monitor_num", L"شاشة" },
    { L"settings.multi_monitor_title", L"شاشة عرض التذكيرات" },
    { L"settings.monitors_detected", L"الشاشات المكتشفة" },
    { L"settings.duration", L"المدة" },
    { L"settings.animations", L"الرسوم المتحركة" },
    { L"settings.animation_style", L"نمط حركة الإشعار" },
    { L"settings.preview_anim", L"معاينة الحركة" },
    { L"anim.classic", L"انسيابي كلاسيكي (الافتراضي)" },
    { L"anim.spring_bloom", L"انبثاق مرن ثلاثي الأبعاد" },
    { L"anim.horizontal_drift", L"انزلاق أفقي هادئ" },
    { L"anim.liquid_float", L"طفو مائي انسيابي" },
    { L"anim.origami_unfurl", L"انبساط هادئ كالطيات" },
    { L"anim.ethereal_sweep", L"همس النور / التدفق الأثيري" },
    { L"anim.zen_breathing", L"نبض السكينة / التنفس الهادئ" },
    { L"anim.ripple_genesis", L"تموّج الواحة" },
    { L"settings.animation_speed", L"سرعة وإيقاع الحركة" },
    { L"speed.cinematic", L"سينمائي متمهل (1.35x)" },
    { L"speed.normal", L"قياسي متزن (1.0x)" },
    { L"speed.snappy", L"سريع وخاطف (0.65x)" },
    { L"settings.popup_surface", L"خامة وشكل الزجاج" },
    { L"surface.frosted", L"زجاج متجمد سائل" },
    { L"surface.obsidian", L"أوبسيديان داكن معتم" },
    { L"surface.aurora", L"هالة الأورورا المتوهجة" },
    { L"settings.corner_style", L"شكل حواف الإشعار" },
    { L"corner.rounded", L"انسيابي متزن (14px)" },
    { L"corner.squircle", L"حديث مدمج (8px)" },
    { L"corner.capsule", L"كبسولي دائري (24px)" },
    { L"settings.arabesque", L"الزخرفة الإسلامية الهندسية" },
    { L"settings.typography_section", L"الخط ومحتوى الذكر" },
    { L"settings.font_size", L"حجم خط الذكر" },
    { L"font.small", L"صغير مدمج" },
    { L"font.medium", L"قياسي (الافتراضي)" },
    { L"font.large", L"كبير وواضح" },
    { L"settings.show_tashkeel", L"عرض التشكيل والحركات" },
    { L"settings.show_counter", L"عدّاد التسبيح التفاعلي" },
    { L"settings.show_source", L"إظهار فضل الذكر والمصدر" },
    { L"settings.enable_sound", L"تفعيل الصوت" },
    { L"settings.sound_preset", L"نغمة التنبيه" },
    { L"sound.chime", L"رنين بلوري (الافتراضي)" },
    { L"sound.water", L"قطرة ماء هادئة" },
    { L"sound.ambient", L"صدى ناي دافئ" },
    { L"sound.wood", L"نقرة خشبية خافتة" },
    { L"sound.custom", L"ملف صوتي مخصص..." },
    { L"settings.volume", L"مستوى الصوت" },
    { L"settings.notification_sound", L"ملف صوتي مخصص" },
    { L"settings.sound_default", L"الافتراضي (نغمة هادئة)" },
    { L"settings.choose_sound", L"استعراض ملف صوتي..." },
    { L"settings.reset_sound", L"استعادة الافتراضي" },
    { L"settings.test_sound", L"تجربة الصوت" },
    { L"settings.version", L"إصدار التطبيق" },
    { L"settings.check_updates", L"فتح صفحة الإصدارات" },
    { L"settings.checking", L"جاري التحقق..." },
    { L"settings.up_to_date", L"التطبيق محدث" },
    { L"settings.update_available", L"يوجد تحديث متاح" },
    { L"settings.update_now", L"تحديث الآن" },
    { L"settings.reset_confirm_title", L"إعادة ضبط الإعدادات" },
    { L"settings.reset_confirm_msg", L"هل أنت متأكد من رغبتك في إعادة ضبط كافة الإعدادات إلى القيم الافتراضية؟ لا يمكن التراجع عن هذا الإجراء." },
    { L"settings.fullscreen_suppression", L"عدم المقاطعة أثناء وضع ملء الشاشة" },
    { L"settings.fullscreen_behavior", L"عند تفعيل ملء الشاشة" },
    { L"settings.fullscreen_queue", L"تأجيل التذكير حتى الخروج من ملء الشاشة" },
    { L"settings.fullscreen_suppress", L"كتم التذكير تماماً" },
    { L"settings.fullscreen_win_notify", L"إشعار ويندوز فقط" },

    // TimePicker
    { L"timepicker.title", L"تحديد الوقت" },
    { L"timepicker.apply", L"تطبيق" },
    { L"timepicker.cancel", L"إلغاء" },
    { L"durationpicker.title", L"تحديد وقت مخصص" },
    { L"durationpicker.custom_opt", L"تخصيص وقت محدد..." },

    // About
    { L"about.title", L"عن التطبيق" },
    { L"about.version", L"الإصدار" },
    { L"about.contact", L"تواصل مع المطور" },
    { L"about.copy", L"نسخ" },
    { L"about.copied", L"تم النسخ!" },
    { L"about.support", L"دعم وتطوير المشروع" },
    { L"about.support_sub", L"مساهمتك الكريمة تساهم في استمرار وتطوير التطبيق وجعله صدقة جارية نافعة" },
    { L"about.vodafone", L"فودافون كاش" },
    { L"about.iban", L"بنك مصر (IBAN)" },
    { L"about.wallet", L"محفظة إلكترونية" },
    { L"about.bank_transfer", L"تحويل بنكي" },
    { L"about.copy_num", L"نسخ الرقم" },
    { L"about.copy_iban", L"نسخ الآيبان" },
    { L"about.scan_qr", L"مسح الكيو ار" },
    { L"about.scan_hint", L"وجّه كاميرا هاتفك نحو الشاشة للتحويل المباشر" },
    { L"about.close_modal", L"إغلاق" },

    // Tray
    { L"tray.show", L"إظهار التطبيق" },
    { L"tray.pause", L"إيقاف التنبيهات مؤقتاً (ساعة)" },
    { L"tray.resume", L"استئناف التنبيهات" },
    { L"tray.settings", L"الإعدادات" },
    { L"tray.exit", L"خروج" },

    // Common
    { L"common.ok", L"موافق" },
    { L"common.cancel", L"إلغاء" },
    { L"common.close", L"إغلاق" },
    { L"common.yes", L"نعم" },
    { L"common.no", L"لا" },
    { L"common.save", L"حفظ" },
    { L"common.delete", L"حذف" },
    { L"common.warning", L"تحذير" },
    { L"common.error", L"خطأ" },
    { L"common.success", L"نجاح" },
};

Localization& Localization::Instance() {
    static Localization instance;
    return instance;
}

Localization::Localization() : m_currentLang(L"en"), m_isRtl(false) {
    g_en.try_emplace(L"qt.about_qt", L"About Qt and licenses");
    g_ar.try_emplace(L"qt.about_qt", L"حول Qt وتراخيصها");
    g_en.try_emplace(L"qt.color.green", L"Green");
    g_ar.try_emplace(L"qt.color.green", L"أخضر");
    g_en.try_emplace(L"qt.color.yellow", L"Yellow");
    g_ar.try_emplace(L"qt.color.yellow", L"أصفر");
    g_en.try_emplace(L"qt.color.blue", L"Blue");
    g_ar.try_emplace(L"qt.color.blue", L"أزرق");
    g_en.try_emplace(L"qt.color.red", L"Red");
    g_ar.try_emplace(L"qt.color.red", L"أحمر");
    g_en.try_emplace(L"qt.color.silver", L"Silver");
    g_ar.try_emplace(L"qt.color.silver", L"فضي");
    g_en.try_emplace(L"qt.color.gold", L"Gold");
    g_ar.try_emplace(L"qt.color.gold", L"ذهبي");
    g_en.try_emplace(L"qt.anim.slide", L"Slide");
    g_ar.try_emplace(L"qt.anim.slide", L"انزلاق");
    g_en.try_emplace(L"qt.anim.spring", L"Spring");
    g_ar.try_emplace(L"qt.anim.spring", L"انبثاق");
    g_en.try_emplace(L"qt.anim.side", L"Side slide");
    g_ar.try_emplace(L"qt.anim.side", L"انزلاق جانبي");
    g_en.try_emplace(L"qt.anim.float", L"Float");
    g_ar.try_emplace(L"qt.anim.float", L"طفو");
    g_en.try_emplace(L"qt.anim.unfold", L"Unfold");
    g_ar.try_emplace(L"qt.anim.unfold", L"انفتاح");
    g_en.try_emplace(L"qt.anim.fade", L"Fade");
    g_ar.try_emplace(L"qt.anim.fade", L"تلاشي");
    g_en.try_emplace(L"qt.anim.expand", L"Expand");
    g_ar.try_emplace(L"qt.anim.expand", L"توسع");
    g_en.try_emplace(L"qt.anim.ripple", L"Ripple");
    g_ar.try_emplace(L"qt.anim.ripple", L"تموج");
    g_en.try_emplace(L"qt.speed.slow", L"Slow");
    g_ar.try_emplace(L"qt.speed.slow", L"هادئ");
    g_en.try_emplace(L"qt.speed.normal", L"Normal");
    g_ar.try_emplace(L"qt.speed.normal", L"عادي");
    g_en.try_emplace(L"qt.speed.fast", L"Fast");
    g_ar.try_emplace(L"qt.speed.fast", L"سريع");
    g_en.try_emplace(L"library.search", L"Search dhikr…");
    g_ar.try_emplace(L"library.search", L"بحث في الأذكار…");
    g_en.try_emplace(L"qt.minute_suffix", L" min");
    g_ar.try_emplace(L"qt.minute_suffix", L" دقيقة");
    g_en.try_emplace(L"qt.schedule_note", L"An end time at or before the start continues into the next day.");
    g_ar.try_emplace(L"qt.schedule_note", L"إذا كان وقت النهاية يسبق البداية أو يساويها، يستمر الجدول إلى اليوم التالي.");
    g_en.try_emplace(L"qt.import_replace", L"Replace the current library with the selected file? Export a backup first if needed.");
    g_ar.try_emplace(L"qt.import_replace", L"استبدال المكتبة الحالية بالملف المختار؟ يمكنك تصدير نسخة احتياطية أولًا.");
    g_en.try_emplace(L"settings.animation_enabled", L"Enable animation");
    g_ar.try_emplace(L"settings.animation_enabled", L"تفعيل الحركة");
    g_en.try_emplace(L"pos.top_left", L"Top left");
    g_ar.try_emplace(L"pos.top_left", L"أعلى اليسار");
    g_en.try_emplace(L"pos.top_center", L"Top center");
    g_ar.try_emplace(L"pos.top_center", L"أعلى الوسط");
    g_en.try_emplace(L"pos.top_right", L"Top right");
    g_ar.try_emplace(L"pos.top_right", L"أعلى اليمين");
    g_en.try_emplace(L"pos.center_left", L"Center left");
    g_ar.try_emplace(L"pos.center_left", L"وسط اليسار");
    g_en.try_emplace(L"pos.center", L"Center");
    g_ar.try_emplace(L"pos.center", L"الوسط");
    g_en.try_emplace(L"pos.center_right", L"Center right");
    g_ar.try_emplace(L"pos.center_right", L"وسط اليمين");
    g_en.try_emplace(L"pos.bottom_left", L"Bottom left");
    g_ar.try_emplace(L"pos.bottom_left", L"أسفل اليسار");
    g_en.try_emplace(L"pos.bottom_center", L"Bottom center");
    g_ar.try_emplace(L"pos.bottom_center", L"أسفل الوسط");
    g_en.try_emplace(L"pos.bottom_right", L"Bottom right");
    g_ar.try_emplace(L"pos.bottom_right", L"أسفل اليمين");
    g_en.try_emplace(L"home.remind_now", L"Show a reminder");
    g_ar.try_emplace(L"home.remind_now", L"عرض ذكر الآن");
    g_en.try_emplace(L"home.pause_15", L"Pause 15 minutes");
    g_ar.try_emplace(L"home.pause_15", L"إيقاف 15 دقيقة");
    g_en.try_emplace(L"home.pause_60", L"Pause one hour");
    g_ar.try_emplace(L"home.pause_60", L"إيقاف ساعة");
    g_en.try_emplace(L"home.resume", L"Resume");
    g_ar.try_emplace(L"home.resume", L"استئناف");
    g_en.try_emplace(L"home.today_count", L"Today");
    g_ar.try_emplace(L"home.today_count", L"أذكار اليوم");
    g_en.try_emplace(L"home.current_streak", L"Current streak");
    g_ar.try_emplace(L"home.current_streak", L"أيام المداومة المتتالية");
    g_en.try_emplace(L"home.total_count", L"Total reminders");
    g_ar.try_emplace(L"home.total_count", L"إجمالي الأذكار");
    g_en.try_emplace(L"reminders.frequency", L"Reminder interval");
    g_ar.try_emplace(L"reminders.frequency", L"تكرار التذكير");
    g_en.try_emplace(L"reminders.every", L"Show a reminder every:");
    g_ar.try_emplace(L"reminders.every", L"إظهار ذكر كل:");
    g_en.try_emplace(L"reminders.quiet_enable", L"Enable quiet hours");
    g_ar.try_emplace(L"reminders.quiet_enable", L"تفعيل ساعات الهدوء (إيقاف مؤقت للتذكيرات ليلاً)");
    g_en.try_emplace(L"reminders.quiet_start", L"From:");
    g_ar.try_emplace(L"reminders.quiet_start", L"من:");
    g_en.try_emplace(L"reminders.quiet_end", L"Until:");
    g_ar.try_emplace(L"reminders.quiet_end", L"إلى:");
    g_en.try_emplace(L"reminders.audio_section", L"Sound and full screen");
    g_ar.try_emplace(L"reminders.audio_section", L"الصوت وملء الشاشة");
    g_en.try_emplace(L"reminders.sound_enable", L"Play a sound with reminders");
    g_ar.try_emplace(L"reminders.sound_enable", L"تشغيل نغمة هادئة عند ظهور التذكير");
    g_en.try_emplace(L"reminders.volume", L"Volume:");
    g_ar.try_emplace(L"reminders.volume", L"مستوى الصوت:");
    g_en.try_emplace(L"reminders.test_audio", L"Test sound");
    g_ar.try_emplace(L"reminders.test_audio", L"تجربة النغمة");
    g_en.try_emplace(L"reminders.fullscreen", L"Suppress reminders during full-screen apps");
    g_ar.try_emplace(L"reminders.fullscreen", L"إيقاف التذكيرات تلقائياً أثناء وضع ملء الشاشة (الألعاب والعروض)");
    g_en.try_emplace(L"library.dialog_title", L"Add custom dhikr");
    g_ar.try_emplace(L"library.dialog_title", L"إضافة ذكر مخصص");
    g_en.try_emplace(L"library.enter_dhikr", L"Dhikr text:");
    g_ar.try_emplace(L"library.enter_dhikr", L"اكتب نص الذكر:");
    g_en.try_emplace(L"library.delete_item_confirm", L"Delete this item?");
    g_ar.try_emplace(L"library.delete_item_confirm", L"هل أنت متأكد من حذف هذا الذكر؟");
    g_en.try_emplace(L"library.all_categories", L"All categories");
    g_ar.try_emplace(L"library.all_categories", L"جميع الأقسام");
    g_en.try_emplace(L"library.add_dhikr", L"Add dhikr");
    g_ar.try_emplace(L"library.add_dhikr", L"+ إضافة ذكر");
    g_en.try_emplace(L"settings.theme_group", L"Appearance");
    g_ar.try_emplace(L"settings.theme_group", L"المظهر والألوان");
    g_en.try_emplace(L"settings.theme_color", L"Theme:");
    g_ar.try_emplace(L"settings.theme_color", L"لون الثيم:");
    g_en.try_emplace(L"settings.animation_group", L"Reminder animation");
    g_ar.try_emplace(L"settings.animation_group", L"أنماط الحركة والأنيميشن");
    g_en.try_emplace(L"settings.anim_style", L"Style:");
    g_ar.try_emplace(L"settings.anim_style", L"نمط ظهور التذكير:");
    g_en.try_emplace(L"settings.preview_anim", L"Preview");
    g_ar.try_emplace(L"settings.preview_anim", L"▶ معاينة الحركة");
    g_en.try_emplace(L"settings.anim_speed", L"Speed:");
    g_ar.try_emplace(L"settings.anim_speed", L"سرعة الحركة:");
    g_en.try_emplace(L"settings.general_group", L"General");
    g_ar.try_emplace(L"settings.general_group", L"عام والتكامل");
    g_en.try_emplace(L"settings.duration", L"Reminder duration:");
    g_ar.try_emplace(L"settings.duration", L"مدة بقاء التذكير على الشاشة:");
    g_en.try_emplace(L"settings.autostart", L"Start at login");
    g_ar.try_emplace(L"settings.autostart", L"التشغيل التلقائي مع بدء تشغيل النظام");
    g_en.try_emplace(L"settings.language", L"Language:");
    g_ar.try_emplace(L"settings.language", L"اللغة (Language):");
    g_en.try_emplace(L"home.status_paused", L"Reminders paused");
    g_ar.try_emplace(L"home.status_paused", L"التذكيرات متوقفة مؤقتاً");
    g_en.try_emplace(L"qt.delete", L"Delete selected");
    g_ar.try_emplace(L"qt.delete", L"حذف المحدد");
    g_en.try_emplace(L"qt.import", L"Import library");
    g_ar.try_emplace(L"qt.import", L"استيراد مكتبة");
    g_en.try_emplace(L"qt.export", L"Export library");
    g_ar.try_emplace(L"qt.export", L"تصدير المكتبة");
    g_en.try_emplace(L"qt.sound.unavailable", L"Sound is unavailable; reminders will remain visible.");
    g_ar.try_emplace(L"qt.sound.unavailable", L"الصوت غير متاح؛ التذكيرات المرئية ستستمر.");
    g_en.try_emplace(L"qt.fullscreen.unsupported", L"Full-screen detection is unavailable on this desktop.");
    g_ar.try_emplace(L"qt.fullscreen.unsupported", L"كشف ملء الشاشة غير متاح على سطح المكتب الحالي.");
    g_en.try_emplace(L"qt.minutes", L"%1 minutes");
    g_ar.try_emplace(L"qt.minutes", L"%1 دقيقة");
    g_en.try_emplace(L"qt.seconds", L"%1 seconds");
    g_ar.try_emplace(L"qt.seconds", L"%1 ثانية");
    g_en.try_emplace(L"qt.no_schedule", L"No reminder scheduled");
    g_ar.try_emplace(L"qt.no_schedule", L"لا يوجد تذكير مجدول");
    g_en.try_emplace(L"qt.save_failed", L"Could not save changes. Check folder permissions.");
    g_ar.try_emplace(L"qt.save_failed", L"تعذر حفظ التغييرات. تحقق من صلاحيات المجلد.");
    g_en.try_emplace(L"qt.weekly", L"Weekly schedule");
    g_ar.try_emplace(L"qt.weekly", L"الجدول الأسبوعي");
    g_en.try_emplace(L"qt.enabled", L"Enable reminders");
    g_ar.try_emplace(L"qt.enabled", L"تفعيل التذكيرات");
    g_en.try_emplace(L"qt.position", L"Position");
    g_ar.try_emplace(L"qt.position", L"الموضع");
    g_en.try_emplace(L"qt.monitor", L"Screen");
    g_ar.try_emplace(L"qt.monitor", L"الشاشة");
    g_en.try_emplace(L"qt.primary", L"Primary screen");
    g_ar.try_emplace(L"qt.primary", L"الشاشة الرئيسية");
    g_en.try_emplace(L"qt.active", L"Pointer screen");
    g_ar.try_emplace(L"qt.active", L"شاشة المؤشر");
    g_en.try_emplace(L"qt.all_screens", L"All screens");
    g_ar.try_emplace(L"qt.all_screens", L"جميع الشاشات");
    g_en.try_emplace(L"qt.show_source", L"Show source");
    g_ar.try_emplace(L"qt.show_source", L"عرض المصدر");
    g_en.try_emplace(L"qt.tashkeel", L"Show Arabic vowel marks");
    g_ar.try_emplace(L"qt.tashkeel", L"عرض التشكيل");
    g_en.try_emplace(L"qt.font_size", L"Text size");
    g_ar.try_emplace(L"qt.font_size", L"حجم النص");
    g_en.try_emplace(L"qt.small", L"Small");
    g_ar.try_emplace(L"qt.small", L"صغير");
    g_en.try_emplace(L"qt.medium", L"Medium");
    g_ar.try_emplace(L"qt.medium", L"متوسط");
    g_en.try_emplace(L"qt.large", L"Large");
    g_ar.try_emplace(L"qt.large", L"كبير");
    g_en.try_emplace(L"qt.quit", L"Quit");
    g_ar.try_emplace(L"qt.quit", L"إنهاء");
    g_en.try_emplace(L"qt.about", L"Local dhikr reminders. Settings and history stay on this device.");
    g_ar.try_emplace(L"qt.about", L"تذكير محلي بالأذكار. الإعدادات والسجل محفوظان على هذا الجهاز.");
    g_en.try_emplace(L"qt.animation_enabled", L"Enable animations");
    g_ar.try_emplace(L"qt.animation_enabled", L"تفعيل الحركة");

}

std::wstring Localization::Get(const std::wstring& key, const std::wstring& fallback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_currentLang == L"ar") {
        auto it = g_ar.find(key);
        if (it != g_ar.end() && !it->second.empty()) return it->second;
    } else {
        auto it = g_en.find(key);
        if (it != g_en.end() && !it->second.empty()) return it->second;
    }
    // Fallback if not found
    return fallback.empty() ? key : fallback;
}

void Localization::SetLanguage(const std::wstring& langCode) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentLang = langCode;
        m_isRtl = (m_currentLang == L"ar");
    }
    NotifyChanged();
}

std::wstring Localization::GetLanguage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentLang;
}

bool Localization::IsRTL() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_isRtl;
}

void Localization::OnLanguageChanged(ChangeCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_changeCallbacks.push_back(cb);
}

void Localization::NotifyChanged() {
    std::vector<ChangeCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        callbacks = m_changeCallbacks;
    }
    for (auto& cb : callbacks) {
        if (cb) cb();
    }
}

} // namespace dhikr
