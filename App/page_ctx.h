#ifndef PAGE_CTX_H
#define PAGE_CTX_H

#include "app.h"
#include "storage_task.h"
#include "display_task.h"

/*
 * 页面上下文 — 集中管理所有页面状态
 * DisplayTask 持有唯一实例，各页面模块通过指针读写
 */
typedef struct {
    /* 当前页与上一页（用于页面切换检测） */
    page_t curPage;
    page_t prevPage;

    /* 传感器数据缓存（优化 OLED 增量刷新） */
    sensor_data_t lastData;
    sensor_data_t prevData;

    /* 菜单导航 */
    uint8_t menuSel;
    uint8_t menuOffset;

    /* 阈值设置（6 个字段：Temp Hi/Lo, Dist, Hum Hi/Lo, Light） */
    int    thresholdValues[6];
    int    thresholdField;
    int    thresholdOffset;

    /* 报警声音模式选择 */
    int alarmSoundSel;
    int alarmSoundOffset;
    int alarmSoundType;
    int alarmSoundLocalMode;

    /* 恢复出厂设置（两步确认） */
    int resetStep;

    /* 历史记录浏览 */
    uint32_t historyIdx;
    uint32_t historyTotal;
    uint8_t  historySubSel;
    uint8_t  historyPlayStep;   /* 双击确认：0=正常, 1=已按一次PLAY */
    uint8_t  historyMenuOffset;
    uint8_t  ltSubSel;          /* Long-term 子菜单选择 */

    /* 关于页面滚动 */
    int aboutOffset;

    /* 帮助页（跟踪上一按键） */
    key_cmd_t helpLastKey;
} page_ctx_t;

#endif /* PAGE_CTX_H */
