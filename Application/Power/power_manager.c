#include "power_manager.h"
#include "pdo.h"
#include "ch224q.h"
#include "i2c.h"

/* Debug Tag for PDO */
#define USB_DBG_TAG "PDO"

/* Include usb_config.h if available to get debug levels */
#if __has_include("usb_config.h")
#include "usb_config.h"
#endif

/* Ensure CONFIG_USB_PRINTF is defined if not provided by usb_config.h */
#ifndef CONFIG_USB_PRINTF
#include <stdio.h>
#define CONFIG_USB_PRINTF printf
#endif

#include "usb_log.h"

#ifndef CONFIG_USB_DBG_LEVEL
#define CONFIG_USB_DBG_LEVEL USB_DBG_INFO
#endif

/* ============================================================================
 * Configuration
 * ========================================================================== */

/* 如果 CH224Q 连接在其他 I2C 总线上，请修改此处 */
/* 注意：请确保在 main.c 中已经初始化了该 I2C 句柄 */
// extern I2C_HandleTypeDef hi2c1; // 假设使用 I2C1
#define POWER_I2C_HANDLE &hi2c3  // 映射到 hi2c1

#define POWER_I2C_TIMEOUT 100

/* ============================================================================
 * Global Variables
 * ========================================================================== */

ch224q_handle_t g_ch224q_handle;
// PDO_Context_t g_pdo_context; // Defined in pdo.c

/* ============================================================================
 * Platform Adaptation Functions (BSP)
 * ========================================================================== */

static int platform_i2c_write(void *handle, uint8_t dev_addr, uint8_t reg_addr, const uint8_t *data, uint16_t len)
{
    I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)handle;
    
    HAL_StatusTypeDef status = HAL_I2C_Mem_Write(   hi2c, 
                                                    dev_addr,
                                                    reg_addr,
                                                    I2C_MEMADD_SIZE_8BIT,
                                                    (uint8_t*)data,
                                                    len,
                                                    POWER_I2C_TIMEOUT
                                                );
    
    return (status == HAL_OK) ? 0 : -1;
}

static int platform_i2c_read(void *handle, uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)handle;
    
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(    hi2c,
                                                    dev_addr,
                                                    reg_addr,
                                                    I2C_MEMADD_SIZE_8BIT,
                                                    data,
                                                    len,
                                                    POWER_I2C_TIMEOUT
                                                );
    
    return (status == HAL_OK) ? 0 : -1;
}

static void platform_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

/* ============================================================================
 * Public API Implementation
 * ========================================================================== */

void Power_Init(void)
{
    USB_LOG_INFO("Initializing Power Management (CH224Q)...\r\n");

    /* 1. 配置底层驱动接口 */
    ch224q_platform_t platform = {
        .i2c_read = platform_i2c_read,
        .i2c_write = platform_i2c_write,
        .delay_ms = platform_delay_ms
    };

    /* 2. 初始化底层 CH224Q 驱动 */
    // 注意：如果在 main.c 中没有初始化 I2C1，这里会挂死或失败
    // 请检查 main.c 中的 MX_I2C1_Init() 是否存在
    ch224q_status_code_t ret = ch224q_init(&g_ch224q_handle, 
                                           &platform, 
                                           POWER_I2C_HANDLE, // 传入 HAL I2C 句柄
                                           CH224Q_I2C_ADDR_DEFAULT);
    
    if (ret != CH224Q_OK) {
        USB_LOG_ERR("CH224Q Init Failed: %d. Check I2C connection!\r\n", ret);
        return;
    }

    /* 3. 初始化上层 PDO 管理 */
    PDO_Init(&g_pdo_context, &g_ch224q_handle);

    USB_LOG_INFO("CH224Q Initialized. Waiting for PD negotiation...\r\n");
}

void Power_Task(void)
{
    static uint32_t last_tick = 0;
    static bool first_negotiation = true;

    // 每 1000ms 检查一次状态
    if (HAL_GetTick() - last_tick > 1000) {
        last_tick = HAL_GetTick();

        // 获取最新状态
        PDO_GetStatus(&g_pdo_context);

        if (g_pdo_context.pdo_info.pd_active) {
            if (first_negotiation) {
                USB_LOG_INFO("PD Negotiation Successful!\r\n");
                
                // 首次握手成功，读取并打印供电能力
                if (PDO_Read(&g_pdo_context) == CH224Q_OK) {
                    Power_PrintStatus();
                }
                
                first_negotiation = false;
            }
        } else {
            // 如果 PD 掉线，重置标志
            if (!first_negotiation) {
                USB_LOG_WRN("PD Disconnected or Protocol Changed.\r\n");
                first_negotiation = true;
            }
        }
    }
}

void Power_SetVoltage(uint16_t voltage_mv)
{
    USB_LOG_INFO("Requesting Voltage: %d mV\r\n", voltage_mv);

    // 1. 尝试匹配固定电压
    if (PDO_IsVoltageSupported(&g_pdo_context, voltage_mv)) {
        // 查找是否为固定电压档位
        const PDO_Entry_t *entry = PDO_FindByVoltage(&g_pdo_context, voltage_mv);
        
        if (entry->type == PDO_TYPE_FIXED_SUPPLY) {
            // 映射 mV 到 CH224Q 枚举
            ch224q_voltage_t target_vol = CH224Q_5V;
            switch(voltage_mv) {
                case 5000: target_vol = CH224Q_5V; break;
                case 9000: target_vol = CH224Q_9V; break;
                case 12000: target_vol = CH224Q_12V; break;
                case 15000: target_vol = CH224Q_15V; break;
                case 20000: target_vol = CH224Q_20V; break;
                case 28000: target_vol = CH224Q_28V; break;
                default: break; 
            }
            PDO_SetVoltage(&g_pdo_context, target_vol);
            USB_LOG_INFO("Set Fixed Voltage: %d mV\r\n", voltage_mv);
            return;
        }
    }

    // 2. 如果支持 PPS，尝试使用 PPS
    if (g_pdo_context.pdo_info.pps_capable) {
        // 配置 PPS 电压
        ch224q_config_pps(&g_ch224q_handle, voltage_mv);
        // 切换到 PPS 模式
        PDO_SetVoltage(&g_pdo_context, CH224Q_PPS);
        USB_LOG_INFO("Set PPS Voltage: %d mV\r\n", voltage_mv);
    } 
    // 3. 如果支持 AVS，尝试使用 AVS
    else if (g_pdo_context.pdo_info.avs_capable) {
        ch224q_config_avs(&g_ch224q_handle, voltage_mv);
        PDO_SetVoltage(&g_pdo_context, CH224Q_AVS);
        USB_LOG_INFO("Set AVS Voltage: %d mV\r\n", voltage_mv);
    }
    else {
        USB_LOG_WRN("Voltage %d mV not supported by charger!\r\n", voltage_mv);
    }
}

void Power_PrintStatus(void)
{
    PDO_PrintAll(&g_pdo_context);
    
    USB_LOG_INFO("Current Status:\r\n");
    USB_LOG_INFO("  Protocol: %s\r\n", PDO_GetProtocolName(g_pdo_context.pdo_info.protocol));
    USB_LOG_INFO("  Voltage:  %d mV\r\n", g_pdo_context.pdo_info.current_voltage_mv);
    USB_LOG_INFO("  Power:    %d W\r\n", g_pdo_context.pdo_info.current_power_w);
}
