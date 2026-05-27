"""Matplotlib 实时曲线面板 (4 子图)"""

import tkinter as tk

import matplotlib
matplotlib.use("TkAgg")

# 配置中文字体, 解决 Y 轴标签显示为方框的问题
import matplotlib.pyplot as plt
plt.rcParams['font.sans-serif'] = ['Microsoft YaHei', 'SimHei']
plt.rcParams['axes.unicode_minus'] = False

from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure
from matplotlib import dates as mdates

from config import PLOT_WINDOW

# 颜色方案
COLORS = {
    "temperature": "#E74C3C",
    "humidity": "#3498DB",
    "light": "#E67E22",
    "distance": "#2ECC71",
}

YLABELS = {
    "temperature": "温度 (°C)",
    "humidity": "湿度 (%)",
    "light": "光照 (ADC)",
    "distance": "距离 (cm)",
}

SENSOR_KEYS = ["temperature", "humidity", "light", "distance"]

# 每种子图的固定 Y 轴范围 (物理量程)
Y_LIMITS = {
    "temperature": (-10, 60),
    "humidity": (0, 100),
    "light": (0, 4095),
    "distance": (0, 100),
}


class PlotPanel(tk.Frame):
    """4 子图实时曲线面板"""

    def __init__(self, parent, **kwargs):
        super().__init__(parent, **kwargs)

        # 数据存储: { key: [(datetime, value), ...] }
        self._data = {k: [] for k in SENSOR_KEYS}
        self._dirty = False

        self._fig = Figure(figsize=(8, 6), dpi=80, tight_layout=True)
        self._axes = []
        self._lines = []

        for i, key in enumerate(SENSOR_KEYS):
            ax = self._fig.add_subplot(4, 1, i + 1, sharex=self._axes[0] if i > 0 else None)
            ax.set_ylabel(YLABELS[key], fontsize=8)
            ax.tick_params(axis="both", labelsize=7)
            ax.grid(True, alpha=0.3)
            (line,) = ax.plot([], [], color=COLORS[key], linewidth=1.2)
            self._axes.append(ax)
            self._lines.append(line)

        self._axes[0].xaxis.set_major_formatter(mdates.DateFormatter("%H:%M:%S"))
        self._axes[-1].set_xlabel("时间", fontsize=8)

        self._canvas = FigureCanvasTkAgg(self._fig, master=self)
        self._canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

    def append_data(self, data):
        """追加一条 SensorData 到缓冲区"""
        ts = data.timestamp
        self._data["temperature"].append((ts, data.temperature))
        self._data["humidity"].append((ts, data.humidity))
        self._data["light"].append((ts, data.light))
        self._data["distance"].append((ts, data.distance))
        self._dirty = True

    def redraw(self):
        """裁剪过期数据并重绘所有子图 (跳过非 dirty 调用)"""
        if not self._dirty:
            return
        self._dirty = False

        import datetime as dt

        cutoff = dt.datetime.now() - dt.timedelta(seconds=PLOT_WINDOW + 2)

        for key, ax, line in zip(SENSOR_KEYS, self._axes, self._lines):
            pts = self._data[key]
            # 裁剪过期点
            pts[:] = [(t, v) for t, v in pts if t > cutoff]

            if not pts:
                line.set_data([], [])
                continue

            times, values = zip(*pts)
            line.set_data(times, values)
            # 固定 Y 轴范围 (物理量程), 数据不会跑出视野
            ax.set_ylim(Y_LIMITS[key])

        # 共享 X 轴范围
        if self._data[SENSOR_KEYS[0]]:
            times = [t for t, _ in self._data[SENSOR_KEYS[0]]]
            if times:
                xmin = times[0]
                xmax = times[-1] + dt.timedelta(seconds=1)
                for ax in self._axes:
                    ax.set_xlim(xmin, xmax)

        self._canvas.draw_idle()
