#include "driver_ir_obstacle.h"
#include "driver_lcd.h"
#include "driver_timer.h"
#include "stm32f1xx_hal.h"
#include "tim.h"
#include "adc.h"

#define IROBSTACLE_GPIO_GROUP GPIOB
#define IROBSTACLE_GPIO_PIN   GPIO_PIN_13

/**********************************************************************
 * 函数名称： IRObstacle_Init
 * 功能描述： 红外避障模块的初始化函数
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 无
 ***********************************************************************/
void IRObstacle_Init(void)
{
    /* PB13在MX_GPIO_Init里被初始化为输入引脚了 */
}

/**********************************************************************
 * 函数名称： IRObstacle_Write
 * 功能描述： 红外避障模块的发送函数
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 0 - 碰到障碍物, 1 - 没有碰到
 ***********************************************************************/
int IRObstacle_Read(void)
{
    if (GPIO_PIN_RESET == HAL_GPIO_ReadPin(IROBSTACLE_GPIO_GROUP, IROBSTACLE_GPIO_PIN))
        return 0;
    else
        return 1;
}


/**********************************************************************
 * 函数名称： IRObstacle_Test
 * 功能描述： 红外避障模块测试程序
 * 输入参数： 无
 * 输出参数： 无
 *            无
 * 返 回 值： 无
 ***********************************************************************/
void IRObstacle_Test(void)
{
    
    IRObstacle_Init();

    while (1)
    {
        LCD_PrintString(0, 0, "IRObstacle: ");

        if (!IRObstacle_Read())
        {
            LCD_PrintString(0, 2, "Crashing");
        }
        else
        {
            LCD_PrintString(0, 2, "Safe      ");            
        }
    }
}

