#include "njw1195a.h"

/* Debug Tag for NJW1195A */
#define USB_DBG_TAG "VOL"

/* Include usb_config.h if available to get debug levels */
#if __has_include("usb_config.h")
#include "usb_config.h"
#endif

/* Ensure CONFIG_USB_PRINTF is defined if not provided by usb_config.h */
#ifndef CONFIG_USB_PRINTF
#include <stdio.h>
#define CONFIG_USB_PRINTF printf
#endif

#include "usb_log.h"

#ifndef CONFIG_USB_DBG_LEVEL
#define CONFIG_USB_DBG_LEVEL USB_DBG_INFO
#endif

/* Private helper function to send 16-bit control word */
static HAL_StatusTypeDef NJW1195A_SendCommand(NJW1195A_HandleTypeDef *hnjw, uint8_t address, uint8_t data);
static HAL_StatusTypeDef NJW1195A_SendCommand_DMA(NJW1195A_HandleTypeDef *hnjw, uint8_t address, uint8_t data);
static void AMP_EN_Pins_EXTI_Init(NJW1195A_HandleTypeDef *hnjw);

/* Initialize the NJW1195A */
HAL_StatusTypeDef NJW1195A_Init(NJW1195A_HandleTypeDef *hnjw) {
    HAL_StatusTypeDef status;

    /* 阶段 1：初始化音频芯片并使其进入全通道 MUTE 状态
     * 此时 AMP EN 引脚依然保持 CubeMX 初始化时的 Low 状态 (运放硬件级关闭)
     */
    status = NJW1195A_Core_Config(hnjw);
    if (status != HAL_OK) {
        return status;
    }

    /* 阶段 2：黄金延时等待供电轨完美稳定
     * NJW1195A 本身已经静音，等待额外的 400ms (加上前面的 100ms 共计 500ms)
     * 确保 SEPIC 和 Cuk 电路输出的运放正负电源纹波完全平息
     */
    USB_LOG_INFO("Waiting for AMP power rails to stabilize...\r\n");
    HAL_Delay(400); 

    /* 阶段 3：释放控制权，开启外部中断
     * 此时系统已处于绝对安全的稳态，将引脚控制权交还给耳机座的机械插入检测开关
     */
    USB_LOG_INFO("Ready to switch EXTI...\r\n");
    AMP_EN_Pins_EXTI_Init(hnjw);
    USB_LOG_INFO("EXTI Switched Successfully!\r\n");

    USB_LOG_INFO("Audio System Initialization and Power Sequencing Complete.\r\n");
    return HAL_OK;
}

/* Core Init configuration function for NJW1195A */
HAL_StatusTypeDef NJW1195A_Core_Config(NJW1195A_HandleTypeDef *hnjw) {
    if (hnjw == NULL || hnjw->LatchPort == NULL || hnjw->hspi == NULL) {
        return HAL_ERROR;
    }
    
    /* Power On */
    HAL_GPIO_WritePin(hnjw->PW_EN_Port, hnjw->PW_EN_Pin, GPIO_PIN_SET);

    /* Ensure LATCH starts HIGH (idle state) */
    HAL_GPIO_WritePin(hnjw->LatchPort, hnjw->LatchPin, GPIO_PIN_SET);
    
    /* Set chip address pins if provided (default = 0x0 if NULL) */
    hnjw->ChipAddress = 0x00;
    if (hnjw->ADR0_Port != NULL) {
        HAL_GPIO_WritePin(hnjw->ADR0_Port, hnjw->ADR0_Pin, GPIO_PIN_RESET);
    }
    if (hnjw->ADR1_Port != NULL) {
        HAL_GPIO_WritePin(hnjw->ADR1_Port, hnjw->ADR1_Pin, GPIO_PIN_RESET);
    }
    
    /* Wait for power stabilization (chip needs time after V+/V- applied) */
    HAL_Delay(100);
    
    /* Initialize state */
    hnjw->IsBusy = 0;
    hnjw->QueuedCommands = 0;
    
    USB_LOG_INFO("NJW1195A Initialized (Chip Address: 0x%01X)\r\n", hnjw->ChipAddress);
    
    /* Set all volumes to mute initially */
    NJW1195A_SetVolume(hnjw, NJW1195A_REG_VOL_CH1, NJW1195A_VOL_MUTE);
    HAL_Delay(2);  // Small delay between commands
    NJW1195A_SetVolume(hnjw, NJW1195A_REG_VOL_CH2, NJW1195A_VOL_MUTE);
    HAL_Delay(2);
    NJW1195A_SetVolume(hnjw, NJW1195A_REG_VOL_CH3, NJW1195A_VOL_MUTE);
    HAL_Delay(2);
    NJW1195A_SetVolume(hnjw, NJW1195A_REG_VOL_CH4, NJW1195A_VOL_MUTE);
    HAL_Delay(2);
    
    /* Set input selectors to Input 1 */
    NJW1195A_SetInput(hnjw, NJW1195A_INPUT_1, NJW1195A_INPUT_1, 
                            NJW1195A_INPUT_1, NJW1195A_INPUT_1);
    
    USB_LOG_INFO("NJW1195A Initial Configuration Complete\r\n");
    return HAL_OK;
}

/* Set Volume Level for a specific channel (blocking) */
HAL_StatusTypeDef NJW1195A_SetVolume(NJW1195A_HandleTypeDef *hnjw, uint8_t channel, uint8_t level) {
    if (hnjw == NULL || hnjw->hspi == NULL) {
        return HAL_ERROR;
    }
    
    /* Validate channel */
    if (channel > NJW1195A_REG_VOL_CH4) {
        return HAL_ERROR;
    }
    
    return NJW1195A_SendCommand(hnjw, channel, level);
}

/* Set Volume Level for a specific channel using DMA */
HAL_StatusTypeDef NJW1195A_SetVolume_DMA(NJW1195A_HandleTypeDef *hnjw, uint8_t channel, uint8_t level) {
    if (hnjw == NULL || hnjw->hspi == NULL) {
        return HAL_ERROR;
    }
    
    /* Check if busy */
    if (hnjw->IsBusy) {
        return HAL_BUSY;
    }
    
    /* Validate channel */
    if (channel > NJW1195A_REG_VOL_CH4) {
        return HAL_ERROR;
    }
    
    return NJW1195A_SendCommand_DMA(hnjw, channel, level);
}

/* Set all 4 volume channels to the same level (blocking) */
HAL_StatusTypeDef NJW1195A_SetAllVolumes(NJW1195A_HandleTypeDef *hnjw, uint8_t level) {
    HAL_StatusTypeDef status;
    
    /* Set each channel sequentially with small delays for zero-cross detection */
    status = NJW1195A_SetVolume(hnjw, NJW1195A_REG_VOL_CH1, level);
    if (status != HAL_OK) return status;
    HAL_Delay(2);
    
    status = NJW1195A_SetVolume(hnjw, NJW1195A_REG_VOL_CH2, level);
    if (status != HAL_OK) return status;
    HAL_Delay(2);
    
    status = NJW1195A_SetVolume(hnjw, NJW1195A_REG_VOL_CH3, level);
    if (status != HAL_OK) return status;
    HAL_Delay(2);
    
    status = NJW1195A_SetVolume(hnjw, NJW1195A_REG_VOL_CH4, level);
    
    return status;
}

/* Set Input Selectors */
HAL_StatusTypeDef NJW1195A_SetInput(NJW1195A_HandleTypeDef *hnjw, 
                                     uint8_t selector1A, uint8_t selector2A,
                                     uint8_t selector1B, uint8_t selector2B) {
    if (hnjw == NULL || hnjw->hspi == NULL) {
        return HAL_ERROR;
    }
    
    /* Build selector data bytes according to datasheet format:
     * D15-D13: Selector 1A (3 bits)
     * D12-D10: Selector 2A (3 bits)
     * Rest: don't care for this register
     */
    uint8_t dataA = ((selector1A & 0x07) << 5) | ((selector2A & 0x07) << 2);
    uint8_t dataB = ((selector1B & 0x07) << 5) | ((selector2B & 0x07) << 2);
    
    HAL_StatusTypeDef status;
    status = NJW1195A_SendCommand(hnjw, NJW1195A_REG_SEL_1A_2A, dataA);
    if (status != HAL_OK) return status;
    
    HAL_Delay(2);
    
    status = NJW1195A_SendCommand(hnjw, NJW1195A_REG_SEL_1B_2B, dataB);
    
    return status;
}

/* Private: Send command via SPI (blocking) */
static HAL_StatusTypeDef NJW1195A_SendCommand(NJW1195A_HandleTypeDef *hnjw, uint8_t address, uint8_t data) {
    /* Build 16-bit control word: D15-D8=data, D7-D4=address, D3-D0=chip_address
     * According to datasheet page 11-12:
     * MSB First: D15 D14 D13 ... D1 D0
     * Format: [Data 8bits][Select Address 4bits][Chip Address 4bits]
     */
    
    uint16_t controlWord = ((uint16_t)data << 8) | ((address & 0x0F) << 4) | (hnjw->ChipAddress & 0x0F);
    
    /* Prepare byte buffer (MSB first for transmission) */
    uint8_t txBuffer[2];
    txBuffer[0] = (controlWord >> 8) & 0xFF;  // High byte (data)
    txBuffer[1] = controlWord & 0xFF;         // Low byte (address + chip addr)
    
    /* 1. LATCH Low (start of frame) */
    HAL_GPIO_WritePin(hnjw->LatchPort, hnjw->LatchPin, GPIO_PIN_RESET);
    
    /* Small delay to meet setup time (datasheet: t7 = 1.6us min) */
    /* At 170MHz, one NOP is ~6ns, so 200 NOPs = ~1.2us - use delay instead */
    for (volatile int i = 0; i < 10; i++) __NOP();
    
    /* 2. Transmit 16 bits via SPI */
    HAL_StatusTypeDef status = HAL_SPI_Transmit(hnjw->hspi, txBuffer, 2, 100);
    
    /* Wait for transmission to complete */
    /* Small delay for t4 (LATCH rise hold time = 4us min) */
    for (volatile int i = 0; i < 20; i++) __NOP();
    
    /* 3. LATCH High (end of frame - data is latched on rising edge) */
    HAL_GPIO_WritePin(hnjw->LatchPort, hnjw->LatchPin, GPIO_PIN_SET);
    
    /* Wait for zero-cross detection to take effect (~1ms typical) */
    /* This prevents clicks/pops when changing volume */
    HAL_Delay(2);
    
    return status;
}

/* Private: Send command via SPI (DMA) */
static HAL_StatusTypeDef NJW1195A_SendCommand_DMA(NJW1195A_HandleTypeDef *hnjw, uint8_t address, uint8_t data) {
    /* Build 16-bit control word */
    uint16_t controlWord = ((uint16_t)data << 8) | ((address & 0x0F) << 4) | (hnjw->ChipAddress & 0x0F);
    
    /* Prepare DMA buffer */
    hnjw->TxBuffer[0] = (controlWord >> 8) & 0xFF;
    hnjw->TxBuffer[1] = controlWord & 0xFF;
    
    hnjw->IsBusy = 1;
    
    /* 1. LATCH Low (start of frame) */
    HAL_GPIO_WritePin(hnjw->LatchPort, hnjw->LatchPin, GPIO_PIN_RESET);
    
    /* Small setup delay */
    for (volatile int i = 0; i < 10; i++) __NOP();
    
    /* 2. Transmit via DMA */
    HAL_StatusTypeDef status = HAL_SPI_Transmit_DMA(hnjw->hspi, (uint8_t*)hnjw->TxBuffer, 2);
    
    /* LATCH will be raised in TxCpltCallback */
    
    return status;
}

/* DMA Transfer Complete Callback */
void NJW1195A_TxCpltCallback(NJW1195A_HandleTypeDef *hnjw) {
    /* Small delay for t4 (LATCH rise hold time) */
    for (volatile int i = 0; i < 20; i++) __NOP();
    
    /* 3. LATCH High (end of frame - apply settings) */
    HAL_GPIO_WritePin(hnjw->LatchPort, hnjw->LatchPin, GPIO_PIN_SET);
    
    hnjw->IsBusy = 0;
    
    /* If there are queued commands, process next one */
    if (hnjw->QueuedCommands > 0) {
        /* Process next command in queue */
        uint8_t nextChannel = hnjw->QueuedChannels[0];
        uint8_t nextLevel = hnjw->QueuedLevels[0];
        
        /* Shift queue */
        for (int i = 0; i < hnjw->QueuedCommands - 1; i++) {
            hnjw->QueuedChannels[i] = hnjw->QueuedChannels[i + 1];
            hnjw->QueuedLevels[i] = hnjw->QueuedLevels[i + 1];
        }
        hnjw->QueuedCommands--;
        
        /* Send next command */
        NJW1195A_SetVolume_DMA(hnjw, nextChannel, nextLevel);
    }
}

/* Helper: Convert dB value to register value
 * Range: +31.5dB to -95dB in 0.5dB steps
 * Register: 0x01 = +31.5dB, 0x40 = 0dB, 0xFE = -95dB, 0x00 or 0xFF = Mute
 */
uint8_t NJW1195A_dBToRegister(float dB) {
    if (dB >= 31.5f) {
        return 0x01;  // +31.5dB (max)
    } else if (dB <= -95.0f) {
        return 0xFE;  // -95dB (min before mute)
    } else {
        /* Formula: register = 0x40 + (dB * 2)
         * Example: 0dB → 0x40, -10dB → 0x36, +10dB → 0x4A
         */
        int regValue = 0x40 - (int)(dB * 2.0f);
        
        /* Clamp to valid range */
        if (regValue < 0x01) regValue = 0x01;
        if (regValue > 0xFE) regValue = 0xFE;
        
        USB_LOG_INFO("dB: %.1f, reg: 0x%02X\n", dB, regValue);
        return (uint8_t)regValue;
    }
}

static void AMP_EN_Pins_EXTI_Init(NJW1195A_HandleTypeDef *hnjw) {
    // GPIO_InitTypeDef GPIO_InitStruct = {0};

    // GPIO_InitStruct.Pin = hnjw->SE_EN_Pin; 
    
    // GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING; 
    
    // GPIO_InitStruct.Pull = GPIO_NOPULL; 
    
    // /* Reinitialize GPIO with new settings,
    //  * overriding CubeMX default Output Pull-down configuration.
    //  */
    // HAL_GPIO_Init(hnjw->SE_EN_Port, &GPIO_InitStruct); 

    // /* Tips:
    //  * 1. EXTI15_10_IRQn is the interrupt line for pins EXTI15 to EXTI10.
    //  * 2. Make sure to enable the corresponding NVIC interrupt in your main.c.
    //  * 3. The EXTI interrupt handler (EXTI15_10_IRQHandler) is defined in stm32f1xx_hal_msp.c.
    //  */
    // HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0); 

    // HAL_GPIO_Init(hnjw->SE_EN_Port, &GPIO_InitStruct); 

    // /* Clean up any pending interrupt flags */
    // __HAL_GPIO_EXTI_CLEAR_IT(hnjw->SE_EN_Pin);

    // HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);

    // HAL_NVIC_EnableIRQ(EXTI15_10_IRQn); 

    // USB_LOG_INFO("AMP EN Pins configured to EXTI Mode.\r\n");

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* * 1. 组合引脚：因为 PB6 和 PB11 都在 GPIOB 上，
     * 可以用位或运算符 '|' 把它们拼在一起，一次性初始化，提高执行效率。
     * (如果你在 hnjw 结构体里存了这两个 Pin，也可以写成 hnjw->DET1_Pin | hnjw->DET2_Pin)
     */
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_11; 
    USB_LOG_INFO("Selected AMP EN Pins (PB6 & PB11) for EXTI Mode.\r\n");
    
    /* 根据拔插防爆音逻辑，选择双边沿触发 */
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING; 
    USB_LOG_INFO("AMP EN Pins (PB6 & PB11) configured with Rising/Falling Edge Trigger.\r\n");
    
    /* 假设外部已有硬件上下拉，设为 NOPULL；若没有，请改为 GPIO_PULLUP / GPIO_PULLDOWN */
    GPIO_InitStruct.Pull = GPIO_NOPULL; 
    USB_LOG_INFO("AMP EN Pins (PB6 & PB11) configured with NOPULL.\r\n");
    
    /* 执行重配置，覆盖掉之前的 Output 模式 */
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct); 
    USB_LOG_INFO("AMP EN Pins (PB6 & PB11) configured to EXTI Mode.\r\n");

    /* * 2. 【救命关键】：分别清除 PB6 和 PB11 在切换模式时产生的“幽灵中断”标志位！
     * 绝对不能漏掉，否则下面 EnableIRQ 的瞬间又会坠入 Default_Handler。
     */
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_6);
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_11);
    USB_LOG_INFO("AMP EN Pins (PB6 & PB11) interrupt flags cleared.\r\n");

    /* * 3. 开启 PB6 对应的中断线 (EXTI9_5_IRQn)
     */
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0); 
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn); 
    USB_LOG_INFO("AMP EN Pin PB6 configured with EXTI9_5_IRQn.\r\n");

    /* * 4. 开启 PB11 对应的中断线 (EXTI15_10_IRQn)
     */
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0); 
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn); 
    USB_LOG_INFO("AMP EN Pin PB11 configured with EXTI15_10_IRQn.\r\n");

    USB_LOG_INFO("AMP EN Pins (PB6 & PB11) configured to EXTI Mode.\r\n");
}