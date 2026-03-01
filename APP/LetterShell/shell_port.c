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
 * @brief Set AK4493 Volume
 * usage: akvol [value]
 * value: dB (-127.0 to 0.0) or Hex/Int (0-255, 255/0xFF=Max, 0/0x00=Mute)
 */
int setAK4493Volume(int argc, char *argv[])
{
    if (argc != 2) {
        shellPrint(&shell, "Usage: akvol [dB | raw]\r\n");
        shellPrint(&shell, "  dB:  -127.0 to 0.0 (e.g. -20)\r\n");
        shellPrint(&shell, "  raw: 0 to 255 (0=Mute, 255=0dB)\r\n");
        return -1;
    }

    // Check if dB mode (contains '.' or '-')
    bool isDbMode = (strchr(argv[1], '.') != NULL) || (strchr(argv[1], '-') != NULL);

    if (isDbMode) {
        char *endptr;
        float dbValue = strtof(argv[1], &endptr);
        
        if (endptr == argv[1]) {
            shellPrint(&shell, "Invalid input format.\r\n");
            return -1;
        }

        if (dbValue > 0.0f || dbValue < -127.5f) {
            shellPrint(&shell, "Error: dB value out of range (-127.5 to 0.0)\r\n");
            return -1;
        }

        if (AK4493_SetVolume_dB(&hak, dbValue) == HAL_OK) {
            shellPrint(&shell, "Set dB: %d\r\n", (int)dbValue);
        } else {
            if (hak.IsInitialized) {
                shellPrint(&shell, "Failed to set volume.\r\n");
            } else {
                shellPrint(&shell, "AK4493 not initialized.\r\n");
            }
        }
    } 
    else {
        // Raw Register Mode
        char *endptr;
        long regLong = strtol(argv[1], &endptr, 0); // Auto-detect 0x or decimal
        
        if (endptr == argv[1]) {
            shellPrint(&shell, "Invalid input format.\r\n");
            return -1;
        }
        
        if (regLong < 0 || regLong > 255) {
            shellPrint(&shell, "Error: Raw value out of range (0-255)\r\n");
            return -1;
        }

        uint8_t regValue = (uint8_t)regLong;
        
        // Warning for Mute
        if (regValue == 0) {
            shellPrint(&shell, "Warning: Raw 0 = MUTE! Use 255 for 0dB.\r\n");
        }

        if (AK4493_SetVolume(&hak, regValue) == HAL_OK) {
            shellPrint(&shell, "Set Reg: 0x%02X\r\n", regValue);
        } else {
            if (hak.IsInitialized) {
                shellPrint(&shell, "Failed to set volume. Please check connection.\r\n");
            } else {
                shellPrint(&shell, "AK4493 not initialized.\r\n");
            }
        }
    }
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), akvol, setAK4493Volume, set AK4493 volume);

/**
 * @brief Set AK4493 Volume with separate L/R control
 * usage: akvol_lr [left_vol] [right_vol]
 * value: dB (-127.0 to 0.0) or Hex/Int (0-255, 255/0xFF=Max, 0/0x00=Mute)
 */
int setAK4493VolumeLR(int argc, char *argv[])
{
    if (argc != 3) {
        shellPrint(&shell, "Usage: akvol_lr [left_vol] [right_vol]\r\n");
        shellPrint(&shell, "  left_vol:  dB (-127.0 to 0.0) or Raw (0-255)\r\n");
        shellPrint(&shell, "  right_vol: dB (-127.0 to 0.0) or Raw (0-255)\r\n");
        return -1;
    }

    uint8_t vols[2]; // 0: Left, 1: Right

    for (int i = 0; i < 2; i++) {
        char *arg = argv[i + 1];
        bool isDb = (strchr(arg, '.') != NULL) || (strchr(arg, '-') != NULL);
        char *endptr;

        if (isDb) {
            float db = strtof(arg, &endptr);
            if (endptr == arg) {
                shellPrint(&shell, "Invalid format for %s channel: %s\r\n", i ? "Right" : "Left", arg);
                return -1;
            }
            if (db > 0.0f || db < -127.5f) {
                shellPrint(&shell, "Error: %s dB value out of range (-127.5 to 0.0)\r\n", i ? "Right" : "Left");
                return -1;
            }
            vols[i] = AK4493_dBToAttenuation(db);
        } else {
            long val = strtol(arg, &endptr, 0);
            if (endptr == arg) {
                shellPrint(&shell, "Invalid format for %s channel: %s\r\n", i ? "Right" : "Left", arg);
                return -1;
            }
            if (val < 0 || val > 255) {
                shellPrint(&shell, "Error: %s Raw value out of range (0-255)\r\n", i ? "Right" : "Left");
                return -1;
            }
            vols[i] = (uint8_t)val;
        }
    }

    if (AK4493_SetVolumeLR(&hak, vols[0], vols[1]) == HAL_OK) {
        shellPrint(&shell, "Set L: 0x%02X, R: 0x%02X\r\n", vols[0], vols[1]);
    } else {
        if (hak.IsInitialized) {
            shellPrint(&shell, "Failed to set volume. Please check connection.\r\n");
        } else {
            shellPrint(&shell, "AK4493 not initialized.\r\n");
        }
    }
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), akvol_lr, setAK4493VolumeLR, set AK4493 volume LR independent);

/**
 * usage: akdump
 */
int getAK4493Status(int argc, char *argv[])
{
    if (AK4493_DumpRegisters(&hak) == HAL_OK) {
        shellPrint(&shell, "AK4493 Registers Dumped to Log.\r\n");
    } else {
        if (hak.IsInitialized) {
            shellPrint(&shell, "Failed to dump AK4493 registers. Please check connection.\r\n");
        } else {
            shellPrint(&shell, "AK4493 not initialized.\r\n");
        }
    }
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), akdump, getAK4493Status, dump ak4493 registers);

/**
 * @brief Set AK4493 Digital Filter
 * usage: akfilter [0-5]
 */
int setAK4493Filter(int argc, char *argv[])
{
    const char *filter_names[] = {
        "Sharp Roll-off",
        "Slow Roll-off",
        "Short Delay Sharp Roll-off (Default)",
        "Short Delay Slow Roll-off",
        "Super Slow Roll-off",
        "Low Dispersion Short Delay"
    };

    if (argc != 2) {
        shellPrint(&shell, "Usage: akfilter [ID]\r\n");
        shellPrint(&shell, "Available Filters:\r\n");
        for (int i = 0; i < 6; i++) {
            shellPrint(&shell, "  %d : %s\r\n", i, filter_names[i]);
        }
        return -1;
    }

    int filterId = atoi(argv[1]);

    if (filterId < 0 || filterId > 5) {
        shellPrint(&shell, "Error: Invalid Filter ID. Use 0-5.\r\n");
        return -1;
    }

    if (AK4493_SetFilter(&hak, (AK4493_FilterTypeDef)filterId) == HAL_OK) {
        shellPrint(&shell, "AK4493 Filter set to: [%d] %s\r\n", filterId, filter_names[filterId]);
    } else {
        if (hak.IsInitialized) {
            shellPrint(&shell, "Failed to set AK4493 filter. Please check connection.\r\n");
        } else {
            shellPrint(&shell, "AK4493 not initialized.\r\n");
        }
    }

    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), akfilter, setAK4493Filter, set AK4493 digital filter);

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

