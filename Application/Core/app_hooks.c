#include "Segger_RTT.h"
#include "audio_manager.h"
#include "main.h"
#include "system_protection.h"
#include "usb_config.h"
#include "usb_log.h"
#include <stdio.h>

/**
 * @brief  EXTI Line Callback.
 * @param  GPIO_Pin: Specifies the pins connected to the EXTI line.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_0) // PC0 (VBUS_PG)
    {
        System_PowerDown_Handler();
    }
    if (GPIO_Pin == GPIO_PIN_2) // PB2 (USB/I2S Detected)
    {
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2) == GPIO_PIN_SET)
        {
            Audio_USB_Detected(1);
        }
        else
        {
            Audio_USB_Detected(0);
        }
    }
    if (GPIO_Pin == GPIO_PIN_6) // PB6 (Input Selector)
    {
        // When EXTI interrupt occurs, read the current pin state
        GPIO_PinState currentState = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6);

        if (currentState == GPIO_PIN_SET)
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

        if (currentState == GPIO_PIN_SET)
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

/**
 * @brief  Tx Transfer completed callback.
 * @param  hspi: pointer to a SPI_HandleTypeDef structure that contains
 *               the configuration information for SPI module.
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI2) // 确认是连接 NJW1195A 的 SPI 接口
    {
        NJW1195A_TxCpltCallback(&hnjw);
    }
}
