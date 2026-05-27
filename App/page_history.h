#ifndef PAGE_HISTORY_H
#define PAGE_HISTORY_H

#include "page_ctx.h"
#include "storage_task.h"

uint32_t count_records_by_type(uint8_t type);
int      read_record_by_type(uint32_t idx, uint8_t type, storage_record_t *rec);

void draw_history_menu(page_ctx_t *ctx);
void draw_longterm_menu(page_ctx_t *ctx);
void draw_history_detail(page_ctx_t *ctx, page_t page);

#endif /* PAGE_HISTORY_H */
