/**
 ******************************************************************************
 * @file    ch224q.h
 * @brief   CH224Q/CH224A USB PD Sink Controller Driver
 * @version 2.0.0
 * @date    2024-02-16
 ******************************************************************************
 * @attention
 * 
 * Based on CH224 Datasheet V2.1
 * Supports: PD3.2 EPR/AVS/PPS/SPR, BC1.2, QC2.0/3.0
 * 
 ******************************************************************************
 */

#ifndef __CH224Q_H__
#define __CH224Q_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * Configuration & Version
 * ========================================================================== */
#define CH224Q_DRIVER_VERSION       "2.0.0"
#define CH224Q_MAX_PDO_COUNT        7        // USB PD spec maximum
#define CH224Q_PDO_DATA_SIZE        48       // 0x60-0x8F register range

/* ============================================================================
 * I2C Address Configuration
 * ========================================================================== */
#define CH224Q_I2C_ADDR_0           0x22     // 7-bit address (AD0=0)
#define CH224Q_I2C_ADDR_1           0x23     // 7-bit address (AD0=1)
#define CH224Q_I2C_ADDR_DEFAULT     CH224Q_I2C_ADDR_0

/* ============================================================================
 * Register Address Definitions (Based on Datasheet Table 5-3)
 * ========================================================================== */
#define CH224Q_REG_STATUS           0x09     // I2C Status Register
#define CH224Q_REG_VOLTAGE          0x0A     // Voltage Control Register
#define CH224Q_REG_CURRENT          0x50     // Current Data Register (50mA units)
#define CH224Q_REG_AVS_HIGH         0x51     // AVS Voltage Config (High 8 bits)
#define CH224Q_REG_AVS_LOW          0x52     // AVS Voltage Config (Low 8 bits)
#define CH224Q_REG_PPS              0x53     // PPS Voltage Config
#define CH224Q_REG_PDO_BASE         0x60     // PD Source Capability base address
#define CH224Q_REG_PDO_END          0x8F     // PD Source Capability end address

/* ============================================================================
 * Status Register (0x09) Bit Definitions
 * ========================================================================== */
#define CH224Q_STATUS_BC_ACTIVE     (1 << 0) // BC1.2 Protocol Active
#define CH224Q_STATUS_QC2_ACTIVE    (1 << 1) // QC2.0 Protocol Active
#define CH224Q_STATUS_QC3_ACTIVE    (1 << 2) // QC3.0 Protocol Active
#define CH224Q_STATUS_PD_ACTIVE     (1 << 3) // PD Protocol Active
#define CH224Q_STATUS_EPR_ACTIVE    (1 << 4) // EPR Mode Active

#define CH224Q_STATUS_QC_MASK       0x06     // QC2/QC3 mask
#define CH224Q_STATUS_PD_MASK       0x18     // PD/EPR mask

/* ============================================================================
 * Voltage Control Values (0x0A Register)
 * ========================================================================== */
#define CH224Q_VOLTAGE_5V           0x00
#define CH224Q_VOLTAGE_9V           0x01
#define CH224Q_VOLTAGE_12V          0x02
#define CH224Q_VOLTAGE_15V          0x03
#define CH224Q_VOLTAGE_20V          0x04
#define CH224Q_VOLTAGE_28V          0x05
#define CH224Q_VOLTAGE_PPS          0x06
#define CH224Q_VOLTAGE_AVS          0x07

/* ============================================================================
 * AVS Configuration (0x51/0x52)
 * ========================================================================== */
#define CH224Q_AVS_ENABLE_BIT       (1 << 15) // Enable bit in high register
#define CH224Q_AVS_VOLTAGE_UNIT     100       // 100mV per unit
#define CH224Q_AVS_MIN_VOLTAGE      5000      // 5V minimum
#define CH224Q_AVS_MAX_VOLTAGE      28000     // 28V maximum

/* ============================================================================
 * PPS Configuration (0x53)
 * ========================================================================== */
#define CH224Q_PPS_VOLTAGE_UNIT     100       // 100mV per unit
#define CH224Q_PPS_MIN_VOLTAGE      5000      // 5V minimum (depends on adapter)
#define CH224Q_PPS_MAX_VOLTAGE      21000     // 21V maximum (SPR PPS)

/* ============================================================================
 * Type Definitions
 * ========================================================================== */

/**
 * @brief CH224Q voltage enumeration
 */
typedef enum {
    CH224Q_5V  = CH224Q_VOLTAGE_5V,
    CH224Q_9V  = CH224Q_VOLTAGE_9V,
    CH224Q_12V = CH224Q_VOLTAGE_12V,
    CH224Q_15V = CH224Q_VOLTAGE_15V,
    CH224Q_20V = CH224Q_VOLTAGE_20V,
    CH224Q_28V = CH224Q_VOLTAGE_28V,
    CH224Q_PPS = CH224Q_VOLTAGE_PPS,
    CH224Q_AVS = CH224Q_VOLTAGE_AVS
} ch224q_voltage_t;

/**
 * @brief Protocol status enumeration
 */
typedef enum {
    CH224Q_PROTOCOL_NONE = 0,
    CH224Q_PROTOCOL_BC12,
    CH224Q_PROTOCOL_QC2,
    CH224Q_PROTOCOL_QC3,
    CH224Q_PROTOCOL_PD,
    CH224Q_PROTOCOL_EPR
} ch224q_protocol_t;

/**
 * @brief Error codes
 */
typedef enum {
    CH224Q_OK = 0,
    CH224Q_ERROR = -1,
    CH224Q_ERROR_INVALID_PARAM = -2,
    CH224Q_ERROR_TIMEOUT = -3,
    CH224Q_ERROR_I2C = -4,
    CH224Q_ERROR_NOT_READY = -5,
    CH224Q_ERROR_OUT_OF_RANGE = -6
} ch224q_status_code_t;

/**
 * @brief CH224Q handle structure
 */
typedef struct {
    void *i2c_handle;           // Platform-specific I2C handle
    uint8_t i2c_address;        // 7-bit I2C address
    ch224q_voltage_t voltage;   // Current voltage setting
    ch224q_protocol_t protocol; // Active protocol
    bool initialized;           // Initialization flag
} ch224q_handle_t;

/**
 * @brief CH224Q status information
 */
typedef struct {
    uint8_t raw_status;         // Raw status register value
    ch224q_protocol_t protocol; // Detected protocol
    bool bc_active;
    bool qc2_active;
    bool qc3_active;
    bool pd_active;
    bool epr_active;
} ch224q_status_info_t;

/**
 * @brief Current measurement data
 */
typedef struct {
    uint8_t raw_value;          // Raw register value
    uint16_t current_ma;        // Current in mA
    uint16_t max_current_ma;    // Maximum available current
} ch224q_current_info_t;

/* ============================================================================
 * I2C Platform Abstraction Layer
 * User must implement these functions for their platform
 * ========================================================================== */

/**
 * @brief I2C write function prototype
 * @param i2c_handle Platform-specific I2C handle
 * @param dev_addr 7-bit device address
 * @param reg_addr Register address
 * @param data Pointer to data buffer
 * @param len Data length
 * @return 0 on success, negative on error
 */
typedef int (*ch224q_i2c_write_fn)(void *i2c_handle, uint8_t dev_addr, 
                                   uint8_t reg_addr, const uint8_t *data, uint16_t len);

/**
 * @brief I2C read function prototype
 * @param i2c_handle Platform-specific I2C handle
 * @param dev_addr 7-bit device address
 * @param reg_addr Register address
 * @param data Pointer to receive buffer
 * @param len Data length to read
 * @return 0 on success, negative on error
 */
typedef int (*ch224q_i2c_read_fn)(void *i2c_handle, uint8_t dev_addr,
                                  uint8_t reg_addr, uint8_t *data, uint16_t len);

/**
 * @brief Delay function prototype
 * @param ms Delay in milliseconds
 */
typedef void (*ch224q_delay_fn)(uint32_t ms);

/**
 * @brief Platform function pointers
 */
typedef struct {
    ch224q_i2c_write_fn i2c_write;
    ch224q_i2c_read_fn i2c_read;
    ch224q_delay_fn delay_ms;
} ch224q_platform_t;

/* ============================================================================
 * API Function Declarations
 * ========================================================================== */

/**
 * @brief Initialize CH224Q driver
 * @param handle Pointer to CH224Q handle structure
 * @param platform Platform function pointers
 * @param i2c_handle Platform-specific I2C handle
 * @param i2c_addr I2C address (CH224Q_I2C_ADDR_0 or CH224Q_I2C_ADDR_1)
 * @return CH224Q_OK on success, error code otherwise
 */
ch224q_status_code_t ch224q_init(ch224q_handle_t *handle, 
                                  const ch224q_platform_t *platform,
                                  void *i2c_handle, 
                                  uint8_t i2c_addr);

/**
 * @brief Deinitialize CH224Q driver
 * @param handle Pointer to CH224Q handle structure
 * @return CH224Q_OK on success
 */
ch224q_status_code_t ch224q_deinit(ch224q_handle_t *handle);

/**
 * @brief Set output voltage
 * @param handle Pointer to CH224Q handle structure
 * @param voltage Desired voltage level
 * @return CH224Q_OK on success, error code otherwise
 */
ch224q_status_code_t ch224q_set_voltage(ch224q_handle_t *handle, 
                                         ch224q_voltage_t voltage);

/**
 * @brief Get current voltage setting
 * @param handle Pointer to CH224Q handle structure
 * @param voltage Pointer to receive voltage value
 * @return CH224Q_OK on success
 */
ch224q_status_code_t ch224q_get_voltage(ch224q_handle_t *handle, 
                                         ch224q_voltage_t *voltage);

/**
 * @brief Read status register
 * @param handle Pointer to CH224Q handle structure
 * @param status Pointer to receive status information
 * @return CH224Q_OK on success, error code otherwise
 */
ch224q_status_code_t ch224q_get_status(ch224q_handle_t *handle, 
                                        ch224q_status_info_t *status);

/**
 * @brief Read current measurement
 * @param handle Pointer to CH224Q handle structure
 * @param current Pointer to receive current information
 * @return CH224Q_OK on success, error code otherwise
 * @note Only valid when PD protocol is active
 */
ch224q_status_code_t ch224q_get_current(ch224q_handle_t *handle, 
                                         ch224q_current_info_t *current);

/**
 * @brief Read PDO (Power Data Object) data
 * @param handle Pointer to CH224Q handle structure
 * @param pdo_data Buffer to receive PDO data (minimum 48 bytes)
 * @param len Length of buffer (should be CH224Q_PDO_DATA_SIZE)
 * @return CH224Q_OK on success, error code otherwise
 * @note PDO data is only valid when adapter is < 100W or in EPR mode (28V)
 */
ch224q_status_code_t ch224q_read_pdo(ch224q_handle_t *handle, 
                                      uint8_t *pdo_data, 
                                      uint16_t len);

/**
 * @brief Configure AVS (Adjustable Voltage Supply) mode
 * @param handle Pointer to CH224Q handle structure
 * @param voltage_mv Desired voltage in millivolts (5000-28000)
 * @return CH224Q_OK on success, error code otherwise
 * @note Must call ch224q_set_voltage(handle, CH224Q_AVS) after first config
 * @note Subsequent voltage changes only need this function
 */
ch224q_status_code_t ch224q_config_avs(ch224q_handle_t *handle, 
                                        uint16_t voltage_mv);

/**
 * @brief Configure PPS (Programmable Power Supply) mode
 * @param handle Pointer to CH224Q handle structure
 * @param voltage_mv Desired voltage in millivolts (depends on adapter)
 * @return CH224Q_OK on success, error code otherwise
 * @note Must call ch224q_set_voltage(handle, CH224Q_PPS) after first config
 * @note Subsequent voltage changes only need this function
 */
ch224q_status_code_t ch224q_config_pps(ch224q_handle_t *handle, 
                                        uint16_t voltage_mv);

/**
 * @brief Read raw register value
 * @param handle Pointer to CH224Q handle structure
 * @param reg_addr Register address
 * @param value Pointer to receive register value
 * @return CH224Q_OK on success, error code otherwise
 */
ch224q_status_code_t ch224q_read_register(ch224q_handle_t *handle,
                                           uint8_t reg_addr,
                                           uint8_t *value);

/**
 * @brief Write raw register value
 * @param handle Pointer to CH224Q handle structure
 * @param reg_addr Register address
 * @param value Value to write
 * @return CH224Q_OK on success, error code otherwise
 */
ch224q_status_code_t ch224q_write_register(ch224q_handle_t *handle,
                                            uint8_t reg_addr,
                                            uint8_t value);

/* Global platform functions pointer (user must set before using driver) */
extern const ch224q_platform_t *g_ch224q_platform;

#ifdef __cplusplus
}
#endif

#endif /* __CH224Q_H__ */
