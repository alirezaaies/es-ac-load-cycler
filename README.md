# RelayOnDuration firmware

فریم‌ور `STM32F107VCT6` برای سامانه پایش و کنترل اتاق آزمون است. توسعه پروژه مرحله‌ای انجام می‌شود و وضعیت عملیاتی هر مرحله در [PROJECT_STATUS.md](PROJECT_STATUS.md) و گزارش کامل فارسی در `docs/relay_on_duration_report.tex` ثبت می‌شود.

## وضعیت فعلی — ۱۴۰۵/۰۶/۲۵ (2026-09-16)

نسخه حاضر «پایه تمیز + LED عیب‌یابی + ارتباط ADE7880» است:

- دو LED روی `PC13/PC14` زنده‌بودن حلقه اصلی و خطای ارتباط را نشان می‌دهند.
- `SPI2` با پایه‌های `PB13/PB14/PB15` و انتخاب تراشه `PB12` به ADE7880 متصل است.
- کتابخانه ADE7880 مستقل از STM32 HAL و مبتنی بر callback است و در پروژه‌های دیگر قابل استفاده مجدد است.
- ولتاژ RMS، جریان RMS، توان اکتیو، توان ظاهری و PF هر سه فاز و جریان نول هر ثانیه خوانده می‌شوند.
- snapshot خام، ولتاژ، جریان، توان اکتیو، توان ظاهری و PF در متغیر `g_app_electrical` برای Watch دیباگر منتشر می‌شوند.
- در خطای ارتباط، LEDها سریع یکی‌درمیان می‌شوند و برنامه هر دو ثانیه بازیابی را امتحان می‌کند.
- ساخت PlatformIO با `-Wall -Wextra -Werror` موفق است: ۹۶۴۰ بایت Flash و ۴۷۲ بایت RAM.
- ساخت واقعی Keil با Arm Compiler 6.24 موفق است: `0 Error(s), 0 Warning(s)`.
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

`MDK-ARM/STM32F107.uvprojx` را باز کنید، Target را Build و سپس با J-Link پروگرام کنید. فایل ADE7880 و مسیرهای include از قبل در پروژه هستند. این نسخه با Arm Compiler 6.24 ساخته و با نتیجه `0 Error(s), 0 Warning(s)` تأیید شده است. اندازه گزارش‌شده Keil برابر `Code=9628`، `RO-data=476`، `RW-data=12` و `ZI-data=1460` بایت است.

اگر خطاهای متعدد `unknown type name DMA_HandleTypeDef` در هدرهای HAL دیده شدند، ابتدا تعریف `HAL_DMA_MODULE_ENABLED` را در `Core/Inc/stm32f1xx_hal_conf.h` و حضور `stm32f1xx_hal_dma.c` در پروژه کنترل کنید. فعال‌بودن این ماژول برای تعریف نوع‌های DMA مورد استفاده داخل handleهای HAL لازم است؛ این موضوع به‌تنهایی به معنی استفاده برنامه از انتقال DMA نیست.

## مشاهده نتیجه ADE7880

متغیر `g_app_electrical` را به Watch اضافه کنید. در حالت سالم:

- `online = true`
- `last_status = ADE7880_STATUS_OK`
- `successful_samples` تقریباً هر ثانیه افزایش می‌یابد.
- `communication_errors` ثابت می‌ماند.
- `voltage_v[0]` و `voltage_v[1]` ولتاژ تقریبی فازهای A و B را نشان می‌دهند.
- `voltage_valid[0]` و `voltage_valid[1]` باید پس از نخستین نمونه موفق `true` باشند.
- `raw.phase[0].current_rms`، `active_power` و `apparent_power` شمار خام فاز A هستند؛ اندیس‌های ۱ و ۲ فازهای B و C هستند.
- `power_factor[0..2]` مستقیماً از قالب Q1.15 تراشه به بازه ۱- تا ۱+ تبدیل می‌شود و پس از نمونه موفق، `power_factor_valid` درست است.
- `current_a`، `active_power_w` و `apparent_power_va` فقط وقتی قابل استفاده‌اند که valid متناظر درست باشد. این پرچم‌ها تا ثبت کالیبراسیون واقعی جریان و توان نادرست می‌مانند.
- LEDها الگوی رقص دارند.

مقادیر `raw` شمار رجیستر هستند. تبدیل ولتاژ A/B فعلاً از ضرایب بازیابی‌شده برنامه قدیمی استفاده می‌کند و برای راه‌اندازی اولیه مناسب است، اما باید با مولتی‌متر مرجع کنترل شود. تبدیل دقیق جریان و توان به نسبت CT، مقاومت burden و کالیبراسیون مرجع نیاز دارد. راهنمای کوتاه استفاده و مثال کالیبراسیون در `Libraries/ADE7880/README.md` قرار دارد.

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
