#ifndef _COMMS_TASK_H
#define _COMMS_TASK_H

#include <stdint.h>
#include "app.h"

/*
 * UART 通信任务
 *
 * 从 xUartTxQueue 接收传感器数据, 通过 USART1 发送给 PC
 *
 * === 当前: 文本格式 (串口助手调试) ===
 * [SENSOR] T:28 H:65 L:1234 D:50\r\n
 *
 * === 后续: 二进制帧格式 (上位机解析) ===
 * [0xAA][0x55][LEN][TYPE][PAYLOAD...][CRC-8]
 */

/* 帧类型定义 (预留) */
#define FRAME_TYPE_SENSOR       0x01    /* 传感器数据帧 */
#define FRAME_TYPE_ACK          0x05    /* 应答帧 */
#define FRAME_TYPE_HEALTH       0x06    /* 系统健康信息帧 */

/* 帧同步头 */
#define FRAME_SYNC1             0xAA
#define FRAME_SYNC2             0x55

/* CRC-8-ATM (poly 0x07), 与 storage_task 使用的算法一致 */
uint8_t comms_crc8(const uint8_t *data, uint32_t len);

/* 通信任务入口 */
void CommsTask(void *params);

#endif /* _COMMS_TASK_H */
