# ES AC Load Cycler

مخزن یکپارچه نرم‌افزار، سخت‌افزار و مستندات سامانه پایش و کنترل اتاق آزمون تجهیزات برودتی است.

## ساختار مخزن

- `embedded/stm32f107/`: فریم‌ور STM32F107VCT6، پروژه‌های PlatformIO، STM32CubeMX و Keil
- `hardware/altium/`: شماتیک‌ها، PCB، کتابخانه‌ها و پروژه Altium
- `docs/`: منبع LaTeX و PDF گزارش فنی پروژه
- `datasheets/`: دیتاشیت قطعات اصلی
- `PROJECT_STATUS.md`: وضعیت جاری، تصمیم‌های فنی، اعتبارسنجی‌ها و کارهای باز

## فریم‌ور

راهنمای ساخت، پروگرام، دیباگ ADE7880 و کالیبراسیون در
[`embedded/stm32f107/README.md`](embedded/stm32f107/README.md) قرار دارد.

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
