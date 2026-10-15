# MSIX packaging

الهوية الافتراضية مأخوذة من تسجيل المشروع في Partner Center:

- Name: `15145Ahmedkamel.AzkarReminder`.
- Publisher: `CN=BD257909-6037-4C23-A6F3-A71B4BD2AF69`.
- PublisherDisplayName: `Ahmedkamel`.

لا تغيّر هذه القيم دون تغيير التسجيل المقابل. `AppxManifest.xml` والأصول مدمجة في المصدر. ملفات الإصدار 1.2.4 الموجودة تاريخيًا في هذا المجلد تخص ذلك الإصدار؛ الإصلاحات الجديدة لا تعيد كتابة تلك الملفات.

## Build

شغّل من PowerShell مع MSVC وWindows SDK وCMake متوفرة:

```powershell
./installer/msix/build_msix.ps1
```

السكربت يقرأ الإصدار من CMake، يعيد بناء Release، يرفض اختلاف إصدار executable، ويستدعي MakeAppx مع فحص رمز الخروج. النتيجة `release/AzkarReminder-<version>.msix` وبصمة SHA-256. يمكنك تحديد `-BuildDirectory` و`-OutputDirectory` لمسارات منفصلة. `scripts/build_msix.bat` يستدعي السكربت نفسه.

الوضع الافتراضي لا ينشئ شهادة ولا يغيّر مخزن الثقة. الحزمة غير موقعة محليًا؛ هذا لا يثبت قبول المتجر أو نجاح تثبيتها. Microsoft Store يتولى توقيع حزم MSIX خلال الإرسال وفق [دليل Microsoft](https://learn.microsoft.com/en-us/windows/msix/package/sign-msix-package-guide).

## Local signing

للتوقيع المحلي استخدم شهادة code signing تملك مفتاحها الخاص، ويطابق Subject الخاص بها Publisher في manifest:

```powershell
./installer/msix/build_msix.ps1 -SignForLocalTest -CertificateThumbprint '<thumbprint>'
# Or sign an existing package:
./installer/msix/sign_msix.ps1 -PackagePath './release/AzkarReminder-1.2.5.msix' -CertificateThumbprint '<thumbprint>'
```

التوقيع المستقل يحدث بصمة الحزمة بعد نجاحه. ملف `.cer` هو شهادة عامة ولا يحتوي المفتاح الخاص اللازم للتوقيع. سكربتا التغليف والتوقيع لا يثبتان شهادة ثقة ولا يثبتان الحزمة.

اختبار CI المنفصل `scripts/smoke_windows_packages.ps1` وقّع نسخة مؤقتة، وثبّتها وفعّلها عبر AUMID، ثم تحقق من هوية العملية ومسار بياناتها وإزالتها. شهادة الثقة مؤقتة داخل runner مخصص؛ الحزمة المرفوعة تبقى غير موقعة. [سجل التحقق](../../docs/BUILD_VALIDATION.md) يحدد البيئة والحدود. ترقية MSIX من إصدار أقدم وقبول Partner Center لم يُختبرا. التشغيل عند تسجيل الدخول يحتاج اختبارًا منفصلًا؛ كتابة HKCU Run قد تخضع لعزل MSIX، ويحتاج دعم المتجر مراجعة `StartupTask`.
