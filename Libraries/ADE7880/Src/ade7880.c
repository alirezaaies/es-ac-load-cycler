/**
 * @file ade7880.c
 * @brief Platform-independent ADE7880 SPI protocol and data conversion.
 */
#include "ade7880.h"

#include <string.h>

#define ADE7880_SPI_READ  0x01U /**< SPI command byte for register reads. */
#define ADE7880_SPI_WRITE 0x00U /**< SPI command byte for register writes. */
#define ADE7880_CONFIG_SWRST (1UL << 7U) /**< Software-reset request bit. */
#define ADE7880_STATUS1_RSTDONE (1UL << 15U) /**< Reset-complete flag. */
#define ADE7880_POWER_UP_DELAY_MS 50U /**< 40 ms data-sheet delay plus margin. */

/** Register lookup tables keep phase selection out of the read algorithm. */
static const uint16_t voltage_register[ADE7880_PHASE_COUNT] = {
    ADE7880_REG_AVRMS, ADE7880_REG_BVRMS, ADE7880_REG_CVRMS
};
static const uint16_t current_register[ADE7880_PHASE_COUNT] = {
    ADE7880_REG_AIRMS, ADE7880_REG_BIRMS, ADE7880_REG_CIRMS
};
static const uint16_t active_power_register[ADE7880_PHASE_COUNT] = {
    ADE7880_REG_AWATT, ADE7880_REG_BWATT, ADE7880_REG_CWATT
};
static const uint16_t apparent_power_register[ADE7880_PHASE_COUNT] = {
    ADE7880_REG_AVA, ADE7880_REG_BVA, ADE7880_REG_CVA
};
static const uint16_t power_factor_register[ADE7880_PHASE_COUNT] = {
    ADE7880_REG_APF, ADE7880_REG_BPF, ADE7880_REG_CPF
};

/** Validate the complete platform binding before asserting chip select. */
static bool valid_device(const ADE7880_Device *device)
{
    return (device != NULL) && device->bound &&
           (device->transport.write != NULL) &&
           (device->transport.read != NULL) &&
           (device->transport.select != NULL) &&
           (device->transport.delay_ms != NULL) &&
           (device->timeout_ms != 0U);
}

/** Convert the ADE7880's sign-extended 24-bit values to standard int32_t. */
static int32_t signed_24(uint32_t value)
{
    value &= 0x00FFFFFFUL;
    if ((value & 0x00800000UL) != 0U) {
        value |= 0xFF000000UL;
    }
    return (int32_t)value;
}

/** Keep CS cleanup in one place so every error leaves the bus inactive. */
static ADE7880_Status transfer(ADE7880_Device *device,
                               const uint8_t *command,
                               size_t command_length,
                               uint8_t *response,
                               size_t response_length)
{
    ADE7880_Status status;

    device->transport.select(device->transport.context, true);
    status = device->transport.write(device->transport.context, command,
                                     command_length, device->timeout_ms);
    if ((status == ADE7880_STATUS_OK) && (response_length != 0U)) {
        status = device->transport.read(device->transport.context, response,
                                        response_length, device->timeout_ms);
    }
    device->transport.select(device->transport.context, false);
    return status;
}

/** Validate and copy the caller's hardware-abstraction callbacks. */
ADE7880_Status ADE7880_Init(ADE7880_Device *device,
                            const ADE7880_Transport *transport,
                            uint32_t timeout_ms)
{
    /* Reject an incomplete transport now, before any GPIO or SPI is touched. */
    if ((device == NULL) || (transport == NULL) ||
        (transport->write == NULL) || (transport->read == NULL) ||
        (transport->select == NULL) || (transport->delay_ms == NULL) ||
        (timeout_ms == 0U)) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }

    memset(device, 0, sizeof(*device));
    /* Copy callbacks so their source structure may be a temporary variable. */
    device->transport = *transport;
    device->timeout_ms = timeout_ms;
    device->bound = true;
    device->transport.select(device->transport.context, false);
    return ADE7880_STATUS_OK;
}

/** Execute one complete register-read transaction. */
ADE7880_Status ADE7880_ReadRegister(ADE7880_Device *device,
                                    uint16_t address,
                                    uint8_t width,
                                    uint32_t *value)
{
    /* Every read begins with command, high address byte, and low address byte. */
    const uint8_t command[3] = {
        ADE7880_SPI_READ, (uint8_t)(address >> 8U), (uint8_t)address
    };
    uint8_t bytes[4] = {0U};
    uint32_t result = 0U;
    ADE7880_Status status;

    if (!valid_device(device) || (value == NULL) ||
        ((width != 1U) && (width != 2U) && (width != 4U))) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }

    status = transfer(device, command, sizeof(command), bytes, width);
    if (status != ADE7880_STATUS_OK) {
        return status;
    }
    /* ADE7880 transmits the most-significant data byte first. */
    for (uint8_t index = 0U; index < width; ++index) {
        result = (result << 8U) | bytes[index];
    }
    *value = result;
    return ADE7880_STATUS_OK;
}

/** Build and transmit one complete register-write frame. */
ADE7880_Status ADE7880_WriteRegister(ADE7880_Device *device,
                                     uint16_t address,
                                     uint8_t width,
                                     uint32_t value)
{
    /* The largest legal frame is a 3-byte header plus a 4-byte value. */
    uint8_t frame[7] = {
        ADE7880_SPI_WRITE, (uint8_t)(address >> 8U), (uint8_t)address
    };

    if (!valid_device(device) ||
        ((width != 1U) && (width != 2U) && (width != 4U))) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }
    /* Append only the selected register width, most-significant byte first. */
    for (uint8_t index = 0U; index < width; ++index) {
        const uint8_t shift = (uint8_t)((width - 1U - index) * 8U);
        frame[3U + index] = (uint8_t)(value >> shift);
    }
    return transfer(device, frame, (size_t)width + 3U, NULL, 0U);
}

/** Write a register and prove the operation by reading it back. */
ADE7880_Status ADE7880_WriteRegisterVerified(ADE7880_Device *device,
                                             uint16_t address,
                                             uint8_t width,
                                             uint32_t value)
{
    uint32_t readback;
    uint32_t mask;
    ADE7880_Status status = ADE7880_WriteRegister(device, address, width, value);

    if (status != ADE7880_STATUS_OK) {
        return status;
    }
    status = ADE7880_ReadRegister(device, address, width, &readback);
    if (status != ADE7880_STATUS_OK) {
        return status;
    }
    /* Ignore unused upper bits when verifying an 8- or 16-bit register. */
    mask = (width == 4U) ? UINT32_MAX : ((1UL << (width * 8U)) - 1UL);
    return ((readback & mask) == (value & mask))
               ? ADE7880_STATUS_OK : ADE7880_STATUS_VERIFY_FAILED;
}

/** Select SPI after power-up and lock the serial interface through CONFIG2. */
ADE7880_Status ADE7880_SelectSpi(ADE7880_Device *device)
{
    if (!valid_device(device)) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }

    /* Three high-to-low SS transitions switch the post-reset port to SPI. */
    for (uint8_t pulse = 0U; pulse < 3U; ++pulse) {
        device->transport.select(device->transport.context, false);
        device->transport.delay_ms(device->transport.context, 1U);
        device->transport.select(device->transport.context, true);
        device->transport.delay_ms(device->transport.context, 1U);
    }
    device->transport.select(device->transport.context, false);
    device->transport.delay_ms(device->transport.context, 1U);

    /* Any CONFIG2 write locks the already selected SPI port. */
    return ADE7880_WriteRegisterVerified(device, ADE7880_REG_CONFIG2, 1U, 0U);
}

/** Perform a bounded software reset and clear the completion flag. */
ADE7880_Status ADE7880_SoftwareReset(ADE7880_Device *device,
                                     uint32_t reset_timeout_ms)
{
    uint32_t config;
    uint32_t status1;
    ADE7880_Status status;

    if (!valid_device(device) || (reset_timeout_ms == 0U)) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }
    status = ADE7880_ReadRegister(device, ADE7880_REG_CONFIG, 2U, &config);
    if (status != ADE7880_STATUS_OK) {
        return status;
    }
    status = ADE7880_WriteRegister(device, ADE7880_REG_CONFIG, 2U,
                                   config | ADE7880_CONFIG_SWRST);
    if (status != ADE7880_STATUS_OK) {
        return status;
    }

    /* Poll once per millisecond; the bounded loop prevents startup lockup. */
    for (uint32_t elapsed = 0U; elapsed < reset_timeout_ms; ++elapsed) {
        status = ADE7880_ReadRegister(device, ADE7880_REG_CONFIG, 2U, &config);
        if (status != ADE7880_STATUS_OK) {
            return status;
        }
        status = ADE7880_ReadRegister(device, ADE7880_REG_STATUS1, 4U, &status1);
        if (status != ADE7880_STATUS_OK) {
            return status;
        }
        if (((config & ADE7880_CONFIG_SWRST) == 0U) &&
            ((status1 & ADE7880_STATUS1_RSTDONE) != 0U)) {
            /* RSTDONE is write-one-to-clear; leave a clean status for the app. */
            return ADE7880_WriteRegister(device, ADE7880_REG_STATUS1, 4U,
                                         ADE7880_STATUS1_RSTDONE);
        }
        device->transport.delay_ms(device->transport.context, 1U);
    }
    return ADE7880_STATUS_TIMEOUT;
}

/** Start the measurement DSP and verify the RUN register. */
ADE7880_Status ADE7880_StartMeasurements(ADE7880_Device *device)
{
    return ADE7880_WriteRegisterVerified(device, ADE7880_REG_RUN, 2U, 1U);
}

/** Run the complete power-up sequence required before measurement reads. */
ADE7880_Status ADE7880_Begin(ADE7880_Device *device,
                             uint32_t reset_timeout_ms)
{
    ADE7880_Status status;

    if (!valid_device(device)) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }
    /* Datasheet power-up is about 40 ms; 50 ms provides explicit margin. */
    device->transport.delay_ms(device->transport.context,
                               ADE7880_POWER_UP_DELAY_MS);
    status = ADE7880_SelectSpi(device);
    if (status == ADE7880_STATUS_OK) {
        status = ADE7880_SoftwareReset(device, reset_timeout_ms);
    }
    if (status == ADE7880_STATUS_OK) {
        status = ADE7880_StartMeasurements(device);
    }
    return status;
}

/** Collect every supported register for one selected phase. */
ADE7880_Status ADE7880_ReadPhase(ADE7880_Device *device,
                                 ADE7880_Phase phase,
                                 ADE7880_PhaseRaw *measurement)
{
    ADE7880_PhaseRaw sample;
    uint32_t value;
    ADE7880_Status status;

    if (!valid_device(device) || (measurement == NULL) ||
        ((uint32_t)phase >= (uint32_t)ADE7880_PHASE_COUNT)) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }

    /* Use a local sample so the caller never receives a partially read phase. */
    status = ADE7880_ReadRegister(device, voltage_register[phase], 4U, &value);
    if (status != ADE7880_STATUS_OK) return status;
    sample.voltage_rms = value & 0x00FFFFFFUL;

    status = ADE7880_ReadRegister(device, current_register[phase], 4U, &value);
    if (status != ADE7880_STATUS_OK) return status;
    sample.current_rms = value & 0x00FFFFFFUL;

    status = ADE7880_ReadRegister(device, active_power_register[phase], 4U, &value);
    if (status != ADE7880_STATUS_OK) return status;
    sample.active_power = signed_24(value);

    status = ADE7880_ReadRegister(device, apparent_power_register[phase], 4U, &value);
    if (status != ADE7880_STATUS_OK) return status;
    sample.apparent_power = signed_24(value);

    status = ADE7880_ReadRegister(device, power_factor_register[phase], 2U, &value);
    if (status != ADE7880_STATUS_OK) return status;
    sample.power_factor_q15 = (int16_t)value;

    *measurement = sample;
    return ADE7880_STATUS_OK;
}

/** Collect three phases and neutral into one all-or-nothing snapshot. */
ADE7880_Status ADE7880_ReadAll(ADE7880_Device *device,
                               ADE7880_MeasurementsRaw *measurements)
{
    ADE7880_MeasurementsRaw sample;
    uint32_t value;
    ADE7880_Status status;

    if (!valid_device(device) || (measurements == NULL)) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }
    /* Publish nothing until all three phase blocks and neutral have succeeded. */
    for (uint32_t phase = 0U; phase < ADE7880_PHASE_COUNT; ++phase) {
        status = ADE7880_ReadPhase(device, (ADE7880_Phase)phase,
                                   &sample.phase[phase]);
        if (status != ADE7880_STATUS_OK) {
            return status;
        }
    }
    status = ADE7880_ReadRegister(device, ADE7880_REG_NIRMS, 4U, &value);
    if (status != ADE7880_STATUS_OK) {
        return status;
    }
    sample.neutral_current_rms = value & 0x00FFFFFFUL;
    *measurements = sample;
    return ADE7880_STATUS_OK;
}

/** Convert one register count with one board-specific scale and offset. */
ADE7880_Status ADE7880_ApplyLinearCalibration(
    float raw, const ADE7880_LinearCalibration *calibration,
    float *engineering_value)
{
    if ((calibration == NULL) || (engineering_value == NULL)) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }
    *engineering_value = raw * calibration->scale + calibration->offset;
    return ADE7880_STATUS_OK;
}

/** Convert every supported quantity in a complete raw snapshot. */
ADE7880_Status ADE7880_Convert(const ADE7880_MeasurementsRaw *raw,
                               const ADE7880_Calibration *calibration,
                               ADE7880_Measurements *measurements)
{
    if ((raw == NULL) || (calibration == NULL) || (measurements == NULL)) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }
    for (uint32_t phase = 0U; phase < ADE7880_PHASE_COUNT; ++phase) {
        (void)ADE7880_ApplyLinearCalibration(
            (float)raw->phase[phase].voltage_rms,
            &calibration->voltage[phase],
            &measurements->phase[phase].voltage_v);
        (void)ADE7880_ApplyLinearCalibration(
            (float)raw->phase[phase].current_rms,
            &calibration->current[phase],
            &measurements->phase[phase].current_a);
        (void)ADE7880_ApplyLinearCalibration(
            (float)raw->phase[phase].active_power,
            &calibration->active_power[phase],
            &measurements->phase[phase].active_power_w);
        (void)ADE7880_ApplyLinearCalibration(
            (float)raw->phase[phase].apparent_power,
            &calibration->apparent_power[phase],
            &measurements->phase[phase].apparent_power_va);
        /* PF has a fixed Q1.15 format and therefore needs no board scale. */
        measurements->phase[phase].power_factor =
            (float)raw->phase[phase].power_factor_q15 / 32768.0F;
    }
    (void)ADE7880_ApplyLinearCalibration(
        (float)raw->neutral_current_rms, &calibration->neutral_current,
        &measurements->neutral_current_a);
    return ADE7880_STATUS_OK;
}

/** Solve the linear calibration equation from two measured reference points. */
ADE7880_Status ADE7880_CalculateLinearCalibration(
    float raw_1, float reference_1, float raw_2, float reference_2,
    ADE7880_LinearCalibration *calibration)
{
    const float raw_span = raw_2 - raw_1;

    if ((calibration == NULL) || (raw_span == 0.0F)) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }
    /* Solve engineering = raw * scale + offset for the two supplied points. */
    calibration->scale = (reference_2 - reference_1) / raw_span;
    calibration->offset = reference_1 - raw_1 * calibration->scale;
    return ADE7880_STATUS_OK;
}
