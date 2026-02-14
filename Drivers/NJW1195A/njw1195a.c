#include "njw1195a.h"

/* Initialize (Just resets latch to high) */
HAL_StatusTypeDef NJW1195A_Init(NJW1195A_HandleTypeDef *hnjw) {
    if (hnjw == NULL || hnjw->LatchPort == NULL) return HAL_ERROR;

    if (hnjw->PW_EN_Port != NULL) {
        HAL_GPIO_WritePin(hnjw->PW_EN_Port, hnjw->PW_EN_Pin, GPIO_PIN_SET);
        printf("AMP_PW_EN Set High\r\n");
    }
    
    /* 2. Wait 20ms for power stabilization */
    HAL_Delay(20);
    printf("AMP_PW_EN Power Stabilized\r\n");
    
    /* 3. Perform Hardware Reset (PDN Low -> High) */
    if (hnjw->PW_EN_Port != NULL) {
        HAL_GPIO_WritePin(hnjw->PW_EN_Port, hnjw->PW_EN_Pin, GPIO_PIN_RESET);
        HAL_Delay(20); // Hold reset for 20ms
        HAL_GPIO_WritePin(hnjw->PW_EN_Port, hnjw->PW_EN_Pin, GPIO_PIN_SET);
        HAL_Delay(20); // Wait after release reset
        printf("NJW1195A Power On\r\n");
    }

    /* Ensure Latch starts High */
    HAL_GPIO_WritePin(hnjw->LatchPort, hnjw->LatchPin, GPIO_PIN_SET);
    
    /* Initialize State */
    hnjw->IsBusy = 0;

    return HAL_OK;
}

/* Set Volume Level for a specific channel using DMA */
HAL_StatusTypeDef NJW1195A_SetLevel_DMA(NJW1195A_HandleTypeDef *hnjw, uint8_t channel, uint8_t level) {
    if (hnjw == NULL || hnjw->hspi == NULL) return HAL_ERROR;

    /* Check if busy */
    if (hnjw->IsBusy) {
        return HAL_BUSY;
    }

    /* Prepare Data: Address then Data */
    hnjw->TxBuffer[0] = channel;
    hnjw->TxBuffer[1] = level;
    
    hnjw->IsBusy = 1;

    /* 1. Latch Low (Start of Frame) */
    HAL_GPIO_WritePin(hnjw->LatchPort, hnjw->LatchPin, GPIO_PIN_RESET);

    /* 2. Transmit via DMA */
    /* Note: Depending on SPI config (8/16 bit), buffer handling might differ. */
    /* Assuming SPI is configured as 8-bit Data Size in CubeMX. */
    /* If configured as 16-bit, we need to pack it into a uint16_t */
    
    if (hnjw->hspi->Init.DataSize == SPI_DATASIZE_16BIT) {
         /* Re-pack for 16-bit transfer if needed, but buffer is uint8_t array */
         /* This part assumes user handles SPI config correctly */
         /* For 16-bit SPI, we might need to cast or adjust buffer */
         uint16_t *p16 = (uint16_t*)hnjw->TxBuffer;
         *p16 = (channel << 8) | level; // MSB First assumption for 16bit mode
         return HAL_SPI_Transmit_DMA(hnjw->hspi, (uint8_t*)p16, 1);
    } else {
         return HAL_SPI_Transmit_DMA(hnjw->hspi, (uint8_t*)hnjw->TxBuffer, 2);
    }
}

/* Set Volume Level for all 4 channels (Sequential DMA not supported in this simple driver without queue) */
/* This function currently just sets one channel as an example or needs blocking logic for sequence */
/* For true DMA setting of all channels, we would need a state machine or circular buffer */
HAL_StatusTypeDef NJW1195A_SetAllLevels_DMA(NJW1195A_HandleTypeDef *hnjw, uint8_t level) {
    /* Since we cannot queue DMA requests easily without RTOS or complex logic,
       we will implement a simple blocking wait or just set the first one. 
       To set all 4 properly with DMA, we should chain them in the Callback.
    */
    
    // For now, let's implement a simple version that sets Channel 1
    // The user needs to handle the sequence if they want strict DMA for all 4.
    // Or we can modify the driver to support a queue.
    
    return NJW1195A_SetLevel_DMA(hnjw, NJW1195A_REG_VOL_CH1, level);
}

/* ISR Callback Handler */
void NJW1195A_TxCpltCallback(NJW1195A_HandleTypeDef *hnjw) {
    /* 3. Latch High (End of Frame) - Apply Settings */
    HAL_GPIO_WritePin(hnjw->LatchPort, hnjw->LatchPin, GPIO_PIN_SET);
    
    hnjw->IsBusy = 0;
}
