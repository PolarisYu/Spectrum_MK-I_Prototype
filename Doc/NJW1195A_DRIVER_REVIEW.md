# NJW1195A Driver Code Review

## Executive Summary
**Status**: ❌ **CRITICAL ISSUES FOUND - DO NOT USE IN PRODUCTION**

The current driver has multiple critical bugs that will prevent the NJW1195A from functioning correctly. The most serious issue is the initialization sequence which appears to be for a different chip entirely.

---

## 🔴 CRITICAL ISSUES

### 1. **WRONG CHIP - Incorrect Power Enable Pin**
**Location**: `njw1195a.c:8-22` (Init function)

**Problem**: The code references a `PW_EN` pin that **does not exist** on the NJW1195A chip.

```c
// WRONG - This pin doesn't exist on NJW1195A
if (hnjw->PW_EN_Port != NULL) {
    HAL_GPIO_WritePin(hnjw->PW_EN_Port, hnjw->PW_EN_Pin, GPIO_PIN_SET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(hnjw->PW_EN_Port, hnjw->PW_EN_Pin, GPIO_PIN_RESET);  // Power OFF?
    HAL_Delay(20);
    HAL_GPIO_WritePin(hnjw->PW_EN_Port, hnjw->PW_EN_Pin, GPIO_PIN_SET);    // Power ON again?
}
```

**Impact**: This initialization sequence appears to be for a different audio IC (possibly from the same family but different variant). The NJW1195A has **no power enable pin** - it's controlled only through:
- V+ / V- power supply pins (18/17)
- 3-wire serial interface (DATA/CLOCK/LATCH)

**Fix**: Remove all `PW_EN` references. The chip is enabled when power is applied to V+/V-.

---

### 2. **Invalid Pin Definitions in Header**
**Location**: `njw1195a.h:27-28`

```c
GPIO_TypeDef *PW_EN_Port;  // ❌ This pin doesn't exist
uint16_t PW_EN_Pin;        // ❌ This pin doesn't exist
```

**NJW1195A Actual Pin List** (from datasheet):
- Pin 14: DATA (serial data input)
- Pin 15: CLOCK (serial clock input)
- Pin 16: LATCH (latch signal input)
- Pin 18: V+ (positive power)
- Pin 17: V- (negative power/ground)
- Pin 19: ADR0 (chip address select 0)
- Pin 20: ADR1 (chip address select 1)

**Fix**: Remove PW_EN definitions, add optional ADR0/ADR1 pins.

---

### 3. **Incorrect Control Word Format**
**Location**: `njw1195a.c:40-49` (SetLevel_DMA)

**Problem**: The control word construction doesn't match the datasheet specification.

**Datasheet Format** (page 11-12):
```
D15 D14 D13 D12 D11 D10 D9 D8 | D7 D6 D5 D4 | D3 D2 D1 D0
[      Data (8 bits)         ] [Addr (4bit)] [Chip Addr(4)]
```

**Current Code**:
```c
hnjw->TxBuffer[0] = channel;  // ❌ Wrong: this is address, not data
hnjw->TxBuffer[1] = level;    // ❌ Wrong: this is data, not address
```

**Correct Format**:
```c
uint16_t word = (data << 8) | (address << 4) | chip_address;
TxBuffer[0] = (word >> 8);  // MSB: data
TxBuffer[1] = word & 0xFF;  // LSB: address + chip addr
```

---

### 4. **Missing Initial Configuration**
**Location**: `njw1195a.c` (Init function)

**Problem**: The chip powers up in an **unknown state** (datasheet page 12 shows initial MUTE). The driver never:
1. Sets initial volume levels
2. Configures input selectors
3. Sends any control commands during init

**Impact**: Audio will not work until user manually sends volume commands.

**Fix**: Send initial configuration commands:
```c
// Set all channels to mute initially
NJW1195A_SetVolume(hnjw, 0, 0x00);  // CH1 mute
NJW1195A_SetVolume(hnjw, 1, 0x00);  // CH2 mute
// ... etc

// Set default input selection
NJW1195A_SetInput(hnjw, INPUT_1, INPUT_1, INPUT_1, INPUT_1);
```

---

### 5. **SetAllLevels_DMA is Broken**
**Location**: `njw1195a.c:56-65`

**Problem**: Only sets channel 1, ignores the other 3 channels.

```c
// Current code - only sets CH1!
return NJW1195A_SetLevel_DMA(hnjw, NJW1195A_REG_VOL_CH1, level);
```

**Impact**: Function name promises to set all 4 channels but only sets 1.

**Fix**: Either implement proper queuing or use blocking sequential calls:
```c
// Option 1: Blocking (simple but works)
for (int ch = 0; ch < 4; ch++) {
    NJW1195A_SetVolume(hnjw, ch, level);
    HAL_Delay(2);  // Wait for zero-cross
}

// Option 2: DMA with queue (complex but efficient)
// [implement command queue mechanism]
```

---

### 6. **No Zero-Cross Detection Delay**
**Location**: All volume setting functions

**Problem**: The NJW1195A has built-in zero-cross detection to prevent clicks/pops. This requires a small delay between volume changes (typically 1-2ms).

**Impact**: May hear clicks or pops when changing volume.

**Fix**: Add delays between sequential volume commands:
```c
NJW1195A_SetVolume(hnjw, 0, level);
HAL_Delay(2);  // Allow zero-cross detection
NJW1195A_SetVolume(hnjw, 1, level);
```

---

### 7. **Timing Violations**
**Location**: All SPI transmission sequences

**Problem**: The datasheet specifies minimum timing requirements:
- t7 (CLOCK setup time): 1.6μs minimum
- t4 (LATCH rise hold time): 4μs minimum

The code has no delays to ensure these timings are met.

**Fix**: Add appropriate delays:
```c
// Before LATCH low
for(volatile int i=0; i<10; i++) __NOP();  

// After SPI transmission, before LATCH high
for(volatile int i=0; i<20; i++) __NOP();
```

---

### 8. **Volume Register Value Confusion**
**Location**: Throughout code

**Problem**: The datasheet shows:
- 0x00 = Mute
- 0x01 = +31.5dB (maximum gain)
- 0x40 = 0dB 
- 0xFE = -95dB
- 0xFF = Mute (alternative)

But the code defines:
```c
#define NJW1195A_VOL_0DB    0x00  // ❌ WRONG - this is MUTE, not 0dB!
#define NJW1195A_VOL_MUTE   0xFF  // ⚠️ Works but 0x00 is more common
```

**Fix**: Correct definitions:
```c
#define NJW1195A_VOL_0DB        0x40  // ✅ 0dB
#define NJW1195A_VOL_MUTE       0x00  // ✅ Mute
#define NJW1195A_VOL_PLUS_31_5  0x01  // ✅ Maximum
#define NJW1195A_VOL_MINUS_95   0xFE  // ✅ Minimum before mute
```

---

## 🟡 DESIGN ISSUES (Non-Critical)

### 9. **Missing Input Selector Functions**
The NJW1195A has 4-input selectors (controlled by registers 0x04 and 0x05), but the driver has no functions to control them.

**Add**:
```c
HAL_StatusTypeDef NJW1195A_SetInput(NJW1195A_HandleTypeDef *hnjw, 
                                     uint8_t sel1A, uint8_t sel2A,
                                     uint8_t sel1B, uint8_t sel2B);
```

---

### 10. **No dB to Register Conversion Helper**
Users need to manually calculate register values for desired dB levels.

**Add helper**:
```c
uint8_t NJW1195A_dBToRegister(float dB) {
    // +31.5dB = 0x01, 0dB = 0x40, -95dB = 0xFE
    // Step = 0.5dB
    if (dB >= 31.5f) return 0x01;
    if (dB <= -95.0f) return 0xFE;
    return (uint8_t)(0x40 - (int)(dB * 2.0f));
}
```

---

### 11. **Blocking vs DMA Inconsistency**
- `NJW1195A_SetLevel_DMA` uses DMA
- `NJW1195A_SetAllLevels_DMA` doesn't actually use DMA properly

Either implement proper DMA queuing for all functions, or make separate blocking/DMA versions.

---

### 12. **Missing Chip Address Support**
The NJW1195A supports 4 different chip addresses (via ADR0/ADR1 pins) to allow up to 4 chips on the same SPI bus. The driver doesn't handle this.

**Add**: Chip address configuration in Init function.

---

### 13. **No Error Handling**
If SPI transmission fails, there's no recovery or error reporting mechanism.

---

### 14. **printf() in Driver**
**Location**: Multiple places

Using printf in embedded driver code is generally bad practice:
- Blocks execution
- Increases code size
- May cause issues if UART isn't configured

**Fix**: Use proper logging framework or remove debug prints for production.

---

## 📋 CORRECTED IMPLEMENTATION

I've created corrected versions of both files:

### ✅ Fixed Features:
1. ✅ Removed non-existent PW_EN pin
2. ✅ Correct 16-bit control word format
3. ✅ Proper initialization sequence
4. ✅ Initial volume and input configuration
5. ✅ Zero-cross detection delays
6. ✅ Timing requirement compliance
7. ✅ Correct volume register values
8. ✅ Input selector control functions
9. ✅ dB to register conversion helper
10. ✅ Proper DMA callback handling
11. ✅ Chip address support (ADR0/ADR1)
12. ✅ Command queuing for multi-channel DMA

### 📁 Files:
- `njw1195a_fixed.h` - Corrected header with proper definitions
- `njw1195a_fixed.c` - Corrected implementation

---

## 🧪 TESTING RECOMMENDATIONS

1. **Hardware Verification**:
   - Verify V+/V- power supply voltages (±7V typical)
   - Check SPI signals with oscilloscope
   - Verify LATCH timing meets datasheet specs

2. **Functional Tests**:
   - Test mute/unmute
   - Test volume sweep from +31.5dB to -95dB
   - Test all 4 input channels
   - Test zero-cross detection (no clicks/pops)

3. **Integration Tests**:
   - Test DMA callback chain for multi-channel
   - Test rapid volume changes
   - Test multiple chips (if using chip addressing)

---

## 📖 DATASHEET REFERENCE

**Key Pages**:
- Page 1: Block diagram and features
- Page 2: Pin functions
- Page 7-10: Application circuits (dual/single supply)
- Page 11: Control data format timing
- Page 12-18: Control data tables (volume/selector mappings)

**Critical Specifications**:
- Operating Voltage: ±3.5V to ±7.5V (dual) or +7V to +15V (single)
- Volume Range: +31.5dB to -95dB in 0.5dB steps
- THD: 0.0003% typ @ 1kHz
- Channel Separation: -120dB typ

---

## ⚠️ SUMMARY

**DO NOT USE ORIGINAL CODE**. It appears to be written for a different chip variant and will not work with the NJW1195A.

**USE PROVIDED CORRECTED VERSION** which properly implements the NJW1195A datasheet specifications.

**VERIFY HARDWARE** before testing - ensure power supply is correct and all timing requirements can be met by your SPI clock speed.

---

## 📞 ADDITIONAL SUPPORT

If you need help integrating the fixed driver or have questions about the NJW1195A:
1. Verify your PCB schematic matches the application circuits in datasheet
2. Check reference voltage configuration (A_REF/B_REF pins)
3. Verify decoupling capacitors on DCCAP pins
4. Ensure proper SPI mode (typically CPOL=0, CPHA=0 or Mode 0)
