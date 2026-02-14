#ifndef __NJW1195A_H__
#define __NJW1195A_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include <stdio.h>

/* Register Map */
#define NJW1195A_REG_VOL_CH1    0x00
#define NJW1195A_REG_VOL_CH2    0x01
#define NJW1195A_REG_VOL_CH3    0x02
#define NJW1195A_REG_VOL_CH4    0x03

/* Volume Settings */
/* 0x00 = 0dB (Max Volume) */
/* 0xFF = Mute (Min Volume) */
/* Step = 0.5dB */
#define NJW1195A_VOL_0DB        0x00
#define NJW1195A_VOL_MUTE       0xFF

/* Driver Handle */
typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *LatchPort;
    uint16_t LatchPin;
    GPIO_TypeDef *PW_EN_Port;
    uint16_t PW_EN_Pin;
    volatile uint8_t TxBuffer[2]; /* Buffer for DMA */
    volatile uint8_t IsBusy;      /* Busy Flag */
} NJW1195A_HandleTypeDef;

/* Function Prototypes */
HAL_StatusTypeDef NJW1195A_Init(NJW1195A_HandleTypeDef *hnjw);
HAL_StatusTypeDef NJW1195A_SetLevel_DMA(NJW1195A_HandleTypeDef *hnjw, uint8_t channel, uint8_t level);
HAL_StatusTypeDef NJW1195A_SetAllLevels_DMA(NJW1195A_HandleTypeDef *hnjw, uint8_t level);

/* Callback to be called from HAL_SPI_TxCpltCallback in main.c */
void NJW1195A_TxCpltCallback(NJW1195A_HandleTypeDef *hnjw);

#ifdef __cplusplus
}
#endif

#endif /* __NJW1195A_H__ */
