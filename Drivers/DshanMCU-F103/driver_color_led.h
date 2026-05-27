#ifndef _DRIVER_COLOR_LED_H
#define _DRIVER_COLOR_LED_H

#include <stdint.h>

/*
 * 预定义颜色 (24bit, 0x00RRGGBB)
 */
#define COLOR_NONE      0x000000    /* 全灭 */
#define COLOR_RED       0xFF0000
#define COLOR_GREEN     0x00FF00
#define COLOR_BLUE      0x0000FF
#define COLOR_YELLOW    0xFFFF00    /* R+G */
#define COLOR_CYAN      0x00FFFF    /* G+B */
#define COLOR_WHITE     0xFFFFFF

/**********************************************************************
 * 函数名称： ColorLED_Init
 * 功能描述： 全彩LED的初始化函数 (调用 MX_TIM2_Init)
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 无
 ***********************************************************************/
void ColorLED_Init(void);

/**********************************************************************
 * 函数名称： ColorLED_Start
 * 功能描述： 启动三个 PWM 通道 (MX_TIM2_Init 后调用一次即可)
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 无
 ***********************************************************************/
void ColorLED_Start(void);

/**********************************************************************
 * 函数名称： ColorLED_Set
 * 功能描述： 全彩LED设置颜色 (完整HAL配置, 适合初始化)
 * 输入参数： color - 24bit颜色,格式为0x00RRGGBB
 * 输出参数： 无
 * 返 回 值： 无
 ***********************************************************************/
void ColorLED_Set(uint32_t color);

/**********************************************************************
 * 函数名称： ColorLED_SetFast
 * 功能描述： 快速设置颜色 (直接写CCR寄存器, 无HAL重配置开销)
 *            必须在 ColorLED_Init / ColorLED_Start 之后调用
 * 输入参数： color - 24bit颜色, 0x00RRGGBB
 * 输出参数： 无
 * 返 回 值： 无
 ***********************************************************************/
void ColorLED_SetFast(uint32_t color);

/**********************************************************************
 * 函数名称： ColorLED_Test
 * 功能描述： 全彩LED测试程序
 * 输入参数： 无
 * 输出参数： 无
 *            无
 * 返 回 值： 无
 ***********************************************************************/
void ColorLED_Test(void);

#endif /* _DRIVER_COLOR_LED_H */

