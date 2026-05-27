#include "page_menu.h"
#include "app.h"
#include "driver_lcd.h"
#include <stdio.h>

#define LINE_TITLE      0
#define LINE_DATA1      2
#define LINE_DATA2      4
#define LINE_PROMPT     6
#define MENU_VISIBLE_ITEMS  3
#define THRESHOLD_VISIBLE   2
#define ALARM_SOUND_VISIBLE 3

/* ---- 菜单项 ---- */
static const char *s_menuItems[] = {
    "Set Threshold", "Alarm Sound", "View History",
    "Help", "About", "Reset",
};

/* ---- 阈值设置 ---- */
static const char *s_thresholdNames[] = {
    "Temp Hi", "Temp Lo", "Dist Lo", "Hum Hi", "Hum Lo", "Light Lo"
};
const int g_thresholdMins[]  = {25, 0, 5, 50, 0, 0};
const int g_thresholdMaxs[]  = {50, 25, 50, 100, 50, 1000};
const int g_thresholdSteps[] = {1, 1, 1, 1, 1, 10};

/* ---- 报警声音类型 ---- */
static const char *s_alarmSoundItems[] = {
    "Temp Hi", "Temp Lo", "Dist", "Hum Hi", "Hum Lo", "Light"
};

void draw_menu(page_ctx_t *ctx)
{
    char buf[17];
    LCD_Clear();
    LCD_PrintString(0, LINE_TITLE, "==MENU==");

    for (int i = 0; i < MENU_VISIBLE_ITEMS; i++)
    {
        int idx = ctx->menuOffset + i;
        if (idx >= 6) break;
        snprintf(buf, sizeof(buf), "%c%s",
                 (idx == ctx->menuSel) ? '>' : ' ', s_menuItems[idx]);
        LCD_PrintString(0, LINE_DATA1 + i * 2, buf);
    }
}

void draw_threshold(page_ctx_t *ctx)
{
    char buf[17];
    LCD_Clear();
    LCD_PrintString(0, LINE_TITLE, "==THRESHOLD==");

    for (int i = 0; i < THRESHOLD_VISIBLE; i++)
    {
        int idx = ctx->thresholdOffset + i;
        if (idx >= 6) break;
        snprintf(buf, sizeof(buf), "%c%s:%d",
                 (idx == ctx->thresholdField) ? '>' : ' ',
                 s_thresholdNames[idx], ctx->thresholdValues[idx]);
        LCD_PrintString(0, LINE_DATA1 + i * 2, buf);
    }

    LCD_PrintString(0, LINE_PROMPT, "PLAY:Sv <>:Fld");
}

void draw_alarm_sound_menu(page_ctx_t *ctx)
{
    char buf[17];
    LCD_Clear();
    LCD_PrintString(0, LINE_TITLE, "==ALARM SOUND==");

    for (int i = 0; i < ALARM_SOUND_VISIBLE; i++)
    {
        int idx = ctx->alarmSoundOffset + i;
        if (idx >= 6) break;
        snprintf(buf, sizeof(buf), "%c%s(%d)",
                 (idx == ctx->alarmSoundSel) ? '>' : ' ',
                 s_alarmSoundItems[idx], g_alarmModes[idx]);
        LCD_PrintString(0, LINE_DATA1 + i * 2, buf);
    }
}

void draw_alarm_sound_set(page_ctx_t *ctx)
{
    char buf[17];
    LCD_Clear();
    snprintf(buf, sizeof(buf), "==%s==", s_alarmSoundItems[ctx->alarmSoundType]);
    LCD_PrintString(0, LINE_TITLE, buf);

    snprintf(buf, sizeof(buf), "Mode: [%d]", ctx->alarmSoundLocalMode);
    LCD_PrintString(0, LINE_DATA1, buf);

    LCD_PrintString(0, LINE_PROMPT, "UP/DN CH-:Back");
}

void draw_reset(page_ctx_t *ctx)
{
    LCD_Clear();
    LCD_PrintString(0, LINE_TITLE, "==RESET==");

    if (ctx->resetStep == 0)
    {
        LCD_PrintString(0, LINE_DATA1, "Press PLAY to");
        LCD_PrintString(0, LINE_DATA2, "reset all defaults");
    }
    else
    {
        LCD_PrintString(0, LINE_DATA1, "Press PLAY again");
        LCD_PrintString(0, LINE_DATA2, "to confirm reset");
    }

    LCD_PrintString(0, LINE_PROMPT, "MENU/BACK:Exit");
}
