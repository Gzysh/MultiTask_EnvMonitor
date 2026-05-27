#ifndef _APP_H
#define _APP_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"

/*
 * 传感器数据结构体
 * sensor_task 采集后通过队列发送给 display_task 和 storage_task
 */
typedef struct {
    int temperature;        /* DHT11 温度, 单位: 摄氏度      */
    int humidity;           /* DHT11 湿度, 单位: 百分比       */
    uint32_t light;         /* 光敏电阻 ADC 值, 范围 0~4095  */
    uint32_t distance;      /* 超声波距离, 单位: cm           */
    uint8_t  record_type;   /* 记录类型: 0=auto,1=manual,2=warning */
} sensor_data_t;

/*
 * 按键命令枚举
 * key_task 通过队列发送给 display_task
 */
typedef enum {
    KEY_NONE = 0,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_CONFIRM,
    KEY_BACK,
    KEY_MENU,
    KEY_HOME,
} key_cmd_t;

/*
 * 事件组位定义
 * sensor_task 设置位, alarm_task 等待位
 */
#define ALARM_TEMP_HIGH     (1 << 0)    /* 温度过高          */
#define ALARM_TEMP_LOW      (1 << 1)    /* 温度过低          */
#define ALARM_DIST_NEAR     (1 << 2)    /* 距离过近          */
#define ALARM_HUM_HIGH      (1 << 3)    /* 湿度过高          */
#define ALARM_HUM_LOW       (1 << 4)    /* 湿度过低          */
#define ALARM_LIGHT_LOW     (1 << 5)    /* 光照过低          */

/*
 * 报警类型枚举 (与事件位序号对齐)
 * 用于 g_alarmModes[] 索引
 */
typedef enum {
    ALARM_TYPE_TEMP_HIGH = 0,
    ALARM_TYPE_TEMP_LOW  = 1,
    ALARM_TYPE_DIST_NEAR = 2,
    ALARM_TYPE_HUM_HIGH  = 3,
    ALARM_TYPE_HUM_LOW   = 4,
    ALARM_TYPE_LIGHT_LOW = 5,
    ALARM_TYPE_COUNT
} alarm_type_t;

/*
 * 默认报警阈值 (运行时变量, 可通过菜单修改)
 */
extern int g_tempHighThreshold;         /* 温度上限, 默认 35℃       */
extern int g_tempLowThreshold;          /* 温度下限, 默认 10℃       */
extern int g_distMinThreshold;          /* 距离下限, 默认 10cm      */
extern int g_humHighThreshold;          /* 湿度上限, 默认 80%       */
extern int g_humLowThreshold;           /* 湿度下限, 默认 20%       */
extern int g_lightLowThreshold;         /* 光照下限, 默认 500       */

/* 各报警类型的声音模式 (1~5, 索引见 alarm_type_t) */
extern uint8_t g_alarmModes[ALARM_TYPE_COUNT];

/*
 * SPI 互斥锁 (防止 DisplayTask 与 StorageTask 同时访问 W25Q64)
 */
extern SemaphoreHandle_t xSpiMutex;

/*
 * 队列句柄 (在 freertos.c 中创建)
 */
extern QueueHandle_t xSensorQueue;      /* sensor_task → display_task */
extern QueueHandle_t xStorageQueue;     /* sensor_task → storage_task */
extern QueueHandle_t xKeyQueue;         /* key_task → display_task     */
extern QueueHandle_t xUartTxQueue;      /* sensor_task → comms_task    */

/*
 * 任务句柄 (在 freertos.c 中创建, 用于健康监控)
 */
extern TaskHandle_t xSensorTaskHandle;
extern TaskHandle_t xKeyTaskHandle;
extern TaskHandle_t xDisplayTaskHandle;
extern TaskHandle_t xAlarmTaskHandle;
extern TaskHandle_t xStorageTaskHandle;
extern TaskHandle_t xCommsTaskHandle;

/*
 * 事件组句柄 (在 freertos.c 中创建)
 */
extern EventGroupHandle_t xAlarmEventGroup;

/*
 * 任务函数声明
 */
void SensorTask(void *params);
void DisplayTask(void *params);
void KeyTask(void *params);
void AlarmTask(void *params);
void StorageTask(void *params);
void CommsTask(void *params);

#endif /* _APP_H */
