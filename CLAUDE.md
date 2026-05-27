# 多任务智能环境监测仪

## 项目概述
基于 STM32F103C8T6 + FreeRTOS v10.0.1 的多任务传感器数据采集与环境监测系统。  
采集温湿度(DHT11)、光照(ADC)、距离(SR04)数据，OLED 显示，红外遥控交互，阈值报警，SPI Flash 存储。

## 构建指南
- IDE: Keil MDK-ARM v5
- 编译器: ARMCC v5/v6
- 工程文件: `MDK-ARM/MultiTask_EnvMonitor.uvprojx`
- FreeRTOS: v10.0.1 (CMSIS RTOS v1 封装)
- HAL: STM32Cube_FW_F1 v1.8.x
- 下载: ST-Link, Flash 算法选 64KB

## 代码规范
- **函数命名**:
  - App 层: `module_name_function()` (snake_case, 如 `storage_write_record`, `draw_home_init`)
  - 驱动层: `Module_CamelCase()` (如 `W25Q64_Read`, `DHT11_Init`, `OLED_Clear`)
  - 任务函数: `TaskName()` (PascalCase, 如 `SensorTask`, `DisplayTask`)
- **变量命名**: 
  - 全局: `g_` 前缀 (`g_tempHighThreshold`)
  - 静态: `s_` 前缀 (`s_alarmDuration`)
  - 局部: 直接命名
- **注释风格**: 函数头用块注释说明功能/参数/返回值，不保留修改历史
- **文件组织**: App/ 放任务逻辑和页面模块，Drivers/ 放外设驱动，Core/ 放 CubeMX 生成代码

## 架构
- **6 个 FreeRTOS 任务**: Sensor(2), Key(2), Display(1), Alarm(3), Storage(1), Comms(1)
- **IPC**: Queue(4) + EventGroup(1) + Mutex(1)
- **数据流**: Sensor → Queue → Display, Storage, Comms; Key → Queue → Display; Sensor → EventGroup → Alarm
- **UART 输出**: CommsTask 从 `xUartTxQueue` 接收传感器数据，通过 USART1(115200-8N1) 发送文本帧到 PC；每 ~30s 发送一行 `[HEALTH]` 健康报告（任务栈高水位 + 堆余量）
- **Python 上位机**: `host_tool/` 目录，pyserial + tkinter + matplotlib，功能包括实时数据曲线(4子图)、CSV记录、健康监控面板(栈进度条+堆余量)
- **RGB LED 状态指示**: TIM2 PWM 驱动全彩 LED，正常→绿灯，报警→红灯，停止阶段→黄灯，上电自检→白灯
- **UI 模块化**: Display 任务按页面拆分为 4 个 page 模块 (`page_home`, `page_menu`, `page_history`, `page_about_help`)，由 `page_ctx.h` 集中管理页面状态，draw 函数统一签名 `void draw_xxx(page_ctx_t *ctx)`

## 存储布局
- W25Q64 (8MB NOR Flash)
- 扇区 0: 系统信息(阈值/声音模式/LT记录)
- 扇区 1~2047: 环形缓冲(524032 条记录)
- 标记位法管理记录槽位，避免计数器问题
- 每条记录带 CRC-8 (poly 0x07) 校验，写入时计算、读取时验证；旧记录 (reserved[1]==0) 跳过校验以兼容升级

## 非阻塞设计
- 蜂鸣器: 计数器状态机 (key_task.c), 无 vTaskDelay 阻塞
- SPI Flash 等待: W25Q64_WaitReady 用 vTaskDelay(1) 替代 mdelay(1) 忙等

## 编译器配置
- ARMCC v5.06, C99 标准
- 未引用的 static 函数用 `#if 0` 包裹（driver_oled.c 中 8 个滚动/配置函数）
- 新增 .c 文件需同步添加到 `uvprojx` 的对应 Group 中

## 已知约束
- PB9 引脚冲突: SR04_TRIG 与 W25Q64_CS 共用，不可同时使用超声波和 SPI Flash
- SRAM 20KB，总堆栈约 5KB，谨慎添加大 buffer

---

# Behavioral Guidelines

Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

---

**These guidelines are working if:** fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.
