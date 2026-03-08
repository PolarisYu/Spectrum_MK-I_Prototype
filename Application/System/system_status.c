#include "system_status.h"
#include "tim.h"

/* Private defines */
// 设置系统状态灯亮度 (0-1000)
#define SET_STATUS_LED_BRIGHTNESS(x)  __HAL_TIM_SET_COMPARE(&htim15, TIM_CHANNEL_1, (x))

// 设置数据灯亮度 (0-1000)
#define SET_DATA_LED_BRIGHTNESS(x)    __HAL_TIM_SET_COMPARE(&htim15, TIM_CHANNEL_2, (x))

void System_Status_Init(void)
{
    /* 启动 TIM15 的两个通道 */
    HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_2);
}

/**
 * @brief Process the status LED breathing effect.
 */
void System_Status_Process(void) {
    static uint32_t last_tick = 0;
    static int16_t brightness = 0;
    static int8_t direction = 10; // 亮度变化步进

    // 每 10ms 更新一次亮度，产生平滑动画
    if (HAL_GetTick() - last_tick > 10) {
        last_tick = HAL_GetTick();

        brightness += direction;

        // 到达最亮或最暗时反转方向
        if (brightness >= 1000) {
            brightness = 1000;
            direction = -10; // 变暗
        } else if (brightness <= 0) {
            brightness = 0;
            direction = 10;  // 变亮
            // 可在此处添加额外延时，让灯灭一小会儿再亮，模拟人的呼吸停顿
        }

        SET_STATUS_LED_BRIGHTNESS(brightness);
    }
}
