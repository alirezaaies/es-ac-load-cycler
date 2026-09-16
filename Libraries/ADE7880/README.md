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
- `current_display[n]`: current in the selected display unit; default is 0.01 A.
- `current_source[n]`: origin of the current conversion coefficient.
- `active_power_w[n]`: active watts when `active_power_valid[n]` is true.
- `active_power_display[n]`: active power in the selected display unit; default is 0.1 W.
- `active_power_source[n]`: origin of the active-power coefficient.
- `apparent_power_va[n]`: apparent VA when `apparent_power_valid[n]` is true.
- `apparent_power_display[n]`: apparent power using the same default 0.1 VA unit.
- `apparent_power_source[n]`: origin of the apparent-power coefficient.
- `power_factor[n]`: signed PF from -1 to +1 after `power_factor_valid[n]` is true.
- `power_factor_abs[n]`: PF magnitude from 0 to 1, independent of lead/lag sign.
- `current_polarity[n]`: NORMAL or REVERSED software correction for the phase CT.
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
- Current: old coefficient `0.00036565` produced 0.01 A units, so the
  provisional SI scale is `0.0000036565 A/count`.
- Active power: old coefficient `0.0170276` produced 0.1 W units, so the
  provisional SI scale is `0.00170276 W/count`; phase A also uses correction
  `1.02`.

The ADE7880 internally gain-matches active and apparent power on each phase,
so the matching legacy power coefficient is also used provisionally for VA and
is explicitly marked `APP_CALIBRATION_DERIVED`. Directly recovered values are
marked `APP_CALIBRATION_LEGACY`. Replace every provisional value with measured
two-point calibration before accuracy-dependent use. Phase C and neutral
current remain invalid until explicit calibrations are supplied.

## SI values and configurable display units

Calculations should always use `current_a`, `active_power_w`, and
`apparent_power_va`. The separate display fields preserve the previous
fixed-point convention without hiding the physical unit:

- `APP_DEFAULT_CURRENT_DISPLAY_UNITS_PER_AMP = 100`: 0.01 A per display unit.
- `APP_DEFAULT_POWER_DISPLAY_UNITS_PER_WATT = 10`: 0.1 W/VA per display unit.

Thus `0.17 A` appears as `17` in `current_display`, while `23.1 W` appears as
`231` in `active_power_display`. Change the two defaults in `app.h`, override
them with compiler definitions, or change them at runtime:

```c
/* Show direct amperes and watts in the optional display fields. */
(void)App_SetDisplayUnits(1.0F, 1.0F);
```

Changing display units never changes calibration or SI values.

## Multi-point calibration

The ADE7880 signal path is designed to be linear. Start with one gain/offset
line fitted across the required range instead of unrelated low/high-range
coefficients that can create a discontinuity. Use at least three well-separated
points; five points give a stronger linearity check. Average multiple raw
samples at every stable point and record the reference instrument at the same
time.

```c
const ADE7880_CalibrationPoint phase_a_current_points[] = {
    {raw_at_0_10_a, 0.10F},
    {raw_at_1_00_a, 1.00F},
    {raw_at_5_00_a, 5.00F}
};
ADE7880_LinearCalibration phase_a_current_result;
float worst_current_error_a;

(void)App_CalibratePhaseMultiPoint(
    APP_PHASE_QUANTITY_CURRENT_RMS,
    ADE7880_PHASE_A,
    phase_a_current_points,
    sizeof(phase_a_current_points) / sizeof(phase_a_current_points[0]),
    &phase_a_current_result,
    &worst_current_error_a);
```

Use `APP_PHASE_QUANTITY_VOLTAGE_RMS` with reference volts,
`APP_PHASE_QUANTITY_ACTIVE_POWER` with reference watts, and
`APP_PHASE_QUANTITY_APPARENT_POWER` with reference VA. The optional result
contains the exact installed `scale` and `offset`; `worst_current_error_a`
reports the largest residual among the supplied points. A large residual means
that the raw/reference records, settling, phase calibration, noise, or hardware
linearity must be investigated rather than hidden with another arbitrary gain.

The calibration takes effect on the next complete sample and is stored in RAM.
After validation at independent points, save the returned scale/offset in code
or nonvolatile memory and reinstall it with `App_SetPhaseCalibration()` after
every reset. A lamp's printed wattage is only nominal; use a reference meter or
accurate source.

Analog Devices recommends gain calibration for every meter. CT phase
calibration is often needed, especially at low power factor, and offset
calibration is useful when high accuracy is required across a large dynamic
range. If the low-load residual remains systematic after a good multi-point
fit, calibrate the ADE7880 RMS/power offset registers rather than adding a
piecewise jump in application software.

## CT direction and signed power factor

For a consumption-only installation, fit the CT so normal load power produces
positive `active_power_w[n]`. Follow the CT manufacturer's P1/K to source,
P2/L to load, S1/k to current-positive, and S2/l to current-negative markings
when those markings exist. Verify the board schematic before assuming terminal
names.

If the installed CT cannot be reversed, configure the correction once after
`App_Init()`:

```c
(void)App_SetCurrentPolarity(ADE7880_PHASE_A,
                             APP_CURRENT_POLARITY_REVERSED);
```

The correction changes the signs of active power and `power_factor`; it does
not change RMS current, apparent power, or `power_factor_abs`. Do not infer and
flip polarity automatically on every negative sample: a bidirectional system
can legitimately report negative active power. A negative signed PF is also
not automatically an error; ADE7880 uses its sign to distinguish leading and
lagging current. Use `power_factor_abs[n]` when only PF magnitude is required.

Never open-circuit a CT secondary while primary current can flow. De-energize
the primary or use the manufacturer's approved shorting procedure before
changing CT wiring.

## Status handling

Always check returned status values:

- `ADE7880_STATUS_OK`: operation succeeded.
- `ADE7880_STATUS_INVALID_ARGUMENT`: pointer, phase, width, or timeout is bad.
- `ADE7880_STATUS_BUS_ERROR`: the platform SPI driver failed.
- `ADE7880_STATUS_TIMEOUT`: SPI or reset did not finish in time.
- `ADE7880_STATUS_VERIFY_FAILED`: a written register read back differently.
