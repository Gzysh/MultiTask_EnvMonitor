#include "driver_timer.h"
#include "stm32f1xx_hal.h"
#include "tim.h"

extern void IRReceiver_IRQ_Callback(void);
extern void RotaryEncoder_IRQ_Callback(void);

/**********************************************************************
 * 函数名称： HAL_GPIO_EXTI_Callback
 * 功能描述： 外部中断的中断回调函数
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 无
 ***********************************************************************/
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    switch (GPIO_Pin)
    {
        case GPIO_PIN_10:
        {
            IRReceiver_IRQ_Callback();
            break;
        }

        case GPIO_PIN_12:
        {
            RotaryEncoder_IRQ_Callback();
            break;
        }

        default:
        {
            break;
        }
    }
}

