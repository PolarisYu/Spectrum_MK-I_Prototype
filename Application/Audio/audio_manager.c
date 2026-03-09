#include "audio_manager.h"
#include "usb_config.h"
#include "usb_log.h"
#include "i2c.h"
#include "spi.h"
#include "gpio.h"

/* Global Handles */
AK4493_HandleTypeDef hak = {0};
NJW1195A_HandleTypeDef hnjw = {0};
CT7302_HandleTypeDef hct = {0};

/* Private variables */
static volatile uint8_t ak4493_init_needed = 0;

/* Initialize all Audio peripherals (DAC, AMP, Receiver) */
void Audio_Init(void)
{
    /* === CT7302 Init === */
    hct.hi2c = &hi2c3;
    CT7302_Init(&hct);

    /* === NJW1195A Init === */
    hnjw.hspi = &hspi2; // 使用 SPI2
    hnjw.LatchPort = SPI2_LATCH_GPIO_Port; // PB12 (来自 main.h)
    hnjw.LatchPin = SPI2_LATCH_Pin;

    hnjw.PW_EN_Port = AMP_PW_EN_GPIO_Port;
    hnjw.PW_EN_Pin = AMP_PW_EN_Pin;
    
    hnjw.SE_EN_Port = SE_AMP_EN_GPIO_Port;
    hnjw.SE_EN_Pin = SE_AMP_EN_Pin;
    hnjw.BAL_EN_Port = BAL_AMP_EN_GPIO_Port;
    hnjw.BAL_EN_Pin = BAL_AMP_EN_Pin;
    
    hnjw.ChipAddress  = 0;
    hnjw.DefaultInput = 2;

    hnjw.IsDiffMode = 1;
    
    NJW1195A_Init(&hnjw);

    // Set initial volume to 15.0dB (User Preference)
    NJW1195A_SetAllVolumes(&hnjw, NJW1195A_dBToRegister(-30.0f));
    USB_LOG_INFO("NJW1195A Init. Volume set to -30.0dB.\r\n");

    /* === AK4493 Init === */
    // 1. 配置 I2C 句柄
    hak.hi2c = &hi2c2;
    
    // 2. 配置 I2C 地址 (默认是 0x26)
    hak.DevAddress = AK4493_DEFAULT_ADDR;
    
    // 3. 配置 PDN 复位引脚 (根据 main.h 中的定义)
    hak.PDN_Port = DAC_PDN_GPIO_Port;
    hak.PDN_Pin = DAC_PDN_Pin;
    hak.PW_EN_Port = DAC_PW_EN_GPIO_Port;
    hak.PW_EN_Pin = DAC_PW_EN_Pin;

    hak.IsInitialized = 0;

    // 4. 初始化 DAC 电源 (Power On only)
    AK4493_PowerOn(&hak);
    USB_LOG_INFO("AK4493 Power On. Waiting for USB/I2S...\r\n");
}

/* Audio Task to be called in main loop */
void Audio_Task(void)
{
    // 1. 处理 NJW1195A 音量队列
    NJW1195A_ProcessQueue(&hnjw);

    if (ak4493_init_needed) {
        USB_LOG_INFO("AK4493 Init Request Detected. Waiting for clock...\r\n");
        HAL_Delay(1000); // Initial wait for USB/I2S clock

        // Retry logic: Try 5 times
        int i;
        for (i = 0; i < 5; i++) {
            USB_LOG_INFO("AK4493 Reg Init Attempt %d/5...\r\n", i + 1);
            if (AK4493_RegInit(&hak) == HAL_OK) {
                USB_LOG_INFO("AK4493 Reg Init Success!\r\n");
                HAL_Delay(10);
                AK4493_SetVolume(&hak, 140); 
                AK4493_SetMute(&hak, 0);           // 解除静音
                HAL_Delay(100);
                // 2. 逐步增加音量
                for (int vol = 140; vol <= 255; vol++) {
                    AK4493_SetVolume(&hak, vol);
                    HAL_Delay(50);
                }
                USB_LOG_INFO("AK4493 Volume ramped to 0dB (Maximum)\r\n");
                hak.IsInitialized = 1;
                ak4493_init_needed = 0; // Clear flag

                NJW1195A_SetAllVolumes(&hnjw, NJW1195A_dBToRegister(0.0));
                break;
            } else {
                USB_LOG_INFO("AK4493 Reg Init Failed. Retrying in 100ms...\r\n");
                HAL_Delay(100);
            }
        }

        if (ak4493_init_needed) {
            USB_LOG_INFO("AK4493 Init Failed after 5 attempts. Check I2S Clock/Connections.\r\n");
            ak4493_init_needed = 0; // Clear flag
            hak.IsInitialized = 0; // Clear flag 
        }
    }
}

/* Callback for USB Detection (called from EXTI) */
void Audio_USB_Detected(uint8_t detected)
{
    if(detected) {
        ak4493_init_needed = 1;
        USB_LOG_INFO("USB Interface Activated.\r\n");
    } else {
        USB_LOG_INFO("USB Interface Removed.\r\n");
        // Optionally de-init or mute here
    }
}
