/** @file character_lcd.c @brief Portable ST7066/HD44780 4-bit driver. */
#include "character_lcd.h"

#include <stddef.h>

#define LCD_COMMAND_CLEAR 0x01U
#define LCD_COMMAND_HOME 0x02U
#define LCD_COMMAND_ENTRY_MODE 0x06U
#define LCD_COMMAND_DISPLAY_CONTROL 0x08U
#define LCD_COMMAND_SHIFT 0x10U
#define LCD_COMMAND_FUNCTION_SET 0x20U
#define LCD_FUNCTION_TWO_LINES 0x08U
#define LCD_COMMAND_SET_CGRAM 0x40U
#define LCD_COMMAND_SET_DDRAM 0x80U
#define LCD_DISPLAY_ON 0x04U
#define LCD_CURSOR_ON 0x02U
#define LCD_BLINK_ON 0x01U
#define LCD_SHIFT_DISPLAY 0x08U
#define LCD_SHIFT_RIGHT 0x04U

#define LCD_POWER_ON_DELAY_US 50000U
#define LCD_ENABLE_PULSE_US 1U
#define LCD_NORMAL_COMMAND_DELAY_US 50U
#define LCD_CLEAR_HOME_DELAY_US 1600U

static bool is_ready(const CharacterLcd *lcd)
{
    return (lcd != NULL) && lcd->initialized;
}

static void set_write_mode(CharacterLcd *lcd)
{
    if (lcd->config.write_rw != NULL) {
        lcd->config.write_rw(lcd->config.context, false);
    }
}

static void pulse_enable(CharacterLcd *lcd)
{
    lcd->config.write_enable(lcd->config.context, true);
    lcd->config.delay_us(lcd->config.context, LCD_ENABLE_PULSE_US);
    lcd->config.write_enable(lcd->config.context, false);
    lcd->config.delay_us(lcd->config.context, LCD_ENABLE_PULSE_US);
}

static void write_nibble(CharacterLcd *lcd, uint8_t nibble)
{
    lcd->config.write_data4(lcd->config.context, nibble & 0x0FU);
    lcd->config.delay_us(lcd->config.context, LCD_ENABLE_PULSE_US);
    pulse_enable(lcd);
}

static void write_byte(CharacterLcd *lcd, bool data, uint8_t value)
{
    lcd->config.write_rs(lcd->config.context, data);
    set_write_mode(lcd);
    write_nibble(lcd, value >> 4U);
    write_nibble(lcd, value);
    lcd->config.delay_us(lcd->config.context,
        ((value == LCD_COMMAND_CLEAR) || (value == LCD_COMMAND_HOME)) && !data
            ? LCD_CLEAR_HOME_DELAY_US
            : LCD_NORMAL_COMMAND_DELAY_US);
}

static uint8_t row_address(uint8_t row)
{
    static const uint8_t offsets[CHARACTER_LCD_MAX_ROWS] = {
        0x00U, 0x40U, 0x14U, 0x54U
    };
    return offsets[row];
}

bool CharacterLcd_Init(CharacterLcd *lcd, const CharacterLcd_Config *config)
{
    if ((lcd == NULL) || (config == NULL) ||
        (config->write_rs == NULL) || (config->write_enable == NULL) ||
        (config->write_data4 == NULL) || (config->delay_us == NULL) ||
        (config->columns == 0U) || (config->rows == 0U) ||
        (config->rows > CHARACTER_LCD_MAX_ROWS)) {
        return false;
    }

    lcd->config = *config;
    lcd->display_control = LCD_DISPLAY_ON;
    lcd->row = 0U;
    lcd->column = 0U;
    lcd->initialized = false;

    lcd->config.write_rs(lcd->config.context, false);
    set_write_mode(lcd);
    lcd->config.write_enable(lcd->config.context, false);
    lcd->config.write_data4(lcd->config.context, 0U);
    lcd->config.delay_us(lcd->config.context, LCD_POWER_ON_DELAY_US);

    /* Vendor four-bit startup sequence; the busy flag is not yet readable. */
    write_nibble(lcd, 0x03U);
    lcd->config.delay_us(lcd->config.context, LCD_NORMAL_COMMAND_DELAY_US);
    write_nibble(lcd, 0x02U);
    lcd->config.delay_us(lcd->config.context, LCD_NORMAL_COMMAND_DELAY_US);

    lcd->initialized = true;
    write_byte(lcd, false,
               (uint8_t)(LCD_COMMAND_FUNCTION_SET |
                         ((config->rows > 1U) ? LCD_FUNCTION_TWO_LINES : 0U)));
    CharacterLcd_SetDisplay(lcd, false, false, false);
    CharacterLcd_Clear(lcd);
    write_byte(lcd, false, LCD_COMMAND_ENTRY_MODE);
    CharacterLcd_SetDisplay(lcd, true, false, false);
    return true;
}

void CharacterLcd_Clear(CharacterLcd *lcd)
{
    if (!is_ready(lcd)) return;
    write_byte(lcd, false, LCD_COMMAND_CLEAR);
    lcd->row = 0U;
    lcd->column = 0U;
}

void CharacterLcd_Home(CharacterLcd *lcd)
{
    if (!is_ready(lcd)) return;
    write_byte(lcd, false, LCD_COMMAND_HOME);
    lcd->row = 0U;
    lcd->column = 0U;
}

bool CharacterLcd_SetCursor(CharacterLcd *lcd, uint8_t column, uint8_t row)
{
    if (!is_ready(lcd) || (column >= lcd->config.columns) ||
        (row >= lcd->config.rows)) {
        return false;
    }
    write_byte(lcd, false,
               (uint8_t)(LCD_COMMAND_SET_DDRAM + row_address(row) + column));
    lcd->row = row;
    lcd->column = column;
    return true;
}

void CharacterLcd_PutChar(CharacterLcd *lcd, char character)
{
    if (!is_ready(lcd)) return;
    if (character == '\n') {
        (void)CharacterLcd_SetCursor(lcd, 0U,
            (uint8_t)((lcd->row + 1U) % lcd->config.rows));
        return;
    }

    write_byte(lcd, true, (uint8_t)character);
    ++lcd->column;
    if (lcd->column >= lcd->config.columns) {
        (void)CharacterLcd_SetCursor(lcd, 0U,
            (uint8_t)((lcd->row + 1U) % lcd->config.rows));
    }
}

void CharacterLcd_Print(CharacterLcd *lcd, const char *text)
{
    if (!is_ready(lcd) || (text == NULL)) return;
    while (*text != '\0') CharacterLcd_PutChar(lcd, *text++);
}

void CharacterLcd_PrintUInt32(CharacterLcd *lcd, uint32_t value)
{
    char digits[10];
    uint8_t count = 0U;

    if (!is_ready(lcd)) return;
    do {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while ((value != 0U) && (count < sizeof(digits)));
    while (count > 0U) CharacterLcd_PutChar(lcd, digits[--count]);
}

void CharacterLcd_PrintInt32(CharacterLcd *lcd, int32_t value)
{
    uint32_t magnitude;

    if (!is_ready(lcd)) return;
    if (value < 0) {
        CharacterLcd_PutChar(lcd, '-');
        magnitude = (uint32_t)(-(value + 1)) + 1U;
    } else {
        magnitude = (uint32_t)value;
    }
    CharacterLcd_PrintUInt32(lcd, magnitude);
}

void CharacterLcd_SetDisplay(CharacterLcd *lcd, bool display_on,
                             bool cursor_on, bool blink_on)
{
    if (!is_ready(lcd)) return;
    lcd->display_control = (display_on ? LCD_DISPLAY_ON : 0U) |
                           (cursor_on ? LCD_CURSOR_ON : 0U) |
                           (blink_on ? LCD_BLINK_ON : 0U);
    write_byte(lcd, false,
               (uint8_t)(LCD_COMMAND_DISPLAY_CONTROL | lcd->display_control));
}

void CharacterLcd_Shift(CharacterLcd *lcd, bool right)
{
    if (!is_ready(lcd)) return;
    write_byte(lcd, false, (uint8_t)(LCD_COMMAND_SHIFT | LCD_SHIFT_DISPLAY |
                                    (right ? LCD_SHIFT_RIGHT : 0U)));
}

bool CharacterLcd_CreateChar(CharacterLcd *lcd, uint8_t slot,
                             const uint8_t pattern[8])
{
    uint8_t saved_row;
    uint8_t saved_column;

    if (!is_ready(lcd) || (slot >= CHARACTER_LCD_CUSTOM_CHARACTER_COUNT) ||
        (pattern == NULL)) {
        return false;
    }
    saved_row = lcd->row;
    saved_column = lcd->column;
    write_byte(lcd, false,
               (uint8_t)(LCD_COMMAND_SET_CGRAM | (uint8_t)(slot << 3U)));
    for (uint8_t row = 0U; row < 8U; ++row) {
        write_byte(lcd, true, pattern[row] & 0x1FU);
    }
    return CharacterLcd_SetCursor(lcd, saved_column, saved_row);
}
