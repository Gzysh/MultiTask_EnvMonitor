#include "page_about_help.h"
#include "app.h"
#include "driver_lcd.h"
#include <stdio.h>

#define LINE_TITLE      0
#define LINE_DATA1      2
#define LINE_DATA2      4
#define LINE_PROMPT     6
#define ABOUT_LINE_COUNT    8

/* 关于页面文本 */
static const char *s_aboutLines[ABOUT_LINE_COUNT] = {
    "Env Monitor 1.0", "FreeRTOS+F103",
    "DHT11+Light+SR04", "OLED 128x64",
    "W25Q64 8MB", "IR Remote Ctrl",
    "Author:lzh", "2026",
};

/* 帮助页文本 */
static const char *s_helpNames[] = {
    "UP Scroll/Inc+",          /* KEY_UP     */
    "DOWN Scrl/Dec-",          /* KEY_DOWN   */
    "LEFT Switch fld",         /* KEY_LEFT   */
    "RIGHT Switch fld",        /* KEY_RIGHT  */
    "PLAY Save/Ok",            /* KEY_CONFIRM */
    "RETRN Return/Back",       /* KEY_BACK   */
    "MENU Menu/Back",          /* KEY_MENU   */
    "HOME Go Home",            /* KEY_HOME   */
};

static const char *s_helpHints[] = {
    "", "", "", "", "",
    "Again: go back",
    "Again: open menu",
    "Again: go home",
};

void draw_about(page_ctx_t *ctx)
{
    LCD_Clear();
    LCD_PrintString(0, LINE_TITLE, "==ABOUT==");

    for (int i = 0; i < 3; i++)
    {
        int idx = ctx->aboutOffset + i;
        if (idx < ABOUT_LINE_COUNT)
            LCD_PrintString(0, LINE_DATA1 + i * 2, s_aboutLines[idx]);
    }

    LCD_PrintString(0, LINE_PROMPT, "UP/DOWN CH-:Back");
}

void draw_help(page_ctx_t *ctx)
{
    LCD_Clear();
    LCD_PrintString(0, LINE_TITLE, "==HELP==");

    if (ctx->helpLastKey == KEY_NONE)
    {
        LCD_PrintString(0, LINE_DATA1, "Press any key to");
        LCD_PrintString(0, LINE_DATA2, "see its function");
    }
    else
    {
        int idx = (int)ctx->helpLastKey - 1;
        if (idx >= 0 && idx < 8)
        {
            LCD_PrintString(0, LINE_DATA1, s_helpNames[idx]);

            if (s_helpHints[idx][0] != '\0')
                LCD_PrintString(0, LINE_PROMPT, s_helpHints[idx]);
            else
                LCD_PrintString(0, LINE_PROMPT, "Press BACK to exit");
        }
    }
}
