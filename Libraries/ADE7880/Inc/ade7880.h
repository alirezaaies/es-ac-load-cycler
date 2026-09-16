/**
 * @file ade7880.h
 * @brief Portable SPI driver for the Analog Devices ADE7880.
 *
 * The library contains no STM32 or HAL dependency. A project supplies small
 * callbacks for SPI, chip select, and delay, so the same source can be reused
 * with another MCU or RTOS.
 */
#ifndef ADE7880_H
#define ADE7880_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Result returned by every operation and by the platform callbacks. */
typedef enum {
    ADE7880_STATUS_OK = 0,
    ADE7880_STATUS_INVALID_ARGUMENT,
    ADE7880_STATUS_BUS_ERROR,
    ADE7880_STATUS_TIMEOUT,
    ADE7880_STATUS_VERIFY_FAILED
} ADE7880_Status;

/** Phase index used by all phase-oriented functions and arrays. */
typedef enum {
    ADE7880_PHASE_A = 0,
    ADE7880_PHASE_B,
    ADE7880_PHASE_C,
    ADE7880_PHASE_COUNT
} ADE7880_Phase;

/** Common register addresses; generic access also supports every other register. */
typedef enum {
    ADE7880_REG_AIRMS = 0x43C0,
    ADE7880_REG_AVRMS = 0x43C1,
    ADE7880_REG_BIRMS = 0x43C2,
    ADE7880_REG_BVRMS = 0x43C3,
    ADE7880_REG_CIRMS = 0x43C4,
    ADE7880_REG_CVRMS = 0x43C5,
    ADE7880_REG_NIRMS = 0x43C6,
    ADE7880_REG_RUN = 0xE228,
    ADE7880_REG_STATUS1 = 0xE503,
    ADE7880_REG_AWATT = 0xE513,
    ADE7880_REG_BWATT = 0xE514,
    ADE7880_REG_CWATT = 0xE515,
    ADE7880_REG_AVA = 0xE519,
    ADE7880_REG_BVA = 0xE51A,
    ADE7880_REG_CVA = 0xE51B,
    ADE7880_REG_CHECKSUM = 0xE51F,
    ADE7880_REG_CONFIG = 0xE618,
    ADE7880_REG_VERSION = 0xE707,
    ADE7880_REG_APF = 0xE902,
    ADE7880_REG_BPF = 0xE903,
    ADE7880_REG_CPF = 0xE904,
    ADE7880_REG_CONFIG2 = 0xEC01
} ADE7880_Register;

/** One raw phase sample. RMS values are 24-bit and PF is signed Q1.15. */
typedef struct {
    uint32_t voltage_rms;
    uint32_t current_rms;
    int32_t active_power;
    int32_t apparent_power;
    int16_t power_factor_q15;
} ADE7880_PhaseRaw;

/** Consistent three-phase snapshot; it is updated only after all reads succeed. */
typedef struct {
    ADE7880_PhaseRaw phase[ADE7880_PHASE_COUNT];
    uint32_t neutral_current_rms;
} ADE7880_MeasurementsRaw;

/** Linear conversion for one quantity: engineering = raw * scale + offset. */
typedef struct {
    float scale;
    float offset;
} ADE7880_LinearCalibration;

/** Calibration values kept outside the IC and easy to store in Flash. */
typedef struct {
    ADE7880_LinearCalibration voltage[ADE7880_PHASE_COUNT];
    ADE7880_LinearCalibration current[ADE7880_PHASE_COUNT];
    ADE7880_LinearCalibration active_power[ADE7880_PHASE_COUNT];
    ADE7880_LinearCalibration apparent_power[ADE7880_PHASE_COUNT];
    ADE7880_LinearCalibration neutral_current;
} ADE7880_Calibration;

/** Measurements converted to volts, amperes, watts, volt-amperes, and PF. */
typedef struct {
    struct {
        float voltage_v;
        float current_a;
        float active_power_w;
        float apparent_power_va;
        float power_factor;
    } phase[ADE7880_PHASE_COUNT];
    float neutral_current_a;
} ADE7880_Measurements;

/** Platform-neutral SPI and timing operations. Chip select uses true=active. */
typedef struct {
    void *context;
    ADE7880_Status (*write)(void *context, const uint8_t *data,
                            size_t length, uint32_t timeout_ms);
    ADE7880_Status (*read)(void *context, uint8_t *data,
                           size_t length, uint32_t timeout_ms);
    void (*select)(void *context, bool active);
    void (*delay_ms)(void *context, uint32_t delay_ms);
} ADE7880_Transport;

/** State for one independently connected ADE7880. */
typedef struct {
    ADE7880_Transport transport;
    uint32_t timeout_ms;
    bool bound;
} ADE7880_Device;

/** Bind a platform transport without communicating with the IC. */
ADE7880_Status ADE7880_Init(ADE7880_Device *device,
                            const ADE7880_Transport *transport,
                            uint32_t timeout_ms);

/** Select and lock SPI, software-reset the IC, and start its DSP. */
ADE7880_Status ADE7880_Begin(ADE7880_Device *device,
                             uint32_t reset_timeout_ms);

/** Toggle SS three times and verify CONFIG2 after locking the SPI port. */
ADE7880_Status ADE7880_SelectSpi(ADE7880_Device *device);

/** Reset all applicable registers and wait for STATUS1.RSTDONE. */
ADE7880_Status ADE7880_SoftwareReset(ADE7880_Device *device,
                                     uint32_t reset_timeout_ms);

/** Start the digital signal processor and verify RUN reads back as one. */
ADE7880_Status ADE7880_StartMeasurements(ADE7880_Device *device);

/** Read an 8-, 16-, or 32-bit register in ADE7880 big-endian wire order. */
ADE7880_Status ADE7880_ReadRegister(ADE7880_Device *device,
                                    uint16_t address,
                                    uint8_t width,
                                    uint32_t *value);

/** Write an 8-, 16-, or 32-bit register in ADE7880 big-endian wire order. */
ADE7880_Status ADE7880_WriteRegister(ADE7880_Device *device,
                                     uint16_t address,
                                     uint8_t width,
                                     uint32_t value);

/** Write a register and compare its complete value with a readback. */
ADE7880_Status ADE7880_WriteRegisterVerified(ADE7880_Device *device,
                                             uint16_t address,
                                             uint8_t width,
                                             uint32_t value);

/** Read voltage, current, active/apparent power, and PF for one phase. */
ADE7880_Status ADE7880_ReadPhase(ADE7880_Device *device,
                                 ADE7880_Phase phase,
                                 ADE7880_PhaseRaw *measurement);

/** Read all phases plus neutral current into one consistent snapshot. */
ADE7880_Status ADE7880_ReadAll(ADE7880_Device *device,
                               ADE7880_MeasurementsRaw *measurements);

/** Apply host-side linear calibration without modifying ADE7880 registers. */
void ADE7880_Convert(const ADE7880_MeasurementsRaw *raw,
                     const ADE7880_Calibration *calibration,
                     ADE7880_Measurements *measurements);

/** Derive scale and offset from two distinct raw/reference calibration points. */
ADE7880_Status ADE7880_CalculateLinearCalibration(
    float raw_1, float reference_1, float raw_2, float reference_2,
    ADE7880_LinearCalibration *calibration);

#ifdef __cplusplus
}
#endif

#endif /* ADE7880_H */
