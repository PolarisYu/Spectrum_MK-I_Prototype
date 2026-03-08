/**
 ******************************************************************************
 * @file    usbpd_def.h
 * @brief   USB Power Delivery Protocol Definitions
 * @version 1.0.0
 * @date    2024-02-16
 ******************************************************************************
 * @attention
 * 
 * Based on USB PD Specification Revision 3.2
 * Defines structures for parsing Source Capability messages
 * 
 ******************************************************************************
 */

#ifndef __USBPD_DEF_H__
#define __USBPD_DEF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * USB PD Message Header (16-bit)
 * ========================================================================== */
typedef union {
    uint16_t d16;
    struct {
        uint16_t MessageType           : 5;   // Bit 0-4
        uint16_t PortDataRole          : 1;   // Bit 5
        uint16_t SpecRevision          : 2;   // Bit 6-7
        uint16_t PortPowerRole         : 1;   // Bit 8
        uint16_t MessageID             : 3;   // Bit 9-11
        uint16_t NumberOfDataObjects   : 3;   // Bit 12-14
        uint16_t Extended              : 1;   // Bit 15
    } MessageHeader;
} USBPD_MessageHeader_t;

/* ============================================================================
 * USB PD Extended Message Header (16-bit)
 * ========================================================================== */
typedef union {
    uint16_t d16;
    struct {
        uint16_t DataSize              : 9;   // Bit 0-8
        uint16_t Reserved              : 1;   // Bit 9
        uint16_t RequestChunk          : 1;   // Bit 10
        uint16_t ChunkNumber           : 4;   // Bit 11-14
        uint16_t Chunked               : 1;   // Bit 15
    } ExtHeader;
} USBPD_ExtendedMessageHeader_t;

/* ============================================================================
 * PDO (Power Data Object) Type Definitions
 * ========================================================================== */

// PDO Type (Bits 31-30)
typedef enum {
    PDO_TYPE_FIXED_SUPPLY = 0,
    PDO_TYPE_BATTERY = 1,
    PDO_TYPE_VARIABLE_SUPPLY = 2,
    PDO_TYPE_APDO = 3  // Augmented Power Data Object
} USBPD_PDO_Type_t;

// APDO Sub-type (Bits 29-28 when PDO Type = 3)
typedef enum {
    APDO_TYPE_SPR_PPS = 0,  // SPR Programmable Power Supply
    APDO_TYPE_EPR_AVS = 1,  // EPR Adjustable Voltage Supply
    APDO_TYPE_RESERVED_2 = 2,
    APDO_TYPE_RESERVED_3 = 3
} USBPD_APDO_SubType_t;

/* ============================================================================
 * Fixed Supply PDO (Source)
 * ========================================================================== */
typedef union {
    uint32_t d32;
    struct {
        uint32_t MaxCurrentIn10mAunits        : 10;  // Bit 0-9
        uint32_t VoltageIn50mVunits           : 10;  // Bit 10-19
        uint32_t PeakCurrent                  : 2;   // Bit 20-21
        uint32_t Reserved                     : 2;   // Bit 22-23
        uint32_t EPRModeCapable               : 1;   // Bit 24 (PD3.1)
        uint32_t DualRolePower                : 1;   // Bit 25
        uint32_t USBCommunicationsCapable     : 1;   // Bit 26
        uint32_t UnconstrainedPower           : 1;   // Bit 27
        uint32_t USBSuspendSupported          : 1;   // Bit 28
        uint32_t DualRoleData                 : 1;   // Bit 29
        uint32_t FixedSupply                  : 2;   // Bit 30-31 (= 0b00)
    };
} USBPD_FixedSupplyPDO_t;

/* ============================================================================
 * Battery Supply PDO (Source)
 * ========================================================================== */
typedef union {
    uint32_t d32;
    struct {
        uint32_t MaxAllowablePowerIn250mWunits : 10; // Bit 0-9
        uint32_t MinVoltageIn50mVunits        : 10;  // Bit 10-19
        uint32_t MaxVoltageIn50mVunits        : 10;  // Bit 20-29
        uint32_t BatterySupply                : 2;   // Bit 30-31 (= 0b01)
    };
} USBPD_BatterySupplyPDO_t;

/* ============================================================================
 * Variable Supply PDO (Source)
 * ========================================================================== */
typedef union {
    uint32_t d32;
    struct {
        uint32_t MaxCurrentIn10mAunits        : 10;  // Bit 0-9
        uint32_t MinVoltageIn50mVunits        : 10;  // Bit 10-19
        uint32_t MaxVoltageIn50mVunits        : 10;  // Bit 20-29
        uint32_t VariableSupply               : 2;   // Bit 30-31 (= 0b10)
    };
} USBPD_VariableSupplyPDO_t;

/* ============================================================================
 * SPR Programmable Power Supply APDO (Source)
 * ========================================================================== */
typedef union {
    uint32_t d32;
    struct {
        uint32_t MaxCurrentIn50mAunits        : 7;   // Bit 0-6
        uint32_t Reserved1                    : 1;   // Bit 7
        uint32_t MinVoltageIn100mVunits       : 8;   // Bit 8-15
        uint32_t Reserved2                    : 1;   // Bit 16
        uint32_t MaxVoltageIn100mVunits       : 8;   // Bit 17-24
        uint32_t Reserved3                    : 2;   // Bit 25-26
        uint32_t PPSPowerLimited              : 1;   // Bit 27
        uint32_t SPR_ProgrammablePowerSupply  : 2;   // Bit 28-29 (= 0b00)
        uint32_t AugmentedPowerDataObject     : 2;   // Bit 30-31 (= 0b11)
    };
} USBPD_SPR_PPS_APDO_t;

/* ============================================================================
 * EPR Adjustable Voltage Supply APDO (Source)
 * ========================================================================== */
typedef union {
    uint32_t d32;
    struct {
        uint32_t MaxCurrentIn50mAunits        : 8;   // Bit 0-7
        uint32_t Reserved1                    : 1;   // Bit 8
        uint32_t MinVoltageIn100mVunits       : 9;   // Bit 9-17
        uint32_t Reserved2                    : 1;   // Bit 18
        uint32_t MaxVoltageIn100mVunits       : 9;   // Bit 19-27
        uint32_t EPR_AdjustableVoltageSupply  : 2;   // Bit 28-29 (= 0b01)
        uint32_t AugmentedPowerDataObject     : 2;   // Bit 30-31 (= 0b11)
    };
} USBPD_EPR_AVS_APDO_t;

/* ============================================================================
 * General PDO Union (All types)
 * ========================================================================== */
typedef union {
    uint32_t d32;
    
    // General structure to access type fields
    struct {
        uint32_t Data                         : 28;  // Bit 0-27
        uint32_t SubTypeOrOtherUsage          : 2;   // Bit 28-29
        uint32_t PDO_Type                     : 2;   // Bit 30-31
    } General;
    
    // Specific PDO types
    USBPD_FixedSupplyPDO_t SourceFixedSupplyPDO;
    USBPD_BatterySupplyPDO_t SourceBatterySupplyPDO;
    USBPD_VariableSupplyPDO_t SourceVariableSupplyPDO;
    USBPD_SPR_PPS_APDO_t SourceSPRProgrammablePowerAPDO;
    USBPD_EPR_AVS_APDO_t SourceEPRAdjustableVoltageAPDO;
    
} USBPD_SourcePDO_t;

/* ============================================================================
 * Power Conversion Macros
 * ========================================================================== */

// Voltage conversions
#define POWER_DECODE_50MV(units)    ((uint16_t)(units) * 50)   // 50mV units to mV
#define POWER_DECODE_100MV(units)   ((uint16_t)(units) * 100)  // 100mV units to mV
#define POWER_ENCODE_50MV(mv)       ((uint16_t)(mv) / 50)      // mV to 50mV units
#define POWER_ENCODE_100MV(mv)      ((uint16_t)(mv) / 100)     // mV to 100mV units

// Current conversions
#define POWER_DECODE_10MA(units)    ((uint16_t)(units) * 10)   // 10mA units to mA
#define POWER_DECODE_50MA(units)    ((uint16_t)(units) * 50)   // 50mA units to mA
#define POWER_ENCODE_10MA(ma)       ((uint16_t)(ma) / 10)      // mA to 10mA units
#define POWER_ENCODE_50MA(ma)       ((uint16_t)(ma) / 50)      // mA to 50mA units

// Power conversions
#define POWER_DECODE_250MW(units)   ((uint16_t)(units) * 250)  // 250mW units to mW
#define POWER_ENCODE_250MW(mw)      ((uint16_t)(mw) / 250)     // mW to 250mW units

/* ============================================================================
 * Helper Macros
 * ========================================================================== */

// Calculate power in watts from voltage and current
#define POWER_CALC_WATTS(voltage_mv, current_ma) \
    ((float)(voltage_mv) / 1000.0f * (float)(current_ma) / 1000.0f)

// Check if voltage is SPR (Standard Power Range) or EPR (Extended Power Range)
#define POWER_IS_SPR(voltage_mv)    ((voltage_mv) <= 20000)    // <= 20V
#define POWER_IS_EPR(voltage_mv)    ((voltage_mv) > 20000)     // > 20V

#ifdef __cplusplus
}
#endif

#endif /* __USBPD_DEF_H__ */
