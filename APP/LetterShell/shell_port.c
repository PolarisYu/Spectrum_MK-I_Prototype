#include "shell.h"
#include "cdc_acm_ringbuffer.h"
#include "main.h"
#include <string.h>
#include <stdlib.h>

#define SHELL_USB_BUSID 0

Shell shell;
char shell_buffer[512];

/**
 * @brief Shell write function for CDC ACM
 * 
 * @param data Data to write
 * @param len Data length
 * @return signed short Bytes written
 */
signed short userShellWrite(char *data, unsigned short len)
{
    return (signed short)cdc_acm_send_data(SHELL_USB_BUSID, (uint8_t *)data, len);
}

/**
 * @brief Shell read function for CDC ACM
 * 
 * @param data Buffer to store read data
 * @param len Buffer length
 * @return signed short Bytes read
 */
signed short userShellRead(char *data, unsigned short len)
{
    return (signed short)cdc_acm_read_data((uint8_t *)data, len);
}

/**
 * @brief Initialize the shell
 * 
 */
void userShellInit(void)
{
    shell.write = userShellWrite;
    shell.read = userShellRead;
    shellInit(&shell, shell_buffer, 512);
}

/**
 * @brief USB Receive Callback
 * This function is called from USB interrupt context
 */
void cdc_acm_data_rx_callback(uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        shellHandler(&shell, data[i]);
    }
}

/**
 * @brief Test command
 */
int testFunc(int argc, char *argv[])
{
    shellPrint(&shell, "Hello World from LetterShell!\r\n");
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), hello, testFunc, test command);

/**
 * @brief Set Power State
 * usage: power [device] [state]
 * devices: amp, dac, led
 * state: 0-off, 1-on
 */
int setPower(int argc, char *argv[])
{
    if (argc != 3) {
        shellPrint(&shell, "usage: power [amp|dac|led] [0|1]\r\n");
        return -1;
    }

    GPIO_TypeDef *port = NULL;
    uint16_t pin = 0;
    int state = atoi(argv[2]);

    if (strcmp(argv[1], "amp") == 0) {
        // AMP_PW_EN
        port = AMP_PW_EN_GPIO_Port;
        pin = AMP_PW_EN_Pin;
    } else if (strcmp(argv[1], "dac") == 0) {
        // DAC_PW_EN
        port = DAC_PW_EN_GPIO_Port;
        pin = DAC_PW_EN_Pin;
    } else if (strcmp(argv[1], "led") == 0) {
        // SYS_STATUS_LED
        port = SYS_STATUS_LED_GPIO_Port;
        pin = SYS_STATUS_LED_Pin;
    } else {
        shellPrint(&shell, "unknown device: %s\r\n", argv[1]);
        return -1;
    }

    if (port) {
        HAL_GPIO_WritePin(port, pin, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
        shellPrint(&shell, "Set %s to %s\r\n", argv[1], state ? "ON" : "OFF");
    }
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), power, setPower, set power control);
