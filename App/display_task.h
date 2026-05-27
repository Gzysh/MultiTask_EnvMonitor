#ifndef _DISPLAY_TASK_H
#define _DISPLAY_TASK_H

/*
 * 页面枚举
 * 通过按键在页面间切换
 */
typedef enum {
    PAGE_HOME = 0,          /* 主界面: 显示所有传感器实时数据 */
    PAGE_MENU,              /* 菜单: 功能选项列表            */
    PAGE_THRESHOLD_SET,     /* 阈值设置: 修改报警阈值        */
    PAGE_HISTORY,           /* 历史记录子菜单 (Manual/Warning/Long-term) */
    PAGE_HISTORY_MANUAL,    /* 手动记录: PLAY 保存的临时记录  */
    PAGE_HISTORY_WARNING,   /* 警告记录: 警报 >10s 自动保存(环形缓冲) */
    PAGE_HISTORY_LONGTERM,  /* Long-term 子菜单 (Manu Save / Alarm Save) */
    PAGE_ABOUT,             /* 关于: 项目信息               */
    PAGE_HELP,              /* 帮助: 按键功能说明           */
    PAGE_ALARM_SOUND_MENU,  /* 报警声音模式子菜单           */
    PAGE_ALARM_SOUND_SET,   /* 单个报警的声音模式设置       */
    PAGE_HISTORY_LT_MANUAL, /* 长期手动记录详情 (扇区0存, 最多10条) */
    PAGE_HISTORY_LT_ALARM,  /* 长期警报记录详情 (扇区0存, 最多10条) */
    PAGE_RESET,             /* 恢复出厂设置                 */
    PAGE_MAX,
    PAGE_INVALID = 0xFF      /* 哨兵值, 用于强制首次绘制 */
} page_t;

#endif /* _DISPLAY_TASK_H */
