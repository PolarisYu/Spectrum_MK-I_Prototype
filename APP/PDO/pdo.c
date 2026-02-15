/**
 ******************************************************************************
 * @file    PDO.c
 * @brief   CH224Q Application Layer - PDO Management Implementation
 * @version 2.0.0
 * @date    2024-02-16
 ******************************************************************************
 */

#include "PDO.h"
#include <string.h>
#include <stdio.h>

// Optional: Include SEGGER_RTT for debugging
#if defined(USE_SEGGER_RTT)
    #include "SEGGER_RTT.h"
    #define PDO_LOG(...)    SEGGER_RTT_printf(0, __VA_ARGS__)
#else
    #define PDO_LOG(...)    // No-op if RTT not available
#endif

/* ============================================================================
 * Global Context (for simplified API)
 * ========================================================================== */
PDO_Context_t g_pdo_context = {0};

/* ============================================================================
 * Voltage Mapping Table
 * ========================================================================== */
static const uint16_t voltage_map[] = {
    5000,   // CH224Q_5V
    9000,   // CH224Q_9V
    12000,  // CH224Q_12V
    15000,  // CH224Q_15V
    20000,  // CH224Q_20V
    28000,  // CH224Q_28V
    0,      // CH224Q_PPS (variable)
    0       // CH224Q_AVS (variable)
};

/* ============================================================================
 * Private Helper Functions
 * ========================================================================== */

/**
 * @brief Parse Fixed Supply PDO
 */
static void parse_fixed_pdo(USBPD_SourcePDO_t *pdo, PDO_Entry_t *entry)
{
    entry->type = PDO_TYPE_FIXED_SUPPLY;
    entry->voltage_mv = POWER_DECODE_50MV(pdo->SourceFixedSupplyPDO.VoltageIn50mVunits);
    entry->max_current_ma = POWER_DECODE_10MA(pdo->SourceFixedSupplyPDO.MaxCurrentIn10mAunits);
    entry->min_voltage_mv = entry->voltage_mv;
    entry->max_voltage_mv = entry->voltage_mv;
    entry->power_w = POWER_CALC_WATTS(entry->voltage_mv, entry->max_current_ma);
    entry->is_epr = POWER_IS_EPR(entry->voltage_mv);
    entry->is_pps = false;
    entry->is_avs = false;
    entry->valid = true;

    // Format description
    snprintf(entry->description, PDO_RESULT_STR_LEN,
             "%s %dV %.2fA %.1fW",
             entry->is_epr ? "EPR" : "SPR",
             entry->voltage_mv / 1000,
             entry->max_current_ma / 1000.0f,
             entry->power_w);
}

/**
 * @brief Parse Battery Supply PDO
 */
static void parse_battery_pdo(USBPD_SourcePDO_t *pdo, PDO_Entry_t *entry)
{
    entry->type = PDO_TYPE_BATTERY;
    entry->min_voltage_mv = POWER_DECODE_50MV(pdo->SourceBatterySupplyPDO.MinVoltageIn50mVunits);
    entry->max_voltage_mv = POWER_DECODE_50MV(pdo->SourceBatterySupplyPDO.MaxVoltageIn50mVunits);
    entry->voltage_mv = entry->max_voltage_mv; // Use max as representative
    
    uint16_t max_power_mw = POWER_DECODE_250MW(pdo->SourceBatterySupplyPDO.MaxAllowablePowerIn250mWunits);
    entry->power_w = max_power_mw / 1000.0f;
    
    // Estimate current at max voltage
    entry->max_current_ma = (uint16_t)((max_power_mw * 1000.0f) / entry->max_voltage_mv);
    
    entry->is_epr = POWER_IS_EPR(entry->max_voltage_mv);
    entry->is_pps = false;
    entry->is_avs = false;
    entry->valid = true;

    snprintf(entry->description, PDO_RESULT_STR_LEN,
             "Battery %d-%dV %.1fW",
             entry->min_voltage_mv / 1000,
             entry->max_voltage_mv / 1000,
             entry->power_w);
}

/**
 * @brief Parse Variable Supply PDO
 */
static void parse_variable_pdo(USBPD_SourcePDO_t *pdo, PDO_Entry_t *entry)
{
    entry->type = PDO_TYPE_VARIABLE_SUPPLY;
    entry->min_voltage_mv = POWER_DECODE_50MV(pdo->SourceVariableSupplyPDO.MinVoltageIn50mVunits);
    entry->max_voltage_mv = POWER_DECODE_50MV(pdo->SourceVariableSupplyPDO.MaxVoltageIn50mVunits);
    entry->voltage_mv = entry->max_voltage_mv; // Use max as representative
    entry->max_current_ma = POWER_DECODE_10MA(pdo->SourceVariableSupplyPDO.MaxCurrentIn10mAunits);
    entry->power_w = POWER_CALC_WATTS(entry->max_voltage_mv, entry->max_current_ma);
    entry->is_epr = POWER_IS_EPR(entry->max_voltage_mv);
    entry->is_pps = false;
    entry->is_avs = false;
    entry->valid = true;

    snprintf(entry->description, PDO_RESULT_STR_LEN,
             "Variable %d-%dV %.2fA",
             entry->min_voltage_mv / 1000,
             entry->max_voltage_mv / 1000,
             entry->max_current_ma / 1000.0f);
}

/**
 * @brief Parse SPR PPS APDO
 */
static void parse_spr_pps_apdo(USBPD_SourcePDO_t *pdo, PDO_Entry_t *entry)
{
    entry->type = PDO_TYPE_APDO;
    entry->min_voltage_mv = POWER_DECODE_100MV(pdo->SourceSPRProgrammablePowerAPDO.MinVoltageIn100mVunits);
    entry->max_voltage_mv = POWER_DECODE_100MV(pdo->SourceSPRProgrammablePowerAPDO.MaxVoltageIn100mVunits);
    entry->voltage_mv = entry->max_voltage_mv; // Use max as representative
    entry->max_current_ma = POWER_DECODE_50MA(pdo->SourceSPRProgrammablePowerAPDO.MaxCurrentIn50mAunits);
    entry->power_w = POWER_CALC_WATTS(entry->max_voltage_mv, entry->max_current_ma);
    entry->is_epr = false; // PPS is SPR only
    entry->is_pps = true;
    entry->is_avs = false;
    entry->valid = true;

    snprintf(entry->description, PDO_RESULT_STR_LEN,
             "PPS %d-%dV %.2fA",
             entry->min_voltage_mv / 1000,
             entry->max_voltage_mv / 1000,
             entry->max_current_ma / 1000.0f);
}

/**
 * @brief Parse EPR AVS APDO
 */
static void parse_epr_avs_apdo(USBPD_SourcePDO_t *pdo, PDO_Entry_t *entry)
{
    entry->type = PDO_TYPE_APDO;
    entry->min_voltage_mv = POWER_DECODE_100MV(pdo->SourceEPRAdjustableVoltageAPDO.MinVoltageIn100mVunits);
    entry->max_voltage_mv = POWER_DECODE_100MV(pdo->SourceEPRAdjustableVoltageAPDO.MaxVoltageIn100mVunits);
    entry->voltage_mv = entry->max_voltage_mv;
    entry->max_current_ma = POWER_DECODE_50MA(pdo->SourceEPRAdjustableVoltageAPDO.MaxCurrentIn50mAunits);
    entry->power_w = POWER_CALC_WATTS(entry->max_voltage_mv, entry->max_current_ma);
    entry->is_epr = true;
    entry->is_pps = false;
    entry->is_avs = true;
    entry->valid = true;

    snprintf(entry->description, PDO_RESULT_STR_LEN,
             "AVS %d-%dV %.2fA",
             entry->min_voltage_mv / 1000,
             entry->max_voltage_mv / 1000,
             entry->max_current_ma / 1000.0f);
}

/**
 * @brief Parse single PDO entry
 */
static void parse_pdo_entry(uint32_t pdo_raw, PDO_Entry_t *entry)
{
    // Clear entry
    memset(entry, 0, sizeof(PDO_Entry_t));

    // Check for empty PDO
    if (pdo_raw == 0) {
        PDO_LOG("Empty PDO\n");
        return;
    }

    USBPD_SourcePDO_t pdo;
    pdo.d32 = pdo_raw;

    // Parse based on PDO type
    switch (pdo.General.PDO_Type) {
        case PDO_TYPE_FIXED_SUPPLY:
            parse_fixed_pdo(&pdo, entry);
            PDO_LOG("Fixed: %s\n", entry->description);
            break;

        case PDO_TYPE_BATTERY:
            parse_battery_pdo(&pdo, entry);
            PDO_LOG("Battery: %s\n", entry->description);
            break;

        case PDO_TYPE_VARIABLE_SUPPLY:
            parse_variable_pdo(&pdo, entry);
            PDO_LOG("Variable: %s\n", entry->description);
            break;

        case PDO_TYPE_APDO:
            // Check APDO sub-type
            if (pdo.General.SubTypeOrOtherUsage == APDO_TYPE_SPR_PPS) {
                parse_spr_pps_apdo(&pdo, entry);
                PDO_LOG("PPS: %s\n", entry->description);
            } else if (pdo.General.SubTypeOrOtherUsage == APDO_TYPE_EPR_AVS) {
                parse_epr_avs_apdo(&pdo, entry);
                PDO_LOG("AVS: %s\n", entry->description);
            } else {
                PDO_LOG("Unknown APDO subtype: %d\n", pdo.General.SubTypeOrOtherUsage);
            }
            break;

        default:
            PDO_LOG("Unknown PDO type: %d\n", pdo.General.PDO_Type);
            break;
    }
}

/**
 * @brief Analyze and parse all PDO entries
 */
static void analyze_pdo_data(PDO_Info_t *pdo_info)
{
    // Clear previous entries
    memset(pdo_info->entries, 0, sizeof(pdo_info->entries));
    pdo_info->entry_count = 0;
    pdo_info->pps_capable = false;
    pdo_info->avs_capable = false;
    pdo_info->epr_capable = false;

    // Parse message header
    pdo_info->message_header.d16 = *(uint16_t*)(pdo_info->raw_data);
    
    uint8_t pdo_count = pdo_info->message_header.MessageHeader.NumberOfDataObjects;
    
    if (pdo_count == 0) {
        PDO_LOG("No PDO objects found\n");
        return;
    }

    // Limit to maximum
    if (pdo_count > PDO_MAX_RESULTS) {
        pdo_count = PDO_MAX_RESULTS;
    }

    PDO_LOG("Parsing %d PDO entries:\n", pdo_count);

    // Parse each PDO (starts at byte 2)
    uint8_t *pdo_data = &pdo_info->raw_data[2];
    
    for (uint8_t i = 0; i < pdo_count; i++) {
        uint32_t pdo_raw = *(uint32_t*)(&pdo_data[i * 4]);
        
        PDO_LOG("[%d] ", i);
        parse_pdo_entry(pdo_raw, &pdo_info->entries[i]);
        
        if (pdo_info->entries[i].valid) {
            pdo_info->entry_count++;
            
            // Update capability flags
            if (pdo_info->entries[i].is_pps) {
                pdo_info->pps_capable = true;
            }
            if (pdo_info->entries[i].is_avs) {
                pdo_info->avs_capable = true;
            }
            if (pdo_info->entries[i].is_epr) {
                pdo_info->epr_capable = true;
            }
        }
    }

    PDO_LOG("Parsed %d valid PDO entries\n", pdo_info->entry_count);
}

/* ============================================================================
 * Public API Implementation
 * ========================================================================== */

/**
 * @brief Initialize PDO management
 */
ch224q_status_code_t PDO_Init(PDO_Context_t *context, ch224q_handle_t *ch224q_handle)
{
    if (context == NULL || ch224q_handle == NULL) {
        return CH224Q_ERROR_INVALID_PARAM;
    }

    memset(context, 0, sizeof(PDO_Context_t));
    context->ch224q_handle = ch224q_handle;
    context->current_voltage = CH224Q_5V;
    context->pdo_info.current_pdo_index = 0xFF; // No PDO selected
    context->initialized = true;

    return CH224Q_OK;
}

/**
 * @brief Read and parse PDO data
 */
ch224q_status_code_t PDO_Read(PDO_Context_t *context)
{
    if (context == NULL || !context->initialized) {
        return CH224Q_ERROR_INVALID_PARAM;
    }

    // Read raw PDO data from CH224Q
    ch224q_status_code_t ret = ch224q_read_pdo(context->ch224q_handle,
                                                context->pdo_info.raw_data,
                                                CH224Q_PDO_DATA_SIZE);
    
    if (ret != CH224Q_OK) {
        return ret;
    }

    // Analyze and parse PDO data
    analyze_pdo_data(&context->pdo_info);

    return CH224Q_OK;
}

/**
 * @brief Get protocol status
 */
ch224q_status_code_t PDO_GetStatus(PDO_Context_t *context)
{
    if (context == NULL || !context->initialized) {
        return CH224Q_ERROR_INVALID_PARAM;
    }

    // Read status from CH224Q
    ch224q_status_info_t status;
    ch224q_status_code_t ret = ch224q_get_status(context->ch224q_handle, &status);
    
    if (ret != CH224Q_OK) {
        return ret;
    }

    // Update PDO info
    context->pdo_info.protocol = status.protocol;
    context->pdo_info.pd_active = (status.pd_active || status.epr_active);

    // Read current if PD active
    if (context->pdo_info.pd_active) {
        ch224q_current_info_t current;
        ret = ch224q_get_current(context->ch224q_handle, &current);
        
        if (ret == CH224Q_OK) {
            context->pdo_info.available_current_ma = current.max_current_ma;
            
            // Calculate current power
            if (context->pdo_info.current_voltage_mv > 0) {
                context->pdo_info.current_power_w = (uint8_t)POWER_CALC_WATTS(
                    context->pdo_info.current_voltage_mv,
                    context->pdo_info.available_current_ma
                );
            }
        }
    }

    return CH224Q_OK;
}

/**
 * @brief Set voltage level
 */
ch224q_status_code_t PDO_SetVoltage(PDO_Context_t *context, ch224q_voltage_t voltage)
{
    if (context == NULL || !context->initialized) {
        return CH224Q_ERROR_INVALID_PARAM;
    }

    ch224q_status_code_t ret = ch224q_set_voltage(context->ch224q_handle, voltage);
    
    if (ret == CH224Q_OK) {
        context->current_voltage = voltage;
        
        // Update current voltage in mV
        if (voltage < CH224Q_PPS) {
            context->pdo_info.current_voltage_mv = voltage_map[voltage];
        }
    }

    return ret;
}

/**
 * @brief Get power at specific voltage
 */
ch224q_status_code_t PDO_GetPowerAtVoltage(PDO_Context_t *context,
                                            uint16_t voltage_mv,
                                            float *power_w,
                                            uint16_t *current_ma)
{
    if (context == NULL || !context->initialized) {
        return CH224Q_ERROR_INVALID_PARAM;
    }

    // Search for matching PDO
    for (uint8_t i = 0; i < context->pdo_info.entry_count; i++) {
        PDO_Entry_t *entry = &context->pdo_info.entries[i];
        
        if (!entry->valid) continue;

        // Check if voltage matches
        bool voltage_match = false;
        
        if (entry->type == PDO_TYPE_FIXED_SUPPLY) {
            voltage_match = (entry->voltage_mv == voltage_mv);
        } else {
            // Variable/PPS/AVS - check range
            voltage_match = (voltage_mv >= entry->min_voltage_mv && 
                           voltage_mv <= entry->max_voltage_mv);
        }

        if (voltage_match) {
            if (power_w) *power_w = entry->power_w;
            if (current_ma) *current_ma = entry->max_current_ma;
            return CH224Q_OK;
        }
    }

    return CH224Q_ERROR; // Voltage not found
}

/**
 * @brief Find PDO by voltage
 */
const PDO_Entry_t* PDO_FindByVoltage(PDO_Context_t *context, uint16_t voltage_mv)
{
    if (context == NULL || !context->initialized) {
        return NULL;
    }

    for (uint8_t i = 0; i < context->pdo_info.entry_count; i++) {
        PDO_Entry_t *entry = &context->pdo_info.entries[i];
        
        if (!entry->valid) continue;

        if (entry->type == PDO_TYPE_FIXED_SUPPLY) {
            if (entry->voltage_mv == voltage_mv) {
                return entry;
            }
        } else {
            if (voltage_mv >= entry->min_voltage_mv && 
                voltage_mv <= entry->max_voltage_mv) {
                return entry;
            }
        }
    }

    return NULL;
}

/**
 * @brief Print all PDO entries
 */
void PDO_PrintAll(PDO_Context_t *context)
{
    if (context == NULL || !context->initialized) {
        return;
    }

    PDO_LOG("\n=== PDO Information ===\n");
    PDO_LOG("Protocol: %s\n", PDO_GetProtocolName(context->pdo_info.protocol));
    PDO_LOG("Total PDOs: %d\n", context->pdo_info.entry_count);
    PDO_LOG("Capabilities: %s%s%s\n",
            context->pdo_info.epr_capable ? "EPR " : "",
            context->pdo_info.pps_capable ? "PPS " : "",
            context->pdo_info.avs_capable ? "AVS " : "");
    PDO_LOG("\nPDO Entries:\n");

    for (uint8_t i = 0; i < context->pdo_info.entry_count; i++) {
        if (context->pdo_info.entries[i].valid) {
            PDO_LOG("[%d] %s\n", i, context->pdo_info.entries[i].description);
        }
    }
    
    PDO_LOG("======================\n\n");
}

/**
 * @brief Get PDO entry by index
 */
const PDO_Entry_t* PDO_GetEntry(PDO_Context_t *context, uint8_t index)
{
    if (context == NULL || !context->initialized) {
        return NULL;
    }

    if (index >= context->pdo_info.entry_count) {
        return NULL;
    }

    if (!context->pdo_info.entries[index].valid) {
        return NULL;
    }

    return &context->pdo_info.entries[index];
}

/**
 * @brief Check if voltage is supported
 */
bool PDO_IsVoltageSupported(PDO_Context_t *context, uint16_t voltage_mv)
{
    return (PDO_FindByVoltage(context, voltage_mv) != NULL);
}

/* ============================================================================
 * Backward Compatibility Functions
 * ========================================================================== */

void Ch224qInit(void)
{
    // Note: User must initialize CH224Q driver separately
    // This function just initializes the PDO context
    // g_pdo_context.ch224q_handle must be set externally
    
    if (g_pdo_context.ch224q_handle != NULL) {
        PDO_Init(&g_pdo_context, g_pdo_context.ch224q_handle);
    }
}

void Ch224qSetVolt(ch224q_voltage_t voltage)
{
    PDO_SetVoltage(&g_pdo_context, voltage);
}

void Ch224qGetPDO(PDO_Info_t *pdo_info)
{
    if (pdo_info == NULL) return;
    
    PDO_Read(&g_pdo_context);
    memcpy(pdo_info, &g_pdo_context.pdo_info, sizeof(PDO_Info_t));
}

void Ch224qGetStatus(PDO_Info_t *pdo_info)
{
    if (pdo_info == NULL) return;
    
    PDO_GetStatus(&g_pdo_context);
    memcpy(pdo_info, &g_pdo_context.pdo_info, sizeof(PDO_Info_t));
}

void PDO_GET(ch224q_handle_t *handle, PDO_Info_t *pdo_info)
{
    if (handle == NULL || pdo_info == NULL) return;
    
    PDO_Context_t temp_context;
    PDO_Init(&temp_context, handle);
    PDO_Read(&temp_context);
    
    memcpy(pdo_info, &temp_context.pdo_info, sizeof(PDO_Info_t));
}

/* ============================================================================
 * Utility Functions
 * ========================================================================== */

uint16_t PDO_VoltageToMillivolts(ch224q_voltage_t voltage)
{
    if (voltage < CH224Q_PPS) {
        return voltage_map[voltage];
    }
    return 0; // Variable voltage
}

const char* PDO_GetProtocolName(ch224q_protocol_t protocol)
{
    switch (protocol) {
        case CH224Q_PROTOCOL_NONE:  return "None";
        case CH224Q_PROTOCOL_BC12:  return "BC1.2";
        case CH224Q_PROTOCOL_QC2:   return "QC2.0";
        case CH224Q_PROTOCOL_QC3:   return "QC3.0";
        case CH224Q_PROTOCOL_PD:    return "USB-PD";
        case CH224Q_PROTOCOL_EPR:   return "USB-PD EPR";
        default:                    return "Unknown";
    }
}

const char* PDO_GetTypeName(USBPD_PDO_Type_t type)
{
    switch (type) {
        case PDO_TYPE_FIXED_SUPPLY:     return "Fixed";
        case PDO_TYPE_BATTERY:          return "Battery";
        case PDO_TYPE_VARIABLE_SUPPLY:  return "Variable";
        case PDO_TYPE_APDO:             return "APDO";
        default:                        return "Unknown";
    }
}
