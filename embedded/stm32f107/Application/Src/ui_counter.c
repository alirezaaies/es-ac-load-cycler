/**
 * @file ui_counter.c
 * @brief STM32 adapter and non-blocking button test for the character LCD.
 */
#include "ui_counter.h"

#include "character_lcd.h"
#include "main.h"

#include <stdbool.h>

#define UI_BUTTON_COUNT 3U
#define UI_BUTTON_DEBOUNCE_MS 30U
#define UI_COUNTER_MAX 100U

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    bool pressed;
    bool press_candidate;
    bool release_candidate;
    uint32_t changed_at_ms;
} UiButton;

static CharacterLcd lcd;
static bool lcd_ready;
static volatile uint8_t button_irq_mask;
volatile uint8_t g_ui_counter_value;

static UiButton buttons[UI_BUTTON_COUNT] = {
    {BUTTON_INC_GPIO_Port, BUTTON_INC_Pin, false, false, false, 0U},
    {BUTTON_DEC_GPIO_Port, BUTTON_DEC_Pin, false, false, false, 0U},
    {BUTTON_RESET_GPIO_Port, BUTTON_RESET_Pin, false, false, false, 0U}
};

static void lcd_write_rs(void *context, bool high)
{
    (void)context;
    HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin,
                      high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void lcd_write_rw(void *context, bool high)
{
    (void)context;
    HAL_GPIO_WritePin(LCD_RW_GPIO_Port, LCD_RW_Pin,
                      high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void lcd_write_enable(void *context, bool high)
{
    (void)context;
    HAL_GPIO_WritePin(LCD_E_GPIO_Port, LCD_E_Pin,
                      high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void lcd_write_data4(void *context, uint8_t nibble)
{
    uint32_t set_mask = 0U;
    const uint32_t all_pins = LCD_D4_Pin | LCD_D5_Pin | LCD_D6_Pin | LCD_D7_Pin;

    (void)context;
    if ((nibble & 0x01U) != 0U) set_mask |= LCD_D4_Pin;
    if ((nibble & 0x02U) != 0U) set_mask |= LCD_D5_Pin;
    if ((nibble & 0x04U) != 0U) set_mask |= LCD_D6_Pin;
    if ((nibble & 0x08U) != 0U) set_mask |= LCD_D7_Pin;
    LCD_D4_GPIO_Port->BSRR = set_mask | ((all_pins & ~set_mask) << 16U);
}

static void lcd_delay_us(void *context, uint32_t microseconds)
{
    const uint32_t cycles_per_us = SystemCoreClock / 1000000U;
    const uint32_t started = DWT->CYCCNT;
    const uint32_t target = cycles_per_us * microseconds;

    (void)context;
    while ((DWT->CYCCNT - started) < target) {
    }
}

static void render_counter(void)
{
    char field[3] = {' ', ' ', '0'};
    uint8_t value = g_ui_counter_value;

    if (!lcd_ready) return;
    if (value >= 100U) {
        field[0] = '1';
        field[1] = '0';
        field[2] = '0';
    } else {
        if (value >= 10U) field[1] = (char)('0' + (value / 10U));
        field[2] = (char)('0' + (value % 10U));
    }
    (void)CharacterLcd_SetCursor(&lcd, 7U, 1U);
    for (uint8_t index = 0U; index < sizeof(field); ++index) {
        CharacterLcd_PutChar(&lcd, field[index]);
    }
}

static void apply_button(uint8_t index)
{
    uint8_t previous = g_ui_counter_value;

    if ((index == 0U) && (g_ui_counter_value < UI_COUNTER_MAX)) {
        ++g_ui_counter_value;
    } else if ((index == 1U) && (g_ui_counter_value > 0U)) {
        --g_ui_counter_value;
    } else if (index == 2U) {
        g_ui_counter_value = 0U;
    }
    if (g_ui_counter_value != previous) render_counter();
}

void UiCounter_Init(void)
{
    const CharacterLcd_Config config = {
        .context = NULL,
        .write_rs = lcd_write_rs,
        .write_rw = lcd_write_rw,
        .write_enable = lcd_write_enable,
        .write_data4 = lcd_write_data4,
        .delay_us = lcd_delay_us,
        .columns = 16U,
        .rows = 2U
    };

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    g_ui_counter_value = 0U;
    button_irq_mask = 0U;
    lcd_ready = CharacterLcd_Init(&lcd, &config);
    if (lcd_ready) {
        CharacterLcd_Print(&lcd, "Counter test");
        (void)CharacterLcd_SetCursor(&lcd, 0U, 1U);
        CharacterLcd_Print(&lcd, "Value:     /100 ");
        render_counter();
    }
}

void UiCounter_Process(void)
{
    const uint32_t now = HAL_GetTick();
    const uint32_t interrupt_state = __get_PRIMASK();
    uint8_t pending;

    __disable_irq();
    pending = button_irq_mask;
    button_irq_mask = 0U;
    if (interrupt_state == 0U) __enable_irq();

    for (uint8_t index = 0U; index < UI_BUTTON_COUNT; ++index) {
        UiButton *button = &buttons[index];
        const bool high = HAL_GPIO_ReadPin(button->port, button->pin) ==
                          GPIO_PIN_SET;

        if (!button->pressed) {
            if ((pending & (uint8_t)(1U << index)) != 0U) {
                button->press_candidate = true;
                button->changed_at_ms = now;
            }
            if (button->press_candidate && !high) {
                button->press_candidate = false;
            } else if (button->press_candidate &&
                       ((uint32_t)(now - button->changed_at_ms) >=
                        UI_BUTTON_DEBOUNCE_MS)) {
                button->press_candidate = false;
                button->pressed = true;
                apply_button(index);
            }
        } else if (high) {
            button->release_candidate = false;
        } else if (!button->release_candidate) {
            button->release_candidate = true;
            button->changed_at_ms = now;
        } else if ((uint32_t)(now - button->changed_at_ms) >=
                   UI_BUTTON_DEBOUNCE_MS) {
            button->release_candidate = false;
            button->pressed = false;
        }
    }
}

/** HAL callback: record only the edge; debounce and UI work stay in main. */
void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)
{
    if (gpio_pin == BUTTON_INC_Pin) {
        button_irq_mask |= 0x01U;
    } else if (gpio_pin == BUTTON_DEC_Pin) {
        button_irq_mask |= 0x02U;
    } else if (gpio_pin == BUTTON_RESET_Pin) {
        button_irq_mask |= 0x04U;
    }
}
