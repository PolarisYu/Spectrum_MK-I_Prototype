#ifndef __AK4493_H__
#define __AK4493_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * AK4493 I2C 地址配置
 * ============================================================================
 * 重要：请根据您的硬件原理图确认CAD0和CAD1引脚的连接！
 * 
 * CAD1 | CAD0 | 7位地址 | 8位写地址 | 说明
 * -----|------|---------|-----------|------------------------
 *  L   |  L   |  0x10   |   0x20    | 两个引脚都接地
 *  L   |  H   |  0x11   |   0x22    | CAD0接VDD，CAD1接地
 *  H   |  L   |  0x12   |   0x24    | CAD0接地，CAD1接VDD
 *  H   |  H   |  0x13   |   0x26    | 两个引脚都接VDD
 */
#define AK4493_I2C_ADDR_0  (0x10 << 1)  // CAD1=L, CAD0=L → 0x20
#define AK4493_I2C_ADDR_1  (0x11 << 1)  // CAD1=L, CAD0=H → 0x22
#define AK4493_I2C_ADDR_2  (0x12 << 1)  // CAD1=H, CAD0=L → 0x24
#define AK4493_I2C_ADDR_3  (0x13 << 1)  // CAD1=H, CAD0=H → 0x26

/* 默认地址（请根据您的硬件修改！） */
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
#define AK4493_REG_0A_CONTROL6      0x0A
#define AK4493_REG_0B_CONTROL7      0x0B
#define AK4493_REG_15_CONTROL8      0x15

#define AK4493_NUM_REGS             0x16  // 总共22个寄存器（0x00-0x15）

/* Control 1 Register Bits (0x00) */
#define AK4493_RSTN     (1 << 0)  // 软复位 (0=复位, 1=正常)
#define AK4493_DIF0     (1 << 1)  // 数字输入格式 bit 0
#define AK4493_DIF1     (1 << 2)  // 数字输入格式 bit 1
#define AK4493_DIF2     (1 << 3)  // 数字输入格式 bit 2
#define AK4493_TDM0     (1 << 4)  // TDM模式 bit 0
#define AK4493_TDM1     (1 << 5)  // TDM模式 bit 1
#define AK4493_ECS      (1 << 6)  // 外部时钟选择
#define AK4493_EXDF     (1 << 6)  // 外部数字滤波器（与ECS相同位）
#define AK4493_ACKS     (1 << 7)  // 自动时钟选择 (0=手动, 1=自动)

/* Control 2 Register Bits (0x01) */
#define AK4493_SMUTE    (1 << 0)  // 软静音
#define AK4493_DEM0     (1 << 1)  // 去加重 bit 0
#define AK4493_DEM1     (1 << 2)  // 去加重 bit 1
#define AK4493_DFS0     (1 << 3)  // DSD滤波器选择 bit 0
#define AK4493_DFS1     (1 << 4)  // DSD滤波器选择 bit 1
#define AK4493_SD       (1 << 5)  // 短延迟滤波器
#define AK4493_DZFM     (1 << 6)  // DZF模式
#define AK4493_DZFE     (1 << 7)  // DZF使能

/* Control 3 Register Bits (0x02) */
#define AK4493_SLOW     (1 << 0)  // 慢速滚降滤波器
#define AK4493_SELLR    (1 << 1)  // L/R声道选择
#define AK4493_DZFB     (1 << 2)  // DZF空白
#define AK4493_MONO     (1 << 3)  // 单声道模式
#define AK4493_DCKB     (1 << 4)  // DSD时钟 bit
#define AK4493_DCKS     (1 << 5)  // DSD时钟选择
#define AK4493_ADP      (1 << 6)  // 自动DSD/PCM检测
#define AK4493_DP       (1 << 7)  // DSD/PCM模式 (0=PCM, 1=DSD)

/* Control 4 Register Bits (0x05) */
#define AK4493_SSLOW    (1 << 0)  // 超慢速滚降滤波器
#define AK4493_DFS2     (1 << 1)  // DSD滤波器选择 bit 2
#define AK4493_INVR     (1 << 6)  // R声道反相
#define AK4493_INVL     (1 << 7)  // L声道反相

/* Control 5 Register Bits (0x07) */
#define AK4493_SYNCE    (1 << 0)  // 同步使能
#define AK4493_GC0      (1 << 1)  // 增益控制 bit 0
#define AK4493_GC1      (1 << 2)  // 增益控制 bit 1
#define AK4493_GC2      (1 << 3)  // 增益控制 bit 2
#define AK4493_MSTBN    (1 << 7)  // 主空白

/* 音量值定义 */
#define AK4493_VOL_0DB          0x00  // 0dB（最大音量）
#define AK4493_VOL_MINUS_40DB   0x50  // -40dB（安全默认值）
#define AK4493_VOL_MINUS_64DB   0x80  // -64dB
#define AK4493_VOL_MINUS_127DB  0xFE  // -127dB（最小）
#define AK4493_VOL_MUTE         0xFF  // 静音

/* 音频输入格式 */
typedef enum {
    AK4493_FORMAT_24BIT_MSB     = 0b000,
    AK4493_FORMAT_I2S_24BIT     = 0b011,
    AK4493_FORMAT_I2S_32BIT     = 0b111,  // 默认
    AK4493_FORMAT_16BIT_LSB     = 0b100,
    AK4493_FORMAT_20BIT_LSB     = 0b101,
    AK4493_FORMAT_24BIT_LSB     = 0b110,
    AK4493_FORMAT_32BIT_LSB     = 0b010,
} AK4493_AudioFormatTypeDef;

/* 数字滤波器类型 */
typedef enum {
    AK4493_FILTER_SHARP_ROLLOFF = 0,
    AK4493_FILTER_SLOW_ROLLOFF,
    AK4493_FILTER_SHORT_DELAY_SHARP,
    AK4493_FILTER_SHORT_DELAY_SLOW,
    AK4493_FILTER_SUPER_SLOW,
    AK4493_FILTER_LOW_DISPERSION
} AK4493_FilterTypeDef;

/* PCM/DSD模式 */
typedef enum {
    AK4493_MODE_PCM = 0,
    AK4493_MODE_DSD
} AK4493_ModeTypeDef;

/* 输出增益 */
typedef enum {
    AK4493_GAIN_2_8Vpp  = 0,
    AK4493_GAIN_3_75Vpp = 1,
} AK4493_GainTypeDef;

/* 去加重设置 */
typedef enum {
    AK4493_DEEMP_OFF    = 0b00,
    AK4493_DEEMP_32KHZ  = 0b01,
    AK4493_DEEMP_44_1KHZ = 0b10,
    AK4493_DEEMP_48KHZ  = 0b11,
} AK4493_DeEmphasisTypeDef;

/* 驱动句柄 - 包含影子寄存器 */
typedef struct {
    /* I2C接口 */
    I2C_HandleTypeDef *hi2c;
    uint16_t DevAddress;
    
    /* PDN引脚（必需） */
    GPIO_TypeDef *PDN_Port;
    uint16_t PDN_Pin;
    
    /* 可选的外部电源使能（板级特定） */
    GPIO_TypeDef *PW_EN_Port;
    uint16_t PW_EN_Pin;
    
    /* 可选的LDOE控制 */
    GPIO_TypeDef *LDOE_Port;
    uint16_t LDOE_Pin;
    
    /* ⭐ 影子寄存器数组 - 缓存所有寄存器的值 */
    uint8_t Regs[AK4493_NUM_REGS];
    
    /* 状态标志 */
    uint8_t IsInitialized;
    uint8_t ClocksReady;  // MCLK/LRCK/BICK是否稳定
} AK4493_HandleTypeDef;

/* 函数原型 */

/* 初始化函数 */
HAL_StatusTypeDef AK4493_Init(AK4493_HandleTypeDef *hak);
HAL_StatusTypeDef AK4493_PowerOn(AK4493_HandleTypeDef *hak);
HAL_StatusTypeDef AK4493_RegInit(AK4493_HandleTypeDef *hak);
HAL_StatusTypeDef AK4493_Reset(AK4493_HandleTypeDef *hak);

/* 音量控制 */
HAL_StatusTypeDef AK4493_SetVolume(AK4493_HandleTypeDef *hak, uint8_t volume);
HAL_StatusTypeDef AK4493_SetVolumeLR(AK4493_HandleTypeDef *hak, uint8_t left, uint8_t right);
HAL_StatusTypeDef AK4493_SetVolume_dB(AK4493_HandleTypeDef *hak, float dB);
HAL_StatusTypeDef AK4493_SetMute(AK4493_HandleTypeDef *hak, uint8_t enable);

/* 音频配置 */
HAL_StatusTypeDef AK4493_SetFilter(AK4493_HandleTypeDef *hak, AK4493_FilterTypeDef filter);
HAL_StatusTypeDef AK4493_SetMode(AK4493_HandleTypeDef *hak, AK4493_ModeTypeDef mode);
HAL_StatusTypeDef AK4493_SetAudioFormat(AK4493_HandleTypeDef *hak, AK4493_AudioFormatTypeDef format);
HAL_StatusTypeDef AK4493_SetGain(AK4493_HandleTypeDef *hak, AK4493_GainTypeDef gain);
HAL_StatusTypeDef AK4493_SetDeEmphasis(AK4493_HandleTypeDef *hak, AK4493_DeEmphasisTypeDef deemp);
HAL_StatusTypeDef AK4493_SetMonoMode(AK4493_HandleTypeDef *hak, uint8_t enable);

/* 低级寄存器访问 */
HAL_StatusTypeDef AK4493_WriteReg(AK4493_HandleTypeDef *hak, uint8_t reg, uint8_t val);
HAL_StatusTypeDef AK4493_WriteRegVerify(AK4493_HandleTypeDef *hak, uint8_t reg, uint8_t val);
uint8_t AK4493_ReadReg(AK4493_HandleTypeDef *hak, uint8_t reg);
void AK4493_DumpRegisters(AK4493_HandleTypeDef *hak);

/* 辅助函数 */
uint8_t AK4493_dBToAttenuation(float dB);
float AK4493_AttenuationTo_dB(uint8_t atten);

#ifdef __cplusplus
}
#endif

#endif /* __AK4493_IMPROVED_H__ */
