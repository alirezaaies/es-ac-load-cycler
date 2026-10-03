# RelayOnDuration firmware

Active firmware: electrical load cycler with active-low PE1/PE2 relays. See [LOAD_CYCLER.md](LOAD_CYCLER.md) for operation, configuration and validation. Historical baseline notes follow.

فریم‌ور `STM32F107VCT6` برای سامانه پایش و کنترل اتاق آزمون است. توسعه پروژه مرحله‌ای انجام می‌شود و وضعیت عملیاتی هر مرحله در [PROJECT_STATUS.md](../../PROJECT_STATUS.md) ثبت می‌شود. گزارش کامل فارسی در `../../docs/relay_on_duration_report.tex` قرار دارد.

## وضعیت فعلی — ۱۴۰۵/۰۶/۲۸ (2026-09-19)

نسخه حاضر «پایه تمیز + LED عیب‌یابی + ارتباط ADE7880 + دمای DS18B20» است:

- دو LED روی `PC13/PC14` زنده‌بودن حلقه اصلی و خطای ارتباط را نشان می‌دهند.
- `SPI2` با پایه‌های `PB13/PB14/PB15` و انتخاب تراشه `PB12` به ADE7880 متصل است.
- کتابخانه ADE7880 مستقل از STM32 HAL و مبتنی بر callback است و در پروژه‌های دیگر قابل استفاده مجدد است.
- ولتاژ RMS، جریان RMS، توان اکتیو، توان ظاهری و PF هر سه فاز و جریان نول هر ثانیه خوانده می‌شوند.
- snapshot خام، ولتاژ، جریان، توان اکتیو، توان ظاهری و PF در متغیر `g_app_electrical` برای Watch دیباگر منتشر می‌شوند.
- در خطای ارتباط، LEDها سریع یکی‌درمیان می‌شوند و برنامه هر دو ثانیه بازیابی را امتحان می‌کند.
- دو گذرگاه 1-Wire روی `PB10` و `PC7` سنسورهای DS18B20 را کشف می‌کنند و جدول شماره منطقی ۱ تا ۳۲ با CRC در Flash می‌ماند.
- تبدیل دما سراسری و غیرمسدودکننده است و ROM و scratchpad هر سنسور با CRC کنترل می‌شوند.
- سنسور تازه بدون هیچ فراخوانی دستی، خودکار به اولین شماره خالی متصل و نگاشت آن ذخیره می‌شود.
- دما و اعتبار هر شماره مستقیماً در `g_temperature_c[]` و `g_temperature_valid[]` برای Watch منتشر می‌شود.
- ساخت PlatformIO با `-Wall -Wextra -Werror` موفق است: ۱۴۹۲۸ بایت Flash و ۳۲۰۴ بایت RAM.
- پروژه Keil با سورس و include جدید همگام و XML آن معتبر است؛ ساخت واقعی همین تغییر DS18B20 باید در ایستگاه دارای Keil تکرار شود.
- ارتباط، ولتاژ و پاسخ جریان/توان با یک بار لامپی روی برد واقعی آزموده شده‌اند؛ کالیبراسیون دقیق با ابزار مرجع هنوز باقی است.

رطوبت، UART، کنترل رله‌ها و اتصال دما به پروتکل/کنترل هنوز در snapshot تمیز فعال نشده‌اند و باید در مراحل بعد به‌صورت کنترل‌شده برگردند. خواندن مستقل دما فعال است، اما آزمون الکتریکی آن روی برد هنوز لازم است.

## ساختار فایل‌ها

- فایل‌های مولد CubeMX: `Core/`, `STM32F107.ioc`
- منطق برنامه: `Application/Inc/app.h`, `Application/Src/app.c`
- رابط ساده دما برای Watch و کد کاربردی: `Application/Inc/app_temperature.h`
- LED عیب‌یابی: `Application/Inc/diagnostic_led.h`, `Application/Src/diagnostic_led.c`
- کتابخانه قابل‌حمل ADE7880: `Libraries/ADE7880/`
- لایه‌های قابل‌حمل دما: `Libraries/OneWire/`, `Libraries/DS18B20/`, `Libraries/DS18B20Manager/`
- ذخیره نگاشت دما در STM32: `Libraries/SensorAddressStore/`
- پروژه Keil: `MDK-ARM/STM32F107.uvprojx`
- پیکربندی PlatformIO: `platformio.ini`
- دیتاشیت‌ها: `../../datasheets/`
- گزارش منبع و PDF: `../../docs/relay_on_duration_report.tex`, `../../docs/relay_on_duration_report.pdf`

## ساخت و پروگرام در VS Code / PlatformIO

### آزمون LCD و سه کلید

نمایشگر سازگار با ST7066 در حالت چهار بیتی به `PA6=E`، `PA7=R/W`، `PA8=RS` و `PA9..PA12=DB4..DB7` متصل است. سه کلید active-high با پایین‌کش خارجی روی `PD9=INC`، `PD10=DEC` و `PD11=RESET` قرار دارند. پس از بوت، سطر دوم مقدار صفر تا صد را نشان می‌دهد؛ هر فشار/رهاسازی معتبر فقط یک تغییر می‌سازد و نگه‌داشتن کلید تکرار خودکار ندارد.

کتابخانه مستقل، API و نمونه انتقال در `Libraries/CharacterLcd/README.md` است. برنامه تست و debounce در `Application/Src/ui_counter.c` قرار دارد و مقدار را می‌توان با `g_ui_counter_value` در Watch دید. نمایشگر Unicode/فارسی را بومی پشتیبانی نمی‌کند؛ متن فعلی ASCII و برای شکل‌های خاص حداکثر هشت جایگاه CGRAM موجود است.

هشدار سخت‌افزاری: دیتاشیت این مدل در تغذیه ۵ ولت، `VIH(min)=0.7VDD=3.5V` می‌خواهد. سطح ۳٫۳ ولتی STM32 تضمین دیتاشیت را ندارد؛ برای محصول نهایی مبدل سطح یا نسخه/تغذیه منطقی سازگار لازم است.

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

`MDK-ARM/STM32F107.uvprojx` را باز کنید، Target را Build و سپس با J-Link پروگرام کنید. فایل‌های ADE7880، OneWire، DS18B20، مدیر دما و مسیرهای include در پروژه هستند. آخرین baseline پیش از زیرسامانه جدید با Arm Compiler 6.24 و نتیجه `0 Error(s), 0 Warning(s)` ساخته شده بود؛ چون Keil در محیط این بازبینی نصب نیست، ساخت واقعی نسخه حاضر باید روی ایستگاه توسعه تکرار شود. فایل `.uvoptx` شامل چیدمان شخصی پنجره‌ها، Watch و جلسه‌ی Keil است؛ محلی باقی می‌ماند اما عمداً در Git نگه‌داری نمی‌شود.

اگر خطاهای متعدد `unknown type name DMA_HandleTypeDef` در هدرهای HAL دیده شدند، ابتدا تعریف `HAL_DMA_MODULE_ENABLED` را در `Core/Inc/stm32f1xx_hal_conf.h` و حضور `stm32f1xx_hal_dma.c` در پروژه کنترل کنید. فعال‌بودن این ماژول برای تعریف نوع‌های DMA مورد استفاده داخل handleهای HAL لازم است؛ این موضوع به‌تنهایی به معنی استفاده برنامه از انتقال DMA نیست.

## راه‌اندازی و مشاهده DS18B20

اتصال فعلی سه‌سیمه و با تغذیه خارجی است: گذرگاه صفر روی `PB10` و گذرگاه یک روی `PC7` قرار دارد. خط داده هر گذرگاه باید مقاومت pull-up خارجی داشته باشد؛ حالت parasite power در این نسخه پشتیبانی نشده است.

هیچ تابعی را از داخل دیباگر فراخوانی نکنید. فقط سنسور را وصل کنید؛ برنامه آن را خودکار کشف، به اولین شماره خالی متصل و در Flash ذخیره می‌کند. برای حفظ ترتیب برچسب‌های فیزیکی، ابتدا سنسور ۱ را وصل و منتظر معتبرشدن آن شوید، سپس سنسور ۲ و سنسورهای بعدی را یکی‌یکی اضافه کنید.

مقدار سنسور شماره `N` در مسیر ساده زیر است:

```c
g_temperature_c[N - 1]
```

و اعتبار آن در مسیر زیر قرار دارد:

```c
g_temperature_valid[N - 1]
```

برای یک سنسور فقط `g_temperature_c[0]` و `g_temperature_valid[0]` را Watch کنید. `g_temperature_sensor_count` تعداد سنسورهای ثبت‌شده را نشان می‌دهد. در کد نیز `App_TemperatureGetCelsius(1, &value)` فقط وقتی مقدار معتبر باشد `true` برمی‌گرداند.

اگر روی یک گذرگاه دقیقاً یک سنسور شماره‌دار غایب و دقیقاً یک ROM جدید حاضر باشد، جایگزینی خودکار و پایدار انجام می‌شود. در حالت مبهم، مدیر حدس نمی‌زند و سنسورها باید یکی‌یکی تعویض شوند؛ ابزار تعمیراتی می‌تواند مستقیماً از API پیشرفته مدیر استفاده کند. رابط عمومی برنامه عمداً ROM، bus و توابع تخصیص را نشان نمی‌دهد. شرح انتقال به MCU دیگر، Flash و چک‌لیست آزمون در [`Libraries/DS18B20Manager/README.md`](Libraries/DS18B20Manager/README.md) و فصل ۱۶ گزارش آمده است.

## مشاهده نتیجه ADE7880

متغیر `g_app_electrical` را به Watch اضافه کنید. در حالت سالم:

- `online = true`
- `last_status = ADE7880_STATUS_OK`
- `successful_samples` تقریباً هر ثانیه افزایش می‌یابد.
- `communication_errors` ثابت می‌ماند.
- `voltage_v[0..2]` ولتاژ تقریبی فازهای A، B و C را نشان می‌دهد.
- پرچم‌های `voltage_valid[0..2]` پس از نخستین نمونه موفق `true` می‌شوند.
- `raw.phase[0].current_rms`، `active_power` و `apparent_power` شمار خام فاز A هستند؛ اندیس‌های ۱ و ۲ فازهای B و C هستند.
- `power_factor[0..2]` مستقیماً از قالب Q1.15 تراشه به بازه ۱- تا ۱+ تبدیل می‌شود و پس از نمونه موفق، `power_factor_valid` درست است.
- `power_factor_abs[0..2]` فقط بزرگی ضریب توان را از صفر تا یک نشان می‌دهد؛ علامت `power_factor` اطلاعات پیش‌فاز/پس‌فاز را حفظ می‌کند.
- `current_polarity[0..2]` جهت جبران نرم‌افزاری CT را نشان می‌دهد و به‌طور پیش‌فرض `APP_CURRENT_POLARITY_NORMAL` است.
- `current_a[0..2]`، `active_power_w[0..2]` و `apparent_power_va[0..2]` برای هر سه فاز با جدول پیش‌فرض منتشر می‌شوند و source اولیه آن‌ها `APP_CALIBRATION_DEFAULT` است.
- `current_display[0..2]` به‌طور پیش‌فرض صدم آمپر و فیلدهای نمایش توان دهم وات/ولت‌آمپر را نشان می‌دهند؛ `App_SetDisplayUnits()` فقط نمایش را تغییر می‌دهد.
- `neutral_current_a` نیز با ضریب موقت جریان فازها فعال است؛ پیش از استفاده دقیق، آن را مستقل کالیبره کنید.
- ضرایب فاز C و نول فعلاً fallback هستند. `valid=true` یعنی تبدیل اجرا شده است، نه اینکه دقت آزمایشگاهی ضریب تأیید شده باشد.
- LEDها الگوی رقص دارند.

مقادیر `raw` شمار رجیستر هستند. جدول `default_calibration` در `Application/Src/app.c` همه ضرایب startup را به‌صورت صریح و سه‌فاز نگه می‌دارد. مقدارهای A/B از برنامه قبلی آمده‌اند؛ C از B و نول از جریان فاز کپی شده‌اند. این مقادیر برای راه‌اندازی مناسب‌اند، اما هر ورودی واقعی باید با ابزار مرجع کنترل شود.

تست لامپ نشان داد خروجی‌های قدیمی جریان و توان fixed-point بوده‌اند: عدد قدیمی `17` برابر حدود `0.17 A` و `231` برابر حدود `23.1 W` است. تبدیل فعلی این تقسیم بر ۱۰۰ و ۱۰ را داخل نرم‌افزار انجام می‌دهد و فیلدهای Watch مستقیماً آمپر و وات هستند. برای یک مصرف‌کننده، CT را طوری نصب کنید که `active_power_w` مثبت شود. اگر تغییر فیزیکی ممکن نیست، پس از `App_Init()` یک‌بار `App_SetCurrentPolarity(ADE7880_PHASE_A, APP_CURRENT_POLARITY_REVERSED)` را فراخوانی کنید. این تنظیم را خودکار نکنید، زیرا توان برگشتی واقعی می‌تواند منفی باشد. ثانویه CT را هنگام عبور جریان اولیه هرگز باز نگذارید.

برای کالیبراسیون، `App_CalibratePhaseMultiPoint()` حداقل دو و ترجیحاً سه تا پنج زوج raw/reference را برای ولتاژ، جریان، توان اکتیو یا توان ظاهری هر فاز نصب می‌کند. `App_CalibrateNeutralCurrentMultiPoint()` همین کار را برای NIRMS انجام می‌دهد. `App_ResetCalibrationToDefaults()` همه overrideهای RAM را حذف و جدول startup را دوباره فعال می‌کند. مثال‌ها و روش نگه‌داری ضرایب در `Libraries/ADE7880/README.md` و فصل ۱۵ گزارش آمده است.

## تولید مجدد CubeMX

تنظیم SPI، پایه‌های 1-Wire، LCD، کلیدها و وقفه‌های EXTI در `STM32F107.ioc` ثبت شده است. منطق محصول بیرون از فایل‌های مولد قرار دارد و فراخوانی‌های `App_Init(&hspi2)` و `App_Process()` در بلوک‌های `USER CODE` هستند. پس از Generate:

1. diff گیت را بررسی کنید.
2. وجود `MX_SPI2_Init`، پایه `ADE_CS=PB12` و حالت open-drain پایه‌های `PB10/PC7` را کنترل کنید.
3. عضویت فایل‌های `Application`، `Libraries/ADE7880`، `Libraries/CharacterLcd` و سه لایه دما در Keil را کنترل کنید.
4. هر دو build را دوباره اجرا کنید.

## ساخت گزارش

```text
cd ../../docs
xelatex -interaction=nonstopmode relay_on_duration_report.tex
xelatex -interaction=nonstopmode relay_on_duration_report.tex
```

PDF نهایی عمداً در Git نگه‌داری می‌شود؛ فایل‌های موقت LaTeX، PlatformIO و Keil نادیده گرفته می‌شوند.

## ورودی‌های لازم برای مرحله بعد

- نسبت CT/PT، مقدار burden و شبکه تقسیم ولتاژ ADE7880
- نتیجه تست روی برد و مقادیر raw در چند نقطه مرجع
- در صورت اتصال، پایه‌های واقعی `RESET` و `IRQ1` تراشه
- شماتیک نهایی برای فعال‌سازی امن سایر سنسورها و خروجی‌ها
