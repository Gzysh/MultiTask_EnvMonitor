#include "page_home.h"
#include "driver_lcd.h"
#include <stdio.h>

#define LINE_TITLE      0
#define LINE_DATA1      2
#define LINE_DATA2      4
#define LINE_PROMPT     6

void draw_home_init(page_ctx_t *ctx)
{
    (void)ctx;
    LCD_Clear();
    LCD_PrintString(0, LINE_TITLE, "==ENV MONITOR==");
    LCD_PrintString(0, LINE_PROMPT, "OK:Save CH-:Back");
}

void draw_home_data(page_ctx_t *ctx)
{
    char buf[17];

    if (ctx->lastData.temperature != ctx->prevData.temperature ||
        ctx->lastData.humidity    != ctx->prevData.humidity)
    {
        snprintf(buf, sizeof(buf), "T:%d.0C H:%d.0%%",
                 ctx->lastData.temperature, ctx->lastData.humidity);
        LCD_ClearLine(0, LINE_DATA1);
        LCD_PrintString(0, LINE_DATA1, buf);
    }

    /* 死区避免 ADC / 超声波微小波动导致闪烁 */
    int32_t diffL = (int32_t)ctx->lastData.light - (int32_t)ctx->prevData.light;
    int32_t diffD = (int32_t)ctx->lastData.distance - (int32_t)ctx->prevData.distance;
    if (diffL > 20 || diffL < -20 || diffD > 3 || diffD < -3)
    {
        snprintf(buf, sizeof(buf), "L:%lu D:%lucm",
                 (unsigned long)ctx->lastData.light,
                 (unsigned long)ctx->lastData.distance);
        LCD_ClearLine(0, LINE_DATA2);
        LCD_PrintString(0, LINE_DATA2, buf);
    }
}
