#ifndef __NJW1195A_H__
#define __NJW1195A_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include <stdio.h>

/* ============================================================
 * NJW1195A - 4-Channel Electronic Volume with Input Selector
 * Driver Header  (Reviewed & Corrected)
 * ============================================================ */

/* -------------------------------------------------------
 * Register Map  (Select Address field, D7-D4 of low byte)
 * ------------------------------------------------------- */
/* Volume control registers */
#define NJW1195A_REG_VOL_CH1        0x00  /* Volume Control 1A (single) / Volume Control A (diff) */
#define NJW1195A_REG_VOL_CH2        0x01  /* Volume Control 1B (single) / Volume Control B (diff) */
#define NJW1195A_REG_VOL_CH3        0x02  /* Volume Control 2A (single only, NOT valid in diff mode) */
#define NJW1195A_REG_VOL_CH4        0x03  /* Volume Control 2B (single only, NOT valid in diff mode) */

/* Input selector registers */
#define NJW1195A_REG_SEL_1A_2A      0x04  /* Input Selector 1A + 2A  (single) / Selector A (diff) */
#define NJW1195A_REG_SEL_1B_2B      0x05  /* Input Selector 1B + 2B  (single) / Selector B (diff) */

/* -------------------------------------------------------
 * Volume register byte values  (D15-D8, MSB of control word)
 * Range: +31.5 dB (0x01) to -95 dB (0xFE), 0.5 dB/step
 * 0x00 = Mute, 0xFF = Mute (initial/power-on state)
 * See datasheet p.13-17 for full table.
 * ------------------------------------------------------- */
#define NJW1195A_VOL_MUTE           0xFF  /* Mute (power-on default per datasheet) */
#define NJW1195A_VOL_0DB            0x40  /* 0 dB */
#define NJW1195A_VOL_PLUS_31_5DB    0x01  /* +31.5 dB (maximum gain) */
#define NJW1195A_VOL_MINUS_95DB     0xFE  /* -95 dB  (minimum before mute) */

/* -------------------------------------------------------
 * Input selector 3-bit field values  (single-end mode)
 * Used in D15-D13 (SEL1) and D12-D10 (SEL2) of the data byte.
 * Datasheet p.18, single transmission truth table.
 * ------------------------------------------------------- */
#define NJW1195A_INPUT_MUTE         0x00  /* 000 -> Mute (power-on default) */
#define NJW1195A_INPUT_1            0x01  /* 001 -> Input 1 */
#define NJW1195A_INPUT_2            0x02  /* 010 -> Input 2 */
#define NJW1195A_INPUT_3            0x03  /* 011 -> Input 3 */
#define NJW1195A_INPUT_4            0x04  /* 100 -> Input 4 */

/* -------------------------------------------------------
 * Differential mode input selector encoding
 * Datasheet p.18, differential transmission truth table.
 * The data byte packs SEL1 (D15-D13) and SEL2 (D12-D10).
 *
 *  Input 1: SEL1 = 010 (0x02), SEL2 = 001 (0x01)
 *           data byte = (0x02 << 5) | (0x01 << 2) = 0x44
 *
 *  Input 2: SEL1 = 011 (0x03), SEL2 = 100 (0x04)
 *           data byte = (0x03 << 5) | (0x04 << 2) = 0x70
 *
 *  Mute   : SEL1 = 000 (0x00), SEL2 = 000 (0x00)
 *           data byte = 0x00
 * ------------------------------------------------------- */
#define NJW1195A_DIFF_INPUT_MUTE    0x00  /* Both selectors = 000 */
#define NJW1195A_DIFF_INPUT_1       ((0x02U << 5) | (0x01U << 2))  /* 0x44 */
#define NJW1195A_DIFF_INPUT_2       ((0x03U << 5) | (0x04U << 2))  /* 0x70 */

/* -------------------------------------------------------
 * Control word field masks  (for reference / documentation)
 * 16-bit word layout: [D15..D8=Data][D7..D4=SelectAddr][D3..D0=ChipAddr]
 * ------------------------------------------------------- */
#define NJW1195A_DATA_MASK          0xFF00U  /* D15-D8  : volume/selector data  */
#define NJW1195A_ADDR_MASK          0x00F0U  /* D7 -D4  : register select address */
#define NJW1195A_CHIPADDR_MASK      0x000FU  /* D3 -D0  : chip address (ADR1, ADR0) */
#define NJW1195A_DATA_SHIFT         8U
#define NJW1195A_ADDR_SHIFT         4U

/* -------------------------------------------------------
 * Driver Handle
 * ------------------------------------------------------- */
typedef struct {
    /* SPI peripheral */
    SPI_HandleTypeDef *hspi;

    /* Power-enable GPIO (required) */
    GPIO_TypeDef      *PW_EN_Port;
    uint16_t           PW_EN_Pin;

    /* LATCH GPIO (required) */
    GPIO_TypeDef      *LatchPort;
    uint16_t           LatchPin;

    /* Amplifier-enable GPIOs (required) */
    GPIO_TypeDef      *SE_EN_Port;   /* Single-ended amplifier enable */
    uint16_t           SE_EN_Pin;
    GPIO_TypeDef      *BAL_EN_Port;  /* Balanced/differential amplifier enable */
    uint16_t           BAL_EN_Pin;

    /* Optional chip-address pins  (default: both LOW -> chip address 0x0) */
    GPIO_TypeDef      *ADR0_Port;    /* NULL = pin held low externally */
    uint16_t           ADR0_Pin;
    GPIO_TypeDef      *ADR1_Port;    /* NULL = pin held low externally */
    uint16_t           ADR1_Pin;

    /* DMA TX buffer (must be in a DMA-accessible memory region) */
    uint8_t            TxBuffer[2];
    volatile uint8_t   IsBusy;       /* 1 = DMA transfer in progress */

    /* Resolved chip address (0-3), computed during Init from ADR0/ADR1 */
    uint8_t            ChipAddress;

    /* Default input selector (single-ended) */
    uint8_t            DefaultInput;

    /* DMA command queue (max 4 pending commands) */
    uint8_t            QueuedCommands;
    uint8_t            QueuedChannels[4];
    uint8_t            QueuedLevels[4];

    /* State flags */
    uint8_t            IsInitialized; /* 0 = not ready, 1 = hardware ready */
    uint8_t            IsDiffMode;    /* 0 = single-ended, 1 = differential */
} NJW1195A_HandleTypeDef;

/* -------------------------------------------------------
 * Public API
 * ------------------------------------------------------- */

/**
 * @brief  Full initialization sequence (power-on, mute, EXTI handover).
 *         Calls NJW1195A_Core_Config() internally.
 */
HAL_StatusTypeDef NJW1195A_Init(NJW1195A_HandleTypeDef *hnjw);

/**
 * @brief  Low-level hardware configuration (power rail, SPI defaults, initial mute).
 *         Safe to call before IsInitialized is set; used internally by Init.
 */
HAL_StatusTypeDef NJW1195A_Core_Config(NJW1195A_HandleTypeDef *hnjw);

/**
 * @brief  Set volume for a single channel (blocking SPI).
 * @param  channel  Register address: NJW1195A_REG_VOL_CH1 .. CH4
 * @param  level    Volume byte (NJW1195A_VOL_MUTE, NJW1195A_VOL_0DB, etc.)
 */
HAL_StatusTypeDef NJW1195A_SetVolume(NJW1195A_HandleTypeDef *hnjw,
                                     uint8_t channel, uint8_t level);

/**
 * @brief  Set volume for a single channel via DMA (non-blocking).
 *         Returns HAL_BUSY if a DMA transfer is already in progress.
 */
HAL_StatusTypeDef NJW1195A_SetVolume_DMA(NJW1195A_HandleTypeDef *hnjw,
                                          uint8_t channel, uint8_t level);

/**
 * @brief  Set all active volume channels to the same level (blocking).
 *         In differential mode only CH1/CH2 are written; CH3/CH4 are skipped.
 */
HAL_StatusTypeDef NJW1195A_SetAllVolumes(NJW1195A_HandleTypeDef *hnjw,
                                          uint8_t level);

/**
 * @brief  Configure input selectors in single-ended mode (blocking).
 *         Selector values: NJW1195A_INPUT_MUTE / INPUT_1 .. INPUT_4
 */
HAL_StatusTypeDef NJW1195A_SetInput(NJW1195A_HandleTypeDef *hnjw,
                                     uint8_t selector1A, uint8_t selector2A,
                                     uint8_t selector1B, uint8_t selector2B);

/**
 * @brief  Configure input selector in differential mode (blocking).
 * @param  selector  1 = Differential Input 1, 2 = Differential Input 2, 0 = Mute
 */
HAL_StatusTypeDef NJW1195A_SetInput_Diff(NJW1195A_HandleTypeDef *hnjw,
                                          uint8_t selector);

/**
 * @brief  Queue a volume command for DMA dispatch.
 *         Must be called when IsBusy == 0 or the queue has space.
 *         Returns HAL_ERROR if the queue is full (max 4 entries).
 */
HAL_StatusTypeDef NJW1195A_EnqueueVolume(NJW1195A_HandleTypeDef *hnjw,
                                          uint8_t channel, uint8_t level);

/**
 * @brief  Must be called from HAL_SPI_TxCpltCallback() in main/irq file.
 *         Asserts LATCH, clears IsBusy, and dispatches next queued command.
 */
void NJW1195A_TxCpltCallback(NJW1195A_HandleTypeDef *hnjw);

/**
 * @brief  Process pending volume commands in the queue.
 * @note   Must be called in main loop (while 1) or RTOS task, never in ISR!
 */
void NJW1195A_ProcessQueue(NJW1195A_HandleTypeDef *hnjw);

/**
 * @brief  Convert a dB value to the corresponding register byte.
 *         Input range: -95.0 to +31.5 dB in 0.5 dB steps.
 *         Values outside range are clamped; use NJW1195A_VOL_MUTE for mute.
 *
 * @note   Register encoding (from datasheet p.13-17):
 *           0x01 = +31.5 dB  (max gain)
 *           0x40 =   0.0 dB
 *           0xFE =  -95.0 dB (min attenuation)
 *           0xFF =   Mute    (power-on default)
 *         Formula: reg = 0x40 - round(dB * 2)
 *         Examples: +10 dB -> 0x2C,  0 dB -> 0x40, -10 dB -> 0x54
 */
uint8_t NJW1195A_dBToRegister(float dB);

#ifdef __cplusplus
}
#endif

#endif /* __NJW1195A_H__ */