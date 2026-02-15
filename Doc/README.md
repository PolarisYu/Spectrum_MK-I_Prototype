# CH224Q/CH224A USB PD Driver - Complete Refactored Version

## 📋 Overview

This is a production-grade driver for CH224Q/CH224A USB Power Delivery sink controller chips. It provides a complete abstraction layer with proper error handling, PDO parsing, and platform independence.

**Key Features:**
- ✅ Full USB PD 3.2 support (EPR, AVS, PPS, SPR)
- ✅ Comprehensive PDO parsing with all PDO types
- ✅ Platform-independent design (easy to port)
- ✅ Proper error handling and validation
- ✅ Complete API with backward compatibility
- ✅ Well-documented code with examples

**Based on:**
- CH224 Datasheet V2.1
- USB PD Specification Revision 3.2

---

## 📁 File Structure

```
ch224q_driver/
├── ch224q.h                    // Low-level driver header
├── ch224q.c                    // Low-level driver implementation
├── usbpd_def.h                 // USB PD protocol definitions
├── PDO.h                       // High-level PDO management header
├── PDO.c                       // High-level PDO management implementation
├── ch224q_platform_stm32.c     // STM32 platform adaptation example
└── README.md                   // This file
```

---

## 🔧 Quick Start

### Step 1: Platform Adaptation

Implement the platform functions for your MCU:

```c
// Example for STM32
#include "ch224q.h"
#include "stm32xxx_hal.h"

extern I2C_HandleTypeDef hi2c1;

static int platform_i2c_write(void *i2c_handle, uint8_t dev_addr, 
                              uint8_t reg_addr, const uint8_t *data, uint16_t len)
{
    I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)i2c_handle;
    uint16_t addr_8bit = (uint16_t)(dev_addr << 1);
    
    HAL_StatusTypeDef status = HAL_I2C_Mem_Write(hi2c, addr_8bit, reg_addr,
                                                  I2C_MEMADD_SIZE_8BIT,
                                                  (uint8_t*)data, len, 100);
    return (status == HAL_OK) ? 0 : -1;
}

static int platform_i2c_read(void *i2c_handle, uint8_t dev_addr,
                             uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)i2c_handle;
    uint16_t addr_8bit = (uint16_t)(dev_addr << 1);
    
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(hi2c, addr_8bit, reg_addr,
                                                 I2C_MEMADD_SIZE_8BIT,
                                                 data, len, 100);
    return (status == HAL_OK) ? 0 : -1;
}

static void platform_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

static const ch224q_platform_t my_platform = {
    .i2c_write = platform_i2c_write,
    .i2c_read = platform_i2c_read,
    .delay_ms = platform_delay_ms
};
```

### Step 2: Initialize Driver

```c
#include "ch224q.h"
#include "PDO.h"

ch224q_handle_t ch224q_handle;
PDO_Context_t pdo_context;

void init_ch224q(void)
{
    // Initialize CH224Q driver
    ch224q_status_code_t ret = ch224q_init(&ch224q_handle,
                                            &my_platform,
                                            &hi2c1,
                                            CH224Q_I2C_ADDR_0);
    
    if (ret != CH224Q_OK) {
        // Handle error
        return;
    }
    
    // Initialize PDO management
    PDO_Init(&pdo_context, &ch224q_handle);
    
    // Read PDO capabilities
    PDO_Read(&pdo_context);
    PDO_PrintAll(&pdo_context);  // Print all available power profiles
}
```

### Step 3: Request Voltage

```c
// Request fixed voltage
ch224q_set_voltage(&ch224q_handle, CH224Q_20V);

// Or using high-level API
PDO_SetVoltage(&pdo_context, CH224Q_12V);
```

---

## 📖 API Reference

### Low-Level Driver (ch224q.h/c)

#### Initialization

```c
ch224q_status_code_t ch224q_init(ch224q_handle_t *handle, 
                                  const ch224q_platform_t *platform,
                                  void *i2c_handle, 
                                  uint8_t i2c_addr);
```

#### Voltage Control

```c
// Set voltage (5V/9V/12V/15V/20V/28V/PPS/AVS)
ch224q_status_code_t ch224q_set_voltage(ch224q_handle_t *handle, 
                                         ch224q_voltage_t voltage);

// Get current voltage setting
ch224q_status_code_t ch224q_get_voltage(ch224q_handle_t *handle, 
                                         ch224q_voltage_t *voltage);
```

#### Status & Monitoring

```c
// Read protocol status
ch224q_status_code_t ch224q_get_status(ch224q_handle_t *handle, 
                                        ch224q_status_info_t *status);

// Read available current (only valid in PD mode)
ch224q_status_code_t ch224q_get_current(ch224q_handle_t *handle, 
                                         ch224q_current_info_t *current);

// Read PDO data
ch224q_status_code_t ch224q_read_pdo(ch224q_handle_t *handle, 
                                      uint8_t *pdo_data, 
                                      uint16_t len);
```

#### Advanced Features

```c
// Configure AVS (Adjustable Voltage Supply)
// Range: 5000-28000mV, 100mV resolution
ch224q_status_code_t ch224q_config_avs(ch224q_handle_t *handle, 
                                        uint16_t voltage_mv);

// Configure PPS (Programmable Power Supply)
// Range: 5000-21000mV (SPR), 100mV resolution
ch224q_status_code_t ch224q_config_pps(ch224q_handle_t *handle, 
                                        uint16_t voltage_mv);
```

### High-Level API (PDO.h/c)

```c
// Initialize PDO management
ch224q_status_code_t PDO_Init(PDO_Context_t *context, 
                               ch224q_handle_t *ch224q_handle);

// Read and parse all PDO entries
ch224q_status_code_t PDO_Read(PDO_Context_t *context);

// Get protocol status
ch224q_status_code_t PDO_GetStatus(PDO_Context_t *context);

// Find PDO by voltage
const PDO_Entry_t* PDO_FindByVoltage(PDO_Context_t *context, 
                                      uint16_t voltage_mv);

// Check if voltage is supported
bool PDO_IsVoltageSupported(PDO_Context_t *context, uint16_t voltage_mv);

// Print all PDOs (debug)
void PDO_PrintAll(PDO_Context_t *context);
```

---

## 💡 Usage Examples

### Example 1: Basic Voltage Request

```c
void example_basic_voltage(void)
{
    ch224q_handle_t handle;
    
    // Initialize
    ch224q_init(&handle, &platform, &hi2c1, CH224Q_I2C_ADDR_0);
    
    // Request 20V
    ch224q_set_voltage(&handle, CH224Q_20V);
    HAL_Delay(100);  // Wait for voltage transition
    
    // Check status
    ch224q_status_info_t status;
    ch224q_get_status(&handle, &status);
    
    if (status.pd_active) {
        // PD successfully negotiated
        ch224q_current_info_t current;
        ch224q_get_current(&handle, &current);
        
        printf("Voltage: 20V, Current: %dmA\n", current.max_current_ma);
    }
}
```

### Example 2: Read All Power Profiles

```c
void example_read_pdo(void)
{
    ch224q_handle_t handle;
    PDO_Context_t context;
    
    // Initialize
    ch224q_init(&handle, &platform, &hi2c1, CH224Q_I2C_ADDR_0);
    PDO_Init(&context, &handle);
    
    // Read PDO data
    PDO_Read(&context);
    
    // Print all available profiles
    printf("Available Power Profiles:\n");
    for (uint8_t i = 0; i < context.pdo_info.entry_count; i++) {
        const PDO_Entry_t *entry = PDO_GetEntry(&context, i);
        if (entry) {
            printf("[%d] %s\n", i, entry->description);
        }
    }
    
    // Check capabilities
    if (context.pdo_info.epr_capable) {
        printf("Adapter supports EPR (>100W)\n");
    }
    if (context.pdo_info.pps_capable) {
        printf("Adapter supports PPS (programmable voltage)\n");
    }
}
```

### Example 3: Use AVS for Custom Voltage

```c
void example_avs_custom_voltage(void)
{
    ch224q_handle_t handle;
    
    // Initialize
    ch224q_init(&handle, &platform, &hi2c1, CH224Q_I2C_ADDR_0);
    
    // First time: Configure AVS voltage and switch to AVS mode
    uint16_t target_voltage_mv = 15000;  // 15.0V
    ch224q_config_avs(&handle, target_voltage_mv);
    ch224q_set_voltage(&handle, CH224Q_AVS);
    
    HAL_Delay(100);  // Wait for transition
    
    // Adjust voltage later (no need to call ch224q_set_voltage again)
    target_voltage_mv = 16500;  // 16.5V
    ch224q_config_avs(&handle, target_voltage_mv);
    
    HAL_Delay(50);  // Wait for voltage adjustment
}
```

### Example 4: Use PPS for Fine Voltage Control

```c
void example_pps_fine_control(void)
{
    ch224q_handle_t handle;
    
    // Initialize
    ch224q_init(&handle, &platform, &hi2c1, CH224Q_I2C_ADDR_0);
    
    // Check if adapter supports PPS
    PDO_Context_t context;
    PDO_Init(&context, &handle);
    PDO_Read(&context);
    
    if (!context.pdo_info.pps_capable) {
        printf("Adapter does not support PPS\n");
        return;
    }
    
    // First time: Configure PPS and switch to PPS mode
    uint16_t target_voltage_mv = 12000;  // 12.0V
    ch224q_config_pps(&handle, target_voltage_mv);
    ch224q_set_voltage(&handle, CH224Q_PPS);
    
    HAL_Delay(100);
    
    // Fine-tune voltage in real-time
    for (uint16_t v = 12000; v <= 15000; v += 100) {
        ch224q_config_pps(&handle, v);
        HAL_Delay(10);
    }
}
```

### Example 5: Intelligent Power Selection

```c
void example_intelligent_power(void)
{
    ch224q_handle_t handle;
    PDO_Context_t context;
    
    // Initialize
    ch224q_init(&handle, &platform, &hi2c1, CH224Q_I2C_ADDR_0);
    PDO_Init(&context, &handle);
    PDO_Read(&context);
    
    // Find highest voltage that can provide at least 3A
    uint16_t best_voltage = 5000;
    float best_power = 0;
    
    for (uint8_t i = 0; i < context.pdo_info.entry_count; i++) {
        const PDO_Entry_t *entry = PDO_GetEntry(&context, i);
        if (entry && entry->type == PDO_TYPE_FIXED_SUPPLY) {
            if (entry->max_current_ma >= 3000) {  // At least 3A
                if (entry->power_w > best_power) {
                    best_power = entry->power_w;
                    best_voltage = entry->voltage_mv;
                }
            }
        }
    }
    
    // Request the best voltage
    if (PDO_IsVoltageSupported(&context, best_voltage)) {
        printf("Selecting %dmV (%.1fW)\n", best_voltage, best_power);
        
        // Convert voltage to enum
        ch224q_voltage_t voltage_enum;
        switch (best_voltage) {
            case 5000:  voltage_enum = CH224Q_5V; break;
            case 9000:  voltage_enum = CH224Q_9V; break;
            case 12000: voltage_enum = CH224Q_12V; break;
            case 15000: voltage_enum = CH224Q_15V; break;
            case 20000: voltage_enum = CH224Q_20V; break;
            case 28000: voltage_enum = CH224Q_28V; break;
            default:    voltage_enum = CH224Q_5V; break;
        }
        
        ch224q_set_voltage(&handle, voltage_enum);
    }
}
```

---

## 🔍 Key Improvements Over Original Code

### 1. **Fixed Critical Status Register Bug** ❌ → ✅

**Original (WRONG):**
```c
if((status == (0x01 << 3)) || (status == (0x01 << 4))){
    pdo_info->status = PD;
}
```

**Fixed:**
```c
if(status & 0x18){  // Bit 3 | Bit 4
    pdo_info->status = PD;
}
```

### 2. **Proper PDO Parsing**

- ✅ Handles all PDO types (Fixed, Battery, Variable, APDO)
- ✅ Parses both SPR and EPR PDOs
- ✅ Extracts PPS and AVS capabilities
- ✅ Calculates actual power availability

### 3. **Robust Error Handling**

- ✅ All functions return error codes
- ✅ Parameter validation
- ✅ Boundary checking
- ✅ Safe overflow prevention

### 4. **Platform Independence**

- ✅ Abstract I2C interface
- ✅ Easy to port to any MCU
- ✅ No hardware dependencies in core code

### 5. **Better Code Organization**

- ✅ Clear separation of concerns
- ✅ Documented data structures
- ✅ Comprehensive examples
- ✅ Backward compatibility maintained

---

## 📚 Register Reference (Quick Guide)

| Register | Address | Function | Access |
|----------|---------|----------|--------|
| Status | 0x09 | Protocol status (PD/QC/BC) | Read |
| Voltage Control | 0x0A | Request voltage level | Write |
| Current Data | 0x50 | Max available current (50mA units) | Read |
| AVS Config High | 0x51 | AVS voltage high byte + enable | Write |
| AVS Config Low | 0x52 | AVS voltage low byte | Write |
| PPS Config | 0x53 | PPS voltage (100mV units) | Write |
| PDO Data | 0x60-0x8F | Source capability data | Read |

---

## ⚠️ Important Notes

### 1. AVS/PPS Usage

**AVS (Adjustable Voltage Supply):**
- Range: 5.0V - 28.0V
- Resolution: 100mV
- First use: Call `ch224q_config_avs()` then `ch224q_set_voltage(CH224Q_AVS)`
- Subsequent: Only call `ch224q_config_avs()`

**PPS (Programmable Power Supply):**
- Range: 5.0V - 21.0V (SPR only)
- Resolution: 100mV
- First use: Call `ch224q_config_pps()` then `ch224q_set_voltage(CH224Q_PPS)`
- Subsequent: Only call `ch224q_config_pps()`

### 2. PDO Data Validity

Per datasheet section 5.2.3:
- PDO data is valid when adapter < 100W (SRCCAP data)
- Or when in EPR mode at 28V (EPR_SRCCAP data)

### 3. Current Reading

Current register (0x50) is only valid when:
- PD or EPR protocol is active
- Handshake completed

### 4. eMarker Simulation

For >20V or >60W:
- Must use Type-C male connector
- Connect 1kΩ resistor from CC2 to GND
- Contact manufacturer for support

---

## 🛠️ Porting to Other Platforms

### Arduino Example

```c
#include <Wire.h>

static int arduino_i2c_write(void *handle, uint8_t dev_addr,
                             uint8_t reg_addr, const uint8_t *data, uint16_t len)
{
    Wire.beginTransmission(dev_addr);
    Wire.write(reg_addr);
    Wire.write(data, len);
    return (Wire.endTransmission() == 0) ? 0 : -1;
}

static int arduino_i2c_read(void *handle, uint8_t dev_addr,
                            uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    Wire.beginTransmission(dev_addr);
    Wire.write(reg_addr);
    Wire.endTransmission(false);
    Wire.requestFrom((int)dev_addr, (int)len);
    
    for (uint16_t i = 0; i < len; i++) {
        if (Wire.available()) {
            data[i] = Wire.read();
        }
    }
    return 0;
}

static void arduino_delay(uint32_t ms) {
    delay(ms);
}
```

### ESP32 Example

```c
#include "driver/i2c.h"

static int esp32_i2c_write(void *handle, uint8_t dev_addr,
                          uint8_t reg_addr, const uint8_t *data, uint16_t len)
{
    i2c_port_t i2c_num = *(i2c_port_t*)handle;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 100 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    return (ret == ESP_OK) ? 0 : -1;
}
```

---

## 📞 Support

For questions or issues:
1. Check the CH224 datasheet (V2.1)
2. Review the examples in this README
3. Examine the platform adaptation example

---

## 📄 License

This driver is provided as-is for use with CH224Q/CH224A chips.
Refer to your project's license requirements.

---

## 🔗 References

- CH224 Datasheet V2.1 (WCH)
- USB Power Delivery Specification Rev 3.2 (USB-IF)
- STM32 HAL Library Documentation

---

**Version:** 2.0.0  
**Date:** 2024-02-16  
**Author:** Refactored for production use
