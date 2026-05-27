#include "page_history.h"
#include "driver_lcd.h"
#include <stdio.h>
#include <string.h>

#define LINE_TITLE      0
#define LINE_DATA1      2
#define LINE_DATA2      4
#define LINE_PROMPT     6

/* 记录类型过滤扫描（环形缓冲区最近 256 条中筛选） */
uint32_t count_records_by_type(uint8_t type)
{
    uint32_t total = storage_get_record_count();
    uint32_t count = 0;
    uint32_t limit = 256;
    if (total > limit) total = limit;

    for (uint32_t i = 0; i < total; i++)
    {
        storage_record_t tmp;
        if (storage_read_newest(i, &tmp) != 0) break;
        if (tmp.reserved[0] == type) count++;
    }
    return count;
}

int read_record_by_type(uint32_t idx, uint8_t type, storage_record_t *rec)
{
    uint32_t total = storage_get_record_count();
    uint32_t found = 0;
    uint32_t limit = 256;
    if (total > limit) total = limit;

    for (uint32_t i = 0; i < total; i++)
    {
        storage_record_t tmp;
        if (storage_read_newest(i, &tmp) != 0) return -1;
        if (tmp.reserved[0] == type)
        {
            if (found == idx) { *rec = tmp; return 0; }
            found++;
        }
    }
    return -1;
}

void draw_history_menu(page_ctx_t *ctx)
{
    char buf[17];
    static const char *items[] = {"Manual", "Warning", "Long-term"};

    LCD_Clear();
    LCD_PrintString(0, LINE_TITLE, "==RECORDS==");

    for (int i = 0; i < 2; i++)
    {
        int idx = ctx->historyMenuOffset + i;
        if (idx >= 3) break;
        snprintf(buf, sizeof(buf), "%c%s",
                 (idx == ctx->historySubSel) ? '>' : ' ', items[idx]);
        LCD_PrintString(0, LINE_DATA1 + i * 2, buf);
    }

    LCD_PrintString(0, LINE_PROMPT, "PLAY:Enter CH-:Back");
}

void draw_longterm_menu(page_ctx_t *ctx)
{
    char buf[17];
    static const char *items[] = {"Manu Save", "Alarm Save"};

    LCD_Clear();
    LCD_PrintString(0, LINE_TITLE, "==LONG-TERM==");

    for (int i = 0; i < 2; i++)
    {
        int idx = i;
        snprintf(buf, sizeof(buf), "%c%s",
                 (idx == ctx->ltSubSel) ? '>' : ' ', items[idx]);
        LCD_PrintString(0, LINE_DATA1 + i * 2, buf);
    }

    LCD_PrintString(0, LINE_PROMPT, "PLAY:Enter CH-:Back");
}

void draw_history_detail(page_ctx_t *ctx, page_t page)
{
    char buf[17];
    storage_record_t rec;
    uint32_t total = 0;
    const char *title;
    int ok = -1;

    switch (page)
    {
    case PAGE_HISTORY_MANUAL:
        title = "==MANUAL==";
        total = ctx->historyTotal;
        ok = read_record_by_type(ctx->historyIdx, RECORD_TYPE_MANUAL, &rec);
        break;
    case PAGE_HISTORY_WARNING:
        title = "==WARNING==";
        total = ctx->historyTotal;
        ok = read_record_by_type(ctx->historyIdx, RECORD_TYPE_WARNING, &rec);
        break;
    case PAGE_HISTORY_LT_MANUAL:
        title = "==MANU SAVE==";
        total = storage_lt_manual_count();
        ok = storage_lt_manual_read(ctx->historyIdx, &rec);
        break;
    case PAGE_HISTORY_LT_ALARM:
        title = "==ALARM SAVE==";
        total = storage_lt_alarm_count();
        ok = storage_lt_alarm_read(ctx->historyIdx, &rec);
        break;
    default:
        return;
    }

    LCD_Clear();

    if (total == 0)
    {
        LCD_PrintString(0, LINE_TITLE, title);
        LCD_PrintString(0, LINE_DATA1, "No records yet");
        LCD_PrintString(0, LINE_PROMPT, "CH-:Back");
        return;
    }

    if (ctx->historyPlayStep)
    {
        LCD_PrintString(0, LINE_TITLE, title);
        LCD_PrintString(0, LINE_DATA1, "Press PLAY again");
        if (page == PAGE_HISTORY_LT_MANUAL || page == PAGE_HISTORY_LT_ALARM)
            LCD_PrintString(0, LINE_DATA2, "to delete");
        else
            LCD_PrintString(0, LINE_DATA2, "Save to L-term");
        LCD_PrintString(0, LINE_PROMPT, "Other key:cancel");
        return;
    }

    snprintf(buf, sizeof(buf), "%lu/%lu",
             (unsigned long)(ctx->historyIdx + 1), (unsigned long)total);
    LCD_PrintString(0, LINE_TITLE, buf);

    if (ok == 0)
    {
        int16_t t_int = rec.temperature / 10;
        int16_t t_dec = rec.temperature % 10;
        uint16_t h_int = rec.humidity / 10;
        uint16_t h_dec = rec.humidity % 10;
        snprintf(buf, sizeof(buf), "T:%d.%dC H:%d.%d%%",
                 t_int, t_dec, h_int, h_dec);
        LCD_PrintString(0, LINE_DATA1, buf);

        snprintf(buf, sizeof(buf), "L:%u D:%ucm",
                 (unsigned)rec.light, (unsigned)rec.distance);
        LCD_PrintString(0, LINE_DATA2, buf);
    }
    else
    {
        LCD_PrintString(0, LINE_DATA1, "Read error");
    }

    LCD_PrintString(0, LINE_PROMPT, "UP/DN PLAY CH-:Bk");
}
