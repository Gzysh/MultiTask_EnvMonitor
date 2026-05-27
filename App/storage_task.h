#ifndef _STORAGE_TASK_H
#define _STORAGE_TASK_H

/*
 * SPI Flash 存储格式
 *
 * W25Q64 容量 8MB (64Mbit), 按 4KB 扇区管理:
 *
 *   扇区 0  (0x000000 ~ 0x000FFF): 系统信息
 *     - 0x0000 ~ 0x0003: MAGIC (0xA55AA55A)
 *     - 0x0004 ~ 0x0007: 写指针偏移 (当前写入地址)
 *     - 0x0008 ~ 0x000B: 写次数 (累计写入条数)
 *     - 0x000C ~ 0x000F: 保留
 *     - 0x0010 ~ 0x002C: 阈值 (7×4B = 28B)
 *     - 0x0030 ~ 0x0035: 声音模式 (6B)
 *     - 0x0036 ~ 0x003F: 保留
 *     - 0x0040 ~ 0x00DF: LT_Manual 记录 (10×16B = 160B)
 *     - 0x00E0 ~ 0x00EF: 保留 (16B)
 *     - 0x00F0 ~ 0x019F: LT_Alarm 记录 (10×16B = 160B)
 *     - 0x01A0 ~ 0x040F: 空闲
 *     - 0x0410 ~ 0x0419: LT_Alarm 标记位 (10B, 0xFF=空 0x00=已用)
 *     - 0x041A ~ 0x0441: 保留
 *     - 0x0442 ~ 0x044B: LT_Manual 标记位 (10B, 0xFF=空 0x00=已用)
 *
 *   扇区 1~N (0x001000 ~ 最大地址): 环形缓冲
 *     每条记录 16 字节, 每扇区存 256 条
 *     写满最后一个扇区后回到扇区 1 循环覆盖
 */

#define STORAGE_MAGIC               0xA55AA55A
#define STORAGE_SECTOR_SIZE         4096
#define STORAGE_DATA_START_ADDR     (1 * STORAGE_SECTOR_SIZE)  /* 从扇区1开始 */

/* 每条传感器记录: 16 字节 */
typedef struct __attribute__((packed)) {
    uint32_t timestamp;         /* 时间戳 (系统 tick)    */
    int16_t  temperature;       /* 温度 (℃ × 10)        */
    uint16_t humidity;          /* 湿度 (% × 10)         */
    uint16_t light;             /* 光照 (ADC 原始值)      */
    uint16_t distance;          /* 距离 (cm)              */
    uint8_t  reserved[2];       /* [0]=record_type, [1]=0 */
} storage_record_t;

#define RECORD_SIZE             sizeof(storage_record_t)
#define RECORDS_PER_SECTOR      (STORAGE_SECTOR_SIZE / RECORD_SIZE)  /* 256 */

/* 阈值在扇区 0 中的偏移 */
#define THRESHOLD_OFFSET_TEMP       0x10    /* g_tempHighThreshold (int, 4 字节) */
#define THRESHOLD_OFFSET_DIST       0x14    /* g_distMinThreshold  (int, 4 字节) */
#define THRESHOLD_OFFSET_HUM_HIGH   0x18    /* g_humHighThreshold  (int, 4 字节) */
#define THRESHOLD_OFFSET_HUM_LOW    0x1C    /* g_humLowThreshold   (int, 4 字节) */
#define THRESHOLD_OFFSET_LIGHT_LO   0x20    /* g_lightLowThreshold (int, 4 字节) */
#define THRESHOLD_OFFSET_TEMP_LO    0x24    /* g_tempLowThreshold  (int, 4 字节) */
#define ALARM_MODE_OFFSET           0x30    /* g_alarmModes[6]     (6 字节)     */

/* Long-term Manual Bank (扇区 0, 最多 10 条) */
#define LT_MANUAL_RECORDS_OFFSET    0x40
#define LT_MANUAL_RECORD_MAX        10
#define LT_MANUAL_MARKERS_OFFSET    0x442

/* Long-term Alarm Bank (扇区 0, 最多 10 条) */
#define LT_ALARM_RECORDS_OFFSET     0xF0
#define LT_ALARM_RECORD_MAX         10
#define LT_ALARM_MARKERS_OFFSET     0x410

/* 记录类型标记 (用于 storage_record_t.reserved[0]) */
#define RECORD_TYPE_AUTO            0x00
#define RECORD_TYPE_MANUAL          0x01
#define RECORD_TYPE_WARNING         0x02
#define RECORD_TYPE_LT_MANUAL       0x03
#define RECORD_TYPE_LT_ALARM        0x04

/* 记录读取接口 */
uint32_t storage_get_record_count(void);
int      storage_read_newest(uint32_t idx, storage_record_t *rec);   /* idx=0 为最新 */

/* 阈值持久化接口 */
void     storage_save_thresholds(void);
void     storage_save_alarm_modes(void);

/* Long-term Manual (DisplayTask 管理, 最多 10 条, 断电保存) */
int      storage_lt_manual_save(const storage_record_t *rec);
int      storage_lt_manual_delete(uint8_t index);
uint32_t storage_lt_manual_count(void);
int      storage_lt_manual_read(uint32_t idx, storage_record_t *rec);

/* Long-term Alarm (DisplayTask 管理, 最多 10 条, 断电保存) */
int      storage_lt_alarm_save(const storage_record_t *rec);
int      storage_lt_alarm_delete(uint8_t index);
uint32_t storage_lt_alarm_count(void);
int      storage_lt_alarm_read(uint32_t idx, storage_record_t *rec);

/* 恢复出厂设置: 清空两个 LT bank */
void     storage_clear_lt_banks(void);

#endif /* _STORAGE_TASK_H */
