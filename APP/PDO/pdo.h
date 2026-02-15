/**
 ******************************************************************************
 * @file    PDO.h
 * @brief   CH224Q Application Layer - PDO Management and Analysis
 * @version 2.0.0
 * @date    2024-02-16
 ******************************************************************************
 * @attention
 * 
 * High-level interface for CH224Q USB PD management
 * Includes PDO parsing, protocol detection, and power management
 * 
 ******************************************************************************
 */

#ifndef PDO_H
#define PDO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ch224q.h"
#include "usbpd_def.h"
#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * Configuration
 * ========================================================================== */
#define PDO_MAX_RESULTS         8       // Maximum number of PDO entries
#define PDO_RESULT_STR_LEN      32      // Length of each result string

/* ============================================================================
 * Type Definitions
 * ========================================================================== */

/**
 * @brief PDO entry information
 */
typedef struct {
    bool valid;                         // Entry is valid
    USBPD_PDO_Type_t type;             // PDO type
    uint16_t voltage_mv;                // Voltage in mV
    uint16_t max_current_ma;            // Maximum current in mA
    uint16_t min_voltage_mv;            // Minimum voltage (for PPS/AVS)
    uint16_t max_voltage_mv;            // Maximum voltage (for PPS/AVS)
    float power_w;                      // Calculated power in watts
    bool is_epr;                        // Is EPR (>20V)
    bool is_pps;                        // Is PPS capable
    bool is_avs;                        // Is AVS capable
    char description[PDO_RESULT_STR_LEN]; // Human-readable description
} PDO_Entry_t;

/**
 * @brief Complete PDO information structure
 */
typedef struct {
    // Raw PDO data
    uint8_t raw_data[CH224Q_PDO_DATA_SIZE];
    
    // Message headers
    USBPD_MessageHeader_t message_header;
    USBPD_ExtendedMessageHeader_t ext_header;
    
    // Parsed PDO entries
    PDO_Entry_t entries[PDO_MAX_RESULTS];
    uint8_t entry_count;                // Number of valid entries
    
    // Current operating parameters
    uint8_t current_pdo_index;          // Currently selected PDO (0xFF = none)
    uint16_t current_voltage_mv;        // Current voltage setting
    uint16_t available_current_ma;      // Available current at current voltage
    uint8_t current_power_w;            // Current power level in watts
    
    // Protocol status
    ch224q_protocol_t protocol;         // Active protocol
    bool pd_active;                     // PD negotiation successful
    bool epr_capable;                   // Adapter supports EPR
    bool pps_capable;                   // Adapter supports PPS
    bool avs_capable;                   // Adapter supports AVS
    
} PDO_Info_t;

/**
 * @brief Application context (for backward compatibility)
 */
typedef struct {
    ch224q_handle_t *ch224q_handle;     // CH224Q driver handle
    PDO_Info_t pdo_info;                // PDO information
    ch224q_voltage_t current_voltage;   // Current voltage setting
    bool initialized;                   // Initialization flag
} PDO_Context_t;

/* ============================================================================
 * Global Variables (for backward compatibility with original code)
 * ========================================================================== */
extern PDO_Context_t g_pdo_context;

/* ============================================================================
 * API Functions
 * ========================================================================== */

/**
 * @brief Initialize PDO management system
 * @param context Pointer to PDO context structure
 * @param ch224q_handle Initialized CH224Q driver handle
 * @return CH224Q_OK on success, error code otherwise
 */
ch224q_status_code_t PDO_Init(PDO_Context_t *context, ch224q_handle_t *ch224q_handle);

/**
 * @brief Read and parse PDO data from CH224Q
 * @param context Pointer to PDO context structure
 * @return CH224Q_OK on success, error code otherwise
 */
ch224q_status_code_t PDO_Read(PDO_Context_t *context);

/**
 * @brief Get current protocol status
 * @param context Pointer to PDO context structure
 * @return CH224Q_OK on success, error code otherwise
 */
ch224q_status_code_t PDO_GetStatus(PDO_Context_t *context);

/**
 * @brief Set voltage level
 * @param context Pointer to PDO context structure
 * @param voltage Desired voltage level
 * @return CH224Q_OK on success, error code otherwise
 */
ch224q_status_code_t PDO_SetVoltage(PDO_Context_t *context, ch224q_voltage_t voltage);

/**
 * @brief Get available power at specific voltage
 * @param context Pointer to PDO context structure
 * @param voltage_mv Voltage in mV
 * @param power_w Pointer to receive power in watts (can be NULL)
 * @param current_ma Pointer to receive current in mA (can be NULL)
 * @return CH224Q_OK if power available, error code otherwise
 */
ch224q_status_code_t PDO_GetPowerAtVoltage(PDO_Context_t *context,
                                            uint16_t voltage_mv,
                                            float *power_w,
                                            uint16_t *current_ma);

/**
 * @brief Find PDO entry by voltage
 * @param context Pointer to PDO context structure
 * @param voltage_mv Target voltage in mV
 * @return Pointer to PDO entry or NULL if not found
 */
const PDO_Entry_t* PDO_FindByVoltage(PDO_Context_t *context, uint16_t voltage_mv);

/**
 * @brief Print all available PDO entries (for debugging)
 * @param context Pointer to PDO context structure
 */
void PDO_PrintAll(PDO_Context_t *context);

/**
 * @brief Get PDO entry by index
 * @param context Pointer to PDO context structure
 * @param index PDO index (0-based)
 * @return Pointer to PDO entry or NULL if invalid
 */
const PDO_Entry_t* PDO_GetEntry(PDO_Context_t *context, uint8_t index);

/**
 * @brief Check if adapter supports specific voltage
 * @param context Pointer to PDO context structure
 * @param voltage_mv Voltage to check in mV
 * @return true if supported, false otherwise
 */
bool PDO_IsVoltageSupported(PDO_Context_t *context, uint16_t voltage_mv);

/* ============================================================================
 * Backward Compatibility Functions (matching original API)
 * ========================================================================== */

/**
 * @brief Initialize CH224Q (simplified interface)
 * @note Uses global context
 */
void Ch224qInit(void);

/**
 * @brief Set voltage (simplified interface)
 * @param voltage Voltage level
 * @note Uses global context
 */
void Ch224qSetVolt(ch224q_voltage_t voltage);

/**
 * @brief Get PDO information (simplified interface)
 * @param pdo_info Pointer to receive PDO info
 * @note Uses global context
 */
void Ch224qGetPDO(PDO_Info_t *pdo_info);

/**
 * @brief Get status (simplified interface)
 * @param pdo_info Pointer to receive PDO info with status
 * @note Uses global context
 */
void Ch224qGetStatus(PDO_Info_t *pdo_info);

/**
 * @brief Legacy PDO_GET function for compatibility
 * @param handle CH224Q handle
 * @param pdo_info PDO info structure
 */
void PDO_GET(ch224q_handle_t *handle, PDO_Info_t *pdo_info);

/* ============================================================================
 * Utility Functions
 * ========================================================================== */

/**
 * @brief Convert voltage enum to millivolts
 * @param voltage Voltage enumeration
 * @return Voltage in mV
 */
uint16_t PDO_VoltageToMillivolts(ch224q_voltage_t voltage);

/**
 * @brief Get protocol name string
 * @param protocol Protocol enumeration
 * @return Protocol name string
 */
const char* PDO_GetProtocolName(ch224q_protocol_t protocol);

/**
 * @brief Get PDO type name string
 * @param type PDO type enumeration
 * @return PDO type name string
 */
const char* PDO_GetTypeName(USBPD_PDO_Type_t type);

#ifdef __cplusplus
}
#endif

#endif /* PDO_H */
