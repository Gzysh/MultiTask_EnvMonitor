"""CSV 数据记录器"""

import csv
import os
from datetime import datetime


class DataLogger:
    """将传感器数据写入 CSV 文件"""

    def __init__(self, directory="logs"):
        self._file = None
        self._writer = None
        self._directory = directory
        self._path = None

    @property
    def path(self):
        return self._path

    def start(self):
        """创建新 CSV 文件并写入表头"""
        os.makedirs(self._directory, exist_ok=True)
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        self._path = os.path.join(self._directory, f"sensor_{ts}.csv")
        self._file = open(self._path, "w", newline="", encoding="utf-8")
        self._writer = csv.writer(self._file)
        self._writer.writerow(
            ["timestamp", "temperature", "humidity", "light", "distance"]
        )
        self._file.flush()
        return self._path

    def write(self, data):
        """写入一行传感器数据"""
        if self._writer is None:
            return
        self._writer.writerow(
            [
                data.timestamp.isoformat(),
                data.temperature,
                data.humidity,
                data.light,
                data.distance,
            ]
        )
        self._file.flush()

    def stop(self):
        """关闭文件"""
        if self._file:
            self._file.close()
            self._file = None
            self._writer = None
