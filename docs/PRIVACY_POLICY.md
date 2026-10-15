# سياسة الخصوصية | Privacy policy

آخر تحديث: 15 أكتوبر 2026. تنطبق هذه الوثيقة على المصدر الحالي للإصدار التجريبي 1.2.5.

## العربية

يعمل التذكير والجدولة والصوت دون اتصال بالإنترنت. يحفظ التطبيق الإعدادات والمكتبة المخصصة وسجل عرض التذكيرات محليًا في ملفات JSON وقاعدة SQLite. لا يرسل الكود الحالي هذه البيانات إلى خدمة خارجية، ولا يحتوي خدمة تتبع أو إعلانات.

عند اختيار فحص التحديثات في واجهة Windows، يفتح التطبيق صفحة إصدارات المشروع على GitHub في المتصفح الافتراضي. الاتصال عندئذ من المتصفح، وقد يتلقى GitHub بيانات الاتصال المعتادة. واجهة Qt لا تحتوي فحص تحديثات حاليًا.

مسارات التخزين:

| النظام | الإعدادات والبيانات |
|---|---|
| Windows | `%LOCALAPPDATA%\Azkar Reminder` |
| Windows MSIX | قد تعزل Windows الملفات في `%LOCALAPPDATA%\Packages\<PackageFamilyName>\LocalCache\Local\Azkar Reminder` |
| macOS | `~/Library/Application Support/Azkar Reminder`؛ السجلات في `~/Library/Logs/Azkar Reminder` |
| Linux | `$XDG_CONFIG_HOME/azkar-reminder` و`$XDG_DATA_HOME/azkar-reminder`، أو `~/.config/azkar-reminder` و`~/.local/share/azkar-reminder` عند عدم تعيينهما |

نسخة MSIX قد تقرأ ملفات الإصدار غير المحزم أيضًا عبر العرض المدمج لمجلد AppData. إزالة حزمة MSIX تحذف بياناتها الخاصة؛ صدّر المكتبة المطلوبة قبل الإزالة. راجع [شرح Microsoft لعزل التطبيقات المحزمة](https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-behind-the-scenes).

تحتوي السجلات على أخطاء التشغيل وبعض مسارات الملفات. تُستخدم هوية تثبيت محلية في الذاكرة عند التحميل؛ لا يرسلها الكود الحالي إلى خادم. صدّر المكتبة إلى المكان الذي تختاره عند طلب نسخة احتياطية، وتحقق من ملفات السجل قبل مشاركتها للدعم.

التشغيل التلقائي يضيف سجلًا لحساب المستخدم: HKCU Run على Windows، ملف autostart على Linux، أو LaunchAgent على macOS. يمكن تعطيله من الإعدادات. يستخدم الصوت جهاز الإخراج المحلي. يعتمد كشف ملء الشاشة على بيانات نوافذ النظام في Windows/X11/macOS؛ لا يدعم كشف ملء الشاشة العام في Wayland. إشعارات Wayland تمر عبر خدمة إشعارات سطح المكتب.

للدعم، استخدم صفحة [المشروع](https://github.com/ahmedkamel6/Azkar-Reminder). لا يوجد حساب أو مزامنة سحابية داخل التطبيق الحالي.

## English

Scheduling, reminder content and audio work without an internet connection. Settings, custom content and delivery history are stored locally as JSON and SQLite files at the paths above. The current source does not transmit those records to an external service and contains no telemetry service or advertising.

MSIX can redirect new AppData files into a private package location while allowing reads from an existing unpackaged installation. Windows removes private package data when the package is uninstalled. Export any library you want to retain before removal.

A user-requested Windows update check opens the project's GitHub releases page in the default browser. That browser makes the network request; GitHub may receive ordinary connection data. The Qt frontend currently has no update checker.

Logs contain runtime errors and some file paths. A local installation identifier is generated in memory during settings loading; the current code does not send it to a server. Export writes a library backup to the user-selected path. Inspect logs before sharing them for support.

Optional startup registration modifies the current user's startup configuration. Audio uses local output devices. Fullscreen checks inspect system window information where supported; general Wayland fullscreen detection is unavailable. Wayland reminders use the desktop notification service. There is no application account or cloud synchronization.
