/**
 ******************************************************************************
 * @file    ch224q_platform_stm32.c
 * @brief   CH224Q Platform Adaptation Layer for STM32
 * @version 1.0.0
 * @date    2024-02-16
 ******************************************************************************
 * @attention
 * 
 * This file shows how to adapt CH224Q driver to STM32 HAL
 * Modify the I2C handle and functions according to your project
 * 
 ******************************************************************************
 */

#include "ch224q.h"
#include "PDO.h"

/* Include your STM32 HAL header */
#include "stm32g4xx_hal.h"  // Change this to your STM32 series

/* ============================================================================
 * Platform-Specific Configuration
 * ========================================================================== */

/* External I2C handle - define this in your main.c or i2c.c */
extern I2C_HandleTypeDef hi2c1;  // Change to your I2C instance

/* CH224Q I2C timeout in milliseconds */
#define CH224Q_I2C_TIMEOUT      100

/* ============================================================================
 * Platform I2C Functions
 * ========================================================================== */

/**
 * @brief Platform I2C write function
 */
static int platform_i2c_write(void *i2c_handle, uint8_t dev_addr, 
                              uint8_t reg_addr, const uint8_t *data, uint16_t len)
{
    I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)i2c_handle;
    
    // STM32 HAL expects 8-bit address (7-bit << 1)
    uint16_t dev_addr_8bit = (uint16_t)(dev_addr << 1);
    
    HAL_StatusTypeDef status = HAL_I2C_Mem_Write(hi2c, 
                                                  dev_addr_8bit,
                                                  reg_addr,
                                                  I2C_MEMADD_SIZE_8BIT,
                                                  (uint8_t*)data,
                                                  len,
                                                  CH224Q_I2C_TIMEOUT);
    
    return (status == HAL_OK) ? 0 : -1;
}

/**
 * @brief Platform I2C read function
 */
static int platform_i2c_read(void *i2c_handle, uint8_t dev_addr,
                             uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)i2c_handle;
    
    // STM32 HAL expects 8-bit address (7-bit << 1)
    uint16_t dev_addr_8bit = (uint16_t)(dev_addr << 1);
    
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(hi2c,
                                                 dev_addr_8bit,
                                                 reg_addr,
                                                 I2C_MEMADD_SIZE_8BIT,
                                                 data,
                                                 len,
                                                 CH224Q_I2C_TIMEOUT);
    
    return (status == HAL_OK) ? 0 : -1;
}

/**
 * @brief Platform delay function
 */
static void platform_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

/* ============================================================================
 * Platform Function Structure
 * ========================================================================== */
static const ch224q_platform_t stm32_platform = {
    .i2c_write = platform_i2c_write,
    .i2c_read = platform_i2c_read,
    .delay_ms = platform_delay_ms
};

/* ============================================================================
 * Global Driver Handle
 * ========================================================================== */
static ch224q_handle_t g_ch224q_handle;

/* ============================================================================
 * Example Usage Functions
 * ========================================================================== */

/**
 * @brief Initialize CH224Q on STM32 platform
 * @return CH224Q_OK on success
 */
ch224q_status_code_t CH224Q_STM32_Init(void)
{
    // Initialize CH224Q driver with STM32 I2C
    ch224q_status_code_t ret = ch224q_init(&g_ch224q_handle,
                                            &stm32_platform,
                                            &hi2c1,  // Your I2C handle
                                            CH224Q_I2C_ADDR_0);
    
    if (ret != CH224Q_OK) {
        return ret;
    }

    // Initialize PDO management (for backward compatibility)
    g_pdo_context.ch224q_handle = &g_ch224q_handle;
    PDO_Init(&g_pdo_context, &g_ch224q_handle);

    return CH224Q_OK;
}

/**
 * @brief Example: Request 20V output
 */
void CH224Q_Example_Request20V(void)
{
    ch224q_set_voltage(&g_ch224q_handle, CH224Q_20V);
}

/**
 * @brief Example: Read and display PDO information
 */
void CH224Q_Example_ReadPDO(void)
{
    PDO_Read(&g_pdo_context);
    PDO_PrintAll(&g_pdo_context);
}

/**
 * @brief Example: Check protocol status
 */
void CH224Q_Example_CheckStatus(void)
{
    ch224q_status_info_t status;
    
    if (ch224q_get_status(&g_ch224q_handle, &status) == CH224Q_OK) {
        // Status successfully read
        if (status.pd_active) {
            // PD protocol active
            ch224q_current_info_t current;
            ch224q_get_current(&g_ch224q_handle, &current);
            // Current: current.max_current_ma
        }
    }
}

/**
 * @brief Example: Use AVS to set custom voltage
 */
void CH224Q_Example_SetAVS(uint16_t voltage_mv)
{
    // First time: Configure AVS and switch to AVS mode
    ch224q_config_avs(&g_ch224q_handle, voltage_mv);
    ch224q_set_voltage(&g_ch224q_handle, CH224Q_AVS);
    
    // Subsequent changes: Only need to reconfigure AVS
    // ch224q_config_avs(&g_ch224q_handle, new_voltage_mv);
}

/**
 * @brief Example: Use PPS to set custom voltage
 */
void CH224Q_Example_SetPPS(uint16_t voltage_mv)
{
    // First time: Configure PPS and switch to PPS mode
    ch224q_config_pps(&g_ch224q_handle, voltage_mv);
    ch224q_set_voltage(&g_ch224q_handle, CH224Q_PPS);
    
    // Subsequent changes: Only need to reconfigure PPS
    // ch224q_config_pps(&g_ch224q_handle, new_voltage_mv);
}

/**
 * @brief Get CH224Q driver handle (for advanced use)
 */
ch224q_handle_t* CH224Q_GetHandle(void)
{
    return &g_ch224q_handle;
}
