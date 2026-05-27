# MultiTask_EnvMonitor — 多任务智能环境监测仪

基于 **STM32F103C8T6** + **FreeRTOS v10.0.1** 的多任务传感器数据采集与环境监测系统。

## 功能特性

- **4 类传感器采集**：DHT11 温湿度、光敏电阻 (ADC)、HC-SR04 超声波测距
- **OLED 实时显示**：SSD1306 128×64，4 页面模块化 UI
- **红外遥控交互**：NEC 协议，8 键遥控器，支持菜单导航和参数设置
- **阈值报警**：6 种报警类型（温/湿/光/距），5 种声音模式，RGB LED 状态指示
- **数据存储**：W25Q64 8MB NOR Flash，环形缓冲 + 长期存储，CRC-8 校验
- **历史记录浏览**：支持手动/自动/报警/长期记录分类查看
- **UART 通信输出**：USART1 传感器数据文本帧 + 健康报告，配套 Python 上位机

## 硬件平台

| 组件 | 型号 | 接口 |
|------|------|------|
| MCU | STM32F103C8T6 (Cortex-M3 @ 72MHz) | — |
| 温湿度 | DHT11 | 单总线 GPIO |
| 光照 | 光敏电阻 + ADC | ADC1 12-bit |
| 超声波 | HC-SR04 | GPIO 触发/回波 |
| 存储 | W25Q64 (8MB) | SPI1 |
| 显示 | SSD1306 128×64 | I2C1 |
| 遥控 | 红外 NEC 协议 | GPIO 中断 |
| 声光 | 有源蜂鸣器 + RGB LED | GPIO / TIM2 PWM |

## 软件架构

### 任务划分

| 任务 | 优先级 | 周期 | 功能 |
|------|--------|------|------|
| SensorTask | 2 | 500ms | 传感器轮询、阈值检测 |
| KeyTask | 2 | 30ms | 红外遥控解码 |
| DisplayTask | 1 | ~50ms | OLED 页面渲染 |
| AlarmTask | 3 | 阻塞 | 蜂鸣器 + RGB LED 报警 |
| StorageTask | 1 | 阻塞 | SPI Flash 读写 |
| CommsTask | 1 | 500ms 触发 | USART1 数据输出 |

### 目录结构

```
MultiTask_EnvMonitor/
├── App/                    # 应用层任务代码
│   ├── sensor_task.c       # 传感器采集任务
│   ├── display_task.c      # 显示交互任务
│   ├── key_task.c          # 红外遥控任务
│   ├── alarm_task.c        # 声光报警任务
│   ├── storage_task.c/h    # 数据存储任务
│   ├── comms_task.c/h      # 通信输出任务
│   ├── page_home.c/h       # 首页模块
│   ├── page_menu.c/h       # 菜单模块
│   ├── page_history.c/h    # 历史记录模块
│   ├── page_about_help.c/h # 关于/帮助模块
│   ├── page_ctx.h          # 页面上下文
│   └── app.h               # 应用层头文件
├── Core/                   # STM32 HAL 配置
│   ├── Inc/                # 头文件
│   └── Src/                # 源文件 (main, freertos, HAL)
├── Drivers/                # 驱动层
│   ├── DshanMCU-F103/      # 外设驱动 (OLED, DHT11, W25Q64, IR 等)
│   ├── STM32F1xx_HAL_Driver/
│   └── CMSIS/
├── MDK-ARM/                # Keil MDK 工程文件
├── Middlewares/             # FreeRTOS 中间件
├── host_tool/              # Python 上位机工具
├── TECHNICAL_DOCUMENT.md   # 详细技术文档
└── MultiTask_EnvMonitor.ioc # CubeMX 工程配置
```

## 构建

1. 使用 STM32CubeMX 打开 `MultiTask_EnvMonitor.ioc` 生成代码
2. 使用 Keil MDK-ARM 打开 `MDK-ARM/MultiTask_EnvMonitor.uvprojx`
3. 编译并下载到 STM32F103C8T6

## 上位机工具

`host_tool/` 目录包含 Python 上位机，基于 pyserial + tkinter + matplotlib：

```bash
cd host_tool
pip install -r requirements.txt
python main.py
```

功能：实时传感器曲线、CSV 数据记录、任务健康监控。

## 详细文档

参见 [TECHNICAL_DOCUMENT.md](TECHNICAL_DOCUMENT.md) 获取完整的技术文档，包括：
- 数据格式与存储布局
- 各任务详细设计
- 页面系统与交互逻辑
- 红外键码映射
- 已知限制与版本历史

## License

MIT License — 详见 [LICENSE](LICENSE)
