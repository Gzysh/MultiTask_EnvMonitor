#ifndef PAGE_MENU_H
#define PAGE_MENU_H

#include "page_ctx.h"

/* 阈值参数表 (各字段 min/max/step，供显示与按键验证共用) */
extern const int g_thresholdMins[6];
extern const int g_thresholdMaxs[6];
extern const int g_thresholdSteps[6];

void draw_menu(page_ctx_t *ctx);
void draw_threshold(page_ctx_t *ctx);
void draw_alarm_sound_menu(page_ctx_t *ctx);
void draw_alarm_sound_set(page_ctx_t *ctx);
void draw_reset(page_ctx_t *ctx);

#endif /* PAGE_MENU_H */
