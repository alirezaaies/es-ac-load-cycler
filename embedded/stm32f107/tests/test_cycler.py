"""Exercise the actual firmware state-machine functions in ARM emulation.
Requires Python unicorn and the PlatformIO ARM GCC toolchain. No board needed.
"""
from pathlib import Path
import subprocess, tempfile, os
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_PC
root=Path(__file__).resolve().parents[1]
s=(root/'Application/Src/ui_counter.c').read_text()
core=s[s.index('typedef enum'):s.index('static void lcd_write_rs')]
core+=s[s.index('static bool editing'):s.index('void UiCounter_Init')]
preamble='''#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
typedef struct { bool online; uint32_t updated_at_ms; float voltage_v[3], current_a[3], active_power_w[3], power_factor_abs[3]; bool voltage_valid[3], current_valid[3], active_power_valid[3], power_factor_valid[3]; } Electrical;
static Electrical g_app_electrical;
#include "cycler_config.h"
typedef struct { uint32_t BSRR; } GPIO_TypeDef;
static GPIO_TypeDef gpio;
#define GPIOE (&gpio)
#define GPIO_PIN_1 2U
#define GPIO_PIN_2 4U
#define BUTTON_INC_GPIO_Port (&gpio)
#define BUTTON_DEC_GPIO_Port (&gpio)
#define BUTTON_RESET_GPIO_Port (&gpio)
#define BUTTON_INC_Pin 1U
#define BUTTON_DEC_Pin 2U
#define BUTTON_RESET_Pin 4U
#define GPIO_PIN_SET 1U
typedef int CharacterLcd;
static uint16_t keys;
static int HAL_GPIO_ReadPin(GPIO_TypeDef *p,uint16_t pin) { (void)p; return (keys&pin)!=0; }
'''
tests='''
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
static void tick(uint32_t t) { service_cycle(t); service_buttons(t); }
static void press(uint16_t key,uint32_t t,uint32_t held) {
 keys=key; tick(t); tick(t+30U); tick(t+30U+held);
 keys=0; tick(t+31U+held); tick(t+61U+held);
}
int test_main(void) {
 page=EDIT_COUNT; draft[0]=100000U; adjust(0,1); CHECK(draft[0]==0);
 adjust(1,1); CHECK(draft[0]==100000U);
 draft[0]=99999; adjust(0,5); CHECK(draft[0]==3);
 page=EDIT_ON; draft[1]=300; adjust(0,1); CHECK(draft[1]==0);
 adjust(1,1); CHECK(draft[1]==300);
 settings[0]=10; settings[1]=3; settings[2]=2;
 page=HOME; press(4,100,3000); CHECK(page==EDIT_COUNT);
 CHECK(!buttons[2].pressed); CHECK(!running && !relay_on);
 press(1,4000,0); CHECK(draft[0]==11);
 press(4,4500,0); CHECK(page==EDIT_ON && settings[0]==10);
 press(4,5000,3000); CHECK(page==HOME && settings[0]==10);
 CHECK(running && cycle==1 && relay_on);
 menu_long(9000); draft[0]=10; draft[1]=3; draft[2]=2;
 menu_short(9001); menu_short(9002); menu_short(9003);
 CHECK(page==SAVED && !running); menu_short(10000);
 for (uint32_t i=0;i<10;++i) {
  uint32_t t=10000+i*5000;
  CHECK(cycle==i+1 && relay_on);
  service_cycle(t+2999); CHECK(relay_on);
  service_cycle(t+3000); CHECK(!relay_on && cycle==i+1);
  service_cycle(t+4999); CHECK(!relay_on);
  service_cycle(t+5000);
 }
 CHECK(finished && !running && !relay_on && cycle==10);
 CHECK(gpio.BSRR==(GPIO_PIN_1|GPIO_PIN_2));
 relays(true); CHECK(gpio.BSRR==((GPIO_PIN_1|GPIO_PIN_2)<<16));
 relays(false); CHECK(gpio.BSRR==(GPIO_PIN_1|GPIO_PIN_2));
 settings[0]=0; start_run(0); CHECK(finished && !relay_on);
 settings[0]=10; settings[1]=0; settings[2]=0; start_run(0); CHECK(finished && !relay_on);
 settings[1]=0; settings[2]=2; start_run(0); CHECK(!relay_on && cycle==1);
 service_cycle(2000); CHECK(cycle==2 && !relay_on);
 settings[1]=3; settings[2]=0; start_run(0);
 service_cycle(3000); CHECK(cycle==2 && relay_on);
 settings[1]=3; settings[2]=2; start_run(0xfffffff0U);
 service_cycle((uint32_t)(0xfffffff0U+3000U)); CHECK(!relay_on && cycle==1);
 page=EDIT_COUNT; draft[0]=0; memset(buttons,0,sizeof(buttons));
 buttons[0].port=&gpio; buttons[0].pin=1;
 keys=1; tick(0); tick(30); CHECK(draft[0]==1);
 for (uint32_t i=0;i<19;++i) tick(530+i*120);
 CHECK(draft[0]==20); tick(530+19*120); CHECK(draft[0]==25);
 struct { char before; char rows[2][16]; char after; } screen={.before='X',.after='Y'};
 g_app_electrical.online=true; g_app_electrical.updated_at_ms=100;
 g_app_electrical.voltage_v[0]=230.1F; g_app_electrical.current_a[0]=12.3F;
 g_app_electrical.active_power_w[0]=9999; g_app_electrical.power_factor_abs[0]=0.98F;
 g_app_electrical.voltage_valid[0]=g_app_electrical.current_valid[0]=true;
 g_app_electrical.active_power_valid[0]=g_app_electrical.power_factor_valid[0]=true;
 page=HOME; settings[0]=10; settings[1]=3; settings[2]=2; start_run(100);
 compose(screen.rows,100);
 CHECK(memcmp(screen.rows[0],"V230.1P9999T   1",16)==0);
 CHECK(memcmp(screen.rows[1],"A 12.3PF0.98*  3",16)==0);
 const uint32_t counts[]={9999,10000,99999,100000};
 for (uint32_t n=0;n<4;n+=1) {
  cycle=counts[n]; compose(screen.rows,100);
  CHECK(screen.before=='X' && screen.after=='Y');
  CHECK(screen.rows[0][6]=='P' && screen.rows[1][6]=='P');
 }
 g_app_electrical.voltage_v[0]=12345; compose(screen.rows,100);
 CHECK(memcmp(screen.rows[0]+1,"12345",5)==0);
 return 0;
}
'''
tool=Path(os.environ['USERPROFILE'])/'.platformio/packages/toolchain-gccarmnoneeabi/bin'
with tempfile.TemporaryDirectory() as d:
 d=Path(d); (d/'test.c').write_text(preamble+core+tests)
 subprocess.run([str(tool/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m3','-mthumb','-O1','-nostartfiles','--specs=nosys.specs', '-I'+str(root/'Application/Inc'),str(d/'test.c'),'-Wl,-Ttext=0x10000','-Wl,-e,test_main','-o',str(d/'test.elf'),'-lm'],check=True,capture_output=True)
 subprocess.run([str(tool/'arm-none-eabi-objcopy.exe'),'-O','binary',str(d/'test.elf'),str(d/'test.bin')],check=True)
 nm=subprocess.check_output([str(tool/'arm-none-eabi-nm.exe'),str(d/'test.elf')],text=True)
 entry=int(next(x.split()[0] for x in nm.splitlines() if x.endswith(' test_main')),16)
 u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS); u.mem_map(0x10000,0x100000)
 u.mem_write(0x10000,(d/'test.bin').read_bytes()); u.reg_write(UC_ARM_REG_SP,0x100000)
 u.reg_write(UC_ARM_REG_LR,0x90001); u.emu_start(entry|1,0x90000,count=100000000)
 assert u.reg_read(UC_ARM_REG_PC)==0x90000, "ARM test exceeded instruction budget"
 result=u.reg_read(UC_ARM_REG_R0)
 assert result==0, f'Firmware assertion failed at generated C line {result}'
 print('PASS: actual ARM cycler code: debounce, hold/release, acceleration, wrap, atomic commit/cancel, 10x3s/2s, zero durations, tick rollover, simultaneous relay outputs, LCD formatting/overflow/bounds')
