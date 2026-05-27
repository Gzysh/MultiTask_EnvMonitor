"""上位机配置常量"""

DEFAULT_PORT = ""
DEFAULT_BAUD = 115200
BAUD_RATES = [9600, 19200, 38400, 57600, 115200, 230400]
PLOT_WINDOW = 60          # 曲线显示最近 N 秒
POLL_MS = 100             # GUI 轮询队列间隔 (ms)
SENSOR_TIMEOUT = 2.0      # 传感器数据超时 (秒)，超时后显示灰色

# 任务名 → 总栈大小 (words)
TASK_STACK_SIZES = {
    "SensorTask": 256,
    "DisplayTask": 256,
    "StorageTask": 256,
    "KeyTask": 128,
    "AlarmTask": 128,
    "CommsTask": 128,
}

# 传感器数值标签
SENSOR_LABELS = {
    "temperature": ("温度", "°C"),
    "humidity": ("湿度", "%"),
    "light": ("光照", "ADC"),
    "distance": ("距离", "cm"),
}
