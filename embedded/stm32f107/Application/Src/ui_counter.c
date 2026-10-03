/** @file ui_counter.c @brief Cooperative load cycler, LCD and button adapter. */
#include "ui_counter.h"
#include "cycler_config.h"
#include "character_lcd.h"
#include "main.h"
#include "app.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

typedef enum { WELCOME, WEBSITE, HOME, EDIT_COUNT, EDIT_ON, EDIT_OFF, SAVED } Page;
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    bool raw, pressed, long_sent;
    uint32_t changed, down, repeat_at, changes;
} Button;
static Button buttons[3] = {
    {.port=BUTTON_INC_GPIO_Port, .pin=BUTTON_INC_Pin},
    {.port=BUTTON_DEC_GPIO_Port, .pin=BUTTON_DEC_Pin},
    {.port=BUTTON_RESET_GPIO_Port, .pin=BUTTON_RESET_Pin}
};
static CharacterLcd lcd;
static bool lcd_ready;
static Page page;
static uint32_t page_at, render_at, phase_at;
static bool running, relay_on, finished;
static uint32_t cycle;
static uint32_t settings[3], draft[3];
static char shown[2][16];
volatile uint32_t g_ui_counter_value;

static void relays(bool on)
{
    const uint32_t pins = GPIO_PIN_1 | GPIO_PIN_2;
    /* One BSRR write switches phase PE2 and neutral PE1 together. */
    GPIOE->BSRR = (on == (CYCLER_RELAY_ACTIVE_HIGH != 0)) ? pins : pins << 16U;
    relay_on = on;
}

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


static bool editing(void) { return page >= EDIT_COUNT && page <= EDIT_OFF; }
static void start_run(uint32_t now)
{
    cycle = settings[0] ? 1U : 0U;
    finished = settings[0] == 0U || (settings[1] == 0U && settings[2] == 0U);
    running = !finished;
    phase_at = now;
    relays(running && settings[1] != 0U);
}

static void service_cycle(uint32_t now)
{
    uint32_t duration;
    if (!running) return;
    duration = settings[relay_on ? 1U : 2U] * 1000U;
    if ((uint32_t)(now - phase_at) < duration) return;
    /* Start deadlines at the physical edge; never catch up with relay bursts. */
    phase_at = now;
    if (relay_on) {
        relays(false);
        if (settings[2] != 0U) return;
    }
    if (cycle >= settings[0]) {
        running = false;
        finished = true;
        relays(false);
    } else {
        ++cycle;
        relays(settings[1] != 0U);
    }
}

static void adjust(uint8_t key, uint32_t step)
{
    uint32_t slot, limit, value;
    if (!editing()) return;
    slot = (uint32_t)(page - EDIT_COUNT);
    limit = slot == 0U ? CYCLER_MAX_COUNT : CYCLER_MAX_SECONDS;
    value = draft[slot];
    /* Inclusive modular arithmetic also handles accelerated boundary crossing. */
    draft[slot] = key == 0U ? (value + step) % (limit + 1U)
                           : (value + limit + 1U - step) % (limit + 1U);
    g_ui_counter_value = draft[slot];
}

static void menu_long(uint32_t now)
{
    if (page == HOME) {
        memcpy(draft, settings, sizeof(draft));
        page = EDIT_COUNT;
        g_ui_counter_value = draft[0];
        /* Editing stops the test; cancellation restarts the old configuration. */
        running = false;
        relays(false);
    } else if (editing()) {
        page = HOME;
        start_run(now);
    }
}

static void menu_short(uint32_t now)
{
    if (page == EDIT_COUNT || page == EDIT_ON) {
        page = (Page)(page + 1);
        g_ui_counter_value = draft[page - EDIT_COUNT];
    } else if (page == EDIT_OFF) {
        memcpy(settings, draft, sizeof(settings));
        page = SAVED;
    } else if (page == SAVED) {
        page = HOME;
        start_run(now);
    }
}

static void service_buttons(uint32_t now)
{
    for (uint8_t i=0; i<3U; ++i) {
        Button *b = &buttons[i];
        bool raw = HAL_GPIO_ReadPin(b->port,b->pin) == GPIO_PIN_SET;
        if (raw != b->raw) { b->raw=raw; b->changed=now; }
        if (raw != b->pressed && (uint32_t)(now-b->changed) >= CYCLER_DEBOUNCE_MS) {
            b->pressed=raw;
            if (raw) {
                b->down=now; b->repeat_at=now; b->changes=1U; b->long_sent=false;
                if (i<2U) adjust(i,1U);
            } else if (i==2U && !b->long_sent) menu_short(now);
        }
        if (!b->pressed || !raw) continue;
        if (i==2U) {
            if (!b->long_sent && (uint32_t)(now-b->down)>=CYCLER_MENU_HOLD_MS) {
                b->long_sent=true;
                menu_long(now);
            }
        } else if ((uint32_t)(now-b->down)>=CYCLER_REPEAT_DELAY_MS &&
                   (uint32_t)(now-b->repeat_at)>=CYCLER_REPEAT_MS) {
            b->repeat_at=now;
            adjust(i,b->changes>=20U ? 5U : 1U);
            if (b->changes<20U) ++b->changes;
        }
    }
}

/* Right-aligned fixed decimal text without printf's optional float support. */
static void number(char *dst, uint8_t width, float value, bool valid, uint8_t decimals)
{
    char text[24];
    uint32_t scale=1U, rounded;
    size_t len;
    memset(dst,' ',width);
    if (!valid || !isfinite(value)) { text[0]='-'; text[1]='-'; text[2]=0; }
    else {
        const char suffixes[] = {' ', 'k', 'M', 'G'};
        float magnitude=fabsf(value);
        /* Keep the preferred precision when it fits; then compact large values. */
        for (uint8_t unit=0U; unit<4U; ++unit) {
            for (int precision=(int)decimals; precision>=0; --precision) {
                scale=1U;
                for (int i=0;i<precision;++i) scale*=10U;
                if (magnitude*(float)scale>1000000000.0F) continue;
                rounded=(uint32_t)(magnitude*(float)scale+0.5F);
                if (precision) snprintf(text,sizeof(text),"%s%lu.%0*lu",value<0?"-":"",
                    (unsigned long)(rounded/scale),precision,(unsigned long)(rounded%scale));
                else snprintf(text,sizeof(text),"%s%lu",value<0?"-":"",(unsigned long)rounded);
                len=strlen(text);
                if (unit) { text[len++]=suffixes[unit]; text[len]=0; }
                if (len<=width) { memcpy(dst+width-len,text,len); return; }
            }
            magnitude/=1000.0F;
        }
        memset(dst,'#',width); return;
    }
    len=strlen(text);
    if (len>width) memset(dst,'#',width);
    else memcpy(dst+width-len,text,len);
}

static void compose(char rows[2][16], uint32_t now)
{
    char text[24];
    uint32_t seconds=0U;
    uint8_t p=CYCLER_DISPLAY_PHASE;
    bool valid=g_app_electrical.online &&
        (uint32_t)(now-g_app_electrical.updated_at_ms)<3000U;
    memset(rows,' ',32U);
    if (page==WELCOME) { memcpy(rows[0],"Welcome to AGFA",15); return; }
    if (page==WEBSITE) { memcpy(rows[0],"www.agfaco.com",13); return; }
    if (page==SAVED) {
        memcpy(rows[0],"Settings saved",14); memcpy(rows[1],"K3: start test",14); return;
    }
    if (editing()) {
        const char *titles[]={"Cycles","ON seconds","OFF seconds"};
        uint32_t slot=(uint32_t)(page-EDIT_COUNT);
        memcpy(rows[0],titles[slot],strlen(titles[slot]));
        snprintf(text,sizeof(text),"%lu",(unsigned long)draft[slot]);
        memcpy(rows[1],text,strlen(text));
        memcpy(rows[1]+7,"K3: next",8); return;
    }
    rows[0][0]='V'; rows[1][0]='A'; rows[0][6]='P'; rows[1][6]='P'; rows[1][7]='F';
    number(rows[0]+1,5,g_app_electrical.voltage_v[p],valid&&g_app_electrical.voltage_valid[p],CYCLER_V_DECIMALS);
    number(rows[1]+1,5,g_app_electrical.current_a[p],valid&&g_app_electrical.current_valid[p],CYCLER_A_DECIMALS);
    number(rows[0]+7,4,g_app_electrical.active_power_w[p],valid&&g_app_electrical.active_power_valid[p],CYCLER_P_DECIMALS);
    number(rows[1]+8,4,g_app_electrical.power_factor_abs[p],valid&&g_app_electrical.power_factor_valid[p],CYCLER_PF_DECIMALS);
    /* PF needs five characters for its label/value, leaving four for time. */
    if (cycle<=9999U) {
        rows[0][11]='T';
        snprintf(text,sizeof(text),"%4lu",(unsigned long)cycle);
        memcpy(rows[0]+12,text,4);
    } else {
        /* Split a six-digit count across the right column, never overwrite P/PF. */
        snprintf(text,sizeof(text),"%06lu",(unsigned long)cycle);
        rows[0][11]='T'; memcpy(rows[0]+12,text,4);
        if ((now/2000U)%2U==0U) {
            rows[1][12]='C'; rows[1][13]=' ';
            memcpy(rows[1]+14,text+4,2);
            return;
        }
    }
    if (finished) memcpy(rows[1]+12,"Done",4);
    else if (!running) memcpy(rows[1]+12,"Idle",4);
    else {
        uint32_t duration=settings[relay_on?1U:2U]*1000U;
        uint32_t elapsed=now-phase_at;
        seconds=elapsed>=duration?0U:(duration-elapsed+999U)/1000U;
        snprintf(text,sizeof(text),"%c%3lu",relay_on?'*':'-',(unsigned long)seconds);
        memcpy(rows[1]+12,text,4);
    }
}

void UiCounter_Init(void)
{
    GPIO_InitTypeDef gpio={0};
    const CharacterLcd_Config config={.context=NULL,.write_rs=lcd_write_rs,
        .write_rw=lcd_write_rw,.write_enable=lcd_write_enable,.write_data4=lcd_write_data4,
        .delay_us=lcd_delay_us,.columns=16U,.rows=2U};
    __HAL_RCC_GPIOE_CLK_ENABLE();
    relays(false);
    gpio.Pin=GPIO_PIN_1|GPIO_PIN_2; gpio.Mode=GPIO_MODE_OUTPUT_PP;
    gpio.Pull=GPIO_NOPULL; gpio.Speed=GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE,&gpio);
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT=0U; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    settings[0]=CYCLER_DEFAULT_COUNT; settings[1]=CYCLER_DEFAULT_ON_SECONDS;
    settings[2]=CYCLER_DEFAULT_OFF_SECONDS;
    page=WELCOME; page_at=HAL_GetTick(); render_at=page_at-100U;
    memset(shown,0,sizeof(shown));
    lcd_ready=CharacterLcd_Init(&lcd,&config);
}

void UiCounter_Process(void)
{
    uint32_t now=HAL_GetTick();
    char rows[2][16];
    service_cycle(now);
    service_buttons(now);
    if ((page==WELCOME || page==WEBSITE) && (uint32_t)(now-page_at)>=CYCLER_SPLASH_MS) {
        page_at=now;
        if (page==WELCOME) page=WEBSITE;
        else page=HOME;
    }
    if (!lcd_ready || (uint32_t)(now-render_at)<100U) return;
    render_at=now;
    compose(rows,now);
    for (uint8_t r=0;r<2U;++r) for (uint8_t c=0;c<16U;++c) {
        if (shown[r][c]==rows[r][c]) continue;
        (void)CharacterLcd_SetCursor(&lcd,c,r);
        CharacterLcd_PutChar(&lcd,rows[r][c]);
        shown[r][c]=rows[r][c];
    }
}

/** Edges need no work in ISR: the foreground samples and debounces both levels. */
void HAL_GPIO_EXTI_Callback(uint16_t pin) { (void)pin; }
