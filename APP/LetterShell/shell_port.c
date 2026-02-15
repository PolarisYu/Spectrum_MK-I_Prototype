#include "shell.h"
#include "cdc_acm_ringbuffer.h"
#include "main.h"
#include "njw1195a.h"
#include "ak4493.h"
#include <string.h>
#include <stdlib.h>

extern NJW1195A_HandleTypeDef hnjw;
extern AK4493_HandleTypeDef hak;

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
 * devices: amp, dac
 * state: 0-off, 1-on
 */
int setPower(int argc, char *argv[])
{
    if (argc != 3) {
        shellPrint(&shell, "usage: power [amp|dac] [0|1]\r\n");
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

/**
 * @brief Set NJW1195A Volume
 * usage: vol [channel|all] [value]
 * channel: 1-4 or 'all'
 * value: -95.0 to +31.5 (dB) or 0x00-0xFF (Register Hex)
 */
// int setChannelVolume(int argc, char *argv[])
// {
//     if (argc < 3) {
//         shellPrint(&shell, "Usage: vol [ch|all] [value]\r\n");
//         shellPrint(&shell, "  ch: 1-4 or 'all'\r\n");
//         shellPrint(&shell, "  value: dB (-95.0 to 31.5) or Hex (0x00-0xFF)\r\n");
//         return -1;
//     }

//     uint8_t regValue = 0;
//     float dbValue = 0.0f;
//     int isDb = 1;

//     // Check if value is hex
//     if (strstr(argv[2], "0x") || strstr(argv[2], "0X")) {
//         regValue = (uint8_t)strtol(argv[2], NULL, 16);
//         isDb = 0;
//     } else {
//         dbValue = strtof(argv[2], NULL);
//         regValue = NJW1195A_dBToRegister(dbValue);
//     }

//     if (strcmp(argv[1], "all") == 0) {
//         // Set all channels
//         if (NJW1195A_SetAllVolumes(&hnjw, regValue) == HAL_OK) {
//             if (isDb)
//                 shellPrint(&shell, "Set ALL channels to %.1f dB (0x%02X)\r\n", dbValue, regValue);
//             else
//                 shellPrint(&shell, "Set ALL channels to 0x%02X\r\n", regValue);
//         } else if (NJW1195A_SetAllVolumes(&hnjw, regValue) == HAL_BUSY) {
//             shellPrint(&shell, "Failed to set volume Busy\r\n");    
//         } else {
//             shellPrint(&shell, "Failed to set volume Error\r\n");
//         }
//     } else {
//         // Set specific channel
//         int ch = atoi(argv[1]);
//         if (ch < 1 || ch > 4) {
//             shellPrint(&shell, "Invalid channel: %d (must be 1-4)\r\n", ch);
//             return -1;
//         }

//         // Map 1-4 to 0-3 (Driver defines CH1 as 0x00)
//         uint8_t channelReg = ch - 1;

//         if (NJW1195A_SetVolume_DMA(&hnjw, channelReg, regValue) == HAL_OK) {
//             if (isDb)
//                 shellPrint(&shell, "Set CH%d to %.1f dB (0x%02X) via DMA\r\n", ch, dbValue, regValue);
//             else
//                 shellPrint(&shell, "Set CH%d to 0x%02X via DMA\r\n", ch, regValue);
//         } else {
//             shellPrint(&shell, "Failed to set volume (Busy/Error)\r\n");
//         }
//     }

//     return 0;
// }
// SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), vol, setChannelVolume, set volume control);

