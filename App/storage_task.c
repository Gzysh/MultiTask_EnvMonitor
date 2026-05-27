#include "app.h"
#include "storage_task.h"
#include "driver_spiflash_w25q64.h"

#include "task.h"
#include "semphr.h"
#include <string.h>
#include <stddef.h>

/* 扇区数: W25Q64 共 8MB / 4KB = 2048 扇区, 扇区0保留, 可用 2047 个 */
#define TOTAL_DATA_SECTORS      2047
#define TOTAL_DATA_SIZE         ((uint32_t)TOTAL_DATA_SECTORS * STORAGE_SECTOR_SIZE)
#define TOTAL_RECORDS           (TOTAL_DATA_SIZE / RECORD_SIZE)

/* CRC-8-ATM (poly 0x07) 逐位计算, 用于存储记录完整性校验 */
static uint8_t storage_crc8(const uint8_t *data, uint32_t len)
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

/* 写缓冲区, 攒够一条记录写入 Flash */
static uint32_t g_writeOffset = STORAGE_DATA_START_ADDR;
static uint32_t g_writeCount  = 0;          /* 累计写入次数, 用于计算记录总数 */

/* 扇区0扩展数据备份缓存 (擦除重写时保留) */
#define SECTOR0_LT_MANUAL_BYTES  ((uint32_t)LT_MANUAL_RECORD_MAX * RECORD_SIZE)
#define SECTOR0_LT_ALARM_BYTES   ((uint32_t)LT_ALARM_RECORD_MAX * RECORD_SIZE)
static uint8_t  s_backup_lt_manual[SECTOR0_LT_MANUAL_BYTES];
static uint8_t  s_backup_lt_manual_markers[LT_MANUAL_RECORD_MAX];
static uint8_t  s_backup_lt_alarm[SECTOR0_LT_ALARM_BYTES];
static uint8_t  s_backup_lt_alarm_markers[LT_ALARM_RECORD_MAX];

/**********************************************************************
 * 函数名称： storage_load_offset
 * 功能描述： 上电时从扇区0读取写指针
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 0 - 成功, -1 - Flash 未初始化
 ***********************************************************************/
static int storage_load_offset(void)
{
    uint32_t magic;

    W25Q64_Init();

    if (W25Q64_Read(0, (uint8_t *)&magic, 4) != 4)
        return -1;

    if (magic == STORAGE_MAGIC)
    {
        /* 已初始化, 读取写指针和写入次数 */
        if (W25Q64_Read(4, (uint8_t *)&g_writeOffset, 4) != 4)
            return -1;
        if (W25Q64_Read(8, (uint8_t *)&g_writeCount, 4) != 4)
            return -1;
    }
    else
    {
        /* 首次使用, 擦除扇区0后初始化 */
        magic = STORAGE_MAGIC;
        g_writeOffset = STORAGE_DATA_START_ADDR;
        g_writeCount  = 0;
        W25Q64_Erase(0, STORAGE_SECTOR_SIZE);
        W25Q64_Write(0, (uint8_t *)&magic, 4);
        W25Q64_Write(4, (uint8_t *)&g_writeOffset, 4);
        W25Q64_Write(8, (uint8_t *)&g_writeCount, 4);
        W25Q64_Write(THRESHOLD_OFFSET_TEMP, (uint8_t *)&g_tempHighThreshold, 4);
        W25Q64_Write(THRESHOLD_OFFSET_DIST, (uint8_t *)&g_distMinThreshold, 4);
        W25Q64_Write(THRESHOLD_OFFSET_HUM_HIGH, (uint8_t *)&g_humHighThreshold, 4);
        W25Q64_Write(THRESHOLD_OFFSET_HUM_LOW, (uint8_t *)&g_humLowThreshold, 4);
        W25Q64_Write(THRESHOLD_OFFSET_LIGHT_LO, (uint8_t *)&g_lightLowThreshold, 4);
        W25Q64_Write(THRESHOLD_OFFSET_TEMP_LO, (uint8_t *)&g_tempLowThreshold, 4);
        W25Q64_Write(ALARM_MODE_OFFSET, g_alarmModes, ALARM_TYPE_COUNT);
        /* 标记位区域在擦除后自动为0xFF(空), 无需写0 */
    }

    return 0;
}

/**********************************************************************
 * 函数名称： storage_save_offset
 * 功能描述： 将写指针保存到扇区0
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 无
 ***********************************************************************/
static void storage_save_offset(void)
{
    if (xSemaphoreTake(xSpiMutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        W25Q64_Write(4, (uint8_t *)&g_writeOffset, 4);
        xSemaphoreGive(xSpiMutex);
    }
}

/**********************************************************************
 * 扇区0扩展数据备份/恢复
 * save_thresholds / save_alarm_modes 擦除扇区0时保留长期+警告记录
 ***********************************************************************/
static void sector0_backup_ext(void)
{
    W25Q64_Read(LT_MANUAL_RECORDS_OFFSET, s_backup_lt_manual, SECTOR0_LT_MANUAL_BYTES);
    W25Q64_Read(LT_MANUAL_MARKERS_OFFSET, s_backup_lt_manual_markers, LT_MANUAL_RECORD_MAX);
    W25Q64_Read(LT_ALARM_RECORDS_OFFSET,  s_backup_lt_alarm, SECTOR0_LT_ALARM_BYTES);
    W25Q64_Read(LT_ALARM_MARKERS_OFFSET,  s_backup_lt_alarm_markers, LT_ALARM_RECORD_MAX);
}

static void sector0_restore_ext(void)
{
    W25Q64_Write(LT_MANUAL_RECORDS_OFFSET, s_backup_lt_manual, SECTOR0_LT_MANUAL_BYTES);
    W25Q64_Write(LT_MANUAL_MARKERS_OFFSET, s_backup_lt_manual_markers, LT_MANUAL_RECORD_MAX);
    W25Q64_Write(LT_ALARM_RECORDS_OFFSET,  s_backup_lt_alarm, SECTOR0_LT_ALARM_BYTES);
    W25Q64_Write(LT_ALARM_MARKERS_OFFSET,  s_backup_lt_alarm_markers, LT_ALARM_RECORD_MAX);
}

/**********************************************************************
 * 函数名称： storage_write_record
 * 功能描述： 将一条传感器记录写入 Flash
 * 输入参数： data - 传感器数据
 * 输出参数： 无
 * 返 回 值： 0 - 成功, -1 - 失败
 ***********************************************************************/
static int storage_write_record(const sensor_data_t *data)
{
    uint32_t sector_start;
    storage_record_t record;
    int ret = -1;

    if (xSemaphoreTake(xSpiMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return -1;

    /* 打包记录 */
    record.timestamp    = xTaskGetTickCount();
    record.temperature  = (int16_t)(data->temperature * 10);
    record.humidity     = (uint16_t)(data->humidity * 10);
    record.light        = (uint16_t)data->light;
    record.distance     = (uint16_t)data->distance;
    record.reserved[0]  = data->record_type;
    record.reserved[1]  = storage_crc8((uint8_t *)&record,
                            offsetof(storage_record_t, reserved[1]));

    /* 如果写到末尾, 回到起始处循环覆盖 */
    if (g_writeOffset >= STORAGE_DATA_START_ADDR + TOTAL_DATA_SIZE)
        g_writeOffset = STORAGE_DATA_START_ADDR;

    /* 检查是否在扇区开头 (需要擦除) */
    sector_start = (g_writeOffset / STORAGE_SECTOR_SIZE) * STORAGE_SECTOR_SIZE;
    if (g_writeOffset == sector_start)
    {
        if (W25Q64_Erase(sector_start, STORAGE_SECTOR_SIZE) < 0)
            goto out;
    }

    /* 写入数据 */
    if (W25Q64_Write(g_writeOffset, (uint8_t *)&record, RECORD_SIZE) < 0)
        goto out;

    g_writeOffset += RECORD_SIZE;
    g_writeCount++;
    storage_save_offset();
    W25Q64_Write(8, (uint8_t *)&g_writeCount, 4);
    ret = 0;

out:
    xSemaphoreGive(xSpiMutex);
    return ret;
}

/**********************************************************************
 * 函数名称： storage_get_record_count
 * 功能描述： 返回当前有效记录总数
 ***********************************************************************/
uint32_t storage_get_record_count(void)
{
    /* 如果写次数超过一圈, 缓冲区已满 */
    if (g_writeCount >= TOTAL_RECORDS)
        return TOTAL_RECORDS;
    return g_writeCount;
}

/**********************************************************************
 * 函数名称： storage_read_newest
 * 功能描述： 读取第 idx 条记录 (0 = 最新)
 * 输入参数： idx - 索引 (0 = 最新记录)
 * 输出参数： rec - 存放记录
 * 返 回 值： 0 - 成功, -1 - 失败
 ***********************************************************************/
int storage_read_newest(uint32_t idx, storage_record_t *rec)
{
    uint32_t addr;

    /* 从写指针往前推 (idx+1) 条 */
    if (g_writeOffset >= (idx + 1) * RECORD_SIZE + STORAGE_DATA_START_ADDR)
    {
        addr = g_writeOffset - (idx + 1) * RECORD_SIZE;
    }
    else
    {
        /* 需要折回 */
        addr = g_writeOffset + TOTAL_DATA_SIZE - (idx + 1) * RECORD_SIZE;
    }

    if (addr > STORAGE_DATA_START_ADDR + TOTAL_DATA_SIZE - RECORD_SIZE ||
        addr < STORAGE_DATA_START_ADDR)
        return -1;

    if (xSemaphoreTake(xSpiMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return -1;

    int ok = (W25Q64_Read(addr, (uint8_t *)rec, RECORD_SIZE) == RECORD_SIZE);
    xSemaphoreGive(xSpiMutex);

    /* CRC 校验: reserved[1] != 0 表示这条记录启用了 CRC (兼容旧记录) */
    if (ok && rec->reserved[1] != 0 &&
        rec->reserved[1] != storage_crc8((uint8_t *)rec,
                             offsetof(storage_record_t, reserved[1])))
        return -1;

    return ok ? 0 : -1;
}

/**********************************************************************
 * 内部辅助: Long-term Bank 通用操作
 * 对扇区 0 中的某一块存储区进行读写, 通过参数区分 Bank
 ***********************************************************************/

/* 保存记录到指定 bank (找空槽写入) */
static int lt_bank_save(uint32_t records_off, uint32_t markers_off,
                        uint32_t max, const storage_record_t *rec)
{
    int ret = -1;
    storage_record_t wr_rec;

    if (xSemaphoreTake(xSpiMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return -1;

    /* 拷贝后写入 CRC，不修改调用方原记录 */
    wr_rec = *rec;
    wr_rec.reserved[1] = storage_crc8((uint8_t *)&wr_rec,
                          offsetof(storage_record_t, reserved[1]));

    for (uint32_t i = 0; i < max; i++)
    {
        uint8_t marker;
        W25Q64_Read(markers_off + i, &marker, 1);
        if (marker == 0xFF)
        {
            uint32_t addr = records_off + i * RECORD_SIZE;
            if (W25Q64_Write(addr, (uint8_t *)&wr_rec, RECORD_SIZE) == RECORD_SIZE)
            {
                uint8_t used = 0x00;
                W25Q64_Write(markers_off + i, &used, 1);
                ret = 0;
            }
            break;
        }
    }
    xSemaphoreGive(xSpiMutex);
    return ret;
}

/* 从指定 bank 删除一条记录 (idx=0=最新), 重写整个扇区 */
static int lt_bank_delete(uint32_t records_off, uint32_t markers_off,
                          uint32_t max, uint8_t index,
                          uint32_t other_records_off, uint32_t other_markers_off,
                          uint32_t other_max)
{
    int ret = -1;
    if (xSemaphoreTake(xSpiMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return -1;

    /* 读本 bank 标记位, 确定总记录数和目标物理槽 */
    uint8_t markers[max];
    W25Q64_Read(markers_off, markers, max);
    uint32_t total = 0;
    for (uint32_t i = 0; i < max; i++)
        if (markers[i] != 0xFF) total++;

    if (index >= total)
        goto out_bank_del;

    uint32_t target_slot;
    {
        uint32_t tgt = total - 1 - index;
        uint32_t found = 0;
        for (uint32_t i = 0; i < max; i++)
        {
            if (markers[i] != 0xFF)
            {
                if (found == tgt) { target_slot = i; break; }
                found++;
            }
        }
    }

    /* 读出本 bank 所有记录到缓存 */
    {
        uint8_t buf[max * RECORD_SIZE];
        uint32_t new_total = total - 1;
        W25Q64_Read(records_off, buf, sizeof(buf));

        /* 删除 target_slot: 后续记录前移 */
        if (target_slot < new_total)
        {
            memmove(buf + target_slot * RECORD_SIZE,
                    buf + (target_slot + 1) * RECORD_SIZE,
                    (new_total - target_slot) * RECORD_SIZE);
        }

        /* 备份另一 bank (擦除后恢复) */
        uint8_t other_buf[other_max * RECORD_SIZE];
        uint8_t other_markers_buf[other_max];
        W25Q64_Read(other_records_off, other_buf, sizeof(other_buf));
        W25Q64_Read(other_markers_off, other_markers_buf, other_max);

        /* 备份扇区0头部 */
        uint32_t magic, wo, wc;
        W25Q64_Read(0, (uint8_t *)&magic, 4);
        W25Q64_Read(4, (uint8_t *)&wo, 4);
        W25Q64_Read(8, (uint8_t *)&wc, 4);

        W25Q64_Erase(0, STORAGE_SECTOR_SIZE);

        /* 重写头部 */
        W25Q64_Write(0, (uint8_t *)&magic, 4);
        W25Q64_Write(4, (uint8_t *)&wo, 4);
        W25Q64_Write(8, (uint8_t *)&wc, 4);
        W25Q64_Write(THRESHOLD_OFFSET_TEMP,     (uint8_t *)&g_tempHighThreshold, 4);
        W25Q64_Write(THRESHOLD_OFFSET_DIST,     (uint8_t *)&g_distMinThreshold, 4);
        W25Q64_Write(THRESHOLD_OFFSET_HUM_HIGH, (uint8_t *)&g_humHighThreshold, 4);
        W25Q64_Write(THRESHOLD_OFFSET_HUM_LOW,  (uint8_t *)&g_humLowThreshold, 4);
        W25Q64_Write(THRESHOLD_OFFSET_LIGHT_LO, (uint8_t *)&g_lightLowThreshold, 4);
        W25Q64_Write(THRESHOLD_OFFSET_TEMP_LO,  (uint8_t *)&g_tempLowThreshold, 4);
        W25Q64_Write(ALARM_MODE_OFFSET, g_alarmModes, ALARM_TYPE_COUNT);

        /* 写回本 bank (已删除) */
        W25Q64_Write(records_off, buf, sizeof(buf));
        {
            uint8_t new_markers[max];
            memset(new_markers, 0xFF, max);
            for (uint32_t i = 0; i < new_total; i++)
                new_markers[i] = 0x00;
            W25Q64_Write(markers_off, new_markers, max);
        }

        /* 恢复另一 bank (来自提前读出的备份) */
        W25Q64_Write(other_records_off, other_buf, sizeof(other_buf));
        W25Q64_Write(other_markers_off, other_markers_buf, other_max);
    }
    ret = 0;

out_bank_del:
    xSemaphoreGive(xSpiMutex);
    return ret;
}

/* 统计指定 bank 的记录数 */
static uint32_t lt_bank_count(uint32_t markers_off, uint32_t max)
{
    uint32_t count = 0;
    if (xSemaphoreTake(xSpiMutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        for (uint32_t i = 0; i < max; i++)
        {
            uint8_t marker;
            W25Q64_Read(markers_off + i, &marker, 1);
            if (marker != 0xFF) count++;
        }
        xSemaphoreGive(xSpiMutex);
    }
    return count;
}

/* 读取指定 bank 的第 idx 条 (0=最新) */
static int lt_bank_read(uint32_t markers_off, uint32_t records_off,
                        uint32_t max, uint32_t idx, storage_record_t *rec)
{
    int ret = -1;
    if (xSemaphoreTake(xSpiMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return -1;

    uint8_t markers[max];
    W25Q64_Read(markers_off, markers, max);
    uint32_t total = 0;
    for (uint32_t i = 0; i < max; i++)
        if (markers[i] != 0xFF) total++;

    if (idx < total)
    {
        uint32_t target = total - 1 - idx;
        uint32_t found = 0;
        for (uint32_t i = 0; i < max; i++)
        {
            if (markers[i] != 0xFF)
            {
                if (found == target)
                {
                    uint32_t addr = records_off + i * RECORD_SIZE;
                    int r = W25Q64_Read(addr, (uint8_t *)rec, RECORD_SIZE);
                    ret = (r == RECORD_SIZE) ? 0 : -1;
                    if (ret == 0 && rec->reserved[1] != 0 &&
                        rec->reserved[1] != storage_crc8((uint8_t *)rec,
                                             offsetof(storage_record_t, reserved[1])))
                        ret = -1;
                    break;
                }
                found++;
            }
        }
    }
    xSemaphoreGive(xSpiMutex);
    return ret;
}

/**********************************************************************
 * Long-term Manual (Bank 0)
 ***********************************************************************/
int storage_lt_manual_save(const storage_record_t *rec)
{
    return lt_bank_save(LT_MANUAL_RECORDS_OFFSET, LT_MANUAL_MARKERS_OFFSET,
                        LT_MANUAL_RECORD_MAX, rec);
}

int storage_lt_manual_delete(uint8_t index)
{
    return lt_bank_delete(LT_MANUAL_RECORDS_OFFSET, LT_MANUAL_MARKERS_OFFSET,
                          LT_MANUAL_RECORD_MAX, index,
                          LT_ALARM_RECORDS_OFFSET, LT_ALARM_MARKERS_OFFSET,
                          LT_ALARM_RECORD_MAX);
}

uint32_t storage_lt_manual_count(void)
{
    return lt_bank_count(LT_MANUAL_MARKERS_OFFSET, LT_MANUAL_RECORD_MAX);
}

int storage_lt_manual_read(uint32_t idx, storage_record_t *rec)
{
    return lt_bank_read(LT_MANUAL_MARKERS_OFFSET, LT_MANUAL_RECORDS_OFFSET,
                        LT_MANUAL_RECORD_MAX, idx, rec);
}

/**********************************************************************
 * Long-term Alarm (Bank 1)
 ***********************************************************************/
int storage_lt_alarm_save(const storage_record_t *rec)
{
    return lt_bank_save(LT_ALARM_RECORDS_OFFSET, LT_ALARM_MARKERS_OFFSET,
                        LT_ALARM_RECORD_MAX, rec);
}

int storage_lt_alarm_delete(uint8_t index)
{
    return lt_bank_delete(LT_ALARM_RECORDS_OFFSET, LT_ALARM_MARKERS_OFFSET,
                          LT_ALARM_RECORD_MAX, index,
                          LT_MANUAL_RECORDS_OFFSET, LT_MANUAL_MARKERS_OFFSET,
                          LT_MANUAL_RECORD_MAX);
}

uint32_t storage_lt_alarm_count(void)
{
    return lt_bank_count(LT_ALARM_MARKERS_OFFSET, LT_ALARM_RECORD_MAX);
}

int storage_lt_alarm_read(uint32_t idx, storage_record_t *rec)
{
    return lt_bank_read(LT_ALARM_MARKERS_OFFSET, LT_ALARM_RECORDS_OFFSET,
                        LT_ALARM_RECORD_MAX, idx, rec);
}

/**********************************************************************
 * 函数名称： storage_clear_lt_banks
 * 功能描述： 清空两个 LT bank 的记录 (恢复出厂设置用)
 *            擦除扇区0并重写头部, 阈值, 模式, 但不写入 LT bank 数据
 *            应在 save_thresholds 之后调用, 确保阈值已持久化
 ***********************************************************************/
void storage_clear_lt_banks(void)
{
    if (xSemaphoreTake(xSpiMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return;

    /* 直接标记位擦除: 无法写 0xFF, 放在这里供 save_thresholds 的 sector0_backup_ext 读取 */
    /* 实际清空通过 save_thresholds 擦除扇区0+重写实现 */
    uint32_t magic, wo, wc;
    W25Q64_Read(0, (uint8_t *)&magic, 4);
    W25Q64_Read(4, (uint8_t *)&wo, 4);
    W25Q64_Read(8, (uint8_t *)&wc, 4);

    /* 备份阈值和声音模式 */
    int th[6];
    W25Q64_Read(THRESHOLD_OFFSET_TEMP,     (uint8_t *)&th[0], 4);
    W25Q64_Read(THRESHOLD_OFFSET_DIST,     (uint8_t *)&th[1], 4);
    W25Q64_Read(THRESHOLD_OFFSET_HUM_HIGH, (uint8_t *)&th[2], 4);
    W25Q64_Read(THRESHOLD_OFFSET_HUM_LOW,  (uint8_t *)&th[3], 4);
    W25Q64_Read(THRESHOLD_OFFSET_LIGHT_LO, (uint8_t *)&th[4], 4);
    W25Q64_Read(THRESHOLD_OFFSET_TEMP_LO,  (uint8_t *)&th[5], 4);
    uint8_t am[ALARM_TYPE_COUNT];
    W25Q64_Read(ALARM_MODE_OFFSET, am, ALARM_TYPE_COUNT);

    W25Q64_Erase(0, STORAGE_SECTOR_SIZE);

    /* 重写头部+阈值+模式, 不写任何 LT bank 记录 */
    W25Q64_Write(0, (uint8_t *)&magic, 4);
    W25Q64_Write(4, (uint8_t *)&wo, 4);
    W25Q64_Write(8, (uint8_t *)&wc, 4);
    W25Q64_Write(THRESHOLD_OFFSET_TEMP,     (uint8_t *)&th[0], 4);
    W25Q64_Write(THRESHOLD_OFFSET_DIST,     (uint8_t *)&th[1], 4);
    W25Q64_Write(THRESHOLD_OFFSET_HUM_HIGH, (uint8_t *)&th[2], 4);
    W25Q64_Write(THRESHOLD_OFFSET_HUM_LOW,  (uint8_t *)&th[3], 4);
    W25Q64_Write(THRESHOLD_OFFSET_LIGHT_LO, (uint8_t *)&th[4], 4);
    W25Q64_Write(THRESHOLD_OFFSET_TEMP_LO,  (uint8_t *)&th[5], 4);
    W25Q64_Write(ALARM_MODE_OFFSET, am, ALARM_TYPE_COUNT);
    /* 标记位区域在擦除后自动为 0xFF, 不必写 */

    xSemaphoreGive(xSpiMutex);
}

/**********************************************************************
 * 函数名称： storage_save_thresholds
 * 功能描述： 将阈值保存到 Flash 扇区 0 (如需擦除则重写整个扇区头)
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 无
 ***********************************************************************/
void storage_save_thresholds(void)
{
    uint32_t magic;
    int old_val[6];

    if (xSemaphoreTake(xSpiMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return;

    /* 读取 Flash 中现有的阈值, 无变化则不写 */
    W25Q64_Read(THRESHOLD_OFFSET_TEMP,     (uint8_t *)&old_val[0], 4);
    W25Q64_Read(THRESHOLD_OFFSET_DIST,     (uint8_t *)&old_val[1], 4);
    W25Q64_Read(THRESHOLD_OFFSET_HUM_HIGH, (uint8_t *)&old_val[2], 4);
    W25Q64_Read(THRESHOLD_OFFSET_HUM_LOW,  (uint8_t *)&old_val[3], 4);
    W25Q64_Read(THRESHOLD_OFFSET_LIGHT_LO, (uint8_t *)&old_val[4], 4);
    W25Q64_Read(THRESHOLD_OFFSET_TEMP_LO,  (uint8_t *)&old_val[5], 4);
    if (old_val[0] == g_tempHighThreshold && old_val[1] == g_distMinThreshold &&
        old_val[2] == g_humHighThreshold  && old_val[3] == g_humLowThreshold &&
        old_val[4] == g_lightLowThreshold  && old_val[5] == g_tempLowThreshold)
    {
        xSemaphoreGive(xSpiMutex);
        return;
    }

    /* 验证扇区 0 已初始化 */
    W25Q64_Read(0, (uint8_t *)&magic, 4);
    if (magic != STORAGE_MAGIC)
    {
        xSemaphoreGive(xSpiMutex);
        return;
    }

    /* 保存现有头部数据 + 扩展数据 */
    {
        uint32_t wo, wc;
        W25Q64_Read(4, (uint8_t *)&wo, 4);
        W25Q64_Read(8, (uint8_t *)&wc, 4);

        /* 备份两个 LT bank (擦除后恢复) */
        sector0_backup_ext();

        /* 擦除扇区 0 后重写全部 */
        W25Q64_Erase(0, STORAGE_SECTOR_SIZE);
        W25Q64_Write(0, (uint8_t *)&magic, 4);
        W25Q64_Write(4, (uint8_t *)&wo, 4);
        W25Q64_Write(8, (uint8_t *)&wc, 4);
        W25Q64_Write(THRESHOLD_OFFSET_TEMP, (uint8_t *)&g_tempHighThreshold, 4);
        W25Q64_Write(THRESHOLD_OFFSET_DIST, (uint8_t *)&g_distMinThreshold, 4);
        W25Q64_Write(THRESHOLD_OFFSET_HUM_HIGH, (uint8_t *)&g_humHighThreshold, 4);
        W25Q64_Write(THRESHOLD_OFFSET_HUM_LOW, (uint8_t *)&g_humLowThreshold, 4);
        W25Q64_Write(THRESHOLD_OFFSET_LIGHT_LO, (uint8_t *)&g_lightLowThreshold, 4);
        W25Q64_Write(THRESHOLD_OFFSET_TEMP_LO, (uint8_t *)&g_tempLowThreshold, 4);
        W25Q64_Write(ALARM_MODE_OFFSET, g_alarmModes, ALARM_TYPE_COUNT);

        /* 恢复两个 LT bank */
        sector0_restore_ext();
    }

    xSemaphoreGive(xSpiMutex);
}

/**********************************************************************
 * 函数名称： storage_save_alarm_modes
 * 功能描述： 将声音模式保存到 Flash 扇区 0
 ***********************************************************************/
void storage_save_alarm_modes(void)
{
    uint32_t magic;
    uint8_t old_modes[ALARM_TYPE_COUNT];

    if (xSemaphoreTake(xSpiMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return;

    /* 读取现有模式, 无变化则不写 */
    W25Q64_Read(ALARM_MODE_OFFSET, old_modes, ALARM_TYPE_COUNT);
    if (memcmp(old_modes, g_alarmModes, ALARM_TYPE_COUNT) == 0)
    {
        xSemaphoreGive(xSpiMutex);
        return;
    }

    /* 验证扇区 0 已初始化 */
    W25Q64_Read(0, (uint8_t *)&magic, 4);
    if (magic != STORAGE_MAGIC)
    {
        xSemaphoreGive(xSpiMutex);
        return;
    }

    {
        uint32_t wo, wc;
        W25Q64_Read(4, (uint8_t *)&wo, 4);
        W25Q64_Read(8, (uint8_t *)&wc, 4);

        /* 备份两个 LT bank */
        sector0_backup_ext();

        W25Q64_Erase(0, STORAGE_SECTOR_SIZE);
        W25Q64_Write(0,  (uint8_t *)&magic, 4);
        W25Q64_Write(4,  (uint8_t *)&wo, 4);
        W25Q64_Write(8,  (uint8_t *)&wc, 4);
        W25Q64_Write(THRESHOLD_OFFSET_TEMP,     (uint8_t *)&g_tempHighThreshold, 4);
        W25Q64_Write(THRESHOLD_OFFSET_DIST,     (uint8_t *)&g_distMinThreshold, 4);
        W25Q64_Write(THRESHOLD_OFFSET_HUM_HIGH, (uint8_t *)&g_humHighThreshold, 4);
        W25Q64_Write(THRESHOLD_OFFSET_HUM_LOW,  (uint8_t *)&g_humLowThreshold, 4);
        W25Q64_Write(THRESHOLD_OFFSET_LIGHT_LO, (uint8_t *)&g_lightLowThreshold, 4);
        W25Q64_Write(THRESHOLD_OFFSET_TEMP_LO,  (uint8_t *)&g_tempLowThreshold, 4);
        W25Q64_Write(ALARM_MODE_OFFSET, g_alarmModes, ALARM_TYPE_COUNT);

        /* 恢复两个 LT bank */
        sector0_restore_ext();
    }

    xSemaphoreGive(xSpiMutex);
}

/**********************************************************************
 * 函数名称： storage_load_thresholds
 * 功能描述： 上电时从 Flash 加载阈值到全局变量
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 无
 ***********************************************************************/
static void storage_load_thresholds(void)
{
    uint32_t magic;

    W25Q64_Read(0, (uint8_t *)&magic, 4);
    if (magic == STORAGE_MAGIC)
    {
        int val;
        if (W25Q64_Read(THRESHOLD_OFFSET_TEMP,     (uint8_t *)&val, 4) == 4)
            g_tempHighThreshold = val;
        if (W25Q64_Read(THRESHOLD_OFFSET_DIST,     (uint8_t *)&val, 4) == 4)
            g_distMinThreshold = val;
        if (W25Q64_Read(THRESHOLD_OFFSET_HUM_HIGH, (uint8_t *)&val, 4) == 4)
            g_humHighThreshold = val;
        if (W25Q64_Read(THRESHOLD_OFFSET_HUM_LOW,  (uint8_t *)&val, 4) == 4)
            g_humLowThreshold = val;
        if (W25Q64_Read(THRESHOLD_OFFSET_LIGHT_LO, (uint8_t *)&val, 4) == 4)
            g_lightLowThreshold = val;
        if (W25Q64_Read(THRESHOLD_OFFSET_TEMP_LO, (uint8_t *)&val, 4) == 4)
            g_tempLowThreshold = val;
    }
}

/**********************************************************************
 * 函数名称： StorageTask
 * 功能描述： SPI Flash 存储任务
 *            手动保存传感器记录到 W25Q64
 * 输入参数： params - 未使用
 * 输出参数： 无
 * 返 回 值： 无
 ***********************************************************************/
void StorageTask(void *params)
{
    sensor_data_t data;

    /* 上电加载写指针 */
    if (storage_load_offset() != 0)
    {
        /* 初始化失败, 挂起自己 */
        vTaskSuspend(NULL);
    }

    /* 加载持久化的阈值 */
    storage_load_thresholds();

    /* 加载声音模式 (验证值在 1~5 范围, 否则保留编译期默认值) */
    {
        uint8_t modes[ALARM_TYPE_COUNT];
        int valid = 1;
        if (W25Q64_Read(ALARM_MODE_OFFSET, modes, ALARM_TYPE_COUNT) == ALARM_TYPE_COUNT)
        {
            for (int i = 0; i < ALARM_TYPE_COUNT; i++)
            {
                if (modes[i] < 1 || modes[i] > 5)
                    { valid = 0; break; }
            }
            if (valid)
                memcpy(g_alarmModes, modes, ALARM_TYPE_COUNT);
        }
    }

    for (;;)
    {
        /* 等待手动保存指令 (无限等待, 省 CPU) */
        if (xQueueReceive(xStorageQueue, &data, portMAX_DELAY) == pdTRUE)
        {
            storage_write_record(&data);
        }
    }
}
