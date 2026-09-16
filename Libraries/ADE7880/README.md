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
- `voltage_source[n]`: NONE, LEGACY, or USER calibration origin.
- `last_status`: the exact result of the latest driver operation.

The application loads recovered legacy voltage scales for phases A and B so a
connected board can show an approximate voltage immediately. Replace them with
`App_SetVoltageCalibration()` after measuring two reference points. Phase C
remains invalid until an explicit calibration is supplied.

## Status handling

Always check returned status values:

- `ADE7880_STATUS_OK`: operation succeeded.
- `ADE7880_STATUS_INVALID_ARGUMENT`: pointer, phase, width, or timeout is bad.
- `ADE7880_STATUS_BUS_ERROR`: the platform SPI driver failed.
- `ADE7880_STATUS_TIMEOUT`: SPI or reset did not finish in time.
- `ADE7880_STATUS_VERIFY_FAILED`: a written register read back differently.

