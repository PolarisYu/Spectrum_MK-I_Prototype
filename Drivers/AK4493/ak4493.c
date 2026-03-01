

#include "ak4493.h"

/* Debug Tag for AK4493 */
#define USB_DBG_TAG "DAC"

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

/**
 * @brief Helper to write a register
 * 
 * @param hak AK4493 handle
 * @param reg Register address
 * @param val Value to write
 * @return HAL_StatusTypeDef HAL_OK if successful, HAL_ERROR otherwise
 */
HAL_StatusTypeDef AK4493_WriteReg(AK4493_HandleTypeDef *hak, uint8_t reg, uint8_t val) {
    HAL_StatusTypeDef status = HAL_I2C_Mem_Write(hak->hi2c, hak->DevAddress, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
    if (status == HAL_OK && reg < AK4493_NUM_REGS) {
        hak->Regs[reg] = val; // 更新本地缓存
    }
    return status;
}

/**
 * @brief Helper to write and verify a register
 * 
 * @param hak AK4493 handle
 * @param reg Register address
 * @param val Value to write
 * @return HAL_StatusTypeDef HAL_OK if successful, HAL_ERROR otherwise
 */
HAL_StatusTypeDef AK4493_WriteRegVerify(AK4493_HandleTypeDef *hak, uint8_t reg, uint8_t val) {
    HAL_StatusTypeDef status = AK4493_WriteReg(hak, reg, val);
    if (status != HAL_OK) {
        USB_LOG_ERR("Failed to write register 0x%02X\r\n", reg);
        return status;
    }
    
    /* Read back and verify */
    uint8_t readback = AK4493_ReadReg(hak, reg);
    if (readback != val) {
        USB_LOG_ERR("Register 0x%02X verify failed: wrote 0x%02X, read 0x%02X\r\n", 
               reg, val, readback);
        return HAL_ERROR;
    }
    
    return HAL_OK;
}

/**
 * @brief Helper to read a register
 * 
 * @param hak AK4493 handle
 * @param reg Register address
 * @return uint8_t Register value
 */
uint8_t AK4493_ReadReg(AK4493_HandleTypeDef *hak, uint8_t reg) {
    uint8_t val = 0;
    HAL_I2C_Mem_Read(hak->hi2c, hak->DevAddress, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
    return val;
}

/**
 * @brief Readback and print all register values
 * 
 * @param hak AK4493 handle
 */
HAL_StatusTypeDef AK4493_DumpRegisters(AK4493_HandleTypeDef *hak) {
    if (hak == NULL || hak->hi2c == NULL || !hak->IsInitialized) return HAL_ERROR;

    // Register name table for enhanced readability
    const char *reg_names[AK4493_NUM_REGS] = {
        [0x00] = "Control 1     ",
        [0x01] = "Control 2     ",
        [0x02] = "Control 3     ",
        [0x03] = "Lch Attenuation",
        [0x04] = "Rch Attenuation",
        [0x05] = "Control 4     ",
        [0x06] = "DSD 1         ",
        [0x07] = "Control 5     ",
        [0x08] = "Sound Control ",
        [0x09] = "DSD 2         ",
        [0x0A] = "Control 6     ",
        [0x0B] = "Control 7     ",
        // 0x0C - 0x14 are reserved
        [0x15] = "Control 8     "
    };

    USB_LOG_INFO("\r\n======= AK4493 Register Dump =======\r\n");
    USB_LOG_INFO("ADDR | HEX  | BINARY   | NAME\r\n");
    USB_LOG_INFO("------------------------------------\r\n");

    for (uint8_t i = 0; i < AK4493_NUM_REGS; i++) {
        // Skip undefined reserved area (0x0C to 0x14) for efficiency
        if (i > 0x0B && i < 0x15) continue;

        // Read from hardware real-time
        uint8_t val = AK4493_ReadReg(hak, i);
        
        // Update shadow register cache
        hak->Regs[i] = val;

        // Print binary for bit-level view
        char bin_str[9];
        for (int j = 0; j < 8; j++) {
            bin_str[7 - j] = (val & (1 << j)) ? '1' : '0';
        }
        bin_str[8] = '\0';

        const char *name = (reg_names[i] != NULL) ? reg_names[i] : "Unknown       ";
        
        // Print register information
        USB_LOG_INFO("0x%02X | 0x%02X | %s | %s\r\n", i, val, bin_str, name);
    }
    USB_LOG_INFO("====================================\r\n");
    return HAL_OK;
}

/**
 * @brief Update specific bit(s) in a register
 * 
 * @param hak AK4493 handle
 * @param reg Register address
 * @param mask Bit mask to apply
 * @param value Value to set (1 for set, 0 for clear)
 * @return HAL_StatusTypeDef HAL_OK if successful, HAL_ERROR otherwise
 */
static HAL_StatusTypeDef AK4493_UpdateBit(AK4493_HandleTypeDef *hak, uint8_t reg, uint8_t mask, uint8_t value) {
    uint8_t current = AK4493_ReadReg(hak, reg);
    if (value) {
        current |= mask;
    } else {
        current &= ~mask;
    }
    return AK4493_WriteReg(hak, reg, current);
}

/**
 * @brief Power On Initialization (Hardware Level)
 * 
 * @param hak AK4493 handle
 * @return HAL_StatusTypeDef HAL_OK if successful, HAL_ERROR otherwise
 */
HAL_StatusTypeDef AK4493_PowerOn(AK4493_HandleTypeDef *hak) {
    /* Step 1: External power enable (if applicable - board specific) */
    if (hak->PW_EN_Port != NULL) {
        HAL_GPIO_WritePin(hak->PW_EN_Port, hak->PW_EN_Pin, GPIO_PIN_SET);
        USB_LOG_INFO("External DAC power enabled\r\n");
    }
    
    /* Step 2: LDO enable (if not hard-wired to VDD) */
    if (hak->LDOE_Port != NULL) {
        HAL_GPIO_WritePin(hak->LDOE_Port, hak->LDOE_Pin, GPIO_PIN_SET);
        USB_LOG_INFO("AK4493 LDO enabled\r\n");
    }
    
    /* Step 3: Wait for power stabilization (datasheet: ensure PDN is LOW during power-up) */
    /* PDN should already be low or we set it low here */
    if (hak->PDN_Port != NULL) {
        HAL_GPIO_WritePin(hak->PDN_Port, hak->PDN_Pin, GPIO_PIN_RESET);
    }
    
    HAL_Delay(500);  // Wait for power supplies to stabilize
    USB_LOG_INFO("Power supply stabilized\r\n");
    
    /* Step 4: Release PDN (datasheet: PDN must be LOW for ≥150ns, we use 20ms to be safe) */
    if (hak->PDN_Port != NULL) {
        HAL_GPIO_WritePin(hak->PDN_Port, hak->PDN_Pin, GPIO_PIN_SET);
        HAL_Delay(20);  // Wait for internal circuits to initialize
        USB_LOG_INFO("AK4493 PDN released - chip active\r\n");
    }
    
    USB_LOG_WRN("Ensure MCLK, LRCK, BICK are stable before calling AK4493_RegInit()\r\n");
    
    return HAL_OK;
}

/**
 * @brief Register Initialization (Must be called AFTER clocks are stable)
 * 
 * @param hak AK4493 handle
 * @return HAL_StatusTypeDef HAL_OK if successful, HAL_ERROR otherwise
 */
HAL_StatusTypeDef AK4493_RegInit(AK4493_HandleTypeDef *hak) {
    if (hak == NULL || hak->hi2c == NULL) {
        return HAL_ERROR;
    }
    
    USB_LOG_INFO("Starting AK4493 register initialization...\r\n");
    
    /* Verify I2C communication */
    if (HAL_I2C_IsDeviceReady(hak->hi2c, hak->DevAddress, 3, 100) != HAL_OK) {
        USB_LOG_ERR("AK4493 I2C communication failed!\r\n");
        USB_LOG_ERR("   Check: 1) I2C address (current: 0x%02X)\r\n", hak->DevAddress);
        USB_LOG_ERR("          2) CAD0/CAD1 pin configuration\r\n");
        USB_LOG_ERR("          3) I2C bus wiring (SDA/SCL)\r\n");
        USB_LOG_ERR("          4) Pull-up resistors on I2C lines\r\n");
        return HAL_ERROR;
    }
    USB_LOG_INFO("AK4493 detected at I2C address 0x%02X\r\n", hak->DevAddress);
    
    /* Software reset */
    AK4493_UpdateBit(hak, AK4493_REG_00_CONTROL1, AK4493_RSTN, 0);
    HAL_Delay(1);
    
    /* ===== Control 1 (0x00): Audio format and clock ===== */
    /* 0x8F = 0b10001111
     * Bit 7 (ACKS) = 1 → Auto clock mode
     * Bit 6 (ECS/EXDF) = 0 → Internal filter
     * Bit 5-4 (TDM) = 0 → Normal mode
     * Bit 3-1 (DIF) = 111 → 32-bit I2S format
     * Bit 0 (RSTN) = 0 → Reset state
     */
    AK4493_WriteReg(hak, AK4493_REG_00_CONTROL1, 0x8E);
    USB_LOG_INFO("Control 1: Auto clock, 32-bit I2S, normal operation\r\n");
    
    /* ===== Control 2 (0x01): Soft mute and filters ===== */
    /* 0xA3 = 0b10100011
     * Bit 7 (DZFE) = 1 → Zero detect enabled
     * Bit 6 (DZFM) = 0 → Zero detect mode 0
     * Bit 5 (SD) = 1 → Short delay filter
     * Bit 4-3 (DFS) = 0 → DSD filter mode 0
     * Bit 2-1 (DEM) = 0 → De-emphasis off
     * Bit 0 (SMUTE) = 1 → Soft mute ENABLED (for safety)
     */
    AK4493_WriteReg(hak, AK4493_REG_01_CONTROL2, 0xA3);
    USB_LOG_INFO("Control 2: Zero detect ON, soft mute ENABLED\r\n");
    
    /* ===== Control 3 (0x02): DSD/PCM mode and channel config ===== */
    /* 0x00 = PCM mode, stereo, normal */
    AK4493_WriteReg(hak, AK4493_REG_02_CONTROL3, 0x00);
    USB_LOG_INFO("Control 3: PCM mode, stereo operation\r\n");
    
    /* ===== Volume (0x03, 0x04): Set to safe default ===== */
    /* Start muted for safety (0xFF = mute) */
    /* Or use 0x50 = -40dB for safe audible level */
    AK4493_WriteReg(hak, AK4493_REG_03_LCH_ATT, AK4493_VOL_MUTE);
    AK4493_WriteReg(hak, AK4493_REG_04_RCH_ATT, AK4493_VOL_MUTE);
    USB_LOG_INFO("Volume: Both channels MUTED (0xFF) for safety\r\n");
    USB_LOG_INFO("Call AK4493_SetMute(0) when ready for audio output\r\n");
    
    /* ===== Control 4 (0x05): Extended filter control ===== */
    /* 0x00 = No inversion, normal filter */
    AK4493_WriteReg(hak, AK4493_REG_05_CONTROL4, 0x00);
    
    /* ===== Control 5 (0x07): Gain control ===== */
    /* 0x02 = GC[2:0] = 001 → ±3.75Vpp output (higher gain mode) */
    /* 0x00 = GC[2:0] = 000 → ±2.8Vpp output (lower gain mode) */
    AK4493_WriteReg(hak, AK4493_REG_07_CONTROL5, 0x08);
    USB_LOG_INFO("Control 5: Gain set to ±3.75Vpp (high gain mode)\r\n");
    
    /* ===== Sound Control (0x08): Sound quality adjustment ===== */
    /* 0x00 = Default sound */
    AK4493_WriteReg(hak, AK4493_REG_08_SOUND_CONTROL, 0x00);

    /* ===== Reset (0x00): Release reset state ===== */
    AK4493_UpdateBit(hak, AK4493_REG_00_CONTROL1, AK4493_RSTN, 1);
    
    USB_LOG_INFO("AK4493 initialization complete!\r\n");
    USB_LOG_INFO("   Format: 32-bit I2S\r\n");
    USB_LOG_INFO("   Filter: Short delay sharp roll-off\r\n");
    USB_LOG_INFO("   Gain: ±3.75Vpp\r\n");
    USB_LOG_INFO("   Volume: MUTED (call SetMute(0) to enable)\r\n");
    
    return HAL_OK;
}

/**
 * @brief Complete initialization (Power + Registers)
 * 
 * @param hak AK4493 handle
 * @return HAL_StatusTypeDef HAL_OK if successful, HAL_ERROR otherwise
 */
HAL_StatusTypeDef AK4493_Init(AK4493_HandleTypeDef *hak) {
    if (hak == NULL || hak->hi2c == NULL) {
        return HAL_ERROR;
    }
    
    /* Hardware power-up */
    HAL_StatusTypeDef status = AK4493_PowerOn(hak);
    if (status != HAL_OK) return status;
    
    /* User must ensure clocks are running before calling RegInit */
    USB_LOG_INFO("\r\nWaiting for I2S clocks to stabilize...\r\n");
    USB_LOG_INFO("   Ensure MCLK, LRCK, BICK are running!\r\n");
    HAL_Delay(100);  // Give time for clocks to start
    
    /* Register initialization */
    return AK4493_RegInit(hak);
}

/**
 * @brief Hardware Reset
 * 
 * @param hak AK4493 handle
 * @return HAL_StatusTypeDef HAL_OK if successful, HAL_ERROR otherwise
 */
HAL_StatusTypeDef AK4493_Reset(AK4493_HandleTypeDef *hak) {
    if (hak->PDN_Port != NULL) {
        HAL_GPIO_WritePin(hak->PDN_Port, hak->PDN_Pin, GPIO_PIN_RESET);
        HAL_Delay(1);  // Minimum 150ns, using 1ms to be safe
        HAL_GPIO_WritePin(hak->PDN_Port, hak->PDN_Pin, GPIO_PIN_SET);
        HAL_Delay(20);
        USB_LOG_INFO("AK4493 hardware reset complete\r\n");
    }
    return HAL_OK;
}

/**
 * @brief Set Volume (both channels)
 * 
 * @param hak AK4493 handle
 * @param volume Volume level (0x00 = 0dB, 0xFF = Mute)
 * @return HAL_StatusTypeDef HAL_OK if successful, HAL_ERROR otherwise
 */
HAL_StatusTypeDef AK4493_SetVolume(AK4493_HandleTypeDef *hak, uint8_t volume) {
    HAL_StatusTypeDef status;
    status = AK4493_WriteReg(hak, AK4493_REG_03_LCH_ATT, volume);
    if (status != HAL_OK) return status;
    status = AK4493_WriteReg(hak, AK4493_REG_04_RCH_ATT, volume);
    return status;
}

/**
 * @brief Set Volume with separate L/R control
 * 
 * @param hak AK4493 handle
 * @param left Left channel volume level (0x00 = 0dB, 0xFF = Mute)
 * @param right Right channel volume level (0x00 = 0dB, 0xFF = Mute)
 * @return HAL_StatusTypeDef HAL_OK if successful, HAL_ERROR otherwise
 */
HAL_StatusTypeDef AK4493_SetVolumeLR(AK4493_HandleTypeDef *hak, uint8_t left, uint8_t right) {
    HAL_StatusTypeDef status;
    status = AK4493_WriteReg(hak, AK4493_REG_03_LCH_ATT, left);
    if (status != HAL_OK) return status;
    status = AK4493_WriteReg(hak, AK4493_REG_04_RCH_ATT, right);
    return status;
}

/**
 * @brief Set Volume in dB (0.0 to -127.0 dB, or use -128.0 for mute)
 * 
 * @param hak AK4493 handle
 * @param dB Desired volume in dB
 * @return HAL_StatusTypeDef HAL_OK if successful, HAL_ERROR otherwise
 */
HAL_StatusTypeDef AK4493_SetVolume_dB(AK4493_HandleTypeDef *hak, float dB) {
    uint8_t atten = AK4493_dBToAttenuation(dB);
    return AK4493_SetVolume(hak, atten);
}

/**
 * @brief Soft Mute Control
 * 
 * @param hak AK4493 handle
 * @param enable 1 to enable mute, 0 to disable
 * @return HAL_StatusTypeDef HAL_OK if successful, HAL_ERROR otherwise
 */
HAL_StatusTypeDef AK4493_SetMute(AK4493_HandleTypeDef *hak, uint8_t enable) {
    /* SMUTE bit is bit 0 of Control 2 (0x01) */
    HAL_StatusTypeDef status = AK4493_UpdateBit(hak, AK4493_REG_01_CONTROL2, AK4493_SMUTE, enable);
    if (status == HAL_OK) {
        USB_LOG_INFO("AK4493 soft mute: %s\r\n", enable ? "ENABLED" : "DISABLED");
    }
    return status;
}

/**
 * @brief Set Digital Filter Type
 * 
 * @param hak AK4493 handle
 * @param filter Filter type (see AK4493_FilterTypeDef)
 * @return HAL_StatusTypeDef HAL_OK if successful, HAL_ERROR otherwise
 */
HAL_StatusTypeDef AK4493_SetFilter(AK4493_HandleTypeDef *hak, AK4493_FilterTypeDef filter) {
    uint8_t sd = 0, slow = 0, sslow = 0;
    if (hak == NULL || hak->hi2c == NULL || !hak->IsInitialized) return HAL_ERROR;

    /* Filter settings based on datasheet Table 31 (page 55) */
    switch (filter) {
        case AK4493_FILTER_SHARP_ROLLOFF:
            sd = 0; slow = 0; sslow = 0;
            break;
            
        case AK4493_FILTER_SLOW_ROLLOFF:
            sd = 0; slow = 1; sslow = 0;
            break;
            
        case AK4493_FILTER_SHORT_DELAY_SHARP:
            sd = 1; slow = 0; sslow = 0;
            break;
            
        case AK4493_FILTER_SHORT_DELAY_SLOW:
            sd = 1; slow = 1; sslow = 0;
            break;
            
        case AK4493_FILTER_SUPER_SLOW:
            sd = 0; slow = 1; sslow = 1;
            break;
            
        case AK4493_FILTER_LOW_DISPERSION:
            /* Low Dispersion = SD + SSLOW (per datasheet) */
            sd = 1; slow = 0; sslow = 1;
            break;
            
        default:
            return HAL_ERROR;
    }
    
    /* Apply settings to registers */
    /* SD: Control 2, bit 5 */
    AK4493_UpdateBit(hak, AK4493_REG_01_CONTROL2, AK4493_SD, sd);
    
    /* SLOW: Control 3, bit 0 */
    AK4493_UpdateBit(hak, AK4493_REG_02_CONTROL3, AK4493_SLOW, slow);
    
    /* SSLOW: Control 4, bit 0 */
    AK4493_UpdateBit(hak, AK4493_REG_05_CONTROL4, AK4493_SSLOW, sslow);
    
    USB_LOG_INFO("Filter changed to mode %d (SD=%d, SLOW=%d, SSLOW=%d)\r\n", filter, sd, slow, sslow);
    
    return HAL_OK;
}

/**
 * @brief Set PCM/DSD Mode
 * 
 * @param hak AK4493 handle
 * @param mode AK4493_MODE_PCM or AK4493_MODE_DSD
 * @return HAL_StatusTypeDef HAL_OK if successful, HAL_ERROR otherwise
 */
HAL_StatusTypeDef AK4493_SetMode(AK4493_HandleTypeDef *hak, AK4493_ModeTypeDef mode) {
    /* Soft reset before mode change */
    AK4493_UpdateBit(hak, AK4493_REG_00_CONTROL1, AK4493_RSTN, 0);
    HAL_Delay(1);
    
    if (mode == AK4493_MODE_DSD) {
        /* Enable DSD mode (DP=1 in Control 3, bit 7) */
        AK4493_UpdateBit(hak, AK4493_REG_02_CONTROL3, AK4493_DP, 1);
        
        /* Configure DSD clock (example: DCKS=0, DCKB=0 for 64fs) */
        /* User should adjust based on their DSD sample rate */
        AK4493_UpdateBit(hak, AK4493_REG_02_CONTROL3, AK4493_DCKS, 0);
        AK4493_UpdateBit(hak, AK4493_REG_02_CONTROL3, AK4493_DCKB, 0);
        
        /* DSD Filter: DFS[2:0] = 000 (Filter 1, 50kHz cutoff) */
        AK4493_UpdateBit(hak, AK4493_REG_01_CONTROL2, AK4493_DFS0, 0);
        AK4493_UpdateBit(hak, AK4493_REG_01_CONTROL2, AK4493_DFS1, 0);
        AK4493_UpdateBit(hak, AK4493_REG_05_CONTROL4, AK4493_DFS2, 0);
        
        /* Additional DSD settings in registers 0x06 and 0x09 can be configured here */
        /* Default values are usually OK */
        
        USB_LOG_INFO("Switched to DSD mode (64fs, Filter 1)\r\n");
    } else {
        /* PCM mode (DP=0 in Control 3, bit 7) */
        AK4493_UpdateBit(hak, AK4493_REG_02_CONTROL3, AK4493_DP, 0);
        USB_LOG_INFO("Switched to PCM mode\r\n");
    }
    
    /* Release reset */
    AK4493_UpdateBit(hak, AK4493_REG_00_CONTROL1, AK4493_RSTN, 1);
    HAL_Delay(10);
    
    return HAL_OK;
}

/**
 * @brief Set Audio Input Format
 * 
 * @param hak AK4493 handle
 * @param format Audio format (see AK4493_AudioFormatTypeDef)
 * @return HAL_StatusTypeDef HAL_OK if successful, HAL_ERROR otherwise
 */
HAL_StatusTypeDef AK4493_SetAudioFormat(AK4493_HandleTypeDef *hak, AK4493_AudioFormatTypeDef format) {
    /* DIF[2:0] are bits 3:1 of Control 1 */
    uint8_t current = AK4493_ReadReg(hak, AK4493_REG_00_CONTROL1);
    
    /* Clear DIF bits */
    current &= ~(0b111 << 1);
    
    /* Set new format */
    current |= (format << 1);
    
    return AK4493_WriteReg(hak, AK4493_REG_00_CONTROL1, current);
}

/**
 * @brief Set Output Gain
 * 
 * @param hak AK4493 handle
 * @param gain Gain level (AK4493_GAIN_3_75Vpp or AK4493_GAIN_2_8Vpp)
 * @return HAL_StatusTypeDef HAL_OK if successful, HAL_ERROR otherwise
 */
HAL_StatusTypeDef AK4493_SetGain(AK4493_HandleTypeDef *hak, AK4493_GainTypeDef gain) {
    /* GC[2:0] are bits 3:1 of Control 5 (0x07) */
    /* GC=001 (0x08) → ±3.75Vpp, GC=000 (0x00) → ±2.8Vpp */
    uint8_t current = AK4493_ReadReg(hak, AK4493_REG_07_CONTROL5);
    current &= ~(0b111 << 1);  // 清除 GC[2:0] 位 (Bit 3,2,1)
    
    if (gain == AK4493_GAIN_3_75Vpp) {
        current |= (1 << 3);   // 设置 GC2=1 (即 0x08)
    } else {
        current |= 0x00;       // 设置 GC[2:0]=000 (2.8Vpp)
    }
    
    return AK4493_WriteReg(hak, AK4493_REG_07_CONTROL5, current);
}

/**
 * @brief Set De-Emphasis Filter
 * 
 * @param hak AK4493 handle
 * @param deemp De-Emphasis type (AK4493_DEEMP_32KHZ or AK4493_DEEMP_44KHZ)
 * @return HAL_StatusTypeDef HAL_OK if successful, HAL_ERROR otherwise
 */
HAL_StatusTypeDef AK4493_SetDeEmphasis(AK4493_HandleTypeDef *hak, AK4493_DeEmphasisTypeDef deemp) {
    /* DEM[1:0] are bits 2:1 of Control 2 */
    uint8_t current = AK4493_ReadReg(hak, AK4493_REG_01_CONTROL2);
    current &= ~(0b11 << 1);  // Clear DEM bits
    current |= (deemp << 1);
    return AK4493_WriteReg(hak, AK4493_REG_01_CONTROL2, current);
}

/**
 * @brief Set Mono Mode
 * 
 * @param hak AK4493 handle
 * @param enable 1 to enable mono mode, 0 to disable
 * @return HAL_StatusTypeDef HAL_OK if successful, HAL_ERROR otherwise
 */
HAL_StatusTypeDef AK4493_SetMonoMode(AK4493_HandleTypeDef *hak, uint8_t enable) {
    /* MONO bit is bit 3 of Control 3 */
    return AK4493_UpdateBit(hak, AK4493_REG_02_CONTROL3, AK4493_MONO, enable);
}

/**
 * @brief Helper: Convert dB to attenuation register value
 * 
 * @param dB Input dB value (-127.0 to 0.0 dB, or -128.0 for mute)
 * @return uint8_t Attenuation register value (0x00 to 0xFF)
 */
uint8_t AK4493_dBToAttenuation(float dB) {
    if (dB >= 0.0f) return 0xFF; // 0dB = 0xFF
    if (dB <= -127.5f) return 0x00; // Mute = 0x00
    
    // Formula: RegVal = (dB / 0.5) + 255
    // Example: -40dB -> (-40 / 0.5) + 255 = -80 + 255 = 175 (0xAF)
    int reg = (int)(dB * 2.0f) + 255;
    if (reg < 1) reg = 1;
    if (reg > 255) reg = 255;
    return (uint8_t)reg;
}

/**
 * @brief Convert attenuation register value to dB
 * 
 * @param atten Attenuation register value (0x00 to 0xFF)
 * @return float dB value (-128.0 for mute, 0.0 for max gain, -127.5 to 0.0 dB)
 */
float AK4493_AttenuationTo_dB(uint8_t atten) {
    if (atten == 0x00) {
        return -128.0f;  // Mute (手册定义 0x00 为 Mute)
    } else if (atten == 0xFF) {
        return 0.0f;     // 0dB (最大)
    } else {
        /* AK4493S 公式: dB = (atten - 255) * 0.5 */
        return (float)(atten - 255) * 0.5f;
    }
}
