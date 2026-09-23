/**
 * @file character_lcd.h
 * @brief Portable write-only driver for HD44780/ST7066 character LCDs.
 *
 * The driver owns no MCU registers and uses only callbacks supplied by the
 * application.  It therefore works with STM32 HAL, another MCU SDK, or direct
 * GPIO code without changes to this library.
 */
#ifndef CHARACTER_LCD_H
#define CHARACTER_LCD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHARACTER_LCD_MAX_ROWS 4U
#define CHARACTER_LCD_CUSTOM_CHARACTER_COUNT 8U

typedef void (*CharacterLcd_WritePin)(void *context, bool high);
typedef void (*CharacterLcd_WriteNibble)(void *context, uint8_t nibble);
typedef void (*CharacterLcd_DelayUs)(void *context, uint32_t microseconds);

/** MCU-specific wiring and display geometry supplied by the application. */
typedef struct {
    void *context;
    CharacterLcd_WritePin write_rs;
    CharacterLcd_WritePin write_rw; /**< Optional; NULL means R/W is grounded. */
    CharacterLcd_WritePin write_enable;
    CharacterLcd_WriteNibble write_data4; /**< Bits 0..3 map to DB4..DB7. */
    CharacterLcd_DelayUs delay_us;
    uint8_t columns;
    uint8_t rows;
} CharacterLcd_Config;

/** Driver state. Allocate one instance per physical display. */
typedef struct {
    CharacterLcd_Config config;
    uint8_t display_control;
    uint8_t row;
    uint8_t column;
    bool initialized;
} CharacterLcd;

/**
 * @brief Initialize one controller in 4-bit, 5x8-font mode.
 * @param lcd Caller-owned state retained for later operations.
 * @param config Complete callbacks and geometry; copied by value.
 * @return true on valid configuration; false without touching hardware when
 *         a required callback or geometry value is invalid.
 */
bool CharacterLcd_Init(CharacterLcd *lcd, const CharacterLcd_Config *config);

/** @brief Clear all cells and place the cursor at column 0, row 0.
 * @param lcd Initialized display instance; NULL/uninitialized is ignored. */
void CharacterLcd_Clear(CharacterLcd *lcd);
/** @brief Return the cursor and display shift home without erasing DDRAM.
 * @param lcd Initialized display instance; NULL/uninitialized is ignored. */
void CharacterLcd_Home(CharacterLcd *lcd);
/** @param lcd Initialized display instance.
 * @param column Zero-based visible column. @param row Zero-based visible row.
 * @return true when the column and row are inside the display. */
bool CharacterLcd_SetCursor(CharacterLcd *lcd, uint8_t column, uint8_t row);
/** @brief Write one ROM/CGRAM code; newline moves to the next visible row.
 * @param lcd Initialized display instance. @param character Code to write. */
void CharacterLcd_PutChar(CharacterLcd *lcd, char character);
/** @brief Write a null-terminated string, with visible-row wrapping.
 * @param lcd Initialized display instance. @param text String to write. */
void CharacterLcd_Print(CharacterLcd *lcd, const char *text);
/** @brief Convert and print one unsigned decimal integer without stdio.
 * @param lcd Initialized display instance. @param value Number to print. */
void CharacterLcd_PrintUInt32(CharacterLcd *lcd, uint32_t value);
/** @brief Convert and print one signed decimal integer without stdio.
 * @param lcd Initialized display instance. @param value Number to print. */
void CharacterLcd_PrintInt32(CharacterLcd *lcd, int32_t value);

/**
 * @brief Enable or disable display pixels, underline cursor, and cursor blink.
 * @param lcd Initialized display instance.
 * @param display_on Show pixels when true while retaining DDRAM when false.
 * @param cursor_on Show the underline cursor when true.
 * @param blink_on Blink the cursor cell when true.
 * DDRAM content is retained when display_on is false.
 */
void CharacterLcd_SetDisplay(CharacterLcd *lcd, bool display_on,
                             bool cursor_on, bool blink_on);

/** @brief Shift the visible window one cell without modifying DDRAM.
 * @param lcd Initialized display instance. @param right Shift right if true. */
void CharacterLcd_Shift(CharacterLcd *lcd, bool right);

/**
 * @brief Store one 5x8 bitmap in CGRAM slot 0..7.
 * @param lcd Initialized display instance.
 * @param slot Character code from 0 through 7.
 * @param pattern Eight rows; only each row's low five bits are used.
 * @return true on success; false for an invalid handle, slot, or pointer.
 * Only the low five bits of each of the eight row values are displayed.
 */
bool CharacterLcd_CreateChar(CharacterLcd *lcd, uint8_t slot,
                             const uint8_t pattern[8]);

#ifdef __cplusplus
}
#endif

#endif /* CHARACTER_LCD_H */
