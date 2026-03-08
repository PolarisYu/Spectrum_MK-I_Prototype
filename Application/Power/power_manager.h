#ifndef __POWER_MANAGER_H__
#define __POWER_MANAGER_H__

#include <stdint.h>
#include <stdbool.h>

/* Exported Functions */

/**
 * @brief Initialize the Power Management System (CH224Q + PDO)
 */
void Power_Init(void);

/**
 * @brief Power Management Task (Call this in main loop)
 *        Handles status polling and protocol detection
 */
void Power_Task(void);

/**
 * @brief Request a specific voltage from the PD charger
 * @param voltage_mv Target voltage in millivolts (e.g., 5000, 9000, 12000, 15000, 20000)
 *                   If PPS is supported, allows fine tuning.
 */
void Power_SetVoltage(uint16_t voltage_mv);

/**
 * @brief Print current power status and negotiated PDO to log
 */
void Power_PrintStatus(void);

#endif /* __POWER_MANAGER_H__ */
