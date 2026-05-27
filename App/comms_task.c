#include "app.h"
#include "comms_task.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

/* USART1 句柄 (在 usart.c 中定义) */
extern UART_HandleTypeDef huart1;

/* 发送缓冲区 */
#define TX_BUF_SIZE     128
static char s_txBuf[TX_BUF_SIZE];

/* 健康报告计数器: 每 ~30 秒发一次 (500ms × 60 = 30s) */
#define HEALTH_INTERVAL 60
static uint32_t s_healthCounter = 0;

/**********************************************************************
 * 函数名称： comms_crc8
 * 功能描述： CRC-8-ATM (poly 0x07) 逐位计算
 *            与 storage_task.c 中的算法一致, 供后续二进制帧使用
 * 输入参数： data - 数据指针, len - 数据长度
 * 返 回 值： CRC-8 校验值
 ***********************************************************************/
uint8_t comms_crc8(const uint8_t *data, uint32_t len)
{
    uint8_t crc = 0;
    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
        {
            if (crc & 0x80)
                crc = (uint8_t)((crc << 1) ^ 0x07);
            else
                crc <<= 1;
        }
    }
    return crc;
}

/**********************************************************************
 * 函数名称： send_health_line
 * 功能描述： 发送系统健康信息 (任务栈峰值余量 + 堆余量)
 *            格式: [HEALTH] TaskName=StackHWM ... HeapFree=xxxx\r\n
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 无
 ***********************************************************************/
static void send_health_line(void)
{
    int pos = 0;

    pos += snprintf(s_txBuf + pos, TX_BUF_SIZE - pos, "[HEALTH] ");

    /* 获取各任务栈余量 (高水位标记: 任务启动以来剩余栈的最小值) */
    pos += snprintf(s_txBuf + pos, TX_BUF_SIZE - pos,
        "SensorTask=%lu ", (unsigned long)uxTaskGetStackHighWaterMark(xSensorTaskHandle));
    pos += snprintf(s_txBuf + pos, TX_BUF_SIZE - pos,
        "DisplayTask=%lu ", (unsigned long)uxTaskGetStackHighWaterMark(xDisplayTaskHandle));
    pos += snprintf(s_txBuf + pos, TX_BUF_SIZE - pos,
        "StorageTask=%lu ", (unsigned long)uxTaskGetStackHighWaterMark(xStorageTaskHandle));
    pos += snprintf(s_txBuf + pos, TX_BUF_SIZE - pos,
        "KeyTask=%lu ", (unsigned long)uxTaskGetStackHighWaterMark(xKeyTaskHandle));
    pos += snprintf(s_txBuf + pos, TX_BUF_SIZE - pos,
        "AlarmTask=%lu ", (unsigned long)uxTaskGetStackHighWaterMark(xAlarmTaskHandle));
    pos += snprintf(s_txBuf + pos, TX_BUF_SIZE - pos,
        "CommsTask=%lu ", (unsigned long)uxTaskGetStackHighWaterMark(xCommsTaskHandle));
    pos += snprintf(s_txBuf + pos, TX_BUF_SIZE - pos,
        "HeapFree=%lu\r\n", (unsigned long)xPortGetFreeHeapSize());

    if (pos > 0)
        HAL_UART_Transmit(&huart1, (uint8_t *)s_txBuf, pos, pdMS_TO_TICKS(100));
}

/**********************************************************************
 * 函数名称： send_text_line
 * 功能描述： 格式化传感器数据为文本行并通过 USART1 发送
 *            格式: [SENSOR] T:xx H:xx L:xxxx D:xx\r\n
 * 输入参数： data - 传感器数据指针
 * 输出参数： 无
 * 返 回 值： 无
 ***********************************************************************/
static void send_text_line(const sensor_data_t *data)
{
    int len = snprintf(s_txBuf, TX_BUF_SIZE,
        "[SENSOR] T:%d H:%d L:%lu D:%lu\r\n",
        data->temperature,
        data->humidity,
        (unsigned long)data->light,
        (unsigned long)data->distance);

    if (len > 0)
        HAL_UART_Transmit(&huart1, (uint8_t *)s_txBuf, len, pdMS_TO_TICKS(100));
}

/**********************************************************************
 * 函数名称： CommsTask
 * 功能描述： 通信任务, 优先级 1
 *            阻塞等待 xUartTxQueue, 收到数据后通过 USART1 发送给 PC
 * 输入参数： params - 未使用
 * 输出参数： 无
 * 返 回 值： 无
 ***********************************************************************/
void CommsTask(void *params)
{
    sensor_data_t data;

    for (;;)
    {
        /* 阻塞等待传感器数据 (无限超时, 不占 CPU) */
        if (xQueueReceive(xUartTxQueue, &data, portMAX_DELAY) == pdTRUE)
        {
            /* 发送文本行 (串口助手直接可读) */
            send_text_line(&data);

            /* 每 HEALTH_INTERVAL 次发送一次健康报告 */
            s_healthCounter++;
            if (s_healthCounter >= HEALTH_INTERVAL)
            {
                s_healthCounter = 0;
                send_health_line();
            }

            /* TODO: 后续用二进制帧替代文本, 供 Python 上位机解析 */
        }
    }
}
