#ifndef __CT7302_H__
#define __CT7302_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include <stdbool.h>

/* ==============================================================================
 * I2C 地址配置
 * ============================================================================== */
#define CT7302_I2C_ADDR_0       (0x20 << 1) // AD0 = 0
#define CT7302_I2C_ADDR_1       (0x22 << 1) // AD0 = 1

/* ==============================================================================
 * 寄存器定义 (基于 Datasheet V1.10 & Ref Code)
 * ============================================================================== */
#define CT7302_REG_DEVICE_ID    0x91  // V1.20 Update
#define CT7302_ID_DEFAULT       0x10

#define CT7302_REG_PAGE_SEL     0xFF
#define CT7302_REG_CHIP_ID      0x2B // Ref code uses 0x2B to check alive
#define CT7302_REG_ID_VAL       0x7F // Read 0x7F for ID

/* System & Power */
#define CT7302_REG_PWR_ST       0x02 // Power State (Bit 3: PDN)
#define CT7302_REG_CLK_CFG      0x0E // System Clock Source

/* Input & Output Routing */
#define CT7302_REG_IN_SEL       0x04 // Input Mux (Ref Code)
#define CT7302_REG_OUT_FS       0x05 // Output ASRC Target Freq (Ref Code)
#define CT7302_REG_MUTE         0x06 // Mute Control

/* Interrupts */
#define CT7302_REG_INT_MASK     0x08 // Interrupt Mask
#define CT7302_REG_INT_STATUS   0x09 // Interrupt Status

/* Status Readback */
#define CT7302_REG_RX_STATUS    0x10 // Lock Status
#define CT7302_REG_IN_FS        0x89 // Input Fs Index
#define CT7302_REG_CH_STATUS    0x77 // Channel Status (Bit depth, DoP)
#define CT7302_REG_DSD_STATUS   0x7A // DSD Detect

/* Main Output (I2S1) */
#define CT7302_REG_MAIN_FMT     0x3C // Format
#define CT7302_REG_MAIN_CLK     0x3D // MCLK/Master/Slave

/* Aux Output (I2S2) */
#define CT7302_REG_AUX_FMT      0x52 // Format
#define CT7302_REG_AUX_SRC      0x53 // Source

/* Volume */
#define CT7302_REG_MAIN_VOL     0x61 // Master Volume (Typical)

/* ==============================================================================
 * 枚举定义
 * ============================================================================== */

/* 输入源选择 (Reg 0x04) */
typedef enum {
    CT7302_IN_SPDIF_0 = 0x00,
    CT7302_IN_SPDIF_1 = 0x01,
    CT7302_IN_SPDIF_2 = 0x02,
    CT7302_IN_SPDIF_3 = 0x03,
    CT7302_IN_SPDIF_4 = 0x04,
    CT7302_IN_I2S_0   = 0x05,
    CT7302_IN_I2S_1   = 0x06,
    CT7302_IN_I2S_2   = 0x07,
} CT7302_Input_t;

/* ASRC 输出目标采样率 (Reg 0x05) */
typedef enum {
    CT7302_OUT_AUTO_44_48   = 0x00, // Bypass / Follow Input
    CT7302_OUT_FIXED_48K    = 0x03,
    CT7302_OUT_FIXED_96K    = 0x06,
    CT7302_OUT_FIXED_192K   = 0x09, // 您需要的 192K 模式
    CT7302_OUT_FIXED_384K   = 0x0C,
    CT7302_OUT_FIXED_768K   = 0x0F
} CT7302_ASRC_Target_t;

/* 数字格式 */
typedef enum {
    CT7302_FMT_I2S    = 0x00,
    CT7302_FMT_LJ     = 0x01,
    CT7302_FMT_RJ_24  = 0x02,
    CT7302_FMT_RJ_16  = 0x03,
    CT7302_FMT_DSD    = 0x04  // Special flag for DSD output setting
} CT7302_Format_t;

/* MCLK 输出倍率 */
typedef enum {
    CT7302_MCLK_128FS = 0x00,
    CT7302_MCLK_256FS = 0x01,
    CT7302_MCLK_512FS = 0x02
} CT7302_MCLK_t;

/* 信号类型状态 */
typedef enum {
    CT7302_SIG_UNLOCK = 0,
    CT7302_SIG_PCM,
    CT7302_SIG_DSD,
    CT7302_SIG_DOP
} CT7302_SignalType_t;

/* ==============================================================================
 * 数据结构
 * ============================================================================== */

/* 综合状态结构体 */
typedef struct {
    bool Locked;                // PLL 锁定状态
    uint32_t InputFs;           // 输入采样率 (Hz)
    uint8_t InputBitDepth;      // 估计位深 (16/24/32)
    CT7302_SignalType_t Type;   // 信号类型
} CT7302_Status_t;

/* 驱动句柄 */
typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint16_t I2C_Addr;
    
    GPIO_TypeDef *RST_Port;     // 硬件复位引脚端口
    uint16_t RST_Pin;           // 硬件复位引脚

    /* 缓存配置 */
    CT7302_Input_t CurrentInput;
    CT7302_ASRC_Target_t ASRC_Target;
    uint8_t CurrentVolume;      // 0-0xFF
    bool IsMuted;
} CT7302_HandleTypeDef;

/* ==============================================================================
 * API 函数声明
 * ============================================================================== */

/* 初始化与基础控制 */
HAL_StatusTypeDef CT7302_Init(CT7302_HandleTypeDef *hct);
HAL_StatusTypeDef CT7302_SetPower(CT7302_HandleTypeDef *hct, bool enable);
HAL_StatusTypeDef CT7302_SoftReset(CT7302_HandleTypeDef *hct);

/* 输入与 ASRC 配置 */
HAL_StatusTypeDef CT7302_SetInput(CT7302_HandleTypeDef *hct, CT7302_Input_t input);
HAL_StatusTypeDef CT7302_SetASRC_Target(CT7302_HandleTypeDef *hct, CT7302_ASRC_Target_t target);

/* 输出端口配置 */
HAL_StatusTypeDef CT7302_ConfigMainOutput(CT7302_HandleTypeDef *hct, bool master, CT7302_Format_t fmt, CT7302_MCLK_t mclk);
HAL_StatusTypeDef CT7302_ConfigAuxOutput(CT7302_HandleTypeDef *hct, bool enable, CT7302_Format_t fmt);

/* 音量与静音 */
HAL_StatusTypeDef CT7302_SetMute(CT7302_HandleTypeDef *hct, bool mute);
HAL_StatusTypeDef CT7302_SetVolume(CT7302_HandleTypeDef *hct, uint8_t vol); // 0x00(Mute) - 0xFF(Max)

/* 状态查询 (核心功能) */
HAL_StatusTypeDef CT7302_GetStatus(CT7302_HandleTypeDef *hct, CT7302_Status_t *status);
uint8_t CT7302_ReadInterrupts(CT7302_HandleTypeDef *hct); // 读取并清除中断

/* DSD 高级配置 */
HAL_StatusTypeDef CT7302_ConfigDSD(CT7302_HandleTypeDef *hct, bool enable_dop_detect);

#ifdef __cplusplus
}
#endif

#endif /* __CT7302_FULL_H__ */