"""ARM regression of real app.c + ADE7880 driver with a simulated SPI device.
Requires Python unicorn and PlatformIO ARM GCC. No physical hardware is accessed.
"""
from pathlib import Path
import os, subprocess, tempfile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_PC
root=Path(__file__).resolve().parents[1]
s=(root/'Application/Src/app.c').read_text()
# Exclude unrelated 1-Wire board adapters; keep electrical processing unchanged.
a=s.index('/** Public numbered temperature'); b=s.index('/*\n * Board startup calibration',a)
s=s[:a]+s[b:]
a=s.index('/** Drive one open-drain'); b=s.index('/** Cooperative ADE startup',a)
s=s[:a]+s[b:]
s=s.replace('#include "ds18b20_manager.h"','').replace('#include "sensor_address_store.h"','')
s=s.replace('    if (CYCLER_ENABLE_TEMPERATURE) initialize_temperature_manager();','')
a=s.index('    if (temperature_manager_ready)'); b=s.index('    if (ade_spi_handle == NULL)',a)
s=s[:a]+s[b:]
a=s.index('bool App_TemperatureGetCelsius'); b=s.index('/** Install a measured voltage',a)
s=s[:a]+s[b:]
mock='#include "app.h"\n#include <string.h>\n#include <math.h>\nstatic uint32_t clock_ms, led_calls, ui_calls, falling_edges, reset_at;\nstatic bool cs_active, inject_error, reset_stuck;\nstatic uint16_t pending_register;\nstatic uint32_t config2, config, status1, run;\nuint32_t HAL_GetTick(void) { return clock_ms; }\nvoid HAL_GPIO_WritePin(int port,uint16_t pin,int state) {\n (void)port; (void)pin;\n bool active=state==GPIO_PIN_RESET;\n if (active && !cs_active) ++falling_edges;\n cs_active=active;\n}\nvoid DiagnosticLed_Init(void) {}\nvoid DiagnosticLed_SetMode(int mode) { (void)mode; }\nvoid DiagnosticLed_Process(void) { ++led_calls; }\nvoid UiCounter_Init(void) {}\nvoid UiCounter_Process(void) { ++ui_calls; }\nstatic int transfer_status(uint32_t timeout) {\n /* Reproduce a transfer taking 3 ms: old 2 ms budget must fail. */\n clock_ms+=3;\n return inject_error || timeout<3 ? HAL_TIMEOUT : HAL_OK;\n}\nHAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *h,uint8_t *bytes,uint16_t len,uint32_t timeout) {\n (void)h;\n int status=transfer_status(timeout);\n if (status!=HAL_OK) return status;\n if (!cs_active || falling_edges<3 || len<3) return HAL_ERROR;\n pending_register=((uint16_t)bytes[1]<<8)|bytes[2];\n if (bytes[0]==0) {\n  uint32_t value=0; for (uint16_t i=3;i<len;++i) value=(value<<8)|bytes[i];\n  switch (pending_register) {\n   case ADE7880_REG_CONFIG2: config2=value; break;\n   case ADE7880_REG_CONFIG: config=value; reset_at=clock_ms; status1=0; run=0; break;\n   case ADE7880_REG_STATUS1: status1 &= ~value; break;\n   case ADE7880_REG_RUN: run=value; break;\n   default: break;\n  }\n }\n return HAL_OK;\n}\nHAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef *h,uint8_t *bytes,uint16_t len,uint32_t timeout) {\n (void)h;\n int status=transfer_status(timeout); if (status!=HAL_OK) return status;\n if (!reset_stuck && (config&128) && clock_ms-reset_at>=30) { config&=~128U; status1|=32768; }\n uint32_t value;\n switch (pending_register) {\n  case ADE7880_REG_CONFIG2: value=config2; break;\n  case ADE7880_REG_CONFIG: value=config; break;\n  case ADE7880_REG_STATUS1: value=status1; break;\n  case ADE7880_REG_RUN: value=run; break;\n  case ADE7880_REG_VERSION: value=0x42; break;\n  case ADE7880_REG_AVRMS: case ADE7880_REG_BVRMS: case ADE7880_REG_CVRMS: value=400000; break;\n  case ADE7880_REG_AIRMS: case ADE7880_REG_BIRMS: case ADE7880_REG_CIRMS: case ADE7880_REG_NIRMS: value=100000; break;\n  case ADE7880_REG_AWATT: case ADE7880_REG_BWATT: case ADE7880_REG_CWATT: value=10000; break;\n  case ADE7880_REG_AVA: case ADE7880_REG_BVA: case ADE7880_REG_CVA: value=11000; break;\n  case ADE7880_REG_APF: case ADE7880_REG_BPF: case ADE7880_REG_CPF: value=30000; break;\n  default: return HAL_ERROR;\n }\n for (uint16_t i=0;i<len;++i) bytes[i]=(uint8_t)(value>>((len-1-i)*8));\n return HAL_OK;\n}\n#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)\nint test_main(void) {\n SPI_HandleTypeDef spi={0}; App_Init(&spi);\n for (unsigned i=0;i<1000 && !g_app_electrical.successful_samples;++i) { ++clock_ms; App_Process(); }\n CHECK(g_app_electrical.online && g_app_electrical.successful_samples==1);\n CHECK(g_app_electrical.last_status==ADE7880_STATUS_OK && run==1 && !cs_active);\n CHECK(g_app_electrical.startup_attempts==1 && g_app_electrical.startup_errors==0);\n CHECK(led_calls>20 && ui_calls>20);\n CHECK(g_app_electrical.voltage_valid[0] && g_app_electrical.current_valid[0]);\n CHECK(g_app_electrical.active_power_valid[0] && g_app_electrical.power_factor_valid[0]);\n CHECK(fabsf(g_app_electrical.voltage_v[0]-227.6575F)<0.01F);\n CHECK(fabsf(g_app_electrical.current_a[0]-0.36565F)<0.001F);\n CHECK(fabsf(g_app_electrical.active_power_w[0]-17.36815F)<0.01F);\n CHECK(fabsf(g_app_electrical.power_factor_abs[0]-0.915527F)<0.001F);\n uint32_t samples=g_app_electrical.successful_samples;\n inject_error=true; clock_ms+=1000; App_Process();\n CHECK(!g_app_electrical.online && g_app_electrical.successful_samples==samples);\n CHECK(g_app_electrical.communication_errors==1 && !cs_active);\n CHECK(g_app_electrical.last_error_status==ADE7880_STATUS_TIMEOUT);\n clock_ms+=2000; App_Process(); ++clock_ms; inject_error=false;\n for (unsigned i=0;i<1000 && g_app_electrical.successful_samples==samples;++i) { ++clock_ms; App_Process(); }\n CHECK(g_app_electrical.online && g_app_electrical.successful_samples==samples+1);\n CHECK(g_app_electrical.startup_attempts==2);\n inject_error=true; clock_ms+=1000; App_Process(); clock_ms+=2000; App_Process();\n for (unsigned i=0;i<100 && !g_app_electrical.startup_errors;++i) { ++clock_ms; App_Process(); }\n CHECK(g_app_electrical.startup_errors==1 && g_app_electrical.failed_startup_stage==3);\n CHECK(g_app_electrical.last_error_status==ADE7880_STATUS_TIMEOUT);\n return 0;\n}\n'
header="""#ifndef MOCK_HAL_H
#define MOCK_HAL_H
#include <stdint.h>
typedef struct { int dummy; } SPI_HandleTypeDef;
typedef enum { HAL_OK=0, HAL_ERROR=1, HAL_TIMEOUT=3 } HAL_StatusTypeDef;
#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1
uint32_t HAL_GetTick(void);
void HAL_GPIO_WritePin(int,uint16_t,int);
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef*,uint8_t*,uint16_t,uint32_t);
HAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef*,uint8_t*,uint16_t,uint32_t);
#endif
"""
tool=Path(os.environ['USERPROFILE'])/'.platformio/packages/toolchain-gccarmnoneeabi/bin'
with tempfile.TemporaryDirectory() as folder:
 d=Path(folder); (d/'app_test.c').write_text(s); (d/'mock.c').write_text(mock)
 (d/'stm32f1xx_hal.h').write_text(header)
 (d/'main.h').write_text('#include "stm32f1xx_hal.h"\n#define ADE_CS_GPIO_Port 0\n#define ADE_CS_Pin 4096\n')
 (d/'diagnostic_led.h').write_text('#define DIAGNOSTIC_LED_MODE_DANCE 2\nvoid DiagnosticLed_Init(void); void DiagnosticLed_SetMode(int); void DiagnosticLed_Process(void);\n')
 (d/'ui_counter.h').write_text('void UiCounter_Init(void); void UiCounter_Process(void);\n')
 command=[str(tool/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m3','-mthumb','-O1','-nostartfiles','--specs=nosys.specs','-I'+str(d),'-I'+str(root/'Application/Inc'),'-I'+str(root/'Libraries/ADE7880/Inc'),str(d/'app_test.c'),str(d/'mock.c'),str(root/'Libraries/ADE7880/Src/ade7880.c'),'-Wl,-Ttext=0x10000','-Wl,-e,test_main','-o',str(d/'test.elf'),'-lm']
 subprocess.run(command,check=True)
 subprocess.run([str(tool/'arm-none-eabi-objcopy.exe'),'-O','binary',str(d/'test.elf'),str(d/'test.bin')],check=True)
 nm=subprocess.check_output([str(tool/'arm-none-eabi-nm.exe'),str(d/'test.elf')],text=True)
 entry=int(next(x.split()[0] for x in nm.splitlines() if x.endswith(' test_main')),16)
 u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS); u.mem_map(0x10000,0x100000)
 u.mem_write(0x10000,(d/'test.bin').read_bytes()); u.reg_write(UC_ARM_REG_SP,0x100000)
 u.reg_write(UC_ARM_REG_LR,0x90001); u.emu_start(entry|1,0x90000,count=1000000)
 assert u.reg_read(UC_ARM_REG_PC)==0x90000, 'Instruction budget exhausted'
 result=u.reg_read(UC_ARM_REG_R0)
 assert result==0, f'Electrical regression failed at mock C line {result}'
 print('PASS: real app + ADE driver: startup, SPI budget, calibrated V/A/W/PF, foreground service, failed read, retry, startup diagnostics')
