#include "app.h"
#include "display_task.h"
#include "page_ctx.h"
#include "page_home.h"
#include "page_menu.h"
#include "page_history.h"
#include "page_about_help.h"
#include "driver_lcd.h"
#include "storage_task.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

#define LINE_TITLE      0
#define LINE_DATA1      2
#define LINE_DATA2      4
#define LINE_PROMPT     6

/* OLED SSD1306 驱动说明:
 *   每字符 2 page = 16 像素高, 8 个 page = 64 像素
 *   每行最多 16 字符, 有效 Y: 0, 2, 4, 6 (4 行)
 */

/* ---- 分发表 ---- */
static void draw_page(page_ctx_t *ctx)
{
    switch (ctx->curPage)
    {
    case PAGE_HOME:               draw_home_init(ctx);               break;
    case PAGE_MENU:               draw_menu(ctx);                    break;
    case PAGE_THRESHOLD_SET:      draw_threshold(ctx);               break;
    case PAGE_ALARM_SOUND_MENU:   draw_alarm_sound_menu(ctx);        break;
    case PAGE_ALARM_SOUND_SET:    draw_alarm_sound_set(ctx);         break;
    case PAGE_HISTORY:            draw_history_menu(ctx);            break;
    case PAGE_HISTORY_MANUAL:     draw_history_detail(ctx, PAGE_HISTORY_MANUAL);    break;
    case PAGE_HISTORY_WARNING:    draw_history_detail(ctx, PAGE_HISTORY_WARNING);   break;
    case PAGE_HISTORY_LONGTERM:   draw_longterm_menu(ctx);                           break;
    case PAGE_HISTORY_LT_MANUAL:  draw_history_detail(ctx, PAGE_HISTORY_LT_MANUAL); break;
    case PAGE_HISTORY_LT_ALARM:   draw_history_detail(ctx, PAGE_HISTORY_LT_ALARM);  break;
    case PAGE_ABOUT:              draw_about(ctx);                   break;
    case PAGE_HELP:               draw_help(ctx);                    break;
    case PAGE_RESET:              draw_reset(ctx);                   break;
    default: break;
    }
}

/* ---- 按键导航 ---- */
static void handle_key_navigation(page_ctx_t *ctx, key_cmd_t key, int *needRedraw)
{
    if (key == KEY_HOME && ctx->curPage != PAGE_HOME && ctx->curPage != PAGE_HELP)
    {
        ctx->curPage = PAGE_HOME;
        ctx->prevData.temperature = ctx->lastData.temperature ^ 1;
        ctx->prevData.light = ~ctx->lastData.light;
        *needRedraw = 1;
        return;
    }

    if (key == KEY_MENU && ctx->curPage != PAGE_HOME && ctx->curPage != PAGE_MENU && ctx->curPage != PAGE_HELP)
    {
        if (ctx->curPage == PAGE_ALARM_SOUND_SET)
        {
            if (g_alarmModes[ctx->alarmSoundType] != ctx->alarmSoundLocalMode)
            {
                g_alarmModes[ctx->alarmSoundType] = ctx->alarmSoundLocalMode;
                storage_save_alarm_modes();
            }
        }
        ctx->curPage = PAGE_MENU;
        *needRedraw = 1;
        return;
    }

    switch (ctx->curPage)
    {
    case PAGE_HOME:
        if (key == KEY_MENU)
            ctx->curPage = PAGE_MENU;
        break;

    case PAGE_MENU:
        if (key == KEY_UP && ctx->menuSel > 0)
        {
            ctx->menuSel--;
            if (ctx->menuSel < ctx->menuOffset)
                ctx->menuOffset--;
            *needRedraw = 1;
        }
        else if (key == KEY_DOWN && ctx->menuSel < 6 - 1)
        {
            ctx->menuSel++;
            if (ctx->menuSel >= ctx->menuOffset + 3)
                ctx->menuOffset++;
            *needRedraw = 1;
        }
        else if (key == KEY_CONFIRM)
        {
            switch (ctx->menuSel)
            {
            case 0:
                ctx->curPage = PAGE_THRESHOLD_SET;
                ctx->thresholdField = 0;
                ctx->thresholdOffset = 0;
                ctx->thresholdValues[0] = g_tempHighThreshold;
                ctx->thresholdValues[1] = g_tempLowThreshold;
                ctx->thresholdValues[2] = g_distMinThreshold;
                ctx->thresholdValues[3] = g_humHighThreshold;
                ctx->thresholdValues[4] = g_humLowThreshold;
                ctx->thresholdValues[5] = g_lightLowThreshold;
                break;
            case 1:
                ctx->curPage = PAGE_ALARM_SOUND_MENU;
                ctx->alarmSoundSel = 0;
                ctx->alarmSoundOffset = 0;
                break;
            case 2:
                ctx->curPage = PAGE_HISTORY;
                ctx->historySubSel = 0;
                ctx->historyPlayStep = 0;
                ctx->historyMenuOffset = 0;
                break;
            case 3:
                ctx->curPage = PAGE_HELP;
                ctx->helpLastKey = KEY_NONE;
                break;
            case 4:
                ctx->curPage = PAGE_ABOUT;
                ctx->aboutOffset = 0;
                break;
            case 5:
                ctx->curPage = PAGE_RESET;
                ctx->resetStep = 0;
                break;
            default: break;
            }
        }
        else if (key == KEY_BACK)
        {
            ctx->curPage = PAGE_HOME;
            ctx->prevData.temperature = ctx->lastData.temperature ^ 1;
            ctx->prevData.light = ~ctx->lastData.light;
        }
        break;

    case PAGE_THRESHOLD_SET:
        if (key == KEY_LEFT)
        {
            if (ctx->thresholdField > 0)
            {
                ctx->thresholdField--;
                if (ctx->thresholdField < ctx->thresholdOffset)
                    ctx->thresholdOffset--;
                *needRedraw = 1;
            }
        }
        else if (key == KEY_RIGHT)
        {
            if (ctx->thresholdField < 6 - 1)
            {
                ctx->thresholdField++;
                if (ctx->thresholdField >= ctx->thresholdOffset + 2)
                    ctx->thresholdOffset++;
                *needRedraw = 1;
            }
        }
        else if (key == KEY_UP)
        {
            int f = ctx->thresholdField;
            if (f < 6 && ctx->thresholdValues[f] + g_thresholdSteps[f] <= g_thresholdMaxs[f])
            {
                ctx->thresholdValues[f] += g_thresholdSteps[f];
                *needRedraw = 1;
            }
        }
        else if (key == KEY_DOWN)
        {
            int f = ctx->thresholdField;
            if (f < 6 && ctx->thresholdValues[f] - g_thresholdSteps[f] >= g_thresholdMins[f])
            {
                ctx->thresholdValues[f] -= g_thresholdSteps[f];
                *needRedraw = 1;
            }
        }
        else if (key == KEY_CONFIRM)
        {
            g_tempHighThreshold  = ctx->thresholdValues[0];
            g_tempLowThreshold   = ctx->thresholdValues[1];
            g_distMinThreshold   = ctx->thresholdValues[2];
            g_humHighThreshold   = ctx->thresholdValues[3];
            g_humLowThreshold    = ctx->thresholdValues[4];
            g_lightLowThreshold  = ctx->thresholdValues[5];
            storage_save_thresholds();
            LCD_Clear();
            LCD_PrintString(0, LINE_TITLE, "==THRESHOLD==");
            LCD_PrintString(0, LINE_DATA1, "Settings Saved!");
            vTaskDelay(pdMS_TO_TICKS(600));
            *needRedraw = 1;
        }
        else if (key == KEY_BACK)
            ctx->curPage = PAGE_MENU;
        break;

    case PAGE_HISTORY:
        if (key == KEY_UP && ctx->historySubSel > 0)
        {
            ctx->historySubSel--;
            if (ctx->historySubSel < ctx->historyMenuOffset)
                ctx->historyMenuOffset--;
            *needRedraw = 1;
        }
        else if (key == KEY_DOWN && ctx->historySubSel < 2)
        {
            ctx->historySubSel++;
            if (ctx->historySubSel >= ctx->historyMenuOffset + 2)
                ctx->historyMenuOffset++;
            *needRedraw = 1;
        }
        else if (key == KEY_CONFIRM)
        {
            ctx->historyPlayStep = 0;
            ctx->historyIdx = 0;
            switch (ctx->historySubSel)
            {
            case 0:
                ctx->historyTotal = count_records_by_type(RECORD_TYPE_MANUAL);
                ctx->curPage = PAGE_HISTORY_MANUAL;
                break;
            case 1:
                ctx->historyTotal = count_records_by_type(RECORD_TYPE_WARNING);
                ctx->curPage = PAGE_HISTORY_WARNING;
                break;
            case 2:
                ctx->curPage = PAGE_HISTORY_LONGTERM;
                ctx->ltSubSel = 0;
                break;
            }
        }
        else if (key == KEY_BACK)
            ctx->curPage = PAGE_MENU;
        break;

    case PAGE_HISTORY_LONGTERM:
        if (key == KEY_UP && ctx->ltSubSel > 0)
        {
            ctx->ltSubSel--;
            *needRedraw = 1;
        }
        else if (key == KEY_DOWN && ctx->ltSubSel < 1)
        {
            ctx->ltSubSel++;
            *needRedraw = 1;
        }
        else if (key == KEY_CONFIRM)
        {
            ctx->historyPlayStep = 0;
            ctx->historyIdx = 0;
            if (ctx->ltSubSel == 0)
            {
                ctx->historyTotal = storage_lt_manual_count();
                ctx->curPage = PAGE_HISTORY_LT_MANUAL;
            }
            else
            {
                ctx->historyTotal = storage_lt_alarm_count();
                ctx->curPage = PAGE_HISTORY_LT_ALARM;
            }
        }
        else if (key == KEY_BACK)
            ctx->curPage = PAGE_HISTORY;
        break;

    case PAGE_HISTORY_MANUAL:
    case PAGE_HISTORY_WARNING:
    case PAGE_HISTORY_LT_MANUAL:
    case PAGE_HISTORY_LT_ALARM:
        if (key == KEY_UP)
        {
            if (ctx->historyIdx + 1 < ctx->historyTotal)
            {
                ctx->historyIdx++;
                ctx->historyPlayStep = 0;
                *needRedraw = 1;
            }
        }
        else if (key == KEY_DOWN)
        {
            if (ctx->historyIdx > 0)
            {
                ctx->historyIdx--;
                ctx->historyPlayStep = 0;
                *needRedraw = 1;
            }
        }
        else if (key == KEY_CONFIRM)
        {
            if (ctx->historyPlayStep == 0)
            {
                ctx->historyPlayStep = 1;
                *needRedraw = 1;
            }
            else
            {
                storage_record_t rec;
                int ok = -1;

                if (ctx->curPage == PAGE_HISTORY_MANUAL)
                    ok = read_record_by_type(ctx->historyIdx, RECORD_TYPE_MANUAL, &rec);
                else if (ctx->curPage == PAGE_HISTORY_WARNING)
                    ok = read_record_by_type(ctx->historyIdx, RECORD_TYPE_WARNING, &rec);
                else
                    ok = 0;

                if (ctx->curPage == PAGE_HISTORY_LT_MANUAL)
                {
                    uint8_t raw_idx = (uint8_t)(ctx->historyTotal - 1 - ctx->historyIdx);
                    if (storage_lt_manual_delete(raw_idx) == 0)
                    {
                        ctx->historyTotal = storage_lt_manual_count();
                        if (ctx->historyIdx >= ctx->historyTotal && ctx->historyTotal > 0)
                            ctx->historyIdx = ctx->historyTotal - 1;
                        LCD_Clear();
                        LCD_PrintString(0, LINE_TITLE, "==MANU SAVE==");
                        LCD_PrintString(0, LINE_DATA1, "Deleted!");
                        vTaskDelay(pdMS_TO_TICKS(600));
                    }
                }
                else if (ctx->curPage == PAGE_HISTORY_LT_ALARM)
                {
                    uint8_t raw_idx = (uint8_t)(ctx->historyTotal - 1 - ctx->historyIdx);
                    if (storage_lt_alarm_delete(raw_idx) == 0)
                    {
                        ctx->historyTotal = storage_lt_alarm_count();
                        if (ctx->historyIdx >= ctx->historyTotal && ctx->historyTotal > 0)
                            ctx->historyIdx = ctx->historyTotal - 1;
                        LCD_Clear();
                        LCD_PrintString(0, LINE_TITLE, "==ALARM SAVE==");
                        LCD_PrintString(0, LINE_DATA1, "Deleted!");
                        vTaskDelay(pdMS_TO_TICKS(600));
                    }
                }
                else if (ok == 0)
                {
                    int saved = -1;
                    if (ctx->curPage == PAGE_HISTORY_MANUAL)
                        saved = storage_lt_manual_save(&rec);
                    else if (ctx->curPage == PAGE_HISTORY_WARNING)
                        saved = storage_lt_alarm_save(&rec);

                    if (saved == 0)
                    {
                        LCD_Clear();
                        LCD_PrintString(0, LINE_TITLE, "==L-SAVE==");
                        LCD_PrintString(0, LINE_DATA1, "Saved!");
                        vTaskDelay(pdMS_TO_TICKS(600));
                    }
                    else
                    {
                        LCD_Clear();
                        LCD_PrintString(0, LINE_TITLE, "==L-SAVE==");
                        LCD_PrintString(0, LINE_DATA1, "Long-term full");
                        LCD_PrintString(0, LINE_DATA2, "(max 10)");
                        vTaskDelay(pdMS_TO_TICKS(600));
                    }
                }
                ctx->historyPlayStep = 0;
                *needRedraw = 1;
            }
        }
        else
        {
            if (ctx->historyPlayStep)
            {
                ctx->historyPlayStep = 0;
                *needRedraw = 1;
            }
            else if (key == KEY_BACK)
            {
                if (ctx->curPage == PAGE_HISTORY_LT_MANUAL || ctx->curPage == PAGE_HISTORY_LT_ALARM)
                    ctx->curPage = PAGE_HISTORY_LONGTERM;
                else
                    ctx->curPage = PAGE_HISTORY;
            }
        }
        break;

    case PAGE_ALARM_SOUND_MENU:
        if (key == KEY_UP && ctx->alarmSoundSel > 0)
        {
            ctx->alarmSoundSel--;
            if (ctx->alarmSoundSel < ctx->alarmSoundOffset)
                ctx->alarmSoundOffset--;
            *needRedraw = 1;
        }
        else if (key == KEY_DOWN && ctx->alarmSoundSel < 6 - 1)
        {
            ctx->alarmSoundSel++;
            if (ctx->alarmSoundSel >= ctx->alarmSoundOffset + 3)
                ctx->alarmSoundOffset++;
            *needRedraw = 1;
        }
        else if (key == KEY_CONFIRM)
        {
            ctx->alarmSoundType = ctx->alarmSoundSel;
            ctx->alarmSoundLocalMode = g_alarmModes[ctx->alarmSoundType];
            ctx->curPage = PAGE_ALARM_SOUND_SET;
        }
        else if (key == KEY_BACK)
            ctx->curPage = PAGE_MENU;
        break;

    case PAGE_ALARM_SOUND_SET:
        if (key == KEY_UP)
        {
            if (ctx->alarmSoundLocalMode < 5)
            {
                ctx->alarmSoundLocalMode++;
                *needRedraw = 1;
            }
        }
        else if (key == KEY_DOWN)
        {
            if (ctx->alarmSoundLocalMode > 1)
            {
                ctx->alarmSoundLocalMode--;
                *needRedraw = 1;
            }
        }
        else if (key == KEY_CONFIRM || key == KEY_BACK)
        {
            if (g_alarmModes[ctx->alarmSoundType] != ctx->alarmSoundLocalMode)
            {
                g_alarmModes[ctx->alarmSoundType] = ctx->alarmSoundLocalMode;
                storage_save_alarm_modes();
            }
            ctx->curPage = PAGE_ALARM_SOUND_MENU;
        }
        break;

    case PAGE_ABOUT:
        if (key == KEY_UP)
        {
            if (ctx->aboutOffset > 0)
            {
                ctx->aboutOffset--;
                *needRedraw = 1;
            }
        }
        else if (key == KEY_DOWN)
        {
            if (ctx->aboutOffset + 3 < 8)
            {
                ctx->aboutOffset++;
                *needRedraw = 1;
            }
        }
        else if (key == KEY_BACK)
            ctx->curPage = PAGE_MENU;
        break;

    case PAGE_HELP:
        if (key == KEY_HOME)
        {
            if (key == ctx->helpLastKey)
            {
                ctx->curPage = PAGE_HOME;
                ctx->prevData.temperature = ctx->lastData.temperature ^ 1;
                ctx->prevData.light = ~ctx->lastData.light;
            }
            else
            {
                ctx->helpLastKey = key;
                *needRedraw = 1;
            }
        }
        else if (key == KEY_MENU || key == KEY_BACK)
        {
            if (key == ctx->helpLastKey)
                ctx->curPage = PAGE_MENU;
            else
            {
                ctx->helpLastKey = key;
                *needRedraw = 1;
            }
        }
        else
        {
            ctx->helpLastKey = key;
            *needRedraw = 1;
        }
        break;

    case PAGE_RESET:
        if (key == KEY_CONFIRM)
        {
            if (ctx->resetStep == 0)
            {
                ctx->resetStep = 1;
                *needRedraw = 1;
            }
            else
            {
                g_tempHighThreshold = 35;
                g_tempLowThreshold  = 10;
                g_distMinThreshold  = 10;
                g_humHighThreshold  = 80;
                g_humLowThreshold   = 20;
                g_lightLowThreshold = 500;
                {
                    uint8_t def[] = {1, 1, 2, 1, 1, 2};
                    memcpy(g_alarmModes, def, 6);
                }
                storage_save_thresholds();
                storage_save_alarm_modes();
                storage_clear_lt_banks();
                LCD_Clear();
                LCD_PrintString(0, LINE_TITLE, "==RESET==");
                LCD_PrintString(0, LINE_DATA1, "Reset OK!");
                vTaskDelay(pdMS_TO_TICKS(600));
                ctx->curPage = PAGE_MENU;
                ctx->resetStep = 0;
            }
        }
        else if (key == KEY_BACK)
        {
            ctx->curPage = PAGE_MENU;
            ctx->resetStep = 0;
        }
        break;

    default:
        if (key == KEY_BACK)
            ctx->curPage = PAGE_MENU;
        break;
    }
}

/* ---- DisplayTask 主循环 ---- */
void DisplayTask(void *params)
{
    key_cmd_t key;
    int needRedraw = 0;
    page_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    LCD_Init();
    LCD_Clear();
    LCD_PrintString(0, LINE_TITLE, "System Starting");

    ctx.curPage = PAGE_HOME;
    ctx.prevPage = PAGE_INVALID;

    for (;;)
    {
        key = KEY_NONE;

        xQueueReceive(xSensorQueue, &ctx.lastData, 0);
        xQueueReceive(xKeyQueue,    &key,         pdMS_TO_TICKS(50));

        if (key != KEY_NONE)
        {
            /* 首页按 PLAY → 手动保存记录 */
            if (ctx.curPage == PAGE_HOME && key == KEY_CONFIRM)
            {
                sensor_data_t save_data = ctx.lastData;
                save_data.record_type = RECORD_TYPE_MANUAL;
                xQueueSend(xStorageQueue, &save_data, 0);

                LCD_Clear();
                LCD_PrintString(0, LINE_TITLE, "==ENV MONITOR==");
                LCD_PrintString(0, LINE_DATA1, "Record Saved!");
                vTaskDelay(pdMS_TO_TICKS(600));

                ctx.prevPage = PAGE_INVALID;
                ctx.prevData.temperature = ctx.lastData.temperature ^ 1;
                ctx.prevData.light = ~ctx.lastData.light;
            }

            handle_key_navigation(&ctx, key, &needRedraw);
            key = KEY_NONE;
        }

        /* 页面切换或强制重绘 */
        if (ctx.curPage != ctx.prevPage)
        {
            ctx.prevPage = ctx.curPage;
            draw_page(&ctx);
            needRedraw = 0;
        }
        else if (needRedraw)
        {
            needRedraw = 0;
            draw_page(&ctx);
        }

        /* 首页：数据变化时增量刷新 */
        if (ctx.curPage == PAGE_HOME)
        {
            draw_home_data(&ctx);
            ctx.prevData = ctx.lastData;
        }
    }
}
