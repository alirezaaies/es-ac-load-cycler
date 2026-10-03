# Electrical load cycler

## Hardware and polarity

- PE1 switches neutral; PE2 switches phase. Both are active-low: LOW energizes/connects, HIGH releases/disconnects. A single GPIOE BSRR write changes both outputs together.
- Initialization preloads both outputs HIGH before configuring output mode. CubeMX also records HIGH as the initial level.
- K1/PD9 increases, K2/PD10 decreases, K3/PD11 confirms or opens/cancels settings. Buttons remain active-high.
- The existing LCD library and PA6..PA12 wiring are preserved.
- ADE7880 uses SPI2 on PB13..PB15 with CS on PB12. All phase and neutral electrical measurements are read once per second. The LCD shows phase A in V/A/W and PF magnitude by default.
- PC13/PC14 alternate continuously, including when ADE communication is offline. Invalid/stale readings show --.

## Operation

Welcome and www.agfaco.com appear for two seconds each, followed by the idle home page with relays released.

Hold K3 for three seconds to open settings. Configure cycles (0..100000), ON seconds (0..300), then OFF seconds (0..300). Short K3 releases advance the menu. All changes remain provisional until the final OFF confirmation. Hold K3 in any editor to discard all provisional changes.

K1/K2 change one unit on a single press. Holding starts repeating after 500 ms at 120 ms intervals. The first 20 changes are single-unit, then changes use five-unit steps. Values wrap modulo the inclusive range.

After confirming OFF, Settings saved appears. Another short K3 press starts the test. Settings are held in RAM and return to defaults after power loss. Entering settings stops the test and releases both relays. Cancelling restarts the previous configuration from cycle 1.

Each cycle runs ON then OFF. The cycle number increases from 1 to the configured count; each phase counts down seconds. After the final OFF, both relays remain released and Done appears without incrementing the cycle beyond the target.

Count zero does not run. ON zero never energizes the relays. OFF zero creates only a brief output transition at the cycle boundary, not a timed mechanical disconnection. Both durations zero finish immediately. Use positive ON/OFF times for physical switching tests.

## Display and configuration

```text
V230.1P9999T   1
A 12.3PF0.98*  3
```

T labels the cycle number; * indicates ON and - indicates OFF. The last three characters show remaining seconds. Preferred precision is 1 decimal for V/A, none for W, and 2 for PF. Large numbers use reduced precision, then k/M/G suffixes to stay inside their fields.

For five/six-digit cycles, the right column splits the zero-padded six-digit number: T0010 above C 00 represents cycle 001000. Every two seconds, the lower right field alternates between the last two cycle digits and countdown/Done. V/A/P/PF retain their positions.

Edit Application/Inc/cycler_config.h for timing, precision, displayed phase and relay polarity. Edit compose() in Application/Src/ui_counter.c for layout. The original filename is retained for Keil compatibility. All libraries remain available; temperature processing is disabled by default through CYCLER_ENABLE_TEMPERATURE.

## Scheduling and validation

HAL SysTick supplies the millisecond clock. Menus, countdowns, splash pages, LEDs and ADE startup/reset waits use cooperative state machines. SPI still uses short HAL polling transfers with a 20 ms timeout per transaction; the LCD retains hardware-required microsecond delays. This is not a hard real-time/DMA implementation. Relay deadlines start at actual transitions rather than generating catch-up bursts.

Build with platformio run -d embedded/stm32f107. Run python embedded/stm32f107/tests/test_cycler.py with Python unicorn and PlatformIO's ARM GCC installed. The test executes actual ARM-compiled state-machine code and covers buttons, acceleration, wrap, atomic confirmation/cancellation, ten 3-second/2-second cycles, zero durations, tick rollover, LCD bounds, and active-low BSRR outputs.

The user confirmed the previous application works on hardware. The corrected active-low polarity still needs programming onto the board. Existing ADE calibration is preserved.

## Electrical reading diagnostics ? 2026-10-03

The cycler refactor reduced the SPI timeout from the established 20 ms budget to 2 ms. Restore 20 ms so delayed transfers do not unnecessarily reject a complete sample. This is a compatibility correction, not a confirmed diagnosis of the physical board. Startup remains cooperative. Do not replace invalid data with zero or bypass the LCD validity checks.

Run `python embedded/stm32f107/tests/test_electrical.py` for the electrical path regression. It compiles real electrical app code and the portable driver for ARM, with a simulated ADE register model. It verifies startup, a modeled 3 ms transfer, calibration publication, foreground servicing, communication failures and recovery. This model does not prove the board's timing or wiring.

Add these expressions to the debugger Watch window (index 0 = phase A, 1 = B, 2 = C):

| Expression | Meaning |
| --- | --- |
| g_app_electrical.voltage_v[0] | RMS voltage, V |
| g_app_electrical.current_a[0] | RMS current, A |
| g_app_electrical.active_power_w[0] | Active power, W |
| g_app_electrical.power_factor_abs[0] | PF magnitude shown on LCD |
| g_app_electrical.power_factor[0] | Signed PF |
| g_app_electrical.online | Startup and last sample communication succeeded |
| g_app_electrical.successful_samples | Must increase once per successful sample |
| g_app_electrical.updated_at_ms | Timestamp of the latest complete sample |
| g_app_electrical.last_status | Most recent operation result |
| g_app_electrical.last_error_status | Most recent error, retained during subsequent successful operations |
| g_app_electrical.startup_attempts | Initializations including retries |
| g_app_electrical.startup_errors | Failed startup attempts |
| g_app_electrical.startup_stage | Current stage |
| g_app_electrical.failed_startup_stage | Stage of last startup failure |
| g_app_electrical.communication_errors | Failed sample reads |
| g_app_electrical.raw.phase[0] | Raw RMS, power and Q1.15 PF registers |

Startup stages: 0 idle; 1 power wait; 2 chip-select pulses; 3 SPI lock; 4 reset request; 5 reset completion; 6 RUN/DSP start; 7 VERSION read. Error codes follow ADE7880_Status in Libraries/ADE7880/Inc/ade7880.h. The LCD requires online communication, a sample younger than 3 seconds, and each quantity's corresponding validity flag. A connected load alone does not make those checks true.
