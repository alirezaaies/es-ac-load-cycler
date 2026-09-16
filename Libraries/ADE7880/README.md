# ADE7880 portable driver

This folder is independent of STM32 HAL. The application supplies four small
callbacks for SPI transmit, SPI receive, chip select, and millisecond delay.
Only `Inc/ade7880.h` and `Src/ade7880.c` are required in another project.

## Minimum integration sequence

1. Configure SPI as master, 8-bit, MSB first, CPOL high, CPHA second edge.
2. Keep the SPI clock at or below 2.5 MHz.
3. Fill an `ADE7880_Transport` with platform callbacks.
4. Call `ADE7880_Init()` once to validate and copy the callbacks.
5. Call `ADE7880_Begin()` once to select SPI, reset the IC, and start its DSP.
6. Call `ADE7880_ReadAll()` whenever a new raw snapshot is required.

```c
ADE7880_Device ade;
ADE7880_MeasurementsRaw raw;
ADE7880_Transport transport = {
    .context = &spi_handle,
    .write = my_spi_write,
    .read = my_spi_read,
    .select = my_chip_select,
    .delay_ms = my_delay_ms
};

if (ADE7880_Init(&ade, &transport, 20U) == ADE7880_STATUS_OK &&
    ADE7880_Begin(&ade, 100U) == ADE7880_STATUS_OK) {
    if (ADE7880_ReadAll(&ade, &raw) == ADE7880_STATUS_OK) {
        /* raw.phase[ADE7880_PHASE_A].voltage_rms is now valid. */
    }
}
```

## Converting a raw voltage to volts

ADE7880 RMS registers are counts. The voltage divider, input network, and board
tolerances decide how many volts one count represents. Calculate a calibration
from two measured points, then apply it:

```c
ADE7880_LinearCalibration voltage_a;
float voltage_v;

if (ADE7880_CalculateLinearCalibration(
        raw_at_100_v, 100.0F,
        raw_at_230_v, 230.0F,
        &voltage_a) == ADE7880_STATUS_OK) {
    (void)ADE7880_ApplyLinearCalibration(
        (float)raw.phase[ADE7880_PHASE_A].voltage_rms,
        &voltage_a, &voltage_v);
}
```

Average several raw samples at each reference point. Use a third independent
voltage to verify the result. Never assume calibration from another board is
accurate enough for protection, billing, or safety decisions.

## Current project behavior

`Application/Src/app.c` already supplies the STM32 callbacks and retries an
offline IC every two seconds. Add `g_app_electrical` to the debugger Watch
window:

- `online`: the IC initialized and the latest complete read succeeded.
- `raw.phase[n]`: direct register values for phase A, B, or C.
- `voltage_v[n]`: converted RMS voltage when `voltage_valid[n]` is true.
- `voltage_source[n]`: NONE, LEGACY, DERIVED, or USER calibration origin.
- `current_a[n]`: RMS amperes when `current_valid[n]` is true.
- `current_source[n]`: origin of the current conversion coefficient.
- `active_power_w[n]`: active watts when `active_power_valid[n]` is true.
- `active_power_source[n]`: origin of the active-power coefficient.
- `apparent_power_va[n]`: apparent VA when `apparent_power_valid[n]` is true.
- `apparent_power_source[n]`: origin of the apparent-power coefficient.
- `power_factor[n]`: signed PF from -1 to +1 after `power_factor_valid[n]` is true.
- `neutral_current_a`: neutral RMS amperes when its valid flag is true.
- `last_status`: the exact result of the latest driver operation.

Array index `0`, `1`, or `2` always represents phase A, B, or C. Power factor
uses the ADE7880 register's fixed signed Q1.15 format and therefore appears
without board calibration. Current and power registers are raw DSP counts;
their engineering values intentionally remain zero with a false valid flag
until a measured calibration is installed.

The application loads the following coefficients recovered from the previous
firmware for phases A and B:

- Voltage: `0.00055963 V/count`; phase A also uses correction `1.017`.
- Current: `0.00036565 A/count`.
- Active power: `0.0170276 W/count`; phase A also uses correction `1.02`.

The ADE7880 internally gain-matches active and apparent power on each phase,
so the matching legacy power coefficient is also used provisionally for VA and
is explicitly marked `APP_CALIBRATION_DERIVED`. Directly recovered values are
marked `APP_CALIBRATION_LEGACY`. Replace every provisional value with measured
two-point calibration before accuracy-dependent use. Phase C and neutral
current remain invalid until explicit calibrations are supplied.

## Calibrating current and power for the Watch window

Record the raw value and a simultaneous trusted reference reading at two
stable, separated, nonzero operating points. Calculate and install one pair
for each phase and quantity:

```c
ADE7880_LinearCalibration phase_a_current;

if (ADE7880_CalculateLinearCalibration(
        raw_at_1_a, 1.0F,
        raw_at_5_a, 5.0F,
        &phase_a_current) == ADE7880_STATUS_OK) {
    (void)App_SetPhaseCalibration(APP_PHASE_QUANTITY_CURRENT_RMS,
                                  ADE7880_PHASE_A,
                                  &phase_a_current);
}
```

Use the same sequence with `APP_PHASE_QUANTITY_ACTIVE_POWER` and reference
watts, or `APP_PHASE_QUANTITY_APPARENT_POWER` and reference VA. Neutral current
uses `App_SetNeutralCurrentCalibration()`. The values become valid on the next
complete one-second sample. These RAM calibrations are lost at reset; after the
coefficients are verified, load them during `App_Init()` or from nonvolatile
memory. A lamp's printed wattage is only a nominal rating and is not a suitable
accuracy reference.

## Status handling

Always check returned status values:

- `ADE7880_STATUS_OK`: operation succeeded.
- `ADE7880_STATUS_INVALID_ARGUMENT`: pointer, phase, width, or timeout is bad.
- `ADE7880_STATUS_BUS_ERROR`: the platform SPI driver failed.
- `ADE7880_STATUS_TIMEOUT`: SPI or reset did not finish in time.
- `ADE7880_STATUS_VERIFY_FAILED`: a written register read back differently.
