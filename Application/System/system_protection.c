#include "system_protection.h"
#include "usb_config.h"
#include "usb_log.h"
#include "gpio.h"

void System_PowerDown_Handler(void)
{
    // USB_LOG_INFO("Power Down Interrupt Triggered!\r\n");
    /* 1. 预先设置低电平：
     * 虽然现在引脚还是 EXTI 输入模式，但这行代码会提前把 0 写进 ODR (输出数据寄存器)。
     * 这样在下一步切换为输出模式的瞬间，引脚会毫无缝隙地直接输出低电平。
     */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_11, GPIO_PIN_RESET);

    /* 2. 重新配置为推挽输出 (Push-Pull) */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;        // 强推挽输出
    GPIO_InitStruct.Pull = GPIO_NOPULL;                // 不需要内部上下拉
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH; // 最高响应速度
    
    // 瞬间强行接管控制权，运放被强制 MUTE
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* 3. 保险起见，再次明确输出低电平 */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_11, GPIO_PIN_RESET);

    /* 4. 打印掉电日志 (注意：因为马上就没电了，这句Log不一定能完整发完到电脑上) */
    USB_LOG_INFO("Power Down Interrupt Triggered! AMP forcefully MUTED.\r\n");

    /* 5. 【终极防御：死亡循环】
     * 此时运放已经安全关闭，正负电源还在正常工作。
     * 我们必须把 CPU 永远卡在这里，防止它退回主循环去执行其他可能导致电平翻转的代码，
     * 直到几毫秒/几十毫秒后电源彻底耗尽，MCU 物理关机。
     */
    while (1)
    {
        // 静静等待系统断电...
    }
}
