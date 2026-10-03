# ES AC Load Cycler

نسخه فعال تا 2026-10-03 روی برد توسط کاربر تأیید شده است: منوی سه‌مرحله‌ای، رله‌های active-low روی PE1/PE2 و نمایش V/A/P/PF. برای ادامه کار از [وضعیت پروژه](PROJECT_STATUS.md) و فصل ۱۸ [گزارش](docs/relay_on_duration_report.pdf) شروع کنید. راهنمای کوتاه در [LOAD_CYCLER.md](embedded/stm32f107/LOAD_CYCLER.md) است؛ تغییر احتمالی بعدی، ظاهر نمایشگر خواهد بود.

مخزن یکپارچه نرم‌افزار، سخت‌افزار و مستندات سامانه پایش و کنترل اتاق آزمون تجهیزات برودتی است.

## ساختار مخزن

- `embedded/stm32f107/`: فریم‌ور STM32F107VCT6، پروژه‌های PlatformIO، STM32CubeMX و Keil
- `hardware/altium/`: شماتیک‌ها، PCB، کتابخانه‌ها و پروژه Altium
- `docs/`: منبع LaTeX و PDF گزارش فنی پروژه
- `datasheets/`: دیتاشیت قطعات اصلی
- `PROJECT_STATUS.md`: وضعیت جاری، تصمیم‌های فنی، اعتبارسنجی‌ها و کارهای باز

## فریم‌ور

راهنمای ساخت، پروگرام، دیباگ ADE7880، شماره‌گذاری پایدار DS18B20 و کالیبراسیون در
[`embedded/stm32f107/README.md`](embedded/stm32f107/README.md) قرار دارد.

راهنمای مستقل زیرسامانه دما، شامل سیم‌بندی `PB10/PC7`، معرفی شماره‌ها، تعویض سنسور و انتقال کتابخانه به میکروکنترلر دیگر، در
[`embedded/stm32f107/Libraries/DS18B20Manager/README.md`](embedded/stm32f107/Libraries/DS18B20Manager/README.md) است.

ساخت با PlatformIO:

```text
cd embedded/stm32f107
pio run -e stm32f107vc_jlink
```

اگر `pio` در PATH ویندوز نیست:

```text
%USERPROFILE%\.platformio\penv\Scripts\platformio.exe run -e stm32f107vc_jlink
```

## سخت‌افزار

پروژه اصلی Altium:

```text
hardware/altium/PCB_Project.PrjPcb
```

فایل‌های منبع طراحی track می‌شوند. History، preview، log، گزارش‌های تولیدی و وضعیت محلی Altium عمداً در Git نگه‌داری نمی‌شوند.

## ادامه کار

پیش از هر تغییر، [`PROJECT_STATUS.md`](PROJECT_STATUS.md) مطالعه و پس از پایان کار به‌روزرسانی شود.
