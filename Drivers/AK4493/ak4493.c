#include "ak4493.h"

/* Helper to write a register */
HAL_StatusTypeDef AK4493_WriteReg(AK4493_HandleTypeDef *hak, uint8_t reg, uint8_t val) {
    return HAL_I2C_Mem_Write(hak->hi2c, hak->DevAddress, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
}

/* Helper to read a register */
uint8_t AK4493_ReadReg(AK4493_HandleTypeDef *hak, uint8_t reg) {
    uint8_t val = 0;
    HAL_I2C_Mem_Read(hak->hi2c, hak->DevAddress, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
    return val;
}

/* Update a specific bit */
static HAL_StatusTypeDef AK4493_UpdateBit(AK4493_HandleTypeDef *hak, uint8_t reg, uint8_t mask, uint8_t value) {
    uint8_t current = AK4493_ReadReg(hak, reg);
    if (value) {
        current |= mask;
    } else {
        current &= ~mask;
    }
    return AK4493_WriteReg(hak, reg, current);
}

/* Power On Initialization */
HAL_StatusTypeDef AK4493_PowerOn(AK4493_HandleTypeDef *hak) {
    /* 1. Pull High DAC_PW_EN */
    if (hak->PW_EN_Port != NULL) {
        HAL_GPIO_WritePin(hak->PW_EN_Port, hak->PW_EN_Pin, GPIO_PIN_SET);
        printf("DAC_PW_EN Set High\r\n");
    }
    
    /* 2. Wait 20ms for power stabilization */
    HAL_Delay(20);
    printf("DAC_PW_EN Power Stabilized\r\n");
    
    /* 3. Perform Hardware Reset (PDN Low -> High) */
    if (hak->PDN_Port != NULL) {
        HAL_GPIO_WritePin(hak->PDN_Port, hak->PDN_Pin, GPIO_PIN_RESET);
        HAL_Delay(20); // Hold reset for 20ms
        HAL_GPIO_WritePin(hak->PDN_Port, hak->PDN_Pin, GPIO_PIN_SET);
        HAL_Delay(20); // Wait after release reset
        printf("AK4493 PDN Released\r\n");
    }
    
    return HAL_OK;
}

/* Register Initialization (Must be called after I2S clock is stable) */
HAL_StatusTypeDef AK4493_RegInit(AK4493_HandleTypeDef *hak) {
    if (hak == NULL || hak->hi2c == NULL) {
        
        return HAL_ERROR;
    }

    /* Check communication */
    if (HAL_I2C_IsDeviceReady(hak->hi2c, hak->DevAddress, 2, 100) != HAL_OK) {
        printf("AK4493 I2C Communication Failed. Check Device Address and I2C Bus.\r\n");
        return HAL_ERROR;
    }

    /* Initialize Registers based on AKdiuno reference */
    /* Reg 0x00: Auto MCLK, 32bit I2S, RSTN=1 */
    /* B10001111 = 0x8F */
    /* Bit 7 (ACK) = 1 (Auto Clock)
       Bit 5 (TDM1) = 0
       Bit 4 (TDM0) = 0
       Bit 3 (DIF2) = 1 \
       Bit 2 (DIF1) = 1  > 32-bit I2S
       Bit 1 (DIF0) = 1 /
       Bit 0 (RSTN) = 1 (Normal Operation)
    */
    AK4493_WriteReg(hak, AK4493_REG_00_CONTROL1, 0x8F);

    /* Reg 0x01: Data Zero Detect Enable */
    /* B10100010 = 0xA2 */
    AK4493_WriteReg(hak, AK4493_REG_01_CONTROL2, 0xA2);

    /* Reg 0x02: Stereo Mode (MONO=0) */
    /* Default is stereo, ensure bit 3 is 0 */
    AK4493_UpdateBit(hak, AK4493_REG_02_CONTROL3, (1 << 3), 0);

    return HAL_OK;
}

/* Initialize the AK4493 (Legacy/Full Init) */
HAL_StatusTypeDef AK4493_Init(AK4493_HandleTypeDef *hak) {
    if (hak == NULL || hak->hi2c == NULL) {
        return HAL_ERROR;
    }

    /* Hardware Reset / Power On */
    // Note: If using split init, call AK4493_PowerOn() and AK4493_RegInit() separately.
    AK4493_Reset(hak);

    /* Check communication */
    /* This check is also inside RegInit, but we can keep it here or just call RegInit */
    
    /* Initialize Registers */
    return AK4493_RegInit(hak);
}

/* Perform Hardware Reset */
HAL_StatusTypeDef AK4493_Reset(AK4493_HandleTypeDef *hak) {
    if (hak->PDN_Port != NULL) {
        HAL_GPIO_WritePin(hak->PDN_Port, hak->PDN_Pin, GPIO_PIN_RESET);
        HAL_Delay(20);
        HAL_GPIO_WritePin(hak->PDN_Port, hak->PDN_Pin, GPIO_PIN_SET);
        HAL_Delay(20);
    }
    return HAL_OK;
}

/* Set Volume (0x00 = Mute, 0xFF = Max) */
HAL_StatusTypeDef AK4493_SetVolume(AK4493_HandleTypeDef *hak, uint8_t volume) {
    HAL_StatusTypeDef status;
    status = AK4493_WriteReg(hak, AK4493_REG_03_LCH_ATT, volume);
    if (status != HAL_OK) return status;
    status = AK4493_WriteReg(hak, AK4493_REG_04_RCH_ATT, volume);
    return status;
}

/* Set Digital Filter */
HAL_StatusTypeDef AK4493_SetFilter(AK4493_HandleTypeDef *hak, AK4493_FilterTypeDef filter) {
    /* Filter bits are distributed across Reg 01, 02, 05 */
    /* This is a simplified implementation based on AKdiuno logic */
    
    uint8_t sd = 0, slow = 0, sslow = 0;

    switch (filter) {
        case AK4493_FILTER_SHARP_ROLLOFF:       sd=0; slow=0; sslow=0; break;
        case AK4493_FILTER_SLOW_ROLLOFF:        sd=0; slow=1; sslow=0; break;
        case AK4493_FILTER_SHORT_DELAY_SHARP:   sd=1; slow=0; sslow=0; break;
        case AK4493_FILTER_SHORT_DELAY_SLOW:    sd=1; slow=1; sslow=0; break;
        case AK4493_FILTER_SUPER_SLOW:          sd=0; slow=1; sslow=1; break; // Special case
        default: return HAL_ERROR;
    }

    if (filter == AK4493_FILTER_SUPER_SLOW) {
        AK4493_UpdateBit(hak, AK4493_REG_01_CONTROL2, (1<<5), sd); // SD=0
        AK4493_UpdateBit(hak, AK4493_REG_02_CONTROL3, (1<<0), slow); // SLOW=1
        AK4493_UpdateBit(hak, AK4493_REG_05_CONTROL4, (1<<0), sslow); // SSLOW=1
    } else {
        AK4493_UpdateBit(hak, AK4493_REG_05_CONTROL4, (1<<0), sslow); // Disable SSLOW
        AK4493_UpdateBit(hak, AK4493_REG_01_CONTROL2, (1<<5), sd);
        AK4493_UpdateBit(hak, AK4493_REG_02_CONTROL3, (1<<0), slow);
    }

    return HAL_OK;
}

/* Set PCM/DSD Mode */
HAL_StatusTypeDef AK4493_SetMode(AK4493_HandleTypeDef *hak, AK4493_ModeTypeDef mode) {
    /* Soft Reset */
    AK4493_UpdateBit(hak, AK4493_REG_00_CONTROL1, AK4493_RSTN, 0);
    HAL_Delay(20);

    if (mode == AK4493_MODE_DSD) {
        AK4493_UpdateBit(hak, AK4493_REG_02_CONTROL3, (1<<7), 1); // DP=1 (DSD)
        /* Additional DSD config can go here (Reg 06, 09) */
    } else {
        AK4493_UpdateBit(hak, AK4493_REG_02_CONTROL3, (1<<7), 0); // DP=0 (PCM)
    }

    /* Release Reset */
    AK4493_UpdateBit(hak, AK4493_REG_00_CONTROL1, AK4493_RSTN, 1);
    
    return HAL_OK;
}
