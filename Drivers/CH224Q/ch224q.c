/**
 ******************************************************************************
 * @file    ch224q.c
 * @brief   CH224Q/CH224A USB PD Sink Controller Driver Implementation
 * @version 2.0.0
 * @date    2024-02-16
 ******************************************************************************
 */

#include "ch224q.h"
#include <string.h>

/* ============================================================================
 * Platform Function Pointer (Must be initialized by user)
 * ========================================================================== */
const ch224q_platform_t *g_ch224q_platform = NULL;

/* ============================================================================
 * Private Helper Functions
 * ========================================================================== */

/**
 * @brief Validate handle
 */
static inline bool is_valid_handle(const ch224q_handle_t *handle)
{
    return (handle != NULL && handle->initialized && g_ch224q_platform != NULL);
}

/**
 * @brief Write single register
 */
static ch224q_status_code_t write_register(ch224q_handle_t *handle, 
                                            uint8_t reg_addr, 
                                            uint8_t value)
{
    if (!is_valid_handle(handle)) {
        return CH224Q_ERROR_INVALID_PARAM;
    }

    int ret = g_ch224q_platform->i2c_write(handle->i2c_handle, 
                                             handle->i2c_address,
                                             reg_addr, 
                                             &value, 
                                             1);
    
    return (ret == 0) ? CH224Q_OK : CH224Q_ERROR_I2C;
}

/**
 * @brief Read single register
 */
static ch224q_status_code_t read_register(ch224q_handle_t *handle,
                                           uint8_t reg_addr,
                                           uint8_t *value)
{
    if (!is_valid_handle(handle) || value == NULL) {
        return CH224Q_ERROR_INVALID_PARAM;
    }

    int ret = g_ch224q_platform->i2c_read(handle->i2c_handle,
                                            handle->i2c_address,
                                            reg_addr,
                                            value,
                                            1);
    
    return (ret == 0) ? CH224Q_OK : CH224Q_ERROR_I2C;
}

/**
 * @brief Read multiple registers
 */
static ch224q_status_code_t read_registers(ch224q_handle_t *handle,
                                            uint8_t reg_addr,
                                            uint8_t *data,
                                            uint16_t len)
{
    if (!is_valid_handle(handle) || data == NULL || len == 0) {
        return CH224Q_ERROR_INVALID_PARAM;
    }

    int ret = g_ch224q_platform->i2c_read(handle->i2c_handle,
                                            handle->i2c_address,
                                            reg_addr,
                                            data,
                                            len);
    
    return (ret == 0) ? CH224Q_OK : CH224Q_ERROR_I2C;
}

/* ============================================================================
 * Public API Implementation
 * ========================================================================== */

/**
 * @brief Initialize CH224Q driver
 */
ch224q_status_code_t ch224q_init(ch224q_handle_t *handle,
                                  const ch224q_platform_t *platform,
                                  void *i2c_handle,
                                  uint8_t i2c_addr)
{
    // Validate parameters
    if (handle == NULL || platform == NULL || i2c_handle == NULL) {
        return CH224Q_ERROR_INVALID_PARAM;
    }

    if (platform->i2c_read == NULL || 
        platform->i2c_write == NULL || 
        platform->delay_ms == NULL) {
        return CH224Q_ERROR_INVALID_PARAM;
    }

    if (i2c_addr != CH224Q_I2C_ADDR_0 && i2c_addr != CH224Q_I2C_ADDR_1) {
        return CH224Q_ERROR_INVALID_PARAM;
    }

    // Initialize handle structure
    memset(handle, 0, sizeof(ch224q_handle_t));
    handle->i2c_handle = i2c_handle;
    handle->i2c_address = i2c_addr;
    handle->voltage = CH224Q_5V;
    handle->protocol = CH224Q_PROTOCOL_NONE;

    // Set global platform pointer
    g_ch224q_platform = platform;

    // Wait for chip to be ready
    platform->delay_ms(10);

    // Try to read status register to verify communication
    uint8_t status;
    ch224q_status_code_t ret = read_register(handle, CH224Q_REG_STATUS, &status);
    if (ret != CH224Q_OK) {
        return CH224Q_ERROR_I2C;
    }

    handle->initialized = true;

    return CH224Q_OK;
}

/**
 * @brief Deinitialize CH224Q driver
 */
ch224q_status_code_t ch224q_deinit(ch224q_handle_t *handle)
{
    if (handle == NULL) {
        return CH224Q_ERROR_INVALID_PARAM;
    }

    memset(handle, 0, sizeof(ch224q_handle_t));
    return CH224Q_OK;
}

/**
 * @brief Set output voltage
 */
ch224q_status_code_t ch224q_set_voltage(ch224q_handle_t *handle,
                                         ch224q_voltage_t voltage)
{
    if (!is_valid_handle(handle)) {
        return CH224Q_ERROR_INVALID_PARAM;
    }

    // Validate voltage value
    if (voltage > CH224Q_AVS) {
        return CH224Q_ERROR_INVALID_PARAM;
    }

    // Write to voltage control register
    ch224q_status_code_t ret = write_register(handle, 
                                               CH224Q_REG_VOLTAGE, 
                                               (uint8_t)voltage);
    
    if (ret == CH224Q_OK) {
        handle->voltage = voltage;
        
        // Wait for voltage transition
        if (g_ch224q_platform && g_ch224q_platform->delay_ms) {
            g_ch224q_platform->delay_ms(50);
        }
    }

    return ret;
}

/**
 * @brief Get current voltage setting
 */
ch224q_status_code_t ch224q_get_voltage(ch224q_handle_t *handle,
                                         ch224q_voltage_t *voltage)
{
    if (!is_valid_handle(handle) || voltage == NULL) {
        return CH224Q_ERROR_INVALID_PARAM;
    }

    *voltage = handle->voltage;
    return CH224Q_OK;
}

/**
 * @brief Read status register
 */
ch224q_status_code_t ch224q_get_status(ch224q_handle_t *handle,
                                        ch224q_status_info_t *status)
{
    if (!is_valid_handle(handle) || status == NULL) {
        return CH224Q_ERROR_INVALID_PARAM;
    }

    // Read status register
    uint8_t raw_status;
    ch224q_status_code_t ret = read_register(handle, 
                                              CH224Q_REG_STATUS, 
                                              &raw_status);
    
    if (ret != CH224Q_OK) {
        return ret;
    }

    // Parse status bits
    memset(status, 0, sizeof(ch224q_status_info_t));
    status->raw_status = raw_status;
    status->bc_active = (raw_status & CH224Q_STATUS_BC_ACTIVE) ? true : false;
    status->qc2_active = (raw_status & CH224Q_STATUS_QC2_ACTIVE) ? true : false;
    status->qc3_active = (raw_status & CH224Q_STATUS_QC3_ACTIVE) ? true : false;
    status->pd_active = (raw_status & CH224Q_STATUS_PD_ACTIVE) ? true : false;
    status->epr_active = (raw_status & CH224Q_STATUS_EPR_ACTIVE) ? true : false;

    // Determine protocol priority: EPR > PD > QC3 > QC2 > BC1.2
    if (status->epr_active) {
        status->protocol = CH224Q_PROTOCOL_EPR;
    } else if (status->pd_active) {
        status->protocol = CH224Q_PROTOCOL_PD;
    } else if (status->qc3_active) {
        status->protocol = CH224Q_PROTOCOL_QC3;
    } else if (status->qc2_active) {
        status->protocol = CH224Q_PROTOCOL_QC2;
    } else if (status->bc_active) {
        status->protocol = CH224Q_PROTOCOL_BC12;
    } else {
        status->protocol = CH224Q_PROTOCOL_NONE;
    }

    // Update handle protocol
    handle->protocol = status->protocol;

    return CH224Q_OK;
}

/**
 * @brief Read current measurement
 */
ch224q_status_code_t ch224q_get_current(ch224q_handle_t *handle,
                                         ch224q_current_info_t *current)
{
    if (!is_valid_handle(handle) || current == NULL) {
        return CH224Q_ERROR_INVALID_PARAM;
    }

    // Current register only valid in PD/EPR mode
    if (handle->protocol != CH224Q_PROTOCOL_PD && 
        handle->protocol != CH224Q_PROTOCOL_EPR) {
        return CH224Q_ERROR_NOT_READY;
    }

    // Read current register (unit: 50mA)
    uint8_t raw_current;
    ch224q_status_code_t ret = read_register(handle,
                                              CH224Q_REG_CURRENT,
                                              &raw_current);
    
    if (ret != CH224Q_OK) {
        return ret;
    }

    // Convert to mA
    memset(current, 0, sizeof(ch224q_current_info_t));
    current->raw_value = raw_current;
    current->max_current_ma = (uint16_t)raw_current * 50;
    current->current_ma = current->max_current_ma; // Alias for compatibility

    return CH224Q_OK;
}

/**
 * @brief Read PDO data
 */
ch224q_status_code_t ch224q_read_pdo(ch224q_handle_t *handle,
                                      uint8_t *pdo_data,
                                      uint16_t len)
{
    if (!is_valid_handle(handle) || pdo_data == NULL) {
        return CH224Q_ERROR_INVALID_PARAM;
    }

    if (len < CH224Q_PDO_DATA_SIZE) {
        return CH224Q_ERROR_INVALID_PARAM;
    }

    // PDO data only valid in specific conditions (see datasheet 5.2.3)
    // - Adapter < 100W: SRCCAP data
    // - EPR mode (28V): EPR_SRCCAP data
    
    // Read 48 bytes from 0x60-0x8F
    ch224q_status_code_t ret = read_registers(handle,
                                               CH224Q_REG_PDO_BASE,
                                               pdo_data,
                                               CH224Q_PDO_DATA_SIZE);
    
    return ret;
}

/**
 * @brief Configure AVS mode
 */
ch224q_status_code_t ch224q_config_avs(ch224q_handle_t *handle,
                                        uint16_t voltage_mv)
{
    if (!is_valid_handle(handle)) {
        return CH224Q_ERROR_INVALID_PARAM;
    }

    // Validate voltage range (5V - 28V)
    if (voltage_mv < CH224Q_AVS_MIN_VOLTAGE || 
        voltage_mv > CH224Q_AVS_MAX_VOLTAGE) {
        return CH224Q_ERROR_OUT_OF_RANGE;
    }

    // Convert mV to 100mV units
    uint16_t voltage_units = voltage_mv / CH224Q_AVS_VOLTAGE_UNIT;

    // Prepare register values
    uint8_t low_byte = (uint8_t)(voltage_units & 0xFF);
    uint8_t high_byte = (uint8_t)((voltage_units >> 8) & 0x7F);
    high_byte |= 0x80;  // Set enable bit (bit 15)

    // Write low byte first (per datasheet requirement)
    ch224q_status_code_t ret = write_register(handle, 
                                               CH224Q_REG_AVS_LOW, 
                                               low_byte);
    if (ret != CH224Q_OK) {
        return ret;
    }

    // Write high byte with enable bit
    ret = write_register(handle, CH224Q_REG_AVS_HIGH, high_byte);
    if (ret != CH224Q_OK) {
        return ret;
    }

    // On first configuration, must call ch224q_set_voltage(CH224Q_AVS)
    // Subsequent changes only need this function

    return CH224Q_OK;
}

/**
 * @brief Configure PPS mode
 */
ch224q_status_code_t ch224q_config_pps(ch224q_handle_t *handle,
                                        uint16_t voltage_mv)
{
    if (!is_valid_handle(handle)) {
        return CH224Q_ERROR_INVALID_PARAM;
    }

    // Validate voltage range (depends on adapter capability)
    if (voltage_mv < CH224Q_PPS_MIN_VOLTAGE || 
        voltage_mv > CH224Q_PPS_MAX_VOLTAGE) {
        return CH224Q_ERROR_OUT_OF_RANGE;
    }

    // Convert mV to 100mV units
    uint8_t voltage_units = (uint8_t)(voltage_mv / CH224Q_PPS_VOLTAGE_UNIT);

    // Write PPS voltage register
    ch224q_status_code_t ret = write_register(handle,
                                               CH224Q_REG_PPS,
                                               voltage_units);
    
    // On first configuration, must call ch224q_set_voltage(CH224Q_PPS)
    // Subsequent changes only need this function

    return ret;
}

/**
 * @brief Read raw register value
 */
ch224q_status_code_t ch224q_read_register(ch224q_handle_t *handle,
                                           uint8_t reg_addr,
                                           uint8_t *value)
{
    return read_register(handle, reg_addr, value);
}

/**
 * @brief Write raw register value
 */
ch224q_status_code_t ch224q_write_register(ch224q_handle_t *handle,
                                            uint8_t reg_addr,
                                            uint8_t value)
{
    return write_register(handle, reg_addr, value);
}
