#include "app.h"
#include "driver_ir_receiver.h"
#include "driver_active_buzzer.h"

#include "task.h"

/*
 * 红外键码 → key_cmd_t 映射表
 * 根据 NEC 协议红外遥控器定义
 */
typedef struct {
    uint8_t ir_code;
    key_cmd_t cmd;
} ir_key_map_t;

static const ir_key_map_t s_keyMap[] = {
    { 0x02, KEY_UP      },    /* ↑ / + 键 */
    { 0x98, KEY_DOWN    },    /* ↓ / - 键 */
    { 0xe0, KEY_LEFT    },    /* ← 左箭头 */
    { 0x90, KEY_RIGHT   },    /* → 右箭头 */
    { 0xc2, KEY_BACK    },    /* 左回旋键 → 返回功能 */
    { 0xa8, KEY_CONFIRM },    /* Play → 手动保存/确认 */
    { 0xe2, KEY_MENU    },    /* Menu 菜单 */
    { 0xa2, KEY_HOME    },    /* Power → 返回首页 */
};

#define KEY_MAP_COUNT   (sizeof(s_keyMap) / sizeof(s_keyMap[0]))

/**********************************************************************
 * 函数名称： map_ir_code
 * 功能描述： 将红外原始键码映射为统一的按键命令
 * 输入参数： ir_code - 红外接收器读取的原始键码
 * 输出参数： 无
 * 返 回 值： key_cmd_t 枚举值, 未识别则返回 KEY_NONE
 ***********************************************************************/
static key_cmd_t map_ir_code(uint8_t ir_code)
{
    for (int i = 0; i < KEY_MAP_COUNT; i++)
    {
        if (s_keyMap[i].ir_code == ir_code)
            return s_keyMap[i].cmd;
    }
    return KEY_NONE;
}

/**********************************************************************
 * 函数名称： KeyTask
 * 功能描述： 红外遥控输入任务
 *            循环读取红外遥控器, 映射为 key_cmd_t 后发队列
 * 输入参数： params - 未使用
 * 输出参数： 无
 * 返 回 值： 无
 ***********************************************************************/
void KeyTask(void *params)
{
    uint8_t dev, data;
    key_cmd_t cmd;
    static uint8_t s_buzzer_remain = 0;         /* 非阻塞蜂鸣器计数 */

    IRReceiver_Init();
    ActiveBuzzer_Init();

    for (;;)
    {
        /* 非阻塞蜂鸣器管理：每次循环递减计数 */
        if (s_buzzer_remain > 0)
        {
            if (--s_buzzer_remain == 0)
                ActiveBuzzer_Control(0);
        }

        if (IRReceiver_Read(&dev, &data) == 0)
        {
            cmd = map_ir_code(data);
            if (cmd != KEY_NONE)
            {
                /* 按键音效: 非阻塞方式，约 60ms(2 周期 × 30ms) */
                ActiveBuzzer_Control(1);
                s_buzzer_remain = 2;

                xQueueSend(xKeyQueue, &cmd, 0);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
