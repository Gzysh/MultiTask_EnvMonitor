#include "app.h"
#include "driver_active_buzzer.h"
#include "driver_led.h"
#include "driver_color_led.h"

#include "task.h"

/*
 * 声音模式表
 * 每种模式定义: 响次数(cycles)、响时长(on_ms)、停时长(off_ms)
 * fast_on_ms/fast_off_ms/switch_ms: 急促模式 (0=不使用)
 */
typedef struct {
    int cycles;      /* 响次数                                */
    int on_ms;       /* 响时长(ms)                            */
    int off_ms;      /* 停时长(ms)                            */
    int fast_on_ms;  /* 急促模式响时长(ms), 0=不使用          */
    int fast_off_ms; /* 急促模式停时长(ms), 0=不使用          */
    int switch_ms;   /* 切换急促模式的阈值(ms), 0=不切换      */
} sound_pattern_t;

static const sound_pattern_t s_soundPatterns[5] = {
    {3,  300, 200, 0,    0,   0},      /* 模式 1: 3 次长响         */
    {5,  100, 100, 0,    0,   0},      /* 模式 2: 5 次短促         */
    {10,  50,  50, 0,    0,   0},      /* 模式 3: 10 次快速连响    */
    {4,  200, 400, 100,  50,  10000},  /* 模式 4: 10秒后加快频率   */
    {2, 1000, 500, 0,    0,   0},      /* 模式 5: 2 次长鸣         */
};

/**********************************************************************
 * 函数名称： AlarmTask
 * 功能描述： 声光报警任务
 *            等待 xAlarmEventGroup, 按各报警类型的声音模式驱动蜂鸣器和 LED
 *            优先级最高(3), 确保报警及时响应
 * 输入参数： params - 未使用
 * 输出参数： 无
 * 返 回 值： 无
 ***********************************************************************/
void AlarmTask(void *params)
{
    EventBits_t bits;

    ActiveBuzzer_Init();
    Led_Init();
    ColorLED_Init();
    ColorLED_Start();

    /* 上电自检: 蜂鸣器响 200ms, RGB LED 亮白色 200ms */
    ActiveBuzzer_Control(1);
    ColorLED_SetFast(COLOR_WHITE);
    vTaskDelay(pdMS_TO_TICKS(200));
    ActiveBuzzer_Control(0);
    ColorLED_SetFast(COLOR_NONE);

    for (;;)
    {
        /* 等待任意报警位, 收到后自动清除 */
        bits = xEventGroupWaitBits(
            xAlarmEventGroup,
            ALARM_TEMP_HIGH | ALARM_TEMP_LOW | ALARM_DIST_NEAR |
            ALARM_HUM_HIGH  | ALARM_HUM_LOW  | ALARM_LIGHT_LOW,
            pdTRUE,             /* 退出前清除标志位 */
            pdFALSE,            /* 任意一个满足即可 */
            portMAX_DELAY       /* 一直等, 没有报警就阻塞 */
        );

        /* 遍历 6 种报警类型, 按优先级 (0~5) 依次处理 */
        for (int type = 0; type < ALARM_TYPE_COUNT; type++)
        {
            if (bits & (1 << type))
            {
                int mode = g_alarmModes[type];
                uint32_t bit_mask = (uint32_t)(1 << type);

                /* 安全钳位 */
                if (mode < 1 || mode > 5)
                    mode = 1;

                const sound_pattern_t *pat = &s_soundPatterns[mode - 1];

                if (pat->switch_ms > 0)
                {
                    /* 时间模式: 持续循环, 超过 switch_ms 后切换为急促 */
                    /* 条件清除后进入 3 秒停止阶段 */
                    TickType_t start = xTaskGetTickCount();
                    int stopping = 0;               /* 0=正常播放, 1=停止阶段 */
                    TickType_t stopStart = 0;

                    for (;;)
                    {
                        TickType_t elapsed = xTaskGetTickCount() - start;

                        if (!stopping)
                        {
                            /* 正常 / 急促播放 */
                            int fast = (elapsed >= pdMS_TO_TICKS(pat->switch_ms));
                            int on_ms = fast ? pat->fast_on_ms : pat->on_ms;
                            int off_ms = fast ? pat->fast_off_ms : pat->off_ms;

                            ActiveBuzzer_Control(1);
                            ColorLED_SetFast(COLOR_RED);
                            vTaskDelay(pdMS_TO_TICKS(on_ms));

                            ActiveBuzzer_Control(0);
                            ColorLED_SetFast(COLOR_NONE);
                            vTaskDelay(pdMS_TO_TICKS(off_ms));

                            /* 安全: 最多持续 60 秒 (不包含停止阶段) */
                            if (elapsed > pdMS_TO_TICKS(60000))
                                break;
                        }
                        else
                        {
                            /* 停止阶段: 响 50ms 停 150ms, 持续 3 秒 */
                            ActiveBuzzer_Control(1);
                            ColorLED_SetFast(COLOR_YELLOW);
                            vTaskDelay(pdMS_TO_TICKS(50));
                            ActiveBuzzer_Control(0);
                            ColorLED_SetFast(COLOR_NONE);
                            vTaskDelay(pdMS_TO_TICKS(150));

                            if ((xTaskGetTickCount() - stopStart) >= pdMS_TO_TICKS(3000))
                                break;
                            continue;
                        }

                        /*
                         * 正确检测条件是否已清除:
                         * 1. 手动清除事件位
                         * 2. 等 SensorTask (500ms 周期) 重新设置
                         * 3. 如果超时未设置 → 条件已清除 → 进入停止阶段
                         */
                        xEventGroupClearBits(xAlarmEventGroup, bit_mask);
                        EventBits_t waitResult = xEventGroupWaitBits(
                            xAlarmEventGroup, bit_mask,
                            pdFALSE, pdFALSE, pdMS_TO_TICKS(550));

                        if ((waitResult & bit_mask) == 0)
                        {
                            if (elapsed >= pdMS_TO_TICKS(pat->switch_ms))
                            {
                                stopping = 1;
                                stopStart = xTaskGetTickCount();
                            }
                            else
                            {
                                break;
                            }
                        }

                        /* 有其他报警类型等待处理则退出 */
                        if (xEventGroupGetBits(xAlarmEventGroup) & ~bit_mask)
                            break;
                    }
                }
                else
                {
                    /* 固定周期模式 */
                    for (int i = 0; i < pat->cycles; i++)
                    {
                        ActiveBuzzer_Control(1);
                        ColorLED_SetFast(COLOR_RED);
                        vTaskDelay(pdMS_TO_TICKS(pat->on_ms));

                        ActiveBuzzer_Control(0);
                        ColorLED_SetFast(COLOR_NONE);
                        vTaskDelay(pdMS_TO_TICKS(pat->off_ms));
                    }
                }
            }
        }
    }
}
