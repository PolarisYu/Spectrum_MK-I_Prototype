/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "cordic.h"
#include "crc.h"
#include "dma.h"
#include "fmac.h"
#include "i2c.h"
#include "rng.h"
#include "rtc.h"
#include "sai.h"
#include "spi.h"
#include "tim.h"
#include "usb.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "cdc_acm_ringbuffer.h"
#include "Segger_RTT.h"
#include "shell.h"
#include "shell_port.h"
#include "ak4493.h"
#include "njw1195a.h"
#include "ct7302.h"

/* Debug Tag for System */
#define USB_DBG_TAG "SYS"

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

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define u_busid 0

// 设置系统状态灯亮度 (0-1000)
#define SET_STATUS_LED_BRIGHTNESS(x)  __HAL_TIM_SET_COMPARE(&htim15, TIM_CHANNEL_1, (x))

// 设置数据灯亮度 (0-1000)
#define SET_DATA_LED_BRIGHTNESS(x)    __HAL_TIM_SET_COMPARE(&htim15, TIM_CHANNEL_2, (x))

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
AK4493_HandleTypeDef hak;
volatile uint8_t ak4493_init_needed = 0;
NJW1195A_HandleTypeDef hnjw;
CT7302_HandleTypeDef hct;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief RTT Log (Retargets the C library printf function to the Segger RTT)
  */
#if defined(__CC_ARM)      // Keil
int fputc(int ch, FILE *f)
{
  SEGGER_RTT_Write(0, (const char *)&ch, 1);
  return ch;
}
#elif defined(__GNUC__)   // GCC
int _write(int fd, char *ptr, int len)  
{  
  SEGGER_RTT_Write(0, ptr, len);
  return len;
  // HAL_Delay();
}
#endif

/**
 * @brief Scans the I2C bus for devices and prints the results via SEGGER RTT.
 * @param hi2c: Pointer to a I2C_HandleTypeDef structure that contains
 * the configuration information for the specified I2C.
 */
// void I2C_ScanBus(I2C_HandleTypeDef *hi2c) {
//     HAL_StatusTypeDef status;
//     uint8_t i;
//     uint8_t found_count = 0;

//     USB_LOG_INFO("--- Starting I2C Bus Scan ---\r\n");
//     USB_LOG_INFO("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\r\n");

//     for (i = 0; i < 128; i++) {
//         // Print row header every 16 addresses
//         if (i % 16 == 0) {
//             USB_LOG_INFO("%02X: ", i);
//         }

//         /* * Use HAL_I2C_IsDeviceReady to probe the address.
//          * Note: The address is shifted left by 1 for the 8-bit format.
//          */
//         status = HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(i << 1), 3, 10);

//         if (status == HAL_OK) {
//             USB_LOG_INFO("%02X ", i); // Device found, print 7-bit hex address
//             found_count++;
//         } else {
//             USB_LOG_INFO("-- "); // No response
//         }

//         // New line after every 16 probes
//         if (i % 16 == 15) {
//             USB_LOG_INFO("\r\n");
//         }
//     }

//     USB_LOG_INFO("--- Scan Finished. Found %d device(s) ---\n", found_count);
// }

/**
 * @brief Process the status LED breathing effect.
 * @details This function controls the brightness of the status LED to simulate
 * a breathing effect. The brightness increases and decreases smoothly, creating
 * a natural breathing feel.
 */
void Process_Status_LED_Breathing(void) {
    static uint32_t last_tick = 0;
    static int16_t brightness = 0;
    static int8_t direction = 10; // 亮度变化步进

    // 每 10ms 更新一次亮度，产生平滑动画
    if (HAL_GetTick() - last_tick > 10) {
        last_tick = HAL_GetTick();

        brightness += direction;

        // 到达最亮或最暗时反转方向
        if (brightness >= 1000) {
            brightness = 1000;
            direction = -10; // 变暗
        } else if (brightness <= 0) {
            brightness = 0;
            direction = 10;  // 变亮
            // 可在此处添加额外延时，让灯灭一小会儿再亮，模拟人的呼吸停顿
        }

        SET_STATUS_LED_BRIGHTNESS(brightness);
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  SEGGER_RTT_Init();
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_CORDIC_Init();
  MX_CRC_Init();
  MX_RNG_Init();
  MX_RTC_Init();
  MX_SPI1_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_TIM15_Init();
  MX_ADC1_Init();
  MX_SAI1_Init();
  MX_I2C2_Init();
  MX_SPI3_Init();
  MX_FMAC_Init();
  MX_SPI2_Init();
  MX_I2C3_Init();
  /* USER CODE BEGIN 2 */
  HAL_Delay(1000);

  /* 启动 TIM15 的两个通道 */
  HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_2);

  HAL_GPIO_WritePin(SE_AMP_EN_GPIO_Port, SE_AMP_EN_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(BAL_AMP_EN_GPIO_Port, BAL_AMP_EN_Pin, GPIO_PIN_RESET);

  cdc_acm_init(u_busid, USB_BASE);
  userShellInit();

  hct.hi2c = &hi2c3;
  CT7302_Init(&hct);

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

  /* AK4493 Register Init is now handled in EXTI callback and main loop */

  HAL_Delay(1000);

  // Initialize NJW1195A
  hnjw.hspi = &hspi2; // 使用 SPI2
  hnjw.LatchPort = SPI2_LATCH_GPIO_Port; // PB12 (来自 main.h)
  hnjw.LatchPin = SPI2_LATCH_Pin;

  hnjw.PW_EN_Port = AMP_PW_EN_GPIO_Port;
  hnjw.PW_EN_Pin = AMP_PW_EN_Pin;
  
  hnjw.SE_EN_Port = SE_AMP_EN_GPIO_Port;
  hnjw.SE_EN_Pin = SE_AMP_EN_Pin;
  hnjw.BAL_EN_Port = BAL_AMP_EN_GPIO_Port;
  hnjw.BAL_EN_Pin = BAL_AMP_EN_Pin;
  
  NJW1195A_Init(&hnjw);

  // Set initial volume to -10dB
  // 10dB / 0.5dB = 20 (0x14)
  // NJW1195A_SetVolume_DMA(&hnjw, 0x01, NJW1195A_dBToRegister(-10.0));
  NJW1195A_SetAllVolumes(&hnjw, NJW1195A_dBToRegister(15.0));

  // HAL_GPIO_WritePin(AMP_PW_EN_GPIO_Port, AMP_PW_EN_Pin, GPIO_PIN_SET);
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      /* 处理系统呼吸灯 */
      Process_Status_LED_Breathing();

      if (ak4493_init_needed) {
          USB_LOG_INFO("AK4493 Init Request Detected. Waiting for clock...\r\n");
          HAL_Delay(1000); // Initial wait for USB/I2S clock

          // Scan I2C bus
          // I2C_ScanBus(hak.hi2c);

          // Retry logic: Try 5 times
          int i;
          for (i = 0; i < 5; i++) {
              USB_LOG_INFO("AK4493 Reg Init Attempt %d/5...\r\n", i + 1);
              if (AK4493_RegInit(&hak) == HAL_OK) {
                  USB_LOG_INFO("AK4493 Reg Init Success!\r\n");
                  HAL_Delay(10);
                  AK4493_SetVolume(&hak, 130); 
                  AK4493_SetMute(&hak, 0);           // 解除静音
                  HAL_Delay(100);
                  // 2. 逐步增加音量
                  for (int vol = 130; vol <= 170; vol++) {
                      AK4493_SetVolume(&hak, vol);
                      HAL_Delay(50);
                  }
                  USB_LOG_INFO("AK4493 Volume ramped to 0dB (Maximum)\r\n");
                  hak.IsInitialized = 1;
                  ak4493_init_needed = 0; // Clear flag
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
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // shellTask(&shell); // Shell is now interrupt-driven
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_LSI
                              |RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV3;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_0) // PC0 (VBUS_PG)
  {
    // USB_LOG_INFO("Power Down Interrupt Triggered!\r\n");
    /* 1. 预先设置低电平：
     * 虽然现在引脚还是 EXTI 输入模式，但这行代码会提前把 0 写进 ODR (输出数据寄存器)。
     * 这样在下一步切换为输出模式的瞬间，引脚会毫无缝隙地直接输出低电平。
     */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_11, GPIO_PIN_RESET);

    /* 2. 重新配置为推挽输出 (Push-Pull) */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;        // 强推挽输出
    GPIO_InitStruct.Pull = GPIO_NOPULL;                // 不需要内部上下拉
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH; // 最高响应速度
    
    // 瞬间强行接管控制权，运放被强制 MUTE
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* 3. 保险起见，再次明确输出低电平 */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_11, GPIO_PIN_RESET);

    /* 4. 打印掉电日志 (注意：因为马上就没电了，这句Log不一定能完整发完到电脑上) */
    USB_LOG_INFO("Power Down Interrupt Triggered! AMP forcefully MUTED.\r\n");

    /* 5. 【终极防御：死亡循环】
     * 此时运放已经安全关闭，正负电源还在正常工作。
     * 我们必须把 CPU 永远卡在这里，防止它退回主循环去执行其他可能导致电平翻转的代码，
     * 直到几毫秒/几十毫秒后电源彻底耗尽，MCU 物理关机。
     */
    while (1)
    {
        // 静静等待系统断电...
    }
  }
  if (GPIO_Pin == GPIO_PIN_2) // PB2 (USB/I2S Detected)
  {
    if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2) == GPIO_PIN_SET)
    {
      ak4493_init_needed = 1;
      USB_LOG_INFO("USB Interface Activated.\r\n");
    }
    else
    {
      USB_LOG_INFO("USB Interface Removed.\r\n");
    }
  }
  if (GPIO_Pin == GPIO_PIN_6) // PB6 (Input Selector)
  {
    // When EXTI interrupt occurs, read the current pin state
    GPIO_PinState currentState = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6);
    
    if(currentState == GPIO_PIN_SET)
    {
      /* Rising Edge */
      USB_LOG_INFO("BAL Plugged In Detected.\r\n");
    }
    else
    {
      /* Falling Edge */
      USB_LOG_INFO("BAL Plugged Out Detected.\r\n");
    }
  }
  if (GPIO_Pin == GPIO_PIN_11) // PB7 (BAL Selector)
  {
    // When EXTI interrupt occurs, read the current pin state
    GPIO_PinState currentState = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11);
    
    if(currentState == GPIO_PIN_SET)
    {
      /* Rising Edge */
      USB_LOG_INFO("SE Plugged In Detected.\r\n");
    }
    else
    {
      /* Falling Edge */
      USB_LOG_INFO("SE Plugged Out Detected.\r\n");
    }
  }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi->Instance == SPI2) // 确认是连接 NJW1195A 的 SPI 接口
  {
    NJW1195A_TxCpltCallback(&hnjw);
  }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
