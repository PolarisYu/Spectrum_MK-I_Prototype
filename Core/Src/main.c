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
#include "usart.h"
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
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define u_busid 0
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
AK4493_HandleTypeDef hak4493;
NJW1195A_HandleTypeDef hnjw;
volatile uint8_t ak4493_init_needed = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief Scans the I2C bus for devices and prints the results via SEGGER RTT.
 * @param hi2c: Pointer to a I2C_HandleTypeDef structure that contains
 * the configuration information for the specified I2C.
 */
void I2C_ScanBus(I2C_HandleTypeDef *hi2c) {
    HAL_StatusTypeDef status;
    uint8_t i;
    uint8_t found_count = 0;

    printf("\r\n--- Starting I2C Bus Scan ---\r\n");
    printf("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\r\n");

    for (i = 0; i < 128; i++) {
        // Print row header every 16 addresses
        if (i % 16 == 0) {
            printf("%02X: ", i);
        }

        /* * Use HAL_I2C_IsDeviceReady to probe the address.
         * Note: The address is shifted left by 1 for the 8-bit format.
         */
        status = HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(i << 1), 3, 10);

        if (status == HAL_OK) {
            printf("%02X ", i); // Device found, print 7-bit hex address
            found_count++;
        } else {
            printf("-- "); // No response
        }

        // New line after every 16 probes
        if (i % 16 == 15) {
            printf("\r\n");
        }
    }

    printf("--- Scan Finished. Found %d device(s) ---\r\n\r\n", found_count);
}

/**
  * @brief USB Log (Retargets the C library printf function to the USART)
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
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  cdc_acm_init(u_busid, USB_BASE);
  userShellInit();

  // 1. 配置 I2C 句柄
  hak4493.hi2c = &hi2c2;
  
  // 2. 配置 I2C 地址 (默认是 0x26)
  hak4493.DevAddress = AK4493_DEFAULT_ADDR;
  
  // 3. 配置 PDN 复位引脚 (根据 main.h 中的定义)
  hak4493.PDN_Port = DAC_PDN_GPIO_Port;
  hak4493.PDN_Pin = DAC_PDN_Pin;
  hak4493.PW_EN_Port = DAC_PW_EN_GPIO_Port;
  hak4493.PW_EN_Pin = DAC_PW_EN_Pin;

    // 4. 初始化 DAC (Power On only)
  AK4493_PowerOn(&hak4493);
  printf("AK4493 Power On. Waiting for USB/I2S...\r\n");

  /* Register Init is now handled in EXTI callback and main loop */

  // Initialize NJW1195A
  hnjw.hspi = &hspi2; // 使用 SPI2
  hnjw.LatchPort = SPI2_LATCH_GPIO_Port; // PB12 (来自 main.h)
  hnjw.LatchPin = SPI2_LATCH_Pin;

  hnjw.PW_EN_Port = AMP_PW_EN_GPIO_Port;
  hnjw.PW_EN_Pin = AMP_PW_EN_Pin;
  
  NJW1195A_Init(&hnjw);

  // 设置初始音量 (例如设置为 -10dB)
  // 10dB / 0.5dB = 20 (0x14)
  NJW1195A_SetAllLevels_DMA(&hnjw, 0x14);

  // HAL_GPIO_WritePin(AMP_PW_EN_GPIO_Port, AMP_PW_EN_Pin, GPIO_PIN_SET);
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      if (ak4493_init_needed) {
          printf("AK4493 Init Request Detected. Waiting for clock...\r\n");
          HAL_Delay(1000); // Initial wait for USB/I2S clock

          // Scan I2C bus
          I2C_ScanBus(hak4493.hi2c);

          // Retry logic: Try 5 times
          int i;
          for (i = 0; i < 5; i++) {
              printf("AK4493 Reg Init Attempt %d/5...\r\n", i + 1);
              if (AK4493_RegInit(&hak4493) == HAL_OK) {
                  printf("AK4493 Reg Init Success!\r\n");
                  AK4493_SetVolume(&hak4493, 2);
                  ak4493_init_needed = 0; 
                  break; 
              } else {
                  printf("AK4493 Reg Init Failed. Retrying in 100ms...\r\n");
                  HAL_Delay(100);
              }
          }

          if (ak4493_init_needed) {
              printf("AK4493 Init Failed after 5 attempts. Check I2S Clock/Connections.\r\n");
              ak4493_init_needed = 0; // Clear flag
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_2) // PB2 (USB/I2S Detected)
  {
      ak4493_init_needed = 1;
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
