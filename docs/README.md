# گزارش پروژه

فایل اصلی گزارش `relay_on_duration_report.tex` و تنظیمات ظاهر در `report-config.tex` است. فصل‌ها در `chapters/` مستقل‌اند. فصل‌های ۱۰ تا ۱۵ به‌ترتیب مرجع توابع کاربرد، مرجع درایورها، شرح جزئی بازآرایی، مرجع کد تولیدشده، آزمون چراغ‌های عیب‌یابی و راه‌اندازی کامل ADE7880 هستند.

فاصله خطوط، فاصله پاراگراف‌ها، فاصله ردیف جدول، رنگ‌ها و فونت‌ها همگی در `report-config.tex` متمرکز شده‌اند. مقدار فعلی فاصله خطوط ۱٫۴۵، فاصله پاراگراف ۸ پوینت و ضریب ارتفاع ردیف جدول ۱٫۳۳ است.

ساخت:

```text
xelatex -interaction=nonstopmode relay_on_duration_report.tex
xelatex -interaction=nonstopmode relay_on_duration_report.tex
```

موتور لازم XeLaTeX است. قالب از XePersian استفاده می‌کند؛ فونت ترجیحی B Nazanin و جایگزین خودکار Tahoma است. تعداد صفحه خروجی مرجع پس از هر تغییر با `pdfinfo` کنترل و در فایل وضعیت ثبت می‌شود.
