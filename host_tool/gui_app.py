"""tkinter 主窗口"""

import queue
import tkinter as tk
from tkinter import ttk
from tkinter import font as tkfont

from config import (
    DEFAULT_PORT,
    DEFAULT_BAUD,
    BAUD_RATES,
    POLL_MS,
    SENSOR_TIMEOUT,
    TASK_STACK_SIZES,
    SENSOR_LABELS,
)
from serial_handler import SerialHandler, list_ports, parse_health_kv
from data_logger import DataLogger
from plot_panel import PlotPanel


class App(tk.Tk):
    """上位机主窗口"""

    def __init__(self, port="", baud=DEFAULT_BAUD):
        super().__init__()
        # 设置 CJK 字体 (解决 Windows 下中文显示为方框的问题)
        try:
            default_font = tkfont.nametofont("TkDefaultFont")
            default_font.configure(family="Microsoft YaHei")
        except Exception:
            pass

        self.title("环境监测仪上位机 v1.0")
        self.geometry("900x780")
        self.minsize(700, 650)

        self._data_queue = queue.Queue()
        self._handler = SerialHandler(self._data_queue)
        self._logger = DataLogger()
        self._logging = False
        self._new_data_arrived = False  # 标记是否有新数据需要重绘

        # 最新传感器/健康数据
        self._last_sensor = None
        self._last_health = {}

        # 健康面板控件引用
        self._health_bars = {}
        self._health_labels = {}

        self._build_ui()
        self._poll_queue()

        if port:
            self._try_connect(port, baud)

    # ── UI 构建 ──────────────────────────────────────────────

    def _build_ui(self):
        self._build_toolbar()
        self._build_sensor_panel()
        self._build_health_panel()
        self._build_plot()
        self._build_statusbar()

    def _build_toolbar(self):
        frame = ttk.Frame(self, padding=4)
        frame.pack(fill=tk.X)

        ttk.Label(frame, text="串口:").pack(side=tk.LEFT)
        self._port_var = tk.StringVar(value=DEFAULT_PORT)
        self._port_combo = ttk.Combobox(
            frame, textvariable=self._port_var, width=12, state="readonly"
        )
        self._port_combo["values"] = self._scan_ports()
        self._port_combo.pack(side=tk.LEFT, padx=2)
        ttk.Button(frame, text="刷新", command=self._refresh_ports, width=5).pack(
            side=tk.LEFT, padx=2
        )

        ttk.Label(frame, text="波特率:").pack(side=tk.LEFT, padx=(8, 0))
        self._baud_var = tk.StringVar(value=str(DEFAULT_BAUD))
        baud_combo = ttk.Combobox(
            frame, textvariable=self._baud_var, values=BAUD_RATES, width=8, state="readonly"
        )
        baud_combo.pack(side=tk.LEFT, padx=2)

        self._connect_btn = ttk.Button(
            frame, text="连接", command=self._toggle_connect, width=8
        )
        self._connect_btn.pack(side=tk.LEFT, padx=(12, 2))

        self._log_btn = ttk.Button(
            frame, text="开始记录", command=self._toggle_log, width=10
        )
        self._log_btn.pack(side=tk.LEFT, padx=(4, 2))
        self._log_btn.state(["disabled"])

        ttk.Separator(self, orient=tk.HORIZONTAL).pack(fill=tk.X)

    def _build_sensor_panel(self):
        frame = ttk.LabelFrame(self, text="传感器数据", padding=6)
        frame.pack(fill=tk.X, padx=6, pady=(4, 0))

        self._sensor_frames = {}
        row_frame = ttk.Frame(frame)
        row_frame.pack(fill=tk.X)

        for i, (key, (label, unit)) in enumerate(SENSOR_LABELS.items()):
            f = ttk.LabelFrame(row_frame, text=label, width=180, height=70)
            f.pack(side=tk.LEFT, padx=4, expand=True, fill=tk.BOTH)
            f.pack_propagate(False)

            val = tk.StringVar(value="--")
            lbl = ttk.Label(f, textvariable=val, font=("Consolas", 24), anchor="center")
            lbl.pack(expand=True)

            unit_lbl = ttk.Label(f, text=unit, font=("", 9), anchor="e")
            unit_lbl.pack(anchor="e", padx=4)

            self._sensor_frames[key] = (val, lbl, f)

    def _build_health_panel(self):
        frame = ttk.LabelFrame(self, text="系统健康 (栈余量 / 堆余量)", padding=6)
        frame.pack(fill=tk.X, padx=6, pady=(4, 0))

        self._heap_var = tk.StringVar(value="堆余量: --")

        # 每个任务一行
        for task_name, total_stack in TASK_STACK_SIZES.items():
            row = ttk.Frame(frame)
            row.pack(fill=tk.X, pady=1)

            ttk.Label(row, text=task_name, width=14, anchor="w").pack(side=tk.LEFT)

            bar = ttk.Progressbar(row, length=200, value=0)
            bar.pack(side=tk.LEFT, padx=4)

            text = tk.StringVar(value=f"-- / {total_stack * 4} B")
            lbl = ttk.Label(row, textvariable=text, font=("Consolas", 9), width=32, anchor="w")
            lbl.pack(side=tk.LEFT)

            self._health_bars[task_name] = bar
            self._health_labels[task_name] = text

        # 堆余量
        heap_row = ttk.Frame(frame)
        heap_row.pack(fill=tk.X, pady=(4, 0))
        ttk.Label(heap_row, text="FreeRTOS 堆", width=14, anchor="w").pack(side=tk.LEFT)
        self._heap_lbl = ttk.Label(
            heap_row, textvariable=self._heap_var, font=("Consolas", 9), anchor="w"
        )
        self._heap_lbl.pack(side=tk.LEFT)

    def _build_plot(self):
        self._plot_panel = PlotPanel(self)
        self._plot_panel.pack(fill=tk.BOTH, expand=True, padx=6, pady=(4, 0))

    def _build_statusbar(self):
        self._status_var = tk.StringVar(value="未连接")
        bar = ttk.Label(self, textvariable=self._status_var, relief=tk.SUNKEN, anchor="w")
        bar.pack(fill=tk.X, side=tk.BOTTOM)

    # ── 串口操作 ─────────────────────────────────────────────

    def _scan_ports(self):
        try:
            return list_ports()
        except Exception:
            return []

    def _refresh_ports(self):
        ports = self._scan_ports()
        self._port_combo["values"] = ports
        if ports and not self._handler.connected:
            self._port_combo.current(0)

    def _toggle_connect(self):
        if self._handler.connected:
            self._disconnect()
        else:
            port = self._port_var.get()
            if not port:
                self._set_status("请选择串口")
                return
            self._try_connect(port, int(self._baud_var.get()))

    def _try_connect(self, port, baud):
        try:
            self._handler.open(port, baud)
            self._handler.start_reading()
            self._set_status(f"已连接 {port} @ {baud}")
            self._connect_btn.config(text="断开")
            self._port_combo.state(["disabled"])
            self._log_btn.state(["!disabled"])
        except Exception as e:
            self._set_status(f"连接失败: {e}")

    def _disconnect(self):
        self._handler.stop_reading()
        if self._logging:
            self._toggle_log()
        self._set_status("已断开")
        self._connect_btn.config(text="连接")
        self._port_combo.state(["!disabled"])
        self._log_btn.state(["disabled"])
        self._clear_sensor_display()

    def _clear_sensor_display(self):
        for key in SENSOR_LABELS:
            self._sensor_frames[key][0].set("--")

    # ── CSV 记录 ─────────────────────────────────────────────

    def _toggle_log(self):
        if self._logging:
            self._logger.stop()
            self._logging = False
            self._log_btn.config(text="开始记录")
            self._set_status(f"记录已保存: {self._logger.path}")
        else:
            path = self._logger.start()
            self._logging = True
            self._log_btn.config(text="停止记录")
            self._set_status(f"正在记录 → {path}")

    # ── 队列轮询 ─────────────────────────────────────────────

    def _poll_queue(self):
        """每 POLL_MS 从 data_queue 取数据并更新 UI"""
        try:
            while True:
                kind, data = self._data_queue.get_nowait()
                if kind == "sensor":
                    self._last_sensor = data
                    self._update_sensor_display(data)
                    self._plot_panel.append_data(data)
                    if self._logging and data.temperature is not None:
                        self._logger.write(data)
                elif kind == "health":
                    self._last_health = parse_health_kv(data.raw_kv)
                    self._update_health_display()

                self._new_data_arrived = True

                # 传感器超时检查: 如果超过 SENSOR_TIMEOUT 没收到新数据, 显示灰色
                import datetime as dt
                if self._last_sensor:
                    age = (dt.datetime.now() - self._last_sensor.timestamp).total_seconds()
                    for key in SENSOR_LABELS:
                        fg = "gray" if age > SENSOR_TIMEOUT else "black"
                        self._sensor_frames[key][1].config(foreground=fg)

        except queue.Empty:
            pass

        # 仅在有新数据时重绘图表, 减少无用 CPU 开销
        if self._new_data_arrived:
            self._new_data_arrived = False
            self._plot_panel.redraw()

        self.after(POLL_MS, self._poll_queue)

    def _update_sensor_display(self, data):
        vals = {
            "temperature": data.temperature,
            "humidity": data.humidity,
            "light": data.light,
            "distance": data.distance,
        }
        for key, val in vals.items():
            if val is not None:
                self._sensor_frames[key][0].set(str(val))

    def _update_health_display(self):
        kv = self._last_health
        for task_name in TASK_STACK_SIZES:
            total = TASK_STACK_SIZES[task_name] * 4  # bytes
            hwm = kv.get(task_name)
            if hwm is not None:
                used = total - hwm
                pct = max(0, min(100, used * 100 / total))
                self._health_bars[task_name]["value"] = pct
                self._health_labels[task_name].set(
                    f"峰值 {used} / {total} B  (余 {hwm} B)"
                )
            else:
                self._health_bars[task_name]["value"] = 0
                self._health_labels[task_name].set(f"-- / {total} B")

        heap = kv.get("HeapFree")
        if heap is not None:
            self._heap_var.set(f"堆余量: {heap} B")

    def _set_status(self, msg):
        self._status_var.set(msg)
