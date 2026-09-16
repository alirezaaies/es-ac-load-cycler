# RelayOnDuration firmware

فریم‌ور `STM32F107VCT6` برای سامانه پایش و کنترل اتاق آزمون است. توسعه پروژه مرحله‌ای انجام می‌شود و وضعیت عملیاتی هر مرحله در [PROJECT_STATUS.md](PROJECT_STATUS.md) و گزارش کامل فارسی در `docs/relay_on_duration_report.tex` ثبت می‌شود.

## وضعیت فعلی — ۱۴۰۵/۰۶/۲۵ (2026-09-16)

نسخه حاضر «پایه تمیز + LED عیب‌یابی + ارتباط ADE7880» است:

- دو LED روی `PC13/PC14` زنده‌بودن حلقه اصلی و خطای ارتباط را نشان می‌دهند.
- `SPI2` با پایه‌های `PB13/PB14/PB15` و انتخاب تراشه `PB12` به ADE7880 متصل است.
- کتابخانه ADE7880 مستقل از STM32 HAL و مبتنی بر callback است و در پروژه‌های دیگر قابل استفاده مجدد است.
- ولتاژ RMS، جریان RMS، توان اکتیو، توان ظاهری و PF هر سه فاز و جریان نول هر ثانیه خوانده می‌شوند.
- snapshot خام در متغیر `g_app_electrical` برای Watch دیباگر منتشر می‌شود.
- در خطای ارتباط، LEDها سریع یکی‌درمیان می‌شوند و برنامه هر دو ثانیه بازیابی را امتحان می‌کند.
- ساخت PlatformIO با `-Wall -Wextra -Werror` موفق است: ۸۰۰۰ بایت Flash و ۲۶۰ بایت RAM.
- آزمون واقعی ADE7880 روی برد و کالیبراسیون هنوز انجام نشده است.

قابلیت‌های کامل محصول مانند حسگرهای دما، رطوبت، UART و کنترل رله‌ها در تاریخچه گزارش ثبت‌اند، اما در snapshot تمیز فعلی هنوز دوباره فعال نشده‌اند و باید در مراحل بعد به‌صورت کنترل‌شده برگردند.

## ساختار فایل‌ها

- فایل‌های مولد CubeMX: `Core/`, `STM32F107.ioc`
- منطق برنامه: `Application/Inc/app.h`, `Application/Src/app.c`
- LED عیب‌یابی: `Application/Inc/diagnostic_led.h`, `Application/Src/diagnostic_led.c`
- کتابخانه قابل‌حمل ADE7880: `Libraries/ADE7880/`
- پروژه Keil: `MDK-ARM/STM32F107.uvprojx`
- پیکربندی PlatformIO: `platformio.ini`
- دیتاشیت‌ها: `datasheets/`
- گزارش منبع و PDF: `docs/relay_on_duration_report.tex`, `docs/relay_on_duration_report.pdf`

## ساخت و پروگرام در VS Code / PlatformIO

```text
pio run -e stm32f107vc_jlink
pio run -e stm32f107vc_jlink --target upload
```

اگر `pio` در PATH ویندوز نیست:

```text
%USERPROFILE%\.platformio\penv\Scripts\platformio.exe run -e stm32f107vc_jlink
```

برای پروگرام J-Link اتصال‌های `PA13=SWDIO`، `PA14=SWCLK`، زمین و مرجع ۳٫۳ ولت لازم‌اند.

## ساخت در Keil

`MDK-ARM/STM32F107.uvprojx` را باز کنید، Target را Build و سپس با J-Link پروگرام کنید. فایل ADE7880 و مسیرهای include از قبل در پروژه هستند. build واقعی Keil این نسخه هنوز باید روی سیستم دارای Keil انجام و نتیجه آن ثبت شود.

## مشاهده نتیجه ADE7880

متغیر `g_app_electrical` را به Watch اضافه کنید. در حالت سالم:

- `online = true`
- `last_status = ADE7880_STATUS_OK`
- `successful_samples` تقریباً هر ثانیه افزایش می‌یابد.
- `communication_errors` ثابت می‌ماند.
- LEDها الگوی رقص دارند.

مقادیر `raw` شمار رجیستر هستند. تبدیل دقیق به ولت، آمپر و وات به نسبت CT/PT و کالیبراسیون مرجع نیاز دارد. API کالیبراسیون دو نقطه‌ای در کتابخانه آماده است، ولی برنامه عمداً ضریب حدسی اعمال نمی‌کند.

## تولید مجدد CubeMX

تنظیم SPI و پایه‌ها در `STM32F107.ioc` ثبت شده است. منطق محصول بیرون از فایل‌های مولد قرار دارد و فراخوانی‌های `App_Init(&hspi2)` و `App_Process()` در بلوک‌های `USER CODE` هستند. پس از Generate:

1. diff گیت را بررسی کنید.
2. وجود `MX_SPI2_Init` و پایه `ADE_CS=PB12` را کنترل کنید.
3. عضویت فایل‌های `Application` و `Libraries/ADE7880` در Keil را کنترل کنید.
4. هر دو build را دوباره اجرا کنید.

## ساخت گزارش

```text
cd docs
xelatex -interaction=nonstopmode relay_on_duration_report.tex
xelatex -interaction=nonstopmode relay_on_duration_report.tex
```

PDF نهایی عمداً در Git نگه‌داری می‌شود؛ فایل‌های موقت LaTeX، PlatformIO و Keil نادیده گرفته می‌شوند.

## ورودی‌های لازم برای مرحله بعد

- نسبت CT/PT، مقدار burden و شبکه تقسیم ولتاژ ADE7880
- نتیجه تست روی برد و مقادیر raw در چند نقطه مرجع
- در صورت اتصال، پایه‌های واقعی `RESET` و `IRQ1` تراشه
- شماتیک نهایی برای فعال‌سازی امن سایر سنسورها و خروجی‌ها
