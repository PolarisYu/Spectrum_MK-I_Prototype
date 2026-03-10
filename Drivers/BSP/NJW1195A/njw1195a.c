#include "njw1195a.h"

/* ============================================================
 * NJW1195A - 4-Channel Electronic Volume with Input Selector
 * Driver Implementation  (Reviewed & Corrected)
 *
 * Fixes applied vs. original:
 *  [F1] Differential Input 1 selector encoding corrected
 *       (SEL1=0x02, SEL2=0x01, per datasheet p.18 truth table).
 *  [F2] DMA dead-lock: IsBusy is now cleared on HAL_SPI_Transmit_DMA failure.
 *  [F3] AMP_EN_Pins_EXTI_Init now uses hnjw->SE_EN / BAL_EN fields instead
 *       of hard-coded PB6/PB11 constants, making the driver portable.
 *  [F4] ISR busy-wait removed: LATCH pulse is now handled inside
 *       NJW1195A_TxCpltCallback only after DMA completes; no blocking delay
 *       inside an interrupt context (t4 hold is still guaranteed by the DMA
 *       transfer duration which already exceeds 4 µs at any SPI speed ≤ 4 MHz).
 *  [F5] Duplicate NULL-guard removed from NJW1195A_SetInput_Diff.
 *  [F6] IsInitialized guard made consistent: internal low-level calls use
 *       NJW1195A_SendCommand directly; public API functions guard IsInitialized.
 *  [F7] DMA command queue completed: NJW1195A_EnqueueVolume() added, queue
 *       overflow bounds-check added in TxCpltCallback.
 *  [F8] dBToRegister comment examples corrected to match actual formula output.
 *  [F9] Unused header macros (NJW1195A_ADDR_MASK etc.) now used inside
 *       NJW1195A_BuildControlWord() helper for consistency.
 * ============================================================ */

/* -------------------------------------------------------
 * Debug / logging glue
 * ------------------------------------------------------- */
#define USB_DBG_TAG "VOL"

#if __has_include("usb_config.h")
#include "usb_config.h"
#endif

#ifndef CONFIG_USB_PRINTF
#include <stdio.h>
#define CONFIG_USB_PRINTF printf
#endif

#include "usb_log.h"

#ifndef CONFIG_USB_DBG_LEVEL
#define CONFIG_USB_DBG_LEVEL USB_DBG_INFO
#endif

/* -------------------------------------------------------
 * DWT cycle-counter (µs-accurate busy-wait)
 * Used ONLY for the short GPIO setup/hold delays required
 * by the NJW1195A serial interface timing (t4, t7).
 * Never used inside ISR context.
 * ------------------------------------------------------- */
#define DWT_CR      (*(volatile uint32_t *)0xE0001000)
#define DWT_CYCCNT  (*(volatile uint32_t *)0xE0001004)
#define DEM_CR      (*(volatile uint32_t *)0xE000EDFC)

static void DWT_Init(void)
{
    DEM_CR    |= 0x01000000U; /* Enable TRCENA */
    DWT_CYCCNT = 0U;          /* Reset cycle counter */
    DWT_CR    |= 1U;          /* Enable CYCCNT */
}

static void DWT_Delay_us(uint32_t us)
{
    extern uint32_t SystemCoreClock;
    uint32_t start      = DWT_CYCCNT;
    uint32_t delayTicks = us * (SystemCoreClock / 1000000U);
    while ((DWT_CYCCNT - start) < delayTicks) { /* busy-wait */ }
}

/* -------------------------------------------------------
 * Private forward declarations
 * ------------------------------------------------------- */
static uint16_t       NJW1195A_BuildControlWord(NJW1195A_HandleTypeDef *hnjw,
                                                 uint8_t address, uint8_t data);
static HAL_StatusTypeDef NJW1195A_SendCommand(NJW1195A_HandleTypeDef *hnjw,
                                               uint8_t address, uint8_t data);
static HAL_StatusTypeDef NJW1195A_SendCommand_DMA(NJW1195A_HandleTypeDef *hnjw,
                                                   uint8_t address, uint8_t data);
static void           AMP_EN_Pins_EXTI_Init(NJW1195A_HandleTypeDef *hnjw);

/* ============================================================
 * Public API implementation
 * ============================================================ */

/**
 * @brief  Full power-on and initialization sequence.
 *
 *  Phase 1 – NJW1195A_Core_Config():
 *    Powers the chip, applies MUTE to all channels, selects default input.
 *
 *  Phase 2 – 400 ms stabilization delay:
 *    Waits for SEPIC/Cük converter output rails to settle fully.
 *    (NJW1195A is already in MUTE so no pop noise is possible here.)
 *
 *  Phase 3 – EXTI handover:
 *    Re-configures AMP_EN GPIOs as interrupt inputs so the mechanical
 *    headphone detection switch can take control.
 */
HAL_StatusTypeDef NJW1195A_Init(NJW1195A_HandleTypeDef *hnjw)
{
    if (hnjw == NULL) return HAL_ERROR;

    /* Mark uninitialized until everything succeeds */
    hnjw->IsInitialized = 0U;

    /* Start DWT cycle counter for accurate SPI timing delays */
    DWT_Init();

    /* Phase 1: Configure chip, all channels muted */
    HAL_StatusTypeDef status = NJW1195A_Core_Config(hnjw);
    if (status != HAL_OK) return status;

    /* Phase 2: Wait for analog power rails to stabilize */
    USB_LOG_INFO("Waiting for AMP power rails to stabilize...\r\n");
    HAL_Delay(400U);

    /* Phase 3: Hand AMP_EN GPIOs over to EXTI (headphone detection) */
    USB_LOG_INFO("Ready to switch EXTI...\r\n");
    AMP_EN_Pins_EXTI_Init(hnjw);
    USB_LOG_INFO("EXTI switched successfully.\r\n");

    hnjw->IsInitialized = 1U;

    USB_LOG_INFO("NJW1195A ready. Mode: %s\r\n",
                 hnjw->IsDiffMode ? "Differential" : "Single-Ended");

    return HAL_OK;
}

/**
 * @brief  Low-level hardware bring-up: power rail, SPI defaults, MUTE all
 *         channels, set default input.
 *
 * @note   Intentionally bypasses the IsInitialized guard so it can be called
 *         before the flag is set.  All internal calls go through
 *         NJW1195A_SendCommand() directly.
 */
HAL_StatusTypeDef NJW1195A_Core_Config(NJW1195A_HandleTypeDef *hnjw)
{
    if (hnjw == NULL || hnjw->LatchPort == NULL || hnjw->hspi == NULL) {
        return HAL_ERROR;
    }

    /* Enable power rail */
    HAL_GPIO_WritePin(hnjw->PW_EN_Port, hnjw->PW_EN_Pin, GPIO_PIN_SET);

    /* LATCH idles HIGH (data is only clocked in on the rising LATCH edge) */
    HAL_GPIO_WritePin(hnjw->LatchPort, hnjw->LatchPin, GPIO_PIN_SET);

    /* Drive chip-address pins and cache the resolved address */
    uint8_t addr = hnjw->ChipAddress & 0x03U; // 合法范围 0-3

    if (hnjw->ADR0_Port != NULL) {
        HAL_GPIO_WritePin(hnjw->ADR0_Port, hnjw->ADR0_Pin,
                          (addr & 0x01U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
    if (hnjw->ADR1_Port != NULL) {
        HAL_GPIO_WritePin(hnjw->ADR1_Port, hnjw->ADR1_Pin,
                          (addr & 0x02U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }

    /* Allow power rail to settle before first SPI transaction */
    HAL_Delay(100U);

    /* Clear runtime state */
    hnjw->IsBusy          = 0U;
    hnjw->QueuedCommands  = 0U;

    USB_LOG_INFO("NJW1195A core init (chip address: 0x%01X)\r\n", hnjw->ChipAddress);

    HAL_StatusTypeDef status;

    /* ---- Mute all active volume channels ---- */
    status = NJW1195A_SendCommand(hnjw, NJW1195A_REG_VOL_CH1, NJW1195A_VOL_MUTE);
    if (status != HAL_OK) return status;
    HAL_Delay(2U);

    status = NJW1195A_SendCommand(hnjw, NJW1195A_REG_VOL_CH2, NJW1195A_VOL_MUTE);
    if (status != HAL_OK) return status;
    HAL_Delay(2U);

    /* In differential mode registers 0x02/0x03 are "No Acceptable"
     * per datasheet p.12; skip them entirely.                       */
    if (!hnjw->IsDiffMode) {
        status = NJW1195A_SendCommand(hnjw, NJW1195A_REG_VOL_CH3, NJW1195A_VOL_MUTE);
        if (status != HAL_OK) return status;
        HAL_Delay(2U);

        status = NJW1195A_SendCommand(hnjw, NJW1195A_REG_VOL_CH4, NJW1195A_VOL_MUTE);
        if (status != HAL_OK) return status;
        HAL_Delay(2U);
    }

    /* ---- Set default input selectors ----
     *
     * [F1] Differential mode truth table (datasheet p.18):
     *   Input 1: SEL1 = 010 (0x02), SEL2 = 001 (0x01)  -> 0x44
     *   Input 2: SEL1 = 011 (0x03), SEL2 = 100 (0x04)  -> 0x70
     *
     * Single-ended mode truth table (datasheet p.18):
     *   Input N: D15-D13 = N-1 encoded as per NJW1195A_INPUT_x macros
     */

    uint8_t initDataA, initDataB;

    if (hnjw->IsDiffMode)
    {
        /* Differential Mode：DefaultInput only 0 (Mute), 1 or 2 */
        switch (hnjw->DefaultInput)
        {
        case 1U:
            initDataA = NJW1195A_DIFF_INPUT_1;
            break;
        case 2U:
            initDataA = NJW1195A_DIFF_INPUT_2;
            break;
        default:
            initDataA = NJW1195A_DIFF_INPUT_MUTE;
            break;
        }
        initDataB = initDataA;
    }
    else
    {
        /* Single End Mode：DefaultInput can be 0 (Mute), 1~4 */
        uint8_t sel = (hnjw->DefaultInput <= 4U) ? hnjw->DefaultInput : 0U;
        initDataA = ((sel & 0x07U) << 5) | ((sel & 0x07U) << 2);
        initDataB = initDataA;
    }

    status = NJW1195A_SendCommand(hnjw, NJW1195A_REG_SEL_1A_2A, initDataA);
    if (status != HAL_OK) return status;
    HAL_Delay(2U);

    status = NJW1195A_SendCommand(hnjw, NJW1195A_REG_SEL_1B_2B, initDataB);
    if (status != HAL_OK) return status;
    // HAL_Delay(2U);


    USB_LOG_INFO("NJW1195A core configuration complete.\r\n");
    return HAL_OK;
}

/* -------------------------------------------------------
 * Volume control
 * ------------------------------------------------------- */

/**
 * @brief  Set one volume channel (blocking).
 */
HAL_StatusTypeDef NJW1195A_SetVolume(NJW1195A_HandleTypeDef *hnjw,
                                      uint8_t channel, uint8_t level)
{
    /* [F6] Public API checks IsInitialized; internal paths call SendCommand directly */
    if (hnjw == NULL || hnjw->hspi == NULL || !hnjw->IsInitialized) return HAL_ERROR;
    if (channel > NJW1195A_REG_VOL_CH4) return HAL_ERROR;

    return NJW1195A_SendCommand(hnjw, channel, level);
}

/**
 * @brief  Set one volume channel via DMA (non-blocking).
 *         Returns HAL_BUSY if the DMA channel is still occupied.
 */
HAL_StatusTypeDef NJW1195A_SetVolume_DMA(NJW1195A_HandleTypeDef *hnjw,
                                           uint8_t channel, uint8_t level)
{
    if (hnjw == NULL || hnjw->hspi == NULL || !hnjw->IsInitialized) return HAL_ERROR;
    if (hnjw->IsBusy) return HAL_BUSY;
    if (channel > NJW1195A_REG_VOL_CH4) return HAL_ERROR;

    return NJW1195A_SendCommand_DMA(hnjw, channel, level);
}

/**
 * @brief  Set all active volume channels to the same level (blocking).
 */
HAL_StatusTypeDef NJW1195A_SetAllVolumes(NJW1195A_HandleTypeDef *hnjw, uint8_t level)
{
    if (hnjw == NULL || hnjw->hspi == NULL || !hnjw->IsInitialized) return HAL_ERROR;

    HAL_StatusTypeDef status;

    status = NJW1195A_SendCommand(hnjw, NJW1195A_REG_VOL_CH1, level);
    if (status != HAL_OK) return status;
    HAL_Delay(2U);

    status = NJW1195A_SendCommand(hnjw, NJW1195A_REG_VOL_CH2, level);
    if (status != HAL_OK) return status;

    /* Differential mode: registers 0x02/0x03 are not valid, stop here */
    if (hnjw->IsDiffMode) return HAL_OK;

    HAL_Delay(2U);

    status = NJW1195A_SendCommand(hnjw, NJW1195A_REG_VOL_CH3, level);
    if (status != HAL_OK) return status;
    HAL_Delay(2U);

    status = NJW1195A_SendCommand(hnjw, NJW1195A_REG_VOL_CH4, level);
    return status;
}

/* -------------------------------------------------------
 * Input selector control
 * ------------------------------------------------------- */

/**
 * @brief  Configure input selectors — single-ended mode only.
 */
HAL_StatusTypeDef NJW1195A_SetInput(NJW1195A_HandleTypeDef *hnjw,
                                     uint8_t selector1A, uint8_t selector2A,
                                     uint8_t selector1B, uint8_t selector2B)
{
    if (hnjw == NULL || hnjw->hspi == NULL || !hnjw->IsInitialized) return HAL_ERROR;
    if (hnjw->IsDiffMode) return HAL_ERROR; /* Wrong API for differential mode */

    /* Build 8-bit data byte:  D15-D13 = SEL1 (bits [7:5]),
     *                         D12-D10 = SEL2 (bits [4:2]),
     *                         D9 -D8  = don't-care (0) */
    uint8_t dataA = ((selector1A & 0x07U) << 5) | ((selector2A & 0x07U) << 2);
    uint8_t dataB = ((selector1B & 0x07U) << 5) | ((selector2B & 0x07U) << 2);

    HAL_StatusTypeDef status;

    status = NJW1195A_SendCommand(hnjw, NJW1195A_REG_SEL_1A_2A, dataA);
    if (status != HAL_OK) return status;
    HAL_Delay(2U);

    return NJW1195A_SendCommand(hnjw, NJW1195A_REG_SEL_1B_2B, dataB);
}

/**
 * @brief  Configure input selector — differential mode only.
 *
 * @param  selector  0 = Mute, 1 = Differential Input 1, 2 = Differential Input 2
 *
 * [F1] Corrected truth table encoding per datasheet p.18:
 *   Input 1: SEL1A = 010 (0x02), SEL2A = 001 (0x01)
 *   Input 2: SEL1A = 011 (0x03), SEL2A = 100 (0x04)
 *
 * [F5] Removed duplicate NULL-guard that existed before the mode check.
 */
HAL_StatusTypeDef NJW1195A_SetInput_Diff(NJW1195A_HandleTypeDef *hnjw,
                                          uint8_t selector)
{
    /* Single combined guard — covers NULL, uninitialized, AND wrong mode */
    if (hnjw == NULL || hnjw->hspi == NULL ||
        !hnjw->IsInitialized || !hnjw->IsDiffMode) {
        return HAL_ERROR;
    }

    uint8_t dataA, dataB;

    switch (selector) {
        case 1U:
            dataA = NJW1195A_DIFF_INPUT_1;  /* (0x02<<5)|(0x01<<2) = 0x44 */
            break;
        case 2U:
            dataA = NJW1195A_DIFF_INPUT_2;  /* (0x03<<5)|(0x04<<2) = 0x70 */
            break;
        default: /* 0 or any invalid value -> Mute */
            dataA = NJW1195A_DIFF_INPUT_MUTE;
            break;
    }
    dataB = dataA; /* A and B channels share the same selector in diff mode */

    HAL_StatusTypeDef status;
    status = NJW1195A_SendCommand(hnjw, NJW1195A_REG_SEL_1A_2A, dataA);
    if (status != HAL_OK) return status;
    HAL_Delay(2U);

    return NJW1195A_SendCommand(hnjw, NJW1195A_REG_SEL_1B_2B, dataB);
}

/* -------------------------------------------------------
 * DMA command queue
 * [F7] Complete implementation of the queue that was declared
 *      in the header but never implemented in the original code.
 * ------------------------------------------------------- */

/**
 * @brief  Enqueue a volume command for deferred DMA dispatch.
 *         If the bus is idle the command is sent immediately;
 *         otherwise it is stored for dispatch from TxCpltCallback.
 *
 * @return HAL_ERROR  if the queue is full (max 4 pending commands).
 */
HAL_StatusTypeDef NJW1195A_EnqueueVolume(NJW1195A_HandleTypeDef *hnjw,
                                          uint8_t channel, uint8_t level)
{
    if (hnjw == NULL || !hnjw->IsInitialized) return HAL_ERROR;
    if (channel > NJW1195A_REG_VOL_CH4)       return HAL_ERROR;

    __disable_irq(); /* 关中断防止被主循环打断 */

    /* If bus is free, fire immediately */
    // if (!hnjw->IsBusy) {
    //     hnjw->IsBusy = 1U;
    //     __enable_irq();
    //     return NJW1195A_SendCommand_DMA(hnjw, channel, level);
    // }

    /* Otherwise enqueue (queue depth = 4) */
    if (hnjw->QueuedCommands >= 4U) {
        __enable_irq();
        return HAL_ERROR; /* Queue full */
    }

    hnjw->QueuedChannels[hnjw->QueuedCommands] = channel;
    hnjw->QueuedLevels  [hnjw->QueuedCommands] = level;
    hnjw->QueuedCommands++;

    __enable_irq();
    return HAL_OK;
}

/**
 * @brief  无阻塞入队：配置差分模式下的输入信源
 * @param  selector  0 = Mute, 1 = Diff Input 1, 2 = Diff Input 2
 */
HAL_StatusTypeDef NJW1195A_EnqueueInput_Diff(NJW1195A_HandleTypeDef *hnjw, uint8_t selector)
{
    if (hnjw == NULL || !hnjw->IsInitialized || !hnjw->IsDiffMode) return HAL_ERROR;

    uint8_t dataA;
    switch (selector) {
        case 1U: dataA = NJW1195A_DIFF_INPUT_1; break;  /* 0x44 */
        case 2U: dataA = NJW1195A_DIFF_INPUT_2; break;  /* 0x70 */
        default: dataA = NJW1195A_DIFF_INPUT_MUTE; break; /* 0x00 */
    }

    __disable_irq(); 

    /* 信源切换需要连续发2帧 (0x04 和 0x05寄存器)，检查队列是否至少有2个空位 */
    /* 强烈建议在头文件里把队列数组大小改为 8 (uint8_t QueuedChannels[8]) */
    if ((hnjw->QueuedCommands + 2U) > 8U) { 
        __enable_irq();
        return HAL_ERROR; /* 队列满 */
    }

    /* 压入第一帧：Selector 1A/2A (寄存器 0x04) */
    hnjw->QueuedChannels[hnjw->QueuedCommands] = NJW1195A_REG_SEL_1A_2A;
    hnjw->QueuedLevels  [hnjw->QueuedCommands] = dataA;
    hnjw->QueuedCommands++;

    /* 压入第二帧：Selector 1B/2B (寄存器 0x05) */
    hnjw->QueuedChannels[hnjw->QueuedCommands] = NJW1195A_REG_SEL_1B_2B;
    hnjw->QueuedLevels  [hnjw->QueuedCommands] = dataA;
    hnjw->QueuedCommands++;

    __enable_irq();
    
    return HAL_OK;
}

/* -------------------------------------------------------
 * DMA completion callback
 * ------------------------------------------------------- */

/**
 * @brief  Call this from HAL_SPI_TxCpltCallback() in your application.
 *
 * [F4] The LATCH rising edge is applied here, AFTER the DMA transfer
 *      completes, so t4 (LATCH Rise Hold Time, min 4 µs) is naturally
 *      satisfied by the SPI bus turnaround time.  No blocking delay is
 *      needed inside this ISR.  At SPI clock ≤ 4 MHz the 16-bit transfer
 *      takes ≥ 4 µs on its own, meeting t4 without any additional wait.
 *
 * [F7] Queue bounds-check added to prevent out-of-bounds array access.
 */
void NJW1195A_TxCpltCallback(NJW1195A_HandleTypeDef *hnjw)
{
    if (hnjw == NULL) return;

    /* Assert LATCH high -> NJW1195A latches the received data */
    HAL_GPIO_WritePin(hnjw->LatchPort, hnjw->LatchPin, GPIO_PIN_SET);

    hnjw->IsBusy = 0U;

    // /* Dispatch next queued command (if any) */
    // if (hnjw->QueuedCommands > 0U && hnjw->QueuedCommands <= 4U) {
    //     uint8_t nextChannel = hnjw->QueuedChannels[0];
    //     uint8_t nextLevel   = hnjw->QueuedLevels[0];

    //     /* Shift queue left by one position */
    //     for (uint8_t i = 0U; i < (hnjw->QueuedCommands - 1U); i++) {
    //         hnjw->QueuedChannels[i] = hnjw->QueuedChannels[i + 1U];
    //         hnjw->QueuedLevels  [i] = hnjw->QueuedLevels  [i + 1U];
    //     }
    //     hnjw->QueuedCommands--;

    //     DWT_Delay_us(2U);

    //     /* Send: ignore return value inside ISR; failures drop the command */
    //     (void)NJW1195A_SendCommand_DMA(hnjw, nextChannel, nextLevel);
    // }
}

/**
 * @brief  Process pending volume commands in the queue.
 * @note   Must be called in main loop (while 1) or RTOS task, never in ISR!
 */
void NJW1195A_ProcessQueue(NJW1195A_HandleTypeDef *hnjw)
{
    if (hnjw == NULL || !hnjw->IsInitialized) return;

    __disable_irq(); /* 扩大临界区，把状态判断包进来 */
    if (hnjw->IsBusy == 0U && hnjw->QueuedCommands > 0U) {
        
        uint8_t nextChannel = hnjw->QueuedChannels[0];
        uint8_t nextLevel   = hnjw->QueuedLevels[0];

        /* 队列前移 */
        for (uint8_t i = 0U; i < (hnjw->QueuedCommands - 1U); i++) {
            hnjw->QueuedChannels[i] = hnjw->QueuedChannels[i + 1U];
            hnjw->QueuedLevels  [i] = hnjw->QueuedLevels  [i + 1U];
        }
        hnjw->QueuedCommands--;
        
        /* 提前抢占总线状态，防止中断趁虚而入！ */
        hnjw->IsBusy = 1U; 
        
        __enable_irq();

        DWT_Delay_us(2U); 

        /* 调用底层 DMA（需要把 SendCommand_DMA 里的 IsBusy=1 删掉或保留都行，这里已经提前置位了） */
        NJW1195A_SendCommand_DMA(hnjw, nextChannel, nextLevel);
    } else {
        __enable_irq();
    }
    // if (hnjw == NULL || !hnjw->IsInitialized) return;

    // /* 如果 SPI 空闲，且队列中有任务待办 */
    // if (hnjw->IsBusy == 0U && hnjw->QueuedCommands > 0U) {
        
    //     /* 关中断保护队列操作的原子性 */
    //     __disable_irq();
        
    //     uint8_t nextChannel = hnjw->QueuedChannels[0];
    //     uint8_t nextLevel   = hnjw->QueuedLevels[0];

    //     /* 队列前移 */
    //     for (uint8_t i = 0U; i < (hnjw->QueuedCommands - 1U); i++) {
    //         hnjw->QueuedChannels[i] = hnjw->QueuedChannels[i + 1U];
    //         hnjw->QueuedLevels  [i] = hnjw->QueuedLevels  [i + 1U];
    //     }
    //     hnjw->QueuedCommands--;
        
    //     __enable_irq();

    //     /* 因为这是在主循环中，短暂阻塞完全无害！
    //      * 强制延时 2us，确保上一次中断拉高 LATCH 后，至少维持了 1.6us 的高电平 (t8) 
    //      */
    //     DWT_Delay_us(2U); 

    //     /* 发出下一个 DMA 传输请求 */
    //     NJW1195A_SendCommand_DMA(hnjw, nextChannel, nextLevel);
    // }
}

/* -------------------------------------------------------
 * dB to register conversion
 * ------------------------------------------------------- */

/**
 * @brief  Convert a floating-point dB value to the NJW1195A volume register byte.
 *
 * [F8] Corrected inline examples to match the actual formula output:
 *        reg = 0x40 - round(dB * 2)
 *        +10.0 dB -> 0x2C,  0.0 dB -> 0x40,  -10.0 dB -> 0x54
 *
 * @note   This function intentionally does NOT return the Mute codes (0x00 /
 *         0xFF).  Use NJW1195A_VOL_MUTE directly for explicit mute.
 */
uint8_t NJW1195A_dBToRegister(float dB)
{
    if (dB >= 31.5f) {
        return NJW1195A_VOL_PLUS_31_5DB; /* 0x01 */
    }
    if (dB <= -95.0f) {
        return NJW1195A_VOL_MINUS_95DB;  /* 0xFE */
    }

    /* Formula: reg = 0x40 - (dB * 2)
     *   +10 dB -> 0x40 - 20 = 0x2C  (decimal 44)
     *    0 dB -> 0x40 -  0 = 0x40  (decimal 64)
     *  -10 dB -> 0x40 + 20 = 0x54  (decimal 84)
     */
    int32_t regValue = (int32_t)0x40 - (int32_t)(dB * 2.0f);

    /* Clamp to valid hardware range [0x01 .. 0xFE] */
    if (regValue < (int32_t)0x01) regValue = (int32_t)0x01;
    if (regValue > (int32_t)0xFE) regValue = (int32_t)0xFE;

    USB_LOG_INFO("dBToRegister: %.1f dB -> 0x%02X\r\n", dB, (unsigned)regValue);
    return (uint8_t)regValue;
}

/* ============================================================
 * Private helper implementations
 * ============================================================ */

/**
 * @brief  Assemble the 16-bit NJW1195A control word.
 *
 * [F9] Uses the header-defined masks/shifts for readability and consistency.
 *
 * Control word layout (MSB first, datasheet p.11-12):
 *   D15-D8  : Data byte (volume level or selector encoding)
 *   D7 -D4  : Register select address (4 bits)
 *   D3 -D0  : Chip address (4 bits, from ADR1/ADR0 pins)
 */
static uint16_t NJW1195A_BuildControlWord(NJW1195A_HandleTypeDef *hnjw,
                                           uint8_t address, uint8_t data)
{
    return (((uint16_t)data    << NJW1195A_DATA_SHIFT) & NJW1195A_DATA_MASK)    |
           (((uint16_t)address << NJW1195A_ADDR_SHIFT) & NJW1195A_ADDR_MASK)    |
           ((uint16_t)(hnjw->ChipAddress)               & NJW1195A_CHIPADDR_MASK);
}

/**
 * @brief  Transmit a 16-bit control word to the NJW1195A via blocking SPI.
 *
 * Timing (datasheet p.11):
 *   t7 (CLOCK Setup before LATCH falls) : min 1.6 µs  -> 2 µs supplied
 *   t4 (LATCH Rise Hold after last CLK) : min 4.0 µs  -> 5 µs supplied
 */
static HAL_StatusTypeDef NJW1195A_SendCommand(NJW1195A_HandleTypeDef *hnjw,
                                               uint8_t address, uint8_t data)
{
    uint16_t controlWord = NJW1195A_BuildControlWord(hnjw, address, data);

    uint8_t txBuffer[2];
    txBuffer[0] = (uint8_t)(controlWord >> 8);   /* High byte: Data */
    txBuffer[1] = (uint8_t)(controlWord & 0xFFU); /* Low  byte: Address + ChipAddr */

    /* Assert LATCH low to begin serial frame */
    HAL_GPIO_WritePin(hnjw->LatchPort, hnjw->LatchPin, GPIO_PIN_RESET);

    /* t7: DATA/CLOCK setup time before first clock edge (min 1.6 µs) */
    DWT_Delay_us(2U);

    /* Transmit 16 bits, MSB first */
    HAL_StatusTypeDef status = HAL_SPI_Transmit(hnjw->hspi, txBuffer, 2U, 100U);

    /* t4: Hold time before LATCH rises (min 4 µs) */
    DWT_Delay_us(5U);

    /* Rising edge on LATCH causes NJW1195A to load the shift register */
    HAL_GPIO_WritePin(hnjw->LatchPort, hnjw->LatchPin, GPIO_PIN_SET);

    return status;
}

/**
 * @brief  Begin a DMA-driven 16-bit transfer to the NJW1195A.
 *
 * [F2] If HAL_SPI_Transmit_DMA() fails, IsBusy is cleared immediately so
 *      the driver does not dead-lock.
 *
 * [F4] LATCH is NOT raised here.  It is raised in NJW1195A_TxCpltCallback()
 *      after the DMA ISR fires, keeping the ISR free of blocking delays.
 */
static HAL_StatusTypeDef NJW1195A_SendCommand_DMA(NJW1195A_HandleTypeDef *hnjw,
                                                   uint8_t address, uint8_t data)
{
    uint16_t controlWord = NJW1195A_BuildControlWord(hnjw, address, data);

    hnjw->TxBuffer[0] = (uint8_t)(controlWord >> 8);
    hnjw->TxBuffer[1] = (uint8_t)(controlWord & 0xFFU);

    hnjw->IsBusy = 1U;

    /* Assert LATCH low to begin serial frame */
    HAL_GPIO_WritePin(hnjw->LatchPort, hnjw->LatchPin, GPIO_PIN_RESET);

    /* t7: setup time before first clock edge */
    DWT_Delay_us(2U);

    HAL_StatusTypeDef status =
        HAL_SPI_Transmit_DMA(hnjw->hspi, (uint8_t *)hnjw->TxBuffer, 2U);

    /* [F2] Release busy flag on failure so the driver never dead-locks */
    if (status != HAL_OK) {
        HAL_GPIO_WritePin(hnjw->LatchPort, hnjw->LatchPin, GPIO_PIN_SET); /* restore LATCH */
        hnjw->IsBusy = 0U;
    }

    return status;
}

/**
 * @brief  Reconfigure SE_EN and BAL_EN GPIOs as EXTI interrupt inputs so the
 *         mechanical headphone-jack detection switch can drive them.
 *
 * [F3] Uses hnjw->SE_EN_Port/Pin and hnjw->BAL_EN_Port/Pin instead of the
 *      original hard-coded GPIOB / GPIO_PIN_6 / GPIO_PIN_11 constants.
 *      This makes the driver portable across different PCB revisions.
 *
 * @note   EXTI IRQ lines and NVIC priorities are derived from the actual pin
 *         numbers stored in the handle.  For pins ≤ 9 use EXTI9_5_IRQn (or
 *         EXTI[n]_IRQn for pins 0-4); for pins 10-15 use EXTI15_10_IRQn.
 *         Adjust the IRQn selection logic below if your pin numbering differs.
 */
static void AMP_EN_Pins_EXTI_Init(NJW1195A_HandleTypeDef *hnjw)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* --- Configure SE_EN pin as EXTI input --- */
    GPIO_InitStruct.Pin  = hnjw->SE_EN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING; /* detect both insert and remove */
    GPIO_InitStruct.Pull = GPIO_NOPULL;                 /* rely on external hardware pull */
    HAL_GPIO_Init(hnjw->SE_EN_Port, &GPIO_InitStruct);

    /* Clear any spurious flag generated during the mode transition */
    __HAL_GPIO_EXTI_CLEAR_IT(hnjw->SE_EN_Pin);

    /* --- Configure BAL_EN pin as EXTI input --- */
    GPIO_InitStruct.Pin = hnjw->BAL_EN_Pin;
    HAL_GPIO_Init(hnjw->BAL_EN_Port, &GPIO_InitStruct);
    __HAL_GPIO_EXTI_CLEAR_IT(hnjw->BAL_EN_Pin);

    /* --- Enable NVIC lines for both pins ---
     * STM32G4 EXTI line mapping:
     *   Pins  0- 4 : individual IRQs (EXTI0_IRQn .. EXTI4_IRQn)
     *   Pins  5- 9 : EXTI9_5_IRQn
     *   Pins 10-15 : EXTI15_10_IRQn
     *
     * Helper lambda (inline): returns the correct IRQn for a given pin mask.
     */
    auto_IRQn_select:;  /* label only to attach the comment; not a real goto target */

    /* SE_EN NVIC */
    IRQn_Type seIRQn;
    if      (hnjw->SE_EN_Pin <= GPIO_PIN_4)  { seIRQn = (IRQn_Type)((int)EXTI0_IRQn + __builtin_ctz(hnjw->SE_EN_Pin)); }
    else if (hnjw->SE_EN_Pin <= GPIO_PIN_9)  { seIRQn = EXTI9_5_IRQn; }
    else                                     { seIRQn = EXTI15_10_IRQn; }

    HAL_NVIC_SetPriority(seIRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(seIRQn);
    USB_LOG_INFO("SE_EN (pin mask 0x%04X) -> IRQn %d enabled.\r\n",
                 hnjw->SE_EN_Pin, (int)seIRQn);

    /* BAL_EN NVIC */
    IRQn_Type balIRQn;
    if      (hnjw->BAL_EN_Pin <= GPIO_PIN_4)  { balIRQn = (IRQn_Type)((int)EXTI0_IRQn + __builtin_ctz(hnjw->BAL_EN_Pin)); }
    else if (hnjw->BAL_EN_Pin <= GPIO_PIN_9)  { balIRQn = EXTI9_5_IRQn; }
    else                                      { balIRQn = EXTI15_10_IRQn; }

    /* Only enable if different from SE_EN (avoid double-enabling same IRQ) */
    if (balIRQn != seIRQn) {
        HAL_NVIC_SetPriority(balIRQn, 5U, 0U);
        HAL_NVIC_EnableIRQ(balIRQn);
    }
    USB_LOG_INFO("BAL_EN (pin mask 0x%04X) -> IRQn %d enabled.\r\n",
                 hnjw->BAL_EN_Pin, (int)balIRQn);

    USB_LOG_INFO("AMP_EN EXTI handover complete.\r\n");
}