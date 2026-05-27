#include "app.h"
#include "driver_dht11.h"
#include "driver_light_sensor.h"
#include "driver_ultrasonic_sr04.h"
#include "driver_color_led.h"
#include "driver_timer.h"
#include "storage_task.h"

#include "task.h"

/**********************************************************************
 * 函数名称： SensorTask
 * 功能描述： 传感器采集任务
 *            每 500ms 采集 DHT11(温湿度) + 光敏(ADC) + 超声波(距离)
 *            打包 sensor_data_t 发队列, 检查阈值并设置事件组
 * 输入参数： params - 未使用
 * 输出参数： 无
 * 返 回 值： 无
 ***********************************************************************/
void SensorTask(void *params)
{
    sensor_data_t data = {0};
    EventBits_t bits = 0;
    static uint8_t s_alarmDuration = 0;     /* 警报连续触发计数 (×500ms) */

    /* 初始化传感器 */
    DHT11_Init();
    LightSensor_Init();
    SR04_Init();

    for (;;)
    {
        /* ---- 采集 DHT11 温湿度 ---- */
        if (DHT11_Read(&data.humidity, &data.temperature) != 0)
        {
            /* 读取失败, 保持上次值 */
        }

        /* ---- 采集光敏电阻 (ADC) ---- */
        if (LightSensor_Read(&data.light) != 0)
        {
            data.light = 0;
        }

        /* ---- 采集超声波距离 ---- */
        if (SR04_Read(&data.distance) != 0)
        {
            data.distance = 0;
        }

        data.record_type = RECORD_TYPE_AUTO;

        /* ---- 发送给显示任务 (阻塞 100ms 以防队列满) ---- */
        xQueueSend(xSensorQueue, &data, pdMS_TO_TICKS(100));

        /* ---- 发送给通信任务 (非阻塞, 队列满丢弃) ---- */
        xQueueSend(xUartTxQueue, &data, 0);

        /* ---- 检查阈值, 设置事件组 ---- */
        bits = 0;
        if (data.temperature > g_tempHighThreshold)
            bits |= ALARM_TEMP_HIGH;
        if (data.temperature < g_tempLowThreshold)
            bits |= ALARM_TEMP_LOW;
        if (data.distance < g_distMinThreshold && data.distance > 0)
            bits |= ALARM_DIST_NEAR;
        if (data.humidity > g_humHighThreshold)
            bits |= ALARM_HUM_HIGH;
        if (data.humidity < g_humLowThreshold && data.humidity > 0)
            bits |= ALARM_HUM_LOW;
        if (data.light < g_lightLowThreshold)
            bits |= ALARM_LIGHT_LOW;

        if (bits != 0)
        {
            xEventGroupSetBits(xAlarmEventGroup, bits);

            /* 警报持续时间计数 (500ms × 20 = 10s) */
            s_alarmDuration++;
            if (s_alarmDuration == 20)
            {
                data.record_type = RECORD_TYPE_WARNING;
                xQueueSend(xStorageQueue, &data, 0);
                s_alarmDuration = 21;   /* 防重复触发, 直到条件清除 */
            }
        }
        else
        {
            s_alarmDuration = 0;
            /* 无报警: 亮绿灯表示系统正常运行 */
            ColorLED_SetFast(COLOR_GREEN);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
