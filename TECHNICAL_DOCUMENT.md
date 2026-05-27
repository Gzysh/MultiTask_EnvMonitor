# 多任务智能环境监测仪 — 技术文档

## 1. 项目概述

基于 **STM32F103C8T6** + **FreeRTOS v10.0.1** 的多任务传感器数据采集与环境监测系统。通过 4 类传感器实时采集环境数据，在 OLED 显示屏上呈现，支持红外遥控交互、阈值报警、数据存储和历史记录浏览。

### 1.1 硬件平台

| 组件 | 型号/规格 | 接口 | 说明 |
|------|-----------|------|------|
| MCU | STM32F103C8T6 | Cortex-M3 @ 72MHz | 64KB Flash, 20KB SRAM |
| 温湿度 | DHT11 | 单总线 GPIO | 精度 ±2°C, ±5%RH |
| 光照 | 光敏电阻 + ADC | ADC1, 12-bit | 原始值 0~4095 |
| 超声波 | HC-SR04 | GPIO 触发/回波 | 量程 2cm~400cm |
| 存储 | W25Q64 | SPI1 | 8MB NOR Flash |
| 显示 | SSD1306 128x64 | I2C1 | 4 行 × 16 字 |
| 遥控 | 红外 NEC 协议 | GPIO 中断 | 8 键遥控器 |
| 声光 | 有源蜂鸣器 + LED | GPIO | 有源蜂鸣器 PA8, LED PC13 |

### 1.2 系统时钟

```
HSE(8MHz) × PLL(x9) = 72MHz SYSCLK
  ├── AHB: 72MHz (div1)
  ├── APB1: 36MHz (div2) — I2C1, TIM2, TIM3, USART1
  └── APB2: 72MHz (div1) — ADC1, SPI1, TIM1, GPIO
```

---

## 2. 软件架构

### 2.1 任务概览

| 任务 | 函数 | 优先级 | 栈大小(字) | 周期 | 功能 |
|------|------|--------|-----------|------|------|
| 传感器采集 | `SensorTask` | 2 | 256 | 500ms | 轮询传感器、检测阈值、发数据 |
| 红外遥控 | `KeyTask` | 2 | 128 | 30ms | 读取红外键码、映射为命令、按键音效 |
| 显示交互 | `DisplayTask` | 1 | 256 | ~50ms | OLED 渲染（4 个 page 模块）、按键处理 |
| 声光报警 | `AlarmTask` | 3 | 128 | 阻塞等待 | 蜂鸣器 + RGB LED 模式播放 |
| 数据存储 | `StorageTask` | 1 | 256 | 阻塞等待 | SPI Flash 写入 |
| 通信输出 | `CommsTask` | 1 | 128 | 500ms触发 | USART1 发送传感器数据 + 健康报告 |

### 2.2 任务优先级

```
最高: AlarmTask(3)
        SensorTask(2)  KeyTask(2)
最低: DisplayTask(1)  StorageTask(1)  CommsTask(1)
```

### 2.3 队列与事件

| 通信对象 | 类型 | 大小 | 生产者 | 消费者 | 用途 |
|----------|------|------|--------|--------|------|
| `xSensorQueue` | Queue | 3 | SensorTask | DisplayTask | 传感器实时数据 |
| `xStorageQueue` | Queue | 5 | DisplayTask, SensorTask | StorageTask | 待写入 Flash 的数据 |
| `xKeyQueue` | Queue | 5 | KeyTask | DisplayTask | 按键命令 |
| `xUartTxQueue` | Queue | 3 | SensorTask | CommsTask | 传感器数据输出到 PC |
| `xAlarmEventGroup` | EventGroup | — | SensorTask | AlarmTask | 6 种报警事件位 |
| `xSpiMutex` | Mutex | — | 任意任务 | 任意任务 | SPI Flash 互斥访问 |

### 2.4 数据流

```
SensorTask ───xSensorQueue──→ DisplayTask ───OLED 显示
     │                             │
     │                             ├──PLAY键 → xStorageQueue──→ StorageTask──→W25Q64(环形缓冲)
     │                             │
     ├──xUartTxQueue──→ CommsTask──USART1──→ PC (串口终端 / Python上位机)
     │
     ├──10秒持续报警 → xStorageQueue──→ StorageTask──→W25Q64(环形缓冲)
     │
     └──阈值超限 → xAlarmEventGroup──→ AlarmTask──→蜂鸣器 + RGB LED
     
KeyTask ───xKeyQueue──→ DisplayTask ───按键响应
```

### 2.5 命名规范

| 层级 | 风格 | 示例 |
|------|------|------|
| App 层函数 | `module_name_function()` (snake_case) | `storage_write_record`, `draw_home_init` |
| 驱动层函数 | `Module_CamelCase()` | `W25Q64_Read`, `DHT11_Init`, `OLED_Clear` |
| 任务函数 | `TaskName()` (PascalCase) | `SensorTask`, `DisplayTask` |
| 全局变量 | `g_` 前缀 | `g_tempHighThreshold` |
| 静态变量 | `s_` 前缀 | `s_alarmDuration` |

---

## 3. 传感器数据格式

### 3.1 运行时数据结构 (`sensor_data_t`)

```c
typedef struct {
    int      temperature;   /* DHT11 温度, 摄氏度          */
    int      humidity;      /* DHT11 湿度, 百分比          */
    uint32_t light;         /* 光敏 ADC, 0~4095           */
    uint32_t distance;      /* 超声波距离, cm              */
    uint8_t  record_type;   /* 0=自动 1=手动 2=警告       */
} sensor_data_t;
```

### 3.2 Flash 存储记录 (`storage_record_t`, 16 字节)

```c
typedef struct __attribute__((packed)) {
    uint32_t timestamp;     /* 系统 tick */
    int16_t  temperature;   /* 温度 ×10, 如 280=28.0°C */
    uint16_t humidity;      /* 湿度 ×10, 如 650=65.0%  */
    uint16_t light;         /* ADC 原始值 */
    uint16_t distance;      /* cm */
    uint8_t  reserved[2];   /* [0]=record_type, [1]=CRC-8 (poly 0x07) */
} storage_record_t;
```

**CRC-8 校验**：
- 算法：CRC-8-ATM (poly 0x07)，bit-by-bit 计算
- 计算范围：除 reserved[1] 外的全部 15 字节
- 写入：`storage_write_record()` 设置 record_type 后自动计算并写入 reserved[1]
- 读取：`storage_read_newest()` 验证 CRC；旧记录 (reserved[1]==0) 跳过校验以兼容固件升级

### 3.3 记录类型

| 宏 | 值 | 说明 | 存储位置 |
|----|----|------|---------|
| `RECORD_TYPE_AUTO` | 0x00 | 传感器自动采集 | 环形缓冲 |
| `RECORD_TYPE_MANUAL` | 0x01 | 首页按 PLAY | 环形缓冲 |
| `RECORD_TYPE_WARNING` | 0x02 | 警报 >10s | 环形缓冲 |
| `RECORD_TYPE_LT_MANUAL` | 0x03 | 长期手动 | 扇区 0 |
| `RECORD_TYPE_LT_ALARM` | 0x04 | 长期警报 | 扇区 0 |

---

## 4. SPI Flash 存储布局

### 4.1 W25Q64 简介

- **容量**: 8MB (64Mbit), 2048 个扇区, 每扇区 4KB
- **特性**: NOR Flash, Page Program 为按位 AND 操作
- **页大小**: 256 字节
- **擦除**: 最小 4KB 扇区擦除, 擦除后全为 0xFF

### 4.2 扇区 0 — 系统信息 (0x000000 ~ 0x000FFF)

```
偏移      大小   内容
──────────────────────────────────────────
0x0000    4B    MAGIC (0xA55AA5A5)
0x0004    4B    写指针偏移 (环形缓冲当前写入地址)
0x0008    4B    写次数 (累计写入总条数)
0x000C    4B    保留
0x0010    4B    g_tempHighThreshold
0x0014    4B    g_distMinThreshold
0x0018    4B    g_humHighThreshold
0x001C    4B    g_humLowThreshold
0x0020    4B    g_lightLowThreshold
0x0024    4B    g_tempLowThreshold
0x0028    8B    保留
0x0030    6B    g_alarmModes[6] (声音模式号 1~5)
0x0036    10B   保留
0x0040    160B  LT_Manual 记录区 (10 条 × 16B)
0x00E0    16B   保留
0x00F0    160B  LT_Alarm 记录区 (10 条 × 16B)
0x0190    624B  空闲
0x0410    10B   LT_Alarm 标记位 (0xFF=空, 0x00=已用)
0x041A    40B   保留
0x0442    10B   LT_Manual 标记位 (0xFF=空, 0x00=已用)
0x044C    0xB4  空闲至扇区末尾
```

### 4.3 扇区 1~2047 — 环形缓冲 (0x001000 ~ 0x7FFFFF)

```
扇区 1:  0x001000 ~ 0x001FFF   256 条记录
扇区 2:  0x002000 ~ 0x002FFF   256 条记录
...
扇区 2047: 0x7FF000 ~ 0x7FFFFF  256 条记录
          ─────────────────
总计:    2047 扇区 × 256 条 = 524,032 条记录
```

环形缓冲从扇区 1 开始顺序写入，写满最后一个扇区后回到扇区 1 循环覆盖。每条记录 16 字节，每扇区 256 条。

### 4.4 标记位原理

标记位(Byte Marker)利用 NOR Flash 的特性：写入 0x00 到已擦除(0xFF)的字节只需一次 Page Program（位 1→0 是合法的）。标记 0xFF=空, 0x00=已用, 永不反向写回 0xFF（直到扇区擦除）。这避免了计数器方案中 NOR Flash 不能安全增量的固有问题。

---

## 5. 各任务详细设计

### 5.1 SensorTask

**周期**: 500ms (vTaskDelay)

**执行流程**:
1. 读取 DHT11 温湿度 — 失败时保持上次值
2. 读取光敏 ADC — 失败时强制设为 0
3. 读取超声波距离 — 失败时强制设为 0
4. 标记 `record_type = RECORD_TYPE_AUTO`
5. 通过 `xSensorQueue` 发送给 DisplayTask
6. 检查 6 项阈值，设置事件组位
7. 若有任何报警，`s_alarmDuration` 递增；达到 20 (10秒) 时：
   - 设置 `record_type = RECORD_TYPE_WARNING`
   - 通过 `xStorageQueue` 发送给 StorageTask（写入环形缓冲）
   - `s_alarmDuration` 锁定为 21 防重复
8. 若无报警，`s_alarmDuration` 清零

**关键阈值**:

| 报警类型 | 条件 | 默认阈值 |
|----------|------|---------|
| 温度过高 | `temperature > g_tempHighThreshold` | 35°C |
| 温度过低 | `temperature < g_tempLowThreshold` | 10°C |
| 距离过近 | `distance < g_distMinThreshold && distance > 0` | 10cm |
| 湿度过高 | `humidity > g_humHighThreshold` | 80% |
| 湿度过低 | `humidity < g_humLowThreshold && humidity > 0` | 20% |
| 光照过低 | `light < g_lightLowThreshold` | 500 |

### 5.2 AlarmTask

**优先级最高 (3)**, 阻塞在 `xEventGroupWaitBits` 上。

- 支持 6 种报警类型 (Temp Hi/Lo, Dist, Hum Hi/Lo, Light Lo)
- 每种报警独立配置声音模式 (1~5)
- 声音模式定义 (cycles, on_ms, off_ms, fast_on_ms, fast_off_ms, switch_ms):

| 模式 | 特性 | 说明 |
|------|------|------|
| 1 | 3 次 300ms 长响 | 一般报警 |
| 2 | 5 次 100ms 短促 | 注意 |
| 3 | 10 次 50ms 快速 | 紧急 |
| 4 | 4 次 200/400ms, 10s 后变 100/50ms 急促 | 可升级 |
| 5 | 2 次 1000ms 长鸣 | 持续注意 |

- 模式 4 有 60 秒最大持续保护
- 条件清除后检测：清除事件位 → 等待 550ms → 若未重新设置则进入停止阶段

### 5.3 KeyTask

**周期**: 30ms (vTaskDelay)

- 红外接收器初始化后每 30ms 读取一次
- 将红外键码映射为 `key_cmd_t` 后发队列
- 每次有效按键触发 40ms 蜂鸣器音效反馈（通过 ActiveBuzzer_Control）
- 未识别键码和遥控器重复码不触发音效
- 支持 8 个按键命令：UP, DOWN, LEFT, RIGHT, CONFIRM, BACK, MENU, HOME

### 5.4 DisplayTask

**周期**: ~50ms（受按键队列 50ms 阻塞超时约束）

**UI 模块化重构**: DisplayTask 原为 ~1093 行单体文件，已拆分为 4 个页面模块 + 1 个上下文头文件：

| 模块 | 文件 | 职责 |
|------|------|------|
| page_home | page_home.c/h | 主页面实时数据显示（`draw_home_init`, `draw_home_data`） |
| page_menu | page_menu.c/h | 菜单导航、阈值、报警声音、恢复出厂设置 |
| page_history | page_history.c/h | 历史记录浏览（环形缓冲 / LT bank） |
| page_about_help | page_about_help.c/h | 关于信息、按键帮助 |
| page_ctx | page_ctx.h | 集中管理页面上下文（curPage, lastData, menuSel, 阈值等） |

所有 draw 函数统一签名 `void draw_xxx(page_ctx_t *ctx)`，DisplayTask 主循环通过 `ctx->curPage` 派发。

**页面系统**:

```
PAGE_HOME           — 首页, 显示传感器实时数据
  │ OK键 → 手动保存记录到环形缓冲
  └── MENU键 → PAGE_MENU

PAGE_MENU           — 菜单
  ├── Set Threshold    → PAGE_THRESHOLD_SET
  ├── Alarm Sound      → PAGE_ALARM_SOUND_MENU
  ├── View History     → PAGE_HISTORY
  ├── Help             → PAGE_HELP
  ├── About            → PAGE_ABOUT
  └── Reset            → PAGE_RESET

PAGE_HISTORY        — 历史记录子菜单
  ├── Manual           → PAGE_HISTORY_MANUAL     (环形缓冲)
  ├── Warning          → PAGE_HISTORY_WARNING    (环形缓冲)
  └── Long-term        → PAGE_HISTORY_LONGTERM   (子菜单)

PAGE_HISTORY_LONGTERM — Long-term 子菜单
  ├── Manu Save        → PAGE_HISTORY_LT_MANUAL  (扇区0 ALT_MANUAL)
  └── Alarm Save       → PAGE_HISTORY_LT_ALARM   (扇区0 ALT_ALARM)

PAGE_THRESHOLD_SET  — 阈值调节 (UP/DOWN 增减, LEFT/RIGHT 切换字段)
PAGE_ALARM_SOUND_MENU/SET — 声音模式选择, 1~5
PAGE_ABOUT          — 关于信息 (上下滚动)
PAGE_HELP           — 按键功能说明
PAGE_RESET          — 恢复出厂设置 (需两次确认)
```

**主循环队列接收顺序** (优化后):

```c
xQueueReceive(xSensorQueue, &s_lastData, 0);                    // 传感器数据 — 非阻塞
xQueueReceive(xKeyQueue,    &key,        pdMS_TO_TICKS(50));    // 按键 — 阻塞 50ms
```

先非阻塞检查传感器数据，再阻塞等待按键（最长 50ms）。该顺序确保按键响应延迟 ≤50ms，防止传感器队列阻塞导致按键丢失。

**UI 操作提示延时**: 所有瞬态提示（"Record Saved!"、"Saved!"、"Deleted!" 等）显示 600ms 后自动清除，兼顾可读性与响应速度。

**双击 PLAY 行为**:

| 当前页 | 双击 PLAY 效果 |
|--------|---------------|
| PAGE_HOME | 单次 → 保存手动记录 |
| PAGE_HISTORY_MANUAL | 保存当前记录到 Manu Save |
| PAGE_HISTORY_WARNING | 保存当前记录到 Alarm Save |
| PAGE_HISTORY_LT_MANUAL | 删除当前记录 |
| PAGE_HISTORY_LT_ALARM | 删除当前记录 |

**数据过滤**:
- `count_records_by_type(type)`: 从环形缓冲最新 256 条中统计指定类型的记录数
- `read_record_by_type(idx, type, &rec)`: 读取指定类型的第 idx 条 (0=最新)

### 5.5 StorageTask

**优先级 1**, 阻塞在 `xQueueReceive` 上。

- 上电初始化: 读取 MAGIC, 写指针, 写计数, 阈值, 声音模式
- SPI Flash 等待: `W25Q64_WaitReady()` 使用 `vTaskDelay(pdMS_TO_TICKS(1))` 替代 `mdelay(1)`，避免阻塞整个系统
- 循环等待 `xStorageQueue`:
  - 收到数据 → 打包为 `storage_record_t`（自动计算 CRC-8 写入 reserved[1]）→ 写入环形缓冲
  - 更新写指针到扇区 0
- 对外提供完整 API 访问扇区 0 的阈值、声音模式、LT_Manual / LT_Alarm

### 5.6 CommsTask — 通信输出

**优先级 1**, 阻塞在 `xQueueReceive(xUartTxQueue)` 上。

**传感器数据发送 (每 500ms)**:
- 从 `xUartTxQueue` 接收 `sensor_data_t`
- 格式化文本行通过 USART1(115200-8N1) 发送:
  ```
  [SENSOR] T:28 H:65 L:1234 D:50\r\n
  ```
- 使用 `snprintf` + `HAL_UART_Transmit` (100ms 超时)

**健康报告发送 (每 ~30s)**:
- 累积计数器，每 60 次传感器发送后插入一行健康报告:
  ```
  [HEALTH] SensorTask=128 DisplayTask=96 KeyTask=64 AlarmTask=80 StorageTask=112 CommsTask=48 HeapFree=4520\r\n
  ```
- 任务栈余量通过 FreeRTOS API `uxTaskGetStackHighWaterMark(handle)` 获取（高水位标记 = 任务启动以来剩余栈的最小值）
- 堆余量通过 `xPortGetFreeHeapSize()` 获取
- 任务句柄在 `freertos.c` 中定义 (`xSensorTaskHandle` 等)，在 `app.h` 中 `extern` 声明

---

## 6. RGB LED 状态指示

### 6.1 硬件连接

| 颜色 | TIM2 通道 | GPIO | 极性 |
|------|-----------|------|------|
| 红 (R) | CH3 | PA2 | TIM_OCPOLARITY_HIGH + 共阳极(低电平亮) |
| 绿 (G) | CH1 | PA15 | TIM_OCPOLARITY_HIGH + 共阳极(低电平亮) |
| 蓝 (B) | CH2 | PB3 | TIM_OCPOLARITY_HIGH + 共阳极(低电平亮) |

### 6.2 驱动接口

- `ColorLED_Init()` — 调用 `MX_TIM2_Init()` 初始化 TIM2 (50Hz PWM)
- `ColorLED_Start()` — 依次启动 CH1/CH2/CH3 的 PWM 输出
- `ColorLED_SetFast(color)` — 直接写 `TIM2->CCR` 寄存器，快速调色

### 6.3 颜色映射

```c
/* POLARITY_HIGH + 共阳极 LED(低电平亮):
 *   CNT < CCR → 输出 HIGH → LED 灭
 *   CNT > CCR → 输出 LOW  → LED 亮
 * 所以 CCR 越大 LED 越暗, 需反相: CCR = (255 - val) * 2000 / 255 */
TIM2->CCR3 = (255 - r) * 2000 / 255;   /* Red   */
TIM2->CCR1 = (255 - g) * 2000 / 255;   /* Green */
TIM2->CCR2 = (255 - b) * 2000 / 255;   /* Blue  */
```

### 6.4 状态颜色定义

| 状态 | 颜色 | RGB | 触发位置 |
|------|------|-----|---------|
| 系统正常 | 绿 | 0x00FF00 | SensorTask (无报警时) |
| 报警中 | 红 | 0xFF0000 | AlarmTask |
| 停止阶段 | 黄 | 0xFFFF00 | AlarmTask (条件清除后 3s) |
| 上电自检 | 白 | 0xFFFFFF | AlarmTask (启动时 200ms) |
| 无报警熄灭 | 灭 | 0x000000 | AlarmTask (蜂鸣器停时) |

## 7. 关键算法

### 7.1 环形缓冲地址计算

```
写入地址: g_writeOffset (从扇区 1 起始, 每次 +16)
回绕条件: g_writeOffset >= STORAGE_DATA_START_ADDR + TOTAL_DATA_SIZE
回绕后:   g_writeOffset = STORAGE_DATA_START_ADDR

读取最新第 idx 条:
  addr = g_writeOffset - (idx+1) × 16          // 未回绕
       = g_writeOffset + TOTAL_DATA_SIZE - (idx+1) × 16  // 已回绕
```

### 7.2 阈值持久化

每次修改阈值时, `storage_save_thresholds()`:
1. 读取 Flash 中现有阈值, 无变化则跳过
2. 备份扇区 0 扩展数据 (两个 LT bank)
3. 擦除扇区 0
4. 重写: MAGIC, 写指针, 写计数, 6 个阈值, 声音模式
5. 恢复 LT bank 数据

此流程保证阈值和声音模式修改不会丢失 LT 记录。

---

## 8. 红外遥控键码映射

| 遥控器标签 | 键码 | 命令 | 功能 |
|-----------|------|------|------|
| ↑         | 0x02 | KEY_UP | 上翻/增加 |
| ↓         | 0x98 | KEY_DOWN | 下翻/减少 |
| ←         | 0xE0 | KEY_LEFT | 左移字段 |
| →         | 0x90 | KEY_RIGHT | 右移字段 |
| ↺         | 0xC2 | KEY_BACK | 返回 |
| PLAY      | 0xA8 | KEY_CONFIRM | 确认/保存 |
| MENU      | 0xE2 | KEY_MENU | 菜单 |
| POWER     | 0xA2 | KEY_HOME | 首页 |

---

## 9. 使用指南

### 9.1 首页

```
==ENV MONITOR==     ← 标题行
T:28.0C H:65.0%    ← 温湿度
L:1234 D:50cm      ← 光照/距离
OK:Save CH-:Back   ← 提示行
```

- PLAY → 保存一条手动记录到历史
- MENU → 进入菜单

### 9.2 阈值设置

```
==THRESHOLD==
>Temp Hi:35         ← >表示当前选中字段
 Dist Lo:10
PLAY:Sv <>:Fld     ← PLAY保存, <>切换字段
```

- LEFT/RIGHT: 切换字段
- UP/DOWN: 增减值
- PLAY: 保存所有阈值到 Flash
- BACK: 返回菜单

### 9.3 声音模式设置

```
==ALARM SOUND==    ← 6 种报警类型
>Temp Hi(1)        ← (1)=当前模式号
 Dist(2)
PLAY:Enter CH-:Back
```

```
选中后:
==Temp Hi==
Mode: [1]           ← UP/DOWN 切换 1~5
UP/DN CH-:Back
```

### 9.4 历史记录查看

```
==RECORDS==
>Manual            ← 手动保存的记录
 Warning           ← 警报>10秒自动保存
 Long-term         ← 长期保存入口
PLAY:Enter CH-:Back
```

进入 Manual/Warning 页:
```
3/15               ← 第 3 条 / 共 15 条
T:28.0C H:65.0%   ← 记录详情
L:1234 D:50cm
UP/DN PLAY CH-:Bk  ← PLAY 两次保存到 Long-term
```

Long-term → 子菜单:
```
==LONG-TERM==
>Manu Save         ← 长期手动
 Alarm Save         ← 长期警报
PLAY:Enter CH-:Back
```

LT 详情页双击 PLAY = 删除当前记录。

### 9.5 恢复出厂设置

```
==RESET==
Press PLAY to      ← 第一次 PLAY
reset all defaults

Press PLAY again   ← 确认
to confirm reset
```

恢复项目:
- 所有阈值恢复默认值
- 所有声音模式恢复默认
- 清空 LT_Manual 和 LT_Alarm 记录
- **不会**清空环形缓冲中的自动/手动/警告记录

---

## 10. 内存与资源使用

| 资源 | 用量 | 说明 |
|------|------|------|
| Flash (Code) | ~25-35KB | 应用 + HAL + FreeRTOS |
| SRAM (Heap) | 8KB | FreeRTOS 动态分配 |
| SRAM (BSS+Data) | ~2KB | 全局变量, 队列, 事件组 |
| 栈总需求 | 1024+512+1024+512+1024+512 字 | 6 个任务 |

### 9.1 关键数据结构内存

| 对象 | 大小 | 说明 |
|------|------|------|
| xSensorQueue | 3 × 20B = 60B | 传感器数据队列 |
| xStorageQueue | 5 × 20B = 80B | 存储写入队列 |
| xKeyQueue | 5 × 4B = 20B | 按键队列 |
| xAlarmEventGroup | ~12B | 事件组 |
| xSpiMutex | ~12B | 互斥锁 |
| 全局变量 | ~40B | 阈值、模式等 |

---

## 11. 已知限制与潜在风险

| # | 问题 | 严重度 | 状态 | 说明 |
|---|------|--------|------|------|
| 1 | W25Q64_WaitReady() 超时始终返回成功 | **严重** | **已修复** | `!timeout` 检查在循环后永远为假，改为 `timeout < 0` |
| 2 | W25Q64_Erase() 对齐检查使用 `&&` 而非 `\|\|` | **严重** | **已修复** | 当仅偏移或仅长度未对齐时检查通过，改为 `\|\|` |
| 3 | W25Q64_Erase() 忽略 Tx 返回值 | **严重** | **已修复** | 擦除命令的 SPI 发送返回值被丢弃，改为检查并返回 |
| 4 | SPI Flash 缺少互斥锁保护 | **中等** | **已修复** | 添加 xSpiMutex 互斥锁，所有 W25Q64 操作均受保护 |
| 5 | 红外接收器缓冲区数组越界 | **严重** | **已修复** | IRQ 回调在检查计数前写入数组，第 68 次越界；已前置越界检查 |
| 6 | 红外 NEC 前导码上限过大 | **中等** | **已修复** | 55ms 上限改为 5.5ms (55000000→5500000)，降低噪声误触发 |
| 7 | DHT11 首次读取变量未初始化 | **中等** | **已修复** | 栈变量添加 `= {0}` 零初始化，失败时保持上次值 |
| 8 | PAGE_ABOUT 滚动条件错误 | **低** | **已修复** | `<=` 导致最后一屏仅显示 2 行，改为 `<` 标准比较 |
| 9 | s_lastData.record_type 被直接修改 | **低** | **已修复** | 首页 PLAY 改为操作局部 `save_data` 副本，不再污染缓存 |
| 10 | 启动瞬态全零 Warning 记录 | **低** | **已修复** | SensorTask 零初始化 + event group 发送前数据有效性过滤 |
| — | PB9 引脚冲突 (SR04_TRIG / W25Q64_CS) | **硬件限制** | **无法修改** | 两功能共用 PB9，不能同时使用超声波测距和 SPI Flash 操作 |

---

## 12. 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0 | 初始 | 基础框架: 5 任务, 传感器采集, OLED 显示, 红外遥控 |
| 2.0 | — | 完善架构, 阈值持久化, 声音模式控制 |
| 3.0 | — | 历史记录: 环形缓冲, 长期存储, 字节标记位修复 |
| 4.0 | — | Warning 改用环形缓冲, Long-term 拆分为 Manu Save + Alarm Save |
| 5.0 | 2026-05 | 修复 9 项 Bug (SPI Flash 超时/对齐/返回值, IR 越界/前导码, DHT11 初值, 显示滚动/缓存); 按键响应优化 (队列顺序、UI 延时、轮询周期); 按键音效 |
| 6.0 | 2026-05 | **架构重构**: display_task 拆分为 4 个 page 模块 (page_home/menu/history/about_help) + page_ctx.h，主循环从 1093 行减至 ~610 行; **数据完整性**: 新增 CRC-8-ATM 校验; **非阻塞**: mdelay(1) → vTaskDelay(1); 注释规范化 (6 个 driver 文件); 消除 10 个编译警告; 工程文件同步添加 page 模块 |
| 7.0 | 2026-05 | **UART 通信**: 新增 CommsTask + xUartTxQueue, USART1 文本帧输出; **RGB LED**: 新增 ColorLED_Start/SetFast 驱动, 状态指示 (绿/红/黄/白); **上位机**: host_tool/ Python 工具 (pyserial+tkinter+matplotlib), 实时曲线/CSV记录/健康监控; **健康监控**: 任务栈高水位 + 堆余量, 每 30s [HEALTH] 报告; 任务句柄变量 (xSensorTaskHandle等); Y 轴固定量程, CJK 字体, 按需重绘优化 |
