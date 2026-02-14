#ifndef __AK4493_H__
#define __AK4493_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"

/* AK4493 I2C Address (7-bit 0x13 -> 8-bit 0x26) */
#define AK4493_I2C_ADDR_0  (0x10 << 1) // CAD1=L, CAD0=L
#define AK4493_I2C_ADDR_1  (0x11 << 1) // CAD1=L, CAD0=H
#define AK4493_I2C_ADDR_2  (0x12 << 1) // CAD1=H, CAD0=L
#define AK4493_I2C_ADDR_3  (0x13 << 1) // CAD1=H, CAD0=H

/* Default Address used in AKdiuno */
#define AK4493_DEFAULT_ADDR AK4493_I2C_ADDR_3

/* Register Map */
#define AK4493_REG_00_CONTROL1      0x00
#define AK4493_REG_01_CONTROL2      0x01
#define AK4493_REG_02_CONTROL3      0x02
#define AK4493_REG_03_LCH_ATT       0x03
#define AK4493_REG_04_RCH_ATT       0x04
#define AK4493_REG_05_CONTROL4      0x05
#define AK4493_REG_06_DSD1          0x06
#define AK4493_REG_07_CONTROL5      0x07
#define AK4493_REG_08_SOUND_CONTROL 0x08
#define AK4493_REG_09_DSD2          0x09
#define AK4493_REG_0A_CONTROL7      0x0A
#define AK4493_REG_0B_CONTROL8      0x0B

/* Control 1 Register Bits */
#define AK4493_RSTN                 (1 << 0)
#define AK4493_DIF0                 (1 << 1)
#define AK4493_DIF1                 (1 << 2)
#define AK4493_DIF2                 (1 << 3)
#define AK4493_TDM0                 (1 << 4)
#define AK4493_TDM1                 (1 << 5)
#define AK4493_ACK                  (1 << 7)

/* Filter Settings */
typedef enum {
    AK4493_FILTER_SHARP_ROLLOFF = 0,
    AK4493_FILTER_SLOW_ROLLOFF,
    AK4493_FILTER_SHORT_DELAY_SHARP,
    AK4493_FILTER_SHORT_DELAY_SLOW,
    AK4493_FILTER_SUPER_SLOW,
    AK4493_FILTER_LOW_DISPERSION
} AK4493_FilterTypeDef;

/* PCM/DSD Mode */
typedef enum {
    AK4493_MODE_PCM = 0,
    AK4493_MODE_DSD
} AK4493_ModeTypeDef;

/* Driver Handle */
typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint16_t DevAddress;
    GPIO_TypeDef *PDN_Port;
    uint16_t PDN_Pin;
} AK4493_HandleTypeDef;

/* Function Prototypes */
HAL_StatusTypeDef AK4493_Init(AK4493_HandleTypeDef *hak);
HAL_StatusTypeDef AK4493_Reset(AK4493_HandleTypeDef *hak);
HAL_StatusTypeDef AK4493_SetVolume(AK4493_HandleTypeDef *hak, uint8_t volume);
HAL_StatusTypeDef AK4493_SetMute(AK4493_HandleTypeDef *hak, uint8_t enable);
HAL_StatusTypeDef AK4493_SetFilter(AK4493_HandleTypeDef *hak, AK4493_FilterTypeDef filter);
HAL_StatusTypeDef AK4493_SetMode(AK4493_HandleTypeDef *hak, AK4493_ModeTypeDef mode);
HAL_StatusTypeDef AK4493_WriteReg(AK4493_HandleTypeDef *hak, uint8_t reg, uint8_t val);
uint8_t AK4493_ReadReg(AK4493_HandleTypeDef *hak, uint8_t reg);

#ifdef __cplusplus
}
#endif

#endif /* __AK4493_H__ */
