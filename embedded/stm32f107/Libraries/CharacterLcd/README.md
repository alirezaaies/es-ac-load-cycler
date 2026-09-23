# کتابخانه نمایشگر کاراکتری

این کتابخانه کنترلرهای سازگار با `HD44780/ST7066` را در حالت چهار بیتی راه‌اندازی می‌کند و به HAL یا مدل مشخصی از میکروکنترلر وابسته نیست. برنامه فقط پنج callback برای `RS`، `R/W`، `E`، نیم‌بایت `DB4..DB7` و تأخیر میکروثانیه فراهم می‌کند.

## رابط ساده

- `CharacterLcd_Init`: راه‌اندازی چهار بیتی و روشن‌کردن نمایشگر
- `CharacterLcd_Clear` و `CharacterLcd_Home`: پاک‌کردن و بازگشت مکان‌نما
- `CharacterLcd_SetCursor`: انتخاب ستون و سطر با اندیس صفر
- `CharacterLcd_PutChar` و `CharacterLcd_Print`: نمایش کاراکتر و رشته C
- `CharacterLcd_PrintUInt32` و `CharacterLcd_PrintInt32`: نمایش عدد بدون نیاز به `sprintf`
- `CharacterLcd_SetDisplay`: کنترل نمایشگر، cursor و blink
- `CharacterLcd_Shift`: جابه‌جایی پنجره دید
- `CharacterLcd_CreateChar`: تعریف حداکثر هشت شکل ۵×۸ در CGRAM

نمونه استفاده پس از مقداردهی callbackها:

```c
CharacterLcd lcd;
CharacterLcd_Config config = {
    .context = &my_board,
    .write_rs = board_write_rs,
    .write_rw = board_write_rw,
    .write_enable = board_write_enable,
    .write_data4 = board_write_nibble,
    .delay_us = board_delay_us,
    .columns = 16,
    .rows = 2
};

if (CharacterLcd_Init(&lcd, &config)) {
    CharacterLcd_Print(&lcd, "Temperature:");
    CharacterLcd_SetCursor(&lcd, 0, 1);
    CharacterLcd_PrintInt32(&lcd, 25);
    CharacterLcd_PutChar(&lcd, (char)0xDF);
    CharacterLcd_PutChar(&lcd, 'C');
}
```

برای یک شکل سفارشی:

```c
static const uint8_t bell[8] = {
    0x04, 0x0E, 0x0E, 0x0E, 0x1F, 0x00, 0x04, 0x00
};
CharacterLcd_CreateChar(&lcd, 0, bell);
CharacterLcd_PutChar(&lcd, 0);
```

کتابخانه عمداً فقط می‌نویسد و `R/W` را همیشه Low نگه می‌دارد. زمان‌های ثابت از دیتاشیت ماژول Vishay گرفته شده‌اند؛ بنابراین جهت پایه‌های داده هیچ‌گاه ورودی نمی‌شود. اگر `R/W` مستقیماً زمین شده باشد callback آن را `NULL` قرار دهید.

نمایشگر متن Unicode یا فارسی را به‌طور بومی پشتیبانی نمی‌کند. مدل `ET` جدول کاراکتر انگلیسی/اروپایی دارد و فقط هشت شکل سفارشی هم‌زمان در CGRAM جا می‌گیرد.

## انتقال به برد دیگر

پوشه `Libraries/CharacterLcd` را منتقل، مسیر `Inc` را به include path اضافه و فایل `Src/character_lcd.c` را وارد build کنید. سپس callbackهای GPIO و delay را در لایه برد جدید بنویسید. هیچ پایه‌ای در خود کتابخانه hard-code نشده است.
