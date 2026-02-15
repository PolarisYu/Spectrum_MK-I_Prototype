#include "ct7302.h"

/* Debug Tag for CT7302 */
#define USB_DBG_TAG "SRC"

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

/* ==============================================================================
 * 内部辅助函数
 * ============================================================================== */
static HAL_StatusTypeDef WriteReg(CT7302_HandleTypeDef *hct, uint8_t reg, uint8_t val) {
    return HAL_I2C_Mem_Write(hct->hi2c, hct->I2C_Addr, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
}

static uint8_t ReadReg(CT7302_HandleTypeDef *hct, uint8_t reg) {
    uint8_t val = 0;
    HAL_I2C_Mem_Read(hct->hi2c, hct->I2C_Addr, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
    return val;
}

static HAL_StatusTypeDef ModifyReg(CT7302_HandleTypeDef *hct, uint8_t reg, uint8_t mask, uint8_t val) {
    uint8_t current = ReadReg(hct, reg);
    current &= ~mask;
    current |= (val & mask);
    return WriteReg(hct, reg, current);
}

/* ==============================================================================
 * 初始化与电源
 * ============================================================================== */
HAL_StatusTypeDef CT7302_Init(CT7302_HandleTypeDef *hct) {
    if (!hct->hi2c) return HAL_ERROR;

    /* 1. 硬件复位 */
    if (hct->RST_Port) {
        HAL_GPIO_WritePin(hct->RST_Port, hct->RST_Pin, GPIO_PIN_RESET);
        HAL_Delay(20);
        HAL_GPIO_WritePin(hct->RST_Port, hct->RST_Pin, GPIO_PIN_SET);
        HAL_Delay(100); // 等待芯片启动 (Ref code: wait chip alive)
    }

    /* 2. 检查芯片是否在线 (Ref code writes 0x2B) */
    WriteReg(hct, CT7302_REG_CHIP_ID, 0x08);
    HAL_Delay(1);
    WriteReg(hct, CT7302_REG_CHIP_ID, 0x00);
    
    // 简单的 ID 读取验证 (可选)
    // uint8_t id = ReadReg(hct, 0x7F); // V1.10 参考代码习惯读取 0x7F 或者写入 0x2B 来判断芯片是否存在

    uint8_t id = ReadReg(hct, 0x91); // V1.20 明确 0x91 为 Device ID
    if (id != 0x10) {
        // ID 不匹配，可能芯片异常或型号不同
        USB_LOG_ERR("CT7302 ID Mismatch: 0x%02X\r\n", id);
    }

    /* 3. 基础电源配置 (Page 0) */
    WriteReg(hct, CT7302_REG_PAGE_SEL, 0x00);
    WriteReg(hct, CT7302_REG_PWR_ST, 0x00); // Power Up, PDN=0
    
    /* 4. 时钟配置: 强制使用外部晶振 (Reg 0x0E = 0x00) */
    /* 重要：如果您使用 SiTime 24.576M，必须设为 0x00 */
    WriteReg(hct, CT7302_REG_CLK_CFG, 0x00);

    /* 5. 默认状态配置 */
    CT7302_SetMute(hct, true);
    
    // 默认输入: SPDIF 0
    CT7302_SetInput(hct, CT7302_IN_SPDIF_0);
    
    // 默认输出: 192kHz 固定 (根据您的需求)
    CT7302_SetASRC_Target(hct, CT7302_OUT_FIXED_192K);
    
    // 主输出配置: I2S Master, 192k对应128Fs (MCLK=24.576M)
    CT7302_ConfigMainOutput(hct, true, CT7302_FMT_I2S, CT7302_MCLK_128FS);
    
    // 默认音量最大
    CT7302_SetVolume(hct, 0xFF);
    
    HAL_Delay(10);
    CT7302_SetMute(hct, false);

    return HAL_OK;
}

HAL_StatusTypeDef CT7302_SetPower(CT7302_HandleTypeDef *hct, bool enable) {
    // Reg 0x02 Bit 3: 1=PowerDown, 0=Normal
    return WriteReg(hct, CT7302_REG_PWR_ST, enable ? 0x00 : 0x08);
}

/* ==============================================================================
 * 输入与 ASRC
 * ============================================================================== */
HAL_StatusTypeDef CT7302_SetInput(CT7302_HandleTypeDef *hct, CT7302_Input_t input) {
    /* Reg 0x04: 输入源选择 (Ref Code) */
    HAL_StatusTypeDef status = WriteReg(hct, CT7302_REG_IN_SEL, (uint8_t)input);
    if (status == HAL_OK) hct->CurrentInput = input;
    return status;
}

HAL_StatusTypeDef CT7302_SetASRC_Target(CT7302_HandleTypeDef *hct, CT7302_ASRC_Target_t target) {
    /* Reg 0x05: ASRC 输出目标采样率 */
    hct->ASRC_Target = target;
    return WriteReg(hct, CT7302_REG_OUT_FS, (uint8_t)target);
}

/* ==============================================================================
 * 输出配置 (Main & Aux)
 * ============================================================================== */
HAL_StatusTypeDef CT7302_ConfigMainOutput(CT7302_HandleTypeDef *hct, bool master, CT7302_Format_t fmt, CT7302_MCLK_t mclk) {
    /* Reg 0x3C: 格式 (Bit 2-0) & 位宽 (Bit 5-3) */
    /* 默认设为 24bit (001 << 3 = 0x08) */
    uint8_t reg3c = 0x08 | (fmt & 0x07);
    WriteReg(hct, CT7302_REG_MAIN_FMT, reg3c);

    /* Reg 0x3D: MCLK与主从模式 */
    /* Bit 7: 0=Master, 1=Slave */
    /* Bit 6-4: MCLK Div */
    uint8_t reg3d = 0;
    if (!master) {
        reg3d |= 0x80; // Slave
    } else {
        reg3d |= (mclk & 0x07) << 4; // Master + MCLK Div
    }
    return WriteReg(hct, CT7302_REG_MAIN_CLK, reg3d);
}

HAL_StatusTypeDef CT7302_ConfigAuxOutput(CT7302_HandleTypeDef *hct, bool enable, CT7302_Format_t fmt) {
    if (!enable) {
        // Mute Aux Source
        return ModifyReg(hct, CT7302_REG_AUX_SRC, 0x1C, 0x14); // Set to Mute (101)
    }

    /* Reg 0x52: Aux 格式 */
    uint8_t reg52 = 0x08 | (fmt & 0x07); // 24bit default
    WriteReg(hct, CT7302_REG_AUX_FMT, reg52);

    /* Reg 0x53: Aux 源选择 */
    /* Bit 4-2: 000=De-jittered (ASRC) output */
    return ModifyReg(hct, CT7302_REG_AUX_SRC, 0x1C, 0x00);
}

/* ==============================================================================
 * 音量控制
 * ============================================================================== */
HAL_StatusTypeDef CT7302_SetMute(CT7302_HandleTypeDef *hct, bool mute) {
    /* Reg 0x06 Bit 7: 1=Mute, 0=Unmute (Ref Code logic) */
    /* Note: Some datasheets say Bit 0, but Ref Code uses mask 0x80. Using Ref Code. */
    hct->IsMuted = mute;
    return ModifyReg(hct, CT7302_REG_MUTE, 0x80, mute ? 0x80 : 0x00);
}

HAL_StatusTypeDef CT7302_SetVolume(CT7302_HandleTypeDef *hct, uint8_t vol) {
    /* Reg 0x61: Master Volume (假如支持) */
    /* 如果芯片不支持数字音量，此函数可能无效，但保留接口 */
    hct->CurrentVolume = vol;
    return WriteReg(hct, CT7302_REG_MAIN_VOL, vol);
}

/* ==============================================================================
 * 状态查询
 * ============================================================================== */
HAL_StatusTypeDef CT7302_GetStatus(CT7302_HandleTypeDef *hct, CT7302_Status_t *status) {
    if (!status) return HAL_ERROR;

    /* 1. 读取锁定状态 (Reg 0x10) */
    /* Ref code checks Bit 4 for Lock */
    uint8_t reg10 = ReadReg(hct, CT7302_REG_RX_STATUS);
    status->Locked = (reg10 & 0x10) ? true : false;

    if (!status->Locked) {
        status->InputFs = 0;
        status->Type = CT7302_SIG_UNLOCK;
        return HAL_OK;
    }

    /* 2. 读取输入采样率 (Reg 0x89) */
    uint8_t reg89 = ReadReg(hct, CT7302_REG_IN_FS) & 0x0F;
    switch(reg89) {
        case 0x02: status->InputFs = 44100; break;
        case 0x03: status->InputFs = 48000; break;
        case 0x05: status->InputFs = 88200; break;
        case 0x06: status->InputFs = 96000; break;
        case 0x08: status->InputFs = 176400; break;
        case 0x09: status->InputFs = 192000; break;
        case 0x0C: status->InputFs = 384000; break;
        case 0x0F: status->InputFs = 768000; break;
        default:   status->InputFs = 0; // Unknown
    }

    /* 3. 读取信号类型与位深 (Reg 0x77 & 0x7A) */
    uint8_t reg77 = ReadReg(hct, CT7302_REG_CH_STATUS);
    uint8_t reg7a = ReadReg(hct, CT7302_REG_DSD_STATUS);

    // 判断类型
    if (reg7a & 0x08) { // Reg 0x7A Bit 3: DSD Status
        status->Type = CT7302_SIG_DSD;
    } else if ((reg77 & 0x20) || (reg77 & 0x10)) { // DoP flags
        status->Type = CT7302_SIG_DOP;
    } else {
        status->Type = CT7302_SIG_PCM;
    }

    // 判断位深 (仅参考)
    if (reg77 & 0x80) { // I2S_RX_32BIT
        status->InputBitDepth = 32;
    } else {
        status->InputBitDepth = 24; // Default guess
    }

    return HAL_OK;
}

uint8_t CT7302_ReadInterrupts(CT7302_HandleTypeDef *hct) {
    /* Reg 0x09: 读取中断状态 (读取后通常会自动清除) */
    return ReadReg(hct, CT7302_REG_INT_STATUS);
}

/* ==============================================================================
 * DSD 高级配置
 * ============================================================================== */
HAL_StatusTypeDef CT7302_ConfigDSD(CT7302_HandleTypeDef *hct, bool enable_dop_detect) {
    /* Reg 0x2C: DoP 检测使能 (根据应用笔记) */
    /* Bit 2: DoP Detect Enable */
    return ModifyReg(hct, 0x2C, 0x04, enable_dop_detect ? 0x04 : 0x00);
}