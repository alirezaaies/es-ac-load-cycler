/**
 * @file ade7880.h
 * @brief Portable SPI driver for the Analog Devices ADE7880.
 *
 * The library contains no STM32 or HAL dependency. A project supplies small
 * callbacks for SPI, chip select, and delay, so the same source can be reused
 * with another MCU or RTOS.
 *
 * Typical use:
 * @code
 * ADE7880_Device device;
 * ADE7880_Transport transport = {
 *     .context = &my_spi,
 *     .write = board_spi_write,
 *     .read = board_spi_read,
 *     .select = board_chip_select,
 *     .delay_ms = board_delay_ms
 * };
 * ADE7880_MeasurementsRaw raw;
 *
 * if (ADE7880_Init(&device, &transport, 20U) == ADE7880_STATUS_OK &&
 *     ADE7880_Begin(&device, 100U) == ADE7880_STATUS_OK) {
 *     (void)ADE7880_ReadAll(&device, &raw);
 * }
 * @endcode
 *
 * @warning RMS and power registers contain ADC/DSP counts, not volts, amperes,
 * or watts. Use calibration measured on the actual board before treating a
 * converted value as an accurate engineering measurement.
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
    ADE7880_STATUS_OK = 0,          /**< Operation completed successfully. */
    ADE7880_STATUS_INVALID_ARGUMENT, /**< Null pointer, bad width, or bad phase. */
    ADE7880_STATUS_BUS_ERROR,       /**< Platform SPI callback reported an error. */
    ADE7880_STATUS_TIMEOUT,         /**< SPI or reset operation exceeded its limit. */
    ADE7880_STATUS_VERIFY_FAILED    /**< Register readback did not match the write. */
} ADE7880_Status;

/** Phase index used by all phase-oriented functions and arrays. */
typedef enum {
    ADE7880_PHASE_A = 0, /**< Phase A and index zero in phase arrays. */
    ADE7880_PHASE_B,     /**< Phase B and index one in phase arrays. */
    ADE7880_PHASE_C,     /**< Phase C and index two in phase arrays. */
    ADE7880_PHASE_COUNT  /**< Number of phases; never pass this as a phase. */
} ADE7880_Phase;

/** Common register addresses; generic access also supports every other register. */
typedef enum {
    ADE7880_REG_AIRMS = 0x43C0,    /**< Phase-A RMS current, 24 useful bits. */
    ADE7880_REG_AVRMS = 0x43C1,    /**< Phase-A RMS voltage, 24 useful bits. */
    ADE7880_REG_BIRMS = 0x43C2,    /**< Phase-B RMS current, 24 useful bits. */
    ADE7880_REG_BVRMS = 0x43C3,    /**< Phase-B RMS voltage, 24 useful bits. */
    ADE7880_REG_CIRMS = 0x43C4,    /**< Phase-C RMS current, 24 useful bits. */
    ADE7880_REG_CVRMS = 0x43C5,    /**< Phase-C RMS voltage, 24 useful bits. */
    ADE7880_REG_NIRMS = 0x43C6,    /**< Neutral RMS current, 24 useful bits. */
    ADE7880_REG_RUN = 0xE228,      /**< Write one to start the internal DSP. */
    ADE7880_REG_STATUS1 = 0xE503,  /**< Interrupt/status flags including RSTDONE. */
    ADE7880_REG_AWATT = 0xE513,    /**< Phase-A signed active power. */
    ADE7880_REG_BWATT = 0xE514,    /**< Phase-B signed active power. */
    ADE7880_REG_CWATT = 0xE515,    /**< Phase-C signed active power. */
    ADE7880_REG_AVA = 0xE519,      /**< Phase-A apparent power. */
    ADE7880_REG_BVA = 0xE51A,      /**< Phase-B apparent power. */
    ADE7880_REG_CVA = 0xE51B,      /**< Phase-C apparent power. */
    ADE7880_REG_CHECKSUM = 0xE51F, /**< Configuration checksum. */
    ADE7880_REG_CONFIG = 0xE618,   /**< Main configuration and software reset. */
    ADE7880_REG_VERSION = 0xE707,  /**< Read-only silicon version byte. */
    ADE7880_REG_APF = 0xE902,      /**< Phase-A signed power factor in Q1.15. */
    ADE7880_REG_BPF = 0xE903,      /**< Phase-B signed power factor in Q1.15. */
    ADE7880_REG_CPF = 0xE904,      /**< Phase-C signed power factor in Q1.15. */
    ADE7880_REG_CONFIG2 = 0xEC01   /**< Serial-port configuration and lock. */
} ADE7880_Register;

/** One raw phase sample. RMS values are 24-bit and PF is signed Q1.15. */
typedef struct {
    uint32_t voltage_rms;     /**< Uncalibrated RMS voltage register count. */
    uint32_t current_rms;     /**< Uncalibrated RMS current register count. */
    int32_t active_power;     /**< Sign-extended 24-bit active-power count. */
    int32_t apparent_power;   /**< Sign-extended apparent-power count. */
    int16_t power_factor_q15; /**< Signed Q1.15 value; divide by 32768. */
} ADE7880_PhaseRaw;

/** Consistent three-phase snapshot; it is updated only after all reads succeed. */
typedef struct {
    ADE7880_PhaseRaw phase[ADE7880_PHASE_COUNT]; /**< A, B, and C samples. */
    uint32_t neutral_current_rms; /**< Uncalibrated neutral-current count. */
} ADE7880_MeasurementsRaw;

/** Linear conversion for one quantity: engineering = raw * scale + offset. */
typedef struct {
    float scale;  /**< Engineering units represented by one raw count. */
    float offset; /**< Engineering value added after scaling. */
} ADE7880_LinearCalibration;

/** Calibration values kept outside the IC and easy to store in Flash. */
typedef struct {
    ADE7880_LinearCalibration voltage[ADE7880_PHASE_COUNT]; /**< Volts. */
    ADE7880_LinearCalibration current[ADE7880_PHASE_COUNT]; /**< Amperes. */
    ADE7880_LinearCalibration active_power[ADE7880_PHASE_COUNT]; /**< Watts. */
    ADE7880_LinearCalibration apparent_power[ADE7880_PHASE_COUNT]; /**< VA. */
    ADE7880_LinearCalibration neutral_current; /**< Neutral amperes. */
} ADE7880_Calibration;

/** Measurements converted to volts, amperes, watts, volt-amperes, and PF. */
typedef struct {
    struct {
        float voltage_v;         /**< Calibrated RMS voltage in volts. */
        float current_a;         /**< Calibrated RMS current in amperes. */
        float active_power_w;    /**< Calibrated active power in watts. */
        float apparent_power_va; /**< Calibrated apparent power in VA. */
        float power_factor;      /**< Dimensionless value from -1 to +1. */
    } phase[ADE7880_PHASE_COUNT];
    float neutral_current_a; /**< Calibrated neutral RMS current in amperes. */
} ADE7880_Measurements;

/** Platform-neutral SPI and timing operations. Chip select uses true=active. */
typedef struct {
    void *context; /**< User pointer passed unchanged to every callback. */
    /** Transmit bytes while chip select is already active. */
    ADE7880_Status (*write)(void *context, const uint8_t *data,
                            size_t length, uint32_t timeout_ms);
    /** Receive bytes while generating SPI clocks and keeping CS active. */
    ADE7880_Status (*read)(void *context, uint8_t *data,
                           size_t length, uint32_t timeout_ms);
    /** Drive chip select; true means selected and therefore electrically low. */
    void (*select)(void *context, bool active);
    /** Blocking millisecond delay used only during initialization/reset. */
    void (*delay_ms)(void *context, uint32_t delay_ms);
} ADE7880_Transport;

/** State for one independently connected ADE7880. */
typedef struct {
    ADE7880_Transport transport; /**< Copy of the validated platform callbacks. */
    uint32_t timeout_ms; /**< Timeout passed to each SPI callback. */
    bool bound; /**< True after ADE7880_Init accepts the transport. */
} ADE7880_Device;

/**
 * @brief Bind platform callbacks without communicating with the IC.
 * @param device Driver instance owned by the caller.
 * @param transport Fully populated SPI, CS, and delay callbacks.
 * @param timeout_ms Nonzero timeout used for each individual SPI operation.
 * @return ADE7880_STATUS_OK or ADE7880_STATUS_INVALID_ARGUMENT.
 */
ADE7880_Status ADE7880_Init(ADE7880_Device *device,
                            const ADE7880_Transport *transport,
                            uint32_t timeout_ms);

/**
 * @brief Select and lock SPI, reset the IC, and start its measurement DSP.
 * @param device Initialized driver instance.
 * @param reset_timeout_ms Maximum time to wait for RSTDONE after reset.
 * @return First error from port selection, reset, or RUN verification.
 */
ADE7880_Status ADE7880_Begin(ADE7880_Device *device,
                             uint32_t reset_timeout_ms);

/**
 * @brief Toggle SS three times and verify CONFIG2 after locking the SPI port.
 * @param device Initialized driver instance.
 * @return ADE7880_STATUS_OK or a transport/readback error.
 */
ADE7880_Status ADE7880_SelectSpi(ADE7880_Device *device);

/**
 * @brief Request a software reset and wait for STATUS1.RSTDONE.
 * @param device Initialized driver instance.
 * @param reset_timeout_ms Nonzero bounded wait time in milliseconds.
 * @return ADE7880_STATUS_OK, a transport error, or ADE7880_STATUS_TIMEOUT.
 */
ADE7880_Status ADE7880_SoftwareReset(ADE7880_Device *device,
                                     uint32_t reset_timeout_ms);

/**
 * @brief Start the digital signal processor and verify RUN reads back as one.
 * @param device Initialized driver instance.
 * @return ADE7880_STATUS_OK or a transport/readback error.
 */
ADE7880_Status ADE7880_StartMeasurements(ADE7880_Device *device);

/**
 * @brief Read an 8-, 16-, or 32-bit register in big-endian wire order.
 * @param device Initialized driver instance.
 * @param address 16-bit ADE7880 register address.
 * @param width Register width in bytes; only 1, 2, and 4 are accepted.
 * @param value Destination receiving a zero-extended 32-bit value.
 * @return ADE7880_STATUS_OK or an argument/transport error.
 */
ADE7880_Status ADE7880_ReadRegister(ADE7880_Device *device,
                                    uint16_t address,
                                    uint8_t width,
                                    uint32_t *value);

/**
 * @brief Write an 8-, 16-, or 32-bit register in big-endian wire order.
 * @param device Initialized driver instance.
 * @param address 16-bit ADE7880 register address.
 * @param width Register width in bytes; only 1, 2, and 4 are accepted.
 * @param value Value whose least-significant width bytes are transmitted.
 * @return ADE7880_STATUS_OK or an argument/transport error.
 */
ADE7880_Status ADE7880_WriteRegister(ADE7880_Device *device,
                                     uint16_t address,
                                     uint8_t width,
                                     uint32_t value);

/**
 * @brief Write a register and compare its complete value with a readback.
 * @return ADE7880_STATUS_VERIFY_FAILED when the masked values differ.
 */
ADE7880_Status ADE7880_WriteRegisterVerified(ADE7880_Device *device,
                                             uint16_t address,
                                             uint8_t width,
                                             uint32_t value);

/**
 * @brief Read voltage, current, active/apparent power, and PF for one phase.
 * @param device Initialized driver instance.
 * @param phase ADE7880_PHASE_A, ADE7880_PHASE_B, or ADE7880_PHASE_C.
 * @param measurement Destination updated only when all phase reads succeed.
 * @return ADE7880_STATUS_OK or the first read error.
 */
ADE7880_Status ADE7880_ReadPhase(ADE7880_Device *device,
                                 ADE7880_Phase phase,
                                 ADE7880_PhaseRaw *measurement);

/**
 * @brief Read all phases plus neutral current into one consistent snapshot.
 * @param device Initialized driver instance.
 * @param measurements Destination updated only after every register succeeds.
 * @return ADE7880_STATUS_OK or the first read error.
 */
ADE7880_Status ADE7880_ReadAll(ADE7880_Device *device,
                               ADE7880_MeasurementsRaw *measurements);

/**
 * @brief Apply one linear calibration pair to one raw value.
 * @param raw Uncalibrated register count.
 * @param calibration Scale and offset obtained for this exact channel.
 * @param engineering_value Destination in the calibration's engineering unit.
 * @return ADE7880_STATUS_OK or ADE7880_STATUS_INVALID_ARGUMENT.
 */
ADE7880_Status ADE7880_ApplyLinearCalibration(
    float raw, const ADE7880_LinearCalibration *calibration,
    float *engineering_value);

/**
 * @brief Convert a complete raw snapshot without modifying ADE7880 registers.
 * @param raw Complete register snapshot returned by ADE7880_ReadAll.
 * @param calibration Per-channel scale and offset values.
 * @param measurements Destination for volts, amperes, watts, VA, and PF.
 * @return ADE7880_STATUS_OK or ADE7880_STATUS_INVALID_ARGUMENT.
 */
ADE7880_Status ADE7880_Convert(const ADE7880_MeasurementsRaw *raw,
                               const ADE7880_Calibration *calibration,
                               ADE7880_Measurements *measurements);

/**
 * @brief Derive scale and offset from two distinct calibration points.
 * @param raw_1 Average raw count measured at the first reference point.
 * @param reference_1 Engineering value at the first point, for example volts.
 * @param raw_2 Average raw count measured at the second reference point.
 * @param reference_2 Engineering value at the second point.
 * @param calibration Destination receiving the calculated scale and offset.
 * @return ADE7880_STATUS_OK, or INVALID_ARGUMENT when points share one raw value.
 */
ADE7880_Status ADE7880_CalculateLinearCalibration(
    float raw_1, float reference_1, float raw_2, float reference_2,
    ADE7880_LinearCalibration *calibration);

#ifdef __cplusplus
}
#endif

#endif /* ADE7880_H */
