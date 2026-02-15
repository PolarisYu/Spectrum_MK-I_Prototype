#ifndef __NJW1195A_H__
#define __NJW1195A_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include <stdio.h>

/* Register Map - Volume Control */
#define NJW1195A_REG_VOL_CH1    0x00
#define NJW1195A_REG_VOL_CH2    0x01
#define NJW1195A_REG_VOL_CH3    0x02
#define NJW1195A_REG_VOL_CH4    0x03

/* Register Map - Input Selector */
#define NJW1195A_REG_SEL_1A_2A  0x04
#define NJW1195A_REG_SEL_1B_2B  0x05

/* Volume Settings (8-bit data value) */
#define NJW1195A_VOL_0DB        0x40  // 0dB (from datasheet control data table)
#define NJW1195A_VOL_MUTE       0x00  // Mute (or 0xFF)
#define NJW1195A_VOL_PLUS_31_5DB 0x01  // +31.5dB (max gain)
#define NJW1195A_VOL_MINUS_95DB 0xFE  // -95dB (min before mute)

/* Input Selector Values (3-bit field in D15-D13 or D12-D10) */
#define NJW1195A_INPUT_MUTE     0x00
#define NJW1195A_INPUT_1        0x01
#define NJW1195A_INPUT_2        0x02
#define NJW1195A_INPUT_3        0x03
#define NJW1195A_INPUT_4        0x04

/* Control Data Format Masks */
#define NJW1195A_ADDR_MASK      0x0F00  // D11-D8: address
#define NJW1195A_DATA_MASK      0x00FF  // D7-D0: data
#define NJW1195A_ADDR_SHIFT     8

/* Driver Handle */
typedef struct {
    SPI_HandleTypeDef *hspi;
    
    /* LATCH pin (required) */
    GPIO_TypeDef *LatchPort;
    uint16_t LatchPin;
    
    /* Optional chip address pins (default: both LOW = address 0x0) */
    GPIO_TypeDef *ADR0_Port;
    uint16_t ADR0_Pin;
    GPIO_TypeDef *ADR1_Port;
    uint16_t ADR1_Pin;
    
    /* DMA buffers and state */
    uint8_t TxBuffer[4];      // Up to 2 bytes per transaction, or queue
    volatile uint8_t IsBusy;
    
    /* Current chip address (0-3) */
    uint8_t ChipAddress;
    
    /* Callback queue for multi-channel operations */
    uint8_t QueuedCommands;
    uint8_t QueuedChannels[4];
    uint8_t QueuedLevels[4];
} NJW1195A_HandleTypeDef;

/* Function Prototypes */
HAL_StatusTypeDef NJW1195A_Init(NJW1195A_HandleTypeDef *hnjw);
HAL_StatusTypeDef NJW1195A_SetVolume(NJW1195A_HandleTypeDef *hnjw, uint8_t channel, uint8_t level);
HAL_StatusTypeDef NJW1195A_SetVolume_DMA(NJW1195A_HandleTypeDef *hnjw, uint8_t channel, uint8_t level);
HAL_StatusTypeDef NJW1195A_SetAllVolumes(NJW1195A_HandleTypeDef *hnjw, uint8_t level);
HAL_StatusTypeDef NJW1195A_SetInput(NJW1195A_HandleTypeDef *hnjw, uint8_t selector1A, uint8_t selector2A, uint8_t selector1B, uint8_t selector2B);

/* Callback to be called from HAL_SPI_TxCpltCallback in main.c */
void NJW1195A_TxCpltCallback(NJW1195A_HandleTypeDef *hnjw);

/* Helper function to convert dB to register value */
uint8_t NJW1195A_dBToRegister(float dB);

#ifdef __cplusplus
}
#endif

#endif /* __NJW1195A_H__ */
