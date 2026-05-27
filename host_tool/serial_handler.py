"""串口管理 + 后台读取线程 + 文本行解析"""

import re
import threading
import queue
from collections import namedtuple
from datetime import datetime

import serial
import serial.tools.list_ports

from config import SENSOR_TIMEOUT

# 传感器数据结构
SensorData = namedtuple(
    "SensorData", ["timestamp", "temperature", "humidity", "light", "distance"]
)

# 健康数据结构
HealthData = namedtuple("HealthData", ["timestamp", "raw_kv"])

# 串口行正则
_RE_SENSOR = re.compile(
    r"\[SENSOR\]\s+T:(-?\d+)\s+H:(-?\d+)\s+L:(\d+)\s+D:(\d+)"
)
_RE_HEALTH = re.compile(r"\[HEALTH\]\s+(.+)")

# 健康数据中的键值对
_RE_KV = re.compile(r"(\w+)=(\d+)")


def list_ports():
    """返回可用串口列表"""
    return [p.device for p in serial.tools.list_ports.comports()]


class SerialHandler(threading.Thread):
    """后台串口读取线程, 解析文本行并推入队列"""

    def __init__(self, data_queue: queue.Queue, port="", baud=115200):
        super().__init__(daemon=True)
        self._queue = data_queue
        self._port = port
        self._baud = baud
        self._ser = None
        self._running = False
        self._connected = False

    @property
    def connected(self):
        return self._connected

    def open(self, port, baud):
        self._port = port
        self._baud = baud

    def start_reading(self):
        """打开串口并启动线程"""
        if self._running:
            return
        self._ser = serial.Serial(
            port=self._port,
            baudrate=self._baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.5,
        )
        self._connected = True
        self._running = True
        super().start()

    def stop_reading(self):
        """停止线程并关闭串口"""
        self._running = False
        if self._ser:
            try:
                self._ser.close()
            except Exception:
                pass
            self._ser = None
        self._connected = False

    def run(self):
        """后台循环: readline → parse → queue"""
        ser = self._ser
        buf = b""

        while self._running and ser and ser.is_open:
            try:
                data = ser.read(256)
                if not data:
                    continue
                buf += data

                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    line = line.strip(b"\r").decode("utf-8", errors="replace")
                    self._parse_line(line)
            except serial.SerialException:
                self._connected = False
                break
            except Exception:
                break

        self._connected = False

    def _parse_line(self, line: str):
        """解析单行文本, 推入 data_queue"""
        now = datetime.now()

        m = _RE_SENSOR.match(line)
        if m:
            data = SensorData(
                timestamp=now,
                temperature=int(m.group(1)),
                humidity=int(m.group(2)),
                light=int(m.group(3)),
                distance=int(m.group(4)),
            )
            self._queue.put(("sensor", data))
            return

        m = _RE_HEALTH.match(line)
        if m:
            data = HealthData(timestamp=now, raw_kv=m.group(1))
            self._queue.put(("health", data))
            return


def parse_health_kv(raw: str):
    """将 'SensorTask=128 DisplayTask=96 HeapFree=4520' 解析为 dict"""
    return {k: int(v) for k, v in _RE_KV.findall(raw)}
