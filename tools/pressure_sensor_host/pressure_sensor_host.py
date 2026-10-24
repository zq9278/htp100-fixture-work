"""Windows pressure sensor bench tool. Run with Python or the packaged EXE."""
from collections import deque
from dataclasses import asdict
from datetime import datetime
from pathlib import Path
import csv
import json
import math
import secrets
import struct
import sys
import time

import serial
from serial.tools import list_ports
from PyQt5 import QtCore, QtGui, QtWidgets as W

import protocol as P
from calibration import CalibrationPoint, fit_points, mass_to_mmhg, mmhg_to_force_mass


def number(value, digits=1):
    return f"{value:.{digits}f}" if math.isfinite(value) else "—"


class PressureChart(W.QWidget):
    def __init__(self):
        super().__init__()
        self.points = deque(maxlen=2400)
        self.last_ms = None
        self.setMinimumHeight(170)

    def add(self, telemetry):
        valid = telemetry.flags & P.VALID
        elapsed = self.points[-1][0]+((telemetry.ms-self.last_ms) & 0xFFFFFFFF)/1000 if self.points else 0
        self.last_ms = telemetry.ms
        self.points.append((elapsed, telemetry.pressure if valid else math.nan,
                            telemetry.filtered if valid else math.nan))
        self.update()

    def paintEvent(self, event):
        painter = QtGui.QPainter(self)
        painter.setRenderHint(QtGui.QPainter.Antialiasing)
        painter.fillRect(self.rect(), QtGui.QColor("#ffffff"))
        box = QtCore.QRectF(65, 24, max(10, self.width()-85), max(10, self.height()-52))
        end = self.points[-1][0] if self.points else time.monotonic()
        start = end - 60
        points = [p for p in self.points if p[0] >= start]
        values = [v for p in points for v in p[1:] if math.isfinite(v)]
        low, high = (min(values), max(values)) if values else (0, 500)
        margin = max(5, (high-low)*0.15)
        low, high = min(0, low-margin), max(10, high+margin)
        painter.setFont(QtGui.QFont("Microsoft YaHei UI", 9))
        for i in range(5):
            y = box.top()+box.height()*i/4
            painter.setPen(QtGui.QColor("#e4eaf1"))
            painter.drawLine(QtCore.QPointF(box.left(), y), QtCore.QPointF(box.right(), y))
            painter.setPen(QtGui.QColor("#64748b"))
            painter.drawText(QtCore.QRectF(0, y-9, 56, 18), QtCore.Qt.AlignRight, f"{high-(high-low)*i/4:.0f}")
        for seconds in (60, 45, 30, 15, 0):
            x = box.right()-box.width()*seconds/60
            painter.drawText(QtCore.QRectF(x-25, box.bottom()+8, 50, 20), QtCore.Qt.AlignCenter, f"-{seconds}s" if seconds else "现在")
        painter.drawText(9, 16, "mmHg")
        painter.drawText(int(box.left()), 16, "最近 60 秒   浅蓝：瞬时压力   深蓝：显示滤波")
        painter.setClipRect(box)
        for index, color, width in ((1, "#a4c9f4", 1), (2, "#1764bc", 2)):
            path = QtGui.QPainterPath()
            pen_down = False
            for t, instantaneous, smooth in points:
                value = instantaneous if index == 1 else smooth
                if not math.isfinite(value):
                    pen_down = False
                    continue
                x = box.left()+(t-start)/60*box.width()
                y = box.bottom()-(value-low)/(high-low)*box.height()
                if pen_down:
                    path.lineTo(x, y)
                else:
                    path.moveTo(x, y)
                    pen_down = True
            painter.setPen(QtGui.QPen(QtGui.QColor(color), width))
            painter.drawPath(path)


class Window(W.QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("HTP100 PC压力调试与砝码校准 · G0 USART1")
        self.resize(1240, 930)
        self.serial = None
        self.decoder = P.Decoder()
        self.sequence = secrets.randbelow(60000)+1
        self.pending = None
        self.identified = False
        self.connected_at = self.last_rx = 0
        self.device_config = None
        self.calibration_points = []
        self.fit = None
        self.telemetry = None
        self.last_device_ms = None
        self.log_file = self.log_writer = None
        self.log_rows = 0
        self.history = []
        self.seen_finished = False
        self.last_status_sequence = None
        self.lost_frames = 0
        self.build_ui()
        self.refresh_ports()
        self.timer = QtCore.QTimer(self)
        self.timer.timeout.connect(self.poll)
        self.timer.start(20)
        self.sync_buttons()

    def build_ui(self):
        self.setStyleSheet("""
            QMainWindow { background:#f2f5f9; }
            QWidget { font-family:'Microsoft YaHei UI'; font-size:10pt; color:#172b45; }
            QGroupBox { background:white; border:1px solid #dce4ee; border-radius:7px;
                        margin-top:12px; padding-top:15px; font-weight:600; }
            QGroupBox::title { subcontrol-origin:margin; left:12px; padding:0 5px; }
            QPushButton { background:#e9f0f9; border:1px solid #cbd8e8; border-radius:4px; padding:6px 12px; }
            QPushButton:hover { background:#dae8fa; }
            QPushButton:disabled { color:#94a3b8; background:#f1f5f9; }
            QLineEdit,QDoubleSpinBox,QComboBox { padding:4px; border:1px solid #cbd5e1; background:white; }
            QTableWidget { background:white; gridline-color:#e2e8f0; }
            QProgressBar { border:1px solid #dce4ee; border-radius:3px; text-align:center; }
            QProgressBar::chunk { background:#9dc3f0; }
        """)
        central = W.QWidget()
        self.setCentralWidget(central)
        root = W.QVBoxLayout(central)
        title = W.QLabel("压力传感器检测")
        title.setStyleSheet("font-size:21pt;font-weight:700;")
        root.addWidget(title)
        subtitle = W.QLabel("PC辅助工具 · 接G0 USART1（PB6/PB7） · H7屏幕独立工作")
        subtitle.setStyleSheet("color:#64748b;")
        root.addWidget(subtitle)
        line = W.QHBoxLayout()
        line.addWidget(W.QLabel("串口"))
        self.ports = W.QComboBox()
        self.ports.setMinimumWidth(230)
        line.addWidget(self.ports)
        self.refresh = W.QPushButton("刷新")
        self.refresh.clicked.connect(self.refresh_ports)
        line.addWidget(self.refresh)
        self.connect_button = W.QPushButton("连接设备")
        self.connect_button.clicked.connect(self.toggle_connection)
        line.addWidget(self.connect_button)
        line.addWidget(W.QLabel("115200 · 8N1"))
        line.addStretch()
        self.connection = W.QLabel("未连接")
        line.addWidget(self.connection)
        root.addLayout(line)

        cards = W.QHBoxLayout()
        live = W.QGroupBox("实时压力")
        layout = W.QVBoxLayout(live)
        self.pressure_label = W.QLabel("— mmHg")
        self.pressure_label.setStyleSheet("font-size:32pt;font-weight:700;color:#175cab;")
        layout.addWidget(self.pressure_label)
        self.load_label = W.QLabel("力值：— N   等效克数：— g")
        self.load_label.setStyleSheet("font-size:14pt;color:#175cab;")
        layout.addWidget(self.load_label)
        self.raw_label = W.QLabel("瞬时：— mmHg   ADC：—")
        self.sensor_label = W.QLabel("请连接设备")
        self.power_label = W.QLabel("供电：—   电量：—   电池：—")
        layout.addWidget(self.raw_label)
        layout.addWidget(self.sensor_label)
        layout.addWidget(self.power_label)
        self.tare_button = W.QPushButton("空载去皮（采样 1 秒）")
        self.tare_button.clicked.connect(lambda: self.command(P.TARE))
        layout.addWidget(self.tare_button)
        cards.addWidget(live, 1)

        peak = W.QGroupBox("定时峰值捕捉")
        layout = W.QVBoxLayout(peak)
        self.peak_label = W.QLabel("— mmHg")
        self.peak_label.setStyleSheet("font-size:32pt;font-weight:700;color:#0f776a;")
        layout.addWidget(self.peak_label)
        self.peak_load_label = W.QLabel("峰值力：— N   峰值克数：— g")
        self.peak_load_label.setStyleSheet("font-size:14pt;color:#0f776a;")
        layout.addWidget(self.peak_load_label)
        self.peak_detail = W.QLabel("未开始 · 捕捉下位机采样中的最大值")
        layout.addWidget(self.peak_detail)
        self.progress = W.QProgressBar()
        self.progress.setRange(0, 1000)
        layout.addWidget(self.progress)
        controls = W.QHBoxLayout()
        controls.addWidget(W.QLabel("时长"))
        self.duration = W.QDoubleSpinBox()
        self.duration.setRange(0.1, 3600)
        self.duration.setDecimals(1)
        self.duration.setValue(10)
        self.duration.setSuffix(" 秒")
        controls.addWidget(self.duration)
        self.start_button = W.QPushButton("开始捕捉")
        self.start_button.clicked.connect(lambda: self.command(P.START, struct.pack("<I", round(self.duration.value()*1000))))
        controls.addWidget(self.start_button)
        self.stop_button = W.QPushButton("提前结束")
        self.stop_button.clicked.connect(lambda: self.command(P.STOP))
        controls.addWidget(self.stop_button)
        layout.addLayout(controls)
        cards.addWidget(peak, 1)
        root.addLayout(cards)
        self.chart = PressureChart()
        root.addWidget(self.chart, 1)

        self.tabs = W.QTabWidget()
        root.addWidget(self.tabs)
        calibration = W.QWidget()
        cl = W.QVBoxLayout(calibration)
        note = W.QLabel("此页为辅助自由采点。完整六点标定、三轮加载卸载复核请在 H7 屏幕的引导校准中完成。PC 采点建议覆盖 0 / 20 / 40 / 60 / 80 / 100% 使用量程，稳定后采样；请勿与 H7 同时修改参数。")
        note.setWordWrap(True)
        cl.addWidget(note)
        row = W.QHBoxLayout()
        row.addWidget(W.QLabel("有效受力面积"))
        self.area = W.QDoubleSpinBox()
        self.area.setDecimals(3)
        self.area.setRange(1, 100000)
        self.area.setValue(84.8)
        self.area.setSuffix(" mm²")
        self.area.valueChanged.connect(self.invalidate_fit)
        row.addWidget(self.area)
        row.addWidget(W.QLabel("本次新增砝码"))
        self.mass = W.QDoubleSpinBox()
        self.mass.setRange(0, 100000)
        self.mass.setDecimals(3)
        self.mass.setSuffix(" g")
        row.addWidget(self.mass)
        self.point_button = W.QPushButton("采集校准点")
        self.point_button.clicked.connect(self.capture_point)
        row.addWidget(self.point_button)
        self.remove_button = W.QPushButton("删除选中点")
        self.remove_button.clicked.connect(self.remove_points)
        row.addWidget(self.remove_button)
        cl.addLayout(row)
        self.points_table = self.table(["砝码 g", "等效压力 mmHg", "ADC 均值", "标准差 counts", "样本数", "拟合残差 mmHg"])
        self.points_table.verticalHeader().setDefaultSectionSize(24)
        self.points_table.setMinimumHeight(130)
        self.points_table.setMaximumHeight(150)
        cl.addWidget(self.points_table)
        row = W.QHBoxLayout()
        self.fit_button = W.QPushButton("计算校准")
        self.fit_button.clicked.connect(self.calculate_fit)
        row.addWidget(self.fit_button)
        self.apply_button = W.QPushButton("应用校准到设备 RAM")
        self.apply_button.clicked.connect(self.apply_fit)
        row.addWidget(self.apply_button)
        self.save_button = W.QPushButton("保存设备参数（断电保留）")
        self.save_button.clicked.connect(lambda: self.command(P.SAVE))
        row.addWidget(self.save_button)
        self.read_button = W.QPushButton("读取设备参数")
        self.read_button.clicked.connect(lambda: self.command(P.GET_CONFIG))
        row.addWidget(self.read_button)
        self.export_button = W.QPushButton("导出校准记录")
        self.export_button.clicked.connect(self.export_calibration)
        row.addWidget(self.export_button)
        cl.addLayout(row)
        self.fit_label = W.QLabel("尚未计算校准。两点可求零点和灵敏度，三个及以上点可检查线性残差。")
        self.fit_label.setWordWrap(True)
        cl.addWidget(self.fit_label)
        self.config_label = W.QLabel("设备参数：—")
        self.config_label.setWordWrap(True)
        cl.addWidget(self.config_label)
        self.tabs.addTab(calibration, "砝码校准")

        results = W.QWidget()
        rl = W.QVBoxLayout(results)
        self.history_table = self.table(["电脑时间", "结果", "时长 s", "峰值 mmHg", "最小 mmHg", "峰值时刻 s", "样本数", "异常读数", "峰值力 N", "峰值克数 g"])
        rl.addWidget(self.history_table)
        export = W.QPushButton("导出峰值结果 CSV")
        export.clicked.connect(self.export_history)
        rl.addWidget(export)
        self.tabs.addTab(results, "峰值记录")
        help_text = W.QTextBrowser()
        help_text.setHtml("""<b>接线和使用</b><p>USB 转 3.3V TTL：RX 接G0 PB6（USART1_TX），TX 接G0 PB7（USART1_RX），GND共地。
        H7屏幕是独立上位机，经G0 USART2与下位机通信；PC使用独立USART1，两个串口不能并联。</p>
        <b>砝码与面积</b><p>空载指保留加载盘／压头、无新增砝码，录入 0 g；其余输入新增砝码总质量。
        砝码应垂直、居中加载。面积填最终测量时的有效受力面积；84.8 mm² 沿用现有项目，需实测确认。
        压力 = 质量(g) × 9.80665 × 1000 ÷ 面积(mm²) ÷ 133.3224。
        传感器实际测量的是力，mmHg 是基于该面积的等效压力。</p>
        <b>校准流程</b><p>采集空载及砝码点 → 计算校准 → 应用到 RAM → 用独立砝码复核 → 保存设备参数。
        日常开机空载去皮即可。更换传感器、加载结构或受力面积后重新校准。
        去皮只改变零点；已校准灵敏度保持不变。去皮结果在点击保存前仅存于 RAM。</p>
        <b>峰值含义</b><p>由下位机在指定时间窗口内逐个有效读数比较，记录未滤波最大值及其时刻。
        大字实时读数经过显示滤波，瞬时读数和峰值不滤波。完成结果保留至下次捕捉或修改校准。
        异常读数／传感器断线会标记结果不完整。ADC 标称 330 SPS，实际采集率受通信和 I²C 访问影响，
        无法保证捕获两次采样之间的极短脉冲；这里的峰值是采样峰值。</p>
        <b>充电与记录</b><p>沿用原 BQ25895 充电配置，可边充电边测量；电量计故障时显示未知。
        曲线显示最近 60 秒；CSV 记录持续写入磁盘，包含瞬时值、滤波值、设备峰值、状态和校准参数。
        界面串口超时会清空实时读数；下位机已开始的捕捉会继续，重新连接可读取当前结果。</p>""")
        self.tabs.addTab(help_text, "使用与校准说明")
        footer = W.QHBoxLayout()
        self.record_button = W.QPushButton("开始记录 CSV")
        self.record_button.clicked.connect(self.toggle_recording)
        footer.addWidget(self.record_button)
        self.message = W.QLabel("就绪。连接设备后先空载去皮，未校准时显示估算值。")
        self.message.setWordWrap(True)
        footer.addWidget(self.message, 1)
        root.addLayout(footer)

    @staticmethod
    def table(headers):
        table = W.QTableWidget(0, len(headers))
        table.setHorizontalHeaderLabels(headers)
        table.horizontalHeader().setSectionResizeMode(W.QHeaderView.Stretch)
        table.setEditTriggers(W.QAbstractItemView.NoEditTriggers)
        table.setSelectionBehavior(W.QAbstractItemView.SelectRows)
        return table

    def refresh_ports(self):
        selected = self.ports.currentData()
        self.ports.clear()
        for port in list_ports.comports():
            self.ports.addItem(f"{port.device}  {port.description}", port.device)
        index = self.ports.findData(selected)
        if index >= 0:
            self.ports.setCurrentIndex(index)

    def tell(self, message, error=False):
        self.message.setText(message)
        self.message.setStyleSheet("color:#b42318;" if error else "color:#334155;")

    def toggle_connection(self):
        if self.serial:
            self.disconnect("已断开连接")
            return
        port = self.ports.currentData()
        if not port:
            self.tell("未发现串口，请连接 USB 转 TTL 后刷新。", True)
            return
        try:
            self.serial = serial.Serial(port, 115200, timeout=0, write_timeout=0.3)
            self.serial.reset_input_buffer()
            self.decoder = P.Decoder()
            self.pending = None
            self.identified = False
            self.device_config = None
            self.telemetry = None
            self.calibration_points.clear()
            self.invalidate_fit()
            self.chart.points.clear()
            self.chart.update()
            self.last_device_ms = None
            self.last_status_sequence = None
            self.lost_frames = 0
            self.seen_finished = False
            self.connected_at = self.last_rx = time.monotonic()
            self.send(P.HELLO)
            self.connect_button.setText("断开")
            self.connection.setText("正在识别设备…")
            self.tell("等待压力检测固件回应。")
        except (serial.SerialException, OSError) as exc:
            self.disconnect(f"连接失败：{exc}")
        self.sync_buttons()

    def disconnect(self, reason):
        if self.serial:
            try:
                self.serial.close()
            except (serial.SerialException, OSError):
                pass
        self.serial = None
        self.pending = None
        self.identified = False
        self.telemetry = None
        self.device_config = None
        self.stop_recording()
        self.connect_button.setText("连接设备")
        self.connection.setText("未连接")
        self.pressure_label.setText("— mmHg")
        self.load_label.setText("力值：— N   等效克数：— g")
        self.raw_label.setText("瞬时：— mmHg   ADC：—")
        self.sensor_label.setText("实时数据已失效")
        self.power_label.setText("供电：—   电量：—   电池：—")
        self.config_label.setText("设备参数：—（需重新连接读取）")
        self.tell(reason, True)
        self.sync_buttons()

    def send(self, command, payload=b""):
        self.sequence = (self.sequence+1) & 0xFFFF
        data = P.frame(command, self.sequence, payload)
        if self.serial.write(data) != len(data):
            raise serial.SerialException("串口写入不完整，命令结果未知")
        return self.sequence

    def command(self, command, payload=b"", **metadata):
        if not self.serial or not self.identified or self.pending:
            return
        try:
            sequence = self.send(command, payload)
            self.pending = dict(command=command, sequence=sequence, started=time.monotonic(), **metadata)
            self.tell("正在采集稳定读数，请勿触碰加载结构…" if command in (P.TARE, P.POINT) else "等待设备回应…")
        except (serial.SerialException, OSError) as exc:
            self.disconnect(f"通信失败：{exc}")
        self.sync_buttons()

    def poll(self):
        if not self.serial:
            return
        try:
            data = self.serial.read(min(8192, self.serial.in_waiting))
            for packet in self.decoder.feed(data):
                self.handle(packet)
                if not self.serial:
                    return
        except (serial.SerialException, OSError, ValueError, struct.error) as exc:
            self.disconnect(f"通信异常：{exc}")
            return
        now = time.monotonic()
        if not self.identified and now-self.connected_at > 4:
            self.disconnect("未识别压力检测固件，请确认串口、接线和主板固件。")
            return
        age = now-self.last_rx
        if age > 1:
            self.pressure_label.setText("— mmHg")
            self.load_label.setText("力值：— N   等效克数：— g")
            self.raw_label.setText("瞬时：— mmHg   ADC：—")
            self.sensor_label.setText("通信数据已过期")
        if age > 4 and self.identified:
            self.disconnect("设备通信超时。下位机捕捉继续；重新连接可读取结果。")
            return
        if self.pending and now-self.pending["started"] > 4:
            command = self.pending["command"]
            self.pending = None
            self.tell(f"命令 0x{command:02X} 超时，执行结果未知。请读取参数／查看采集状态后再操作。", True)
        self.sync_buttons()

    def handle(self, packet):
        if packet.command == P.INFO:
            if len(packet.payload) != 4 or packet.payload[:2] != b"\x01\x00":
                raise ValueError("设备固件版本不兼容")
            self.identified = True
            self.connection.setText("设备已连接 · 固件 1.0")
            self.tell("连接成功。可空载去皮、采集校准点或开始峰值捕捉。")
        elif packet.command == P.CONFIG:
            config = P.Config.decode(packet.payload)
            if self.pending and self.pending["sequence"] == packet.sequence and self.pending["command"] == P.SET_CONFIG:
                expected = self.pending["expected"]
                if config.zero != expected.zero or config.flags != expected.flags or not math.isclose(config.grams_per_count, expected.grams_per_count, rel_tol=1e-6) or not math.isclose(config.area, expected.area, rel_tol=1e-6):
                    raise ValueError("设备回读与申请写入的校准不一致")
            self.device_config = config
            area_changed = abs(self.area.value()-config.area) > 0.0005
            self.area.blockSignals(True)
            self.area.setValue(config.area)
            self.area.blockSignals(False)
            if area_changed:
                self.invalidate_fit()
            if self.pending and self.pending["sequence"] == packet.sequence:
                self.pending["readback"] = config
                if self.pending["command"] == P.GET_CONFIG:
                    self.pending = None
                    self.tell("设备参数已读取。")
            self.show_config()
        elif packet.command == P.STATUS:
            self.last_rx = time.monotonic()
            telemetry = P.Telemetry.decode(packet.payload)
            if self.last_device_ms is not None and (telemetry.ms-self.last_device_ms) & 0xFFFFFFFF > 0x80000000:
                self.disconnect("检测到设备重启，请重新连接以读取校准参数。")
                return
            self.last_device_ms = telemetry.ms
            if self.last_status_sequence is not None:
                delta = (packet.sequence-self.last_status_sequence) & 0xFFFF
                if 1 < delta < 0x8000:
                    self.lost_frames += delta-1
            self.last_status_sequence = packet.sequence
            self.update_telemetry(telemetry)
        elif self.pending and self.pending["sequence"] == packet.sequence:
            if packet.command == P.POINT_RESULT and self.pending["command"] == P.POINT:
                self.pending["sample"] = P.PointSample.decode(packet.payload)
            elif packet.command == P.ACK:
                if len(packet.payload) != 2 or packet.payload[0] != self.pending["command"]:
                    raise ValueError("命令回应不匹配")
                pending, self.pending = self.pending, None
                status = packet.payload[1]
                if status:
                    self.tell(P.ERRORS.get(status, f"设备错误 {status}"), True)
                elif pending["command"] == P.POINT:
                    if "sample" not in pending:
                        self.tell("缺少校准采样结果，请重新采集。", True)
                    else:
                        self.calibration_points.append(CalibrationPoint(pending["grams"], pending["sample"]))
                        self.invalidate_fit()
                        self.tell("校准点已采集。请放置下一组砝码，静置后继续。")
                elif pending["command"] in (P.SAVE, P.SET_CONFIG, P.TARE):
                    if "readback" not in pending:
                        self.tell("缺少参数回读，执行结果未知，请读取设备参数。", True)
                    else:
                        self.tell({P.SAVE:"已写入 EEPROM 并校验，断电后保留。", P.SET_CONFIG:"校准已应用到 RAM，请复核后保存。", P.TARE:"空载去皮完成；灵敏度保持不变，保存后断电保留。"}[pending["command"]])
                else:
                    self.tell("捕捉已开始，以设备计时为准。" if pending["command"] == P.START else "已提前结束，保留部分捕捉结果。")
        self.sync_buttons()

    def show_config(self):
        c = self.device_config
        state = "已校准" if c.flags & 1 else "未校准（估算灵敏度）"
        saved = "RAM 有未保存修改" if c.dirty else f"EEPROM 版本 {c.generation}"
        self.config_label.setText(f"设备参数：{state} · 零点 {c.zero} · {c.grams_per_count:.9g} g/count · 面积 {c.area:g} mm² · {saved}")

    def update_telemetry(self, t):
        self.telemetry = t
        fresh = t.flags & P.VALID
        self.pressure_label.setText(f"{number(t.filtered) if fresh else '—'} mmHg")
        force, grams = mmhg_to_force_mass(t.filtered if fresh else math.nan, t.area_mm2)
        self.load_label.setText(f"力值：{number(force, 3)} N   等效克数：{number(grams)} g")
        self.raw_label.setText(f"瞬时：{number(t.pressure) if fresh else '—'} mmHg   ADC：{t.raw if fresh else '—'}")
        if t.flags & P.SATURATED:
            state = "ADC 饱和：检查过载或接线"
        elif not fresh:
            state = "传感器无有效数据：检查接线／供电"
        elif not t.flags & P.ZEROED:
            state = "尚未建立零点，请先空载去皮或校准"
        else:
            state = "已校准" if t.flags & P.CALIBRATED else "未校准 · 当前 mmHg 为估算值"
        self.sensor_label.setText(f"{state}   ·   遥测缺帧 {self.lost_frames}")
        supply = ("已充满" if t.flags & P.FULL else "外部供电／充电" if t.flags & P.CHARGING else "电池供电") if t.flags & P.POWER_VALID else "未知"
        self.power_label.setText(f"供电：{supply}   电量：{str(t.soc)+'%' if t.soc<=100 else '未知'}   电池：{str(t.battery_mv)+' mV' if t.battery_mv else '未知'}")
        self.peak_label.setText(f"{number(t.peak)} mmHg")
        peak_force, peak_grams = mmhg_to_force_mass(t.peak, t.area_mm2)
        self.peak_load_label.setText(f"峰值力：{number(peak_force, 3)} N   峰值克数：{number(peak_grams)} g")
        states = ("未开始", "捕捉中", "已完成", "提前结束")
        quality = f" · 有 {t.errors} 次异常，结果不完整" if t.errors else ""
        if t.state in (2, 3) and t.count == 0:
            quality = " · 无有效样本，结果无效"
        estimated = " · 估算值" if not t.flags & P.CALIBRATED and t.state else ""
        self.peak_detail.setText(f"{states[t.state]} · {t.elapsed_ms/1000:.1f}/{t.duration_ms/1000:.1f} 秒 · {t.count} 样本{estimated}{quality}")
        self.peak_detail.setWordWrap(True)
        self.progress.setValue(round(1000*t.elapsed_ms/t.duration_ms) if t.duration_ms else 0)
        self.chart.add(t)
        if t.state in (0, 1):
            self.seen_finished = False
        elif not self.seen_finished:
            self.seen_finished = True
            row = [datetime.now().isoformat(timespec="seconds"), states[t.state]+quality+estimated,
                   t.elapsed_ms/1000, number(t.peak, 3), number(t.minimum, 3), t.peak_at_ms/1000, t.count, t.errors,
                   number(peak_force, 3), number(peak_grams)]
            self.history.append(row)
            self.add_table_row(self.history_table, row)
        if self.log_writer:
            c = self.device_config
            instant_force, instant_grams = mmhg_to_force_mass(t.pressure if fresh else math.nan, t.area_mm2)
            self.log_writer.writerow([datetime.now().isoformat(timespec="milliseconds"), *asdict(t).values(),
                                      instant_force, instant_grams, force, grams, peak_force, peak_grams,
                                      c.zero if c else "", c.grams_per_count if c else "", c.area if c else "", self.lost_frames])
            self.log_rows += 1
            if self.log_rows % 20 == 0:
                self.log_file.flush()

    def sync_buttons(self):
        ready = bool(self.serial and self.identified and self.device_config)
        free = ready and self.pending is None
        running = self.telemetry is not None and self.telemetry.state == 1
        fresh = self.telemetry is not None and bool(self.telemetry.flags & P.VALID) and time.monotonic()-self.last_rx < 1
        self.ports.setEnabled(not self.serial)
        self.refresh.setEnabled(not self.serial)
        self.tare_button.setEnabled(free and fresh and not running)
        self.point_button.setEnabled(free and fresh and not running)
        self.start_button.setEnabled(free and fresh and not running and bool(self.device_config.flags & 2))
        self.stop_button.setEnabled(free and running)
        self.duration.setEnabled(not running)
        self.read_button.setEnabled(free)
        self.apply_button.setEnabled(free and not running and self.fit is not None)
        self.save_button.setEnabled(free and not running and self.device_config.flags == 3)
        self.fit_button.setEnabled(len(self.calibration_points) >= 2 and not self.pending)
        self.remove_button.setEnabled(not self.pending and bool(self.calibration_points))
        self.area.setEnabled(not self.pending and not running)
        self.mass.setEnabled(not self.pending)
        self.record_button.setEnabled(ready or bool(self.log_file))

    def capture_point(self):
        self.command(P.POINT, grams=self.mass.value())

    @staticmethod
    def add_table_row(table, values):
        row = table.rowCount()
        table.insertRow(row)
        for column, value in enumerate(values):
            table.setItem(row, column, W.QTableWidgetItem(str(value)))

    def invalidate_fit(self, *_):
        self.fit = None
        self.fit_label.setText("校准点／面积已变更，请计算校准。")
        self.render_points()
        self.sync_buttons()

    def render_points(self):
        self.points_table.setRowCount(0)
        for i, point in enumerate(self.calibration_points):
            s = point.sample
            residual = number(self.fit.residuals_mmhg[i], 3) if self.fit else "—"
            self.add_table_row(self.points_table, [f"{point.grams:g}", number(mass_to_mmhg(point.grams, self.area.value()), 3),
                                                  number(s.mean, 2), number(s.stddev, 2), s.count, residual])

    def remove_points(self):
        rows = sorted({index.row() for index in self.points_table.selectedIndexes()}, reverse=True)
        for row in rows:
            del self.calibration_points[row]
        self.invalidate_fit()

    def calculate_fit(self):
        try:
            self.fit = fit_points(self.calibration_points, self.area.value())
            f = self.fit
            two = "；仅两点不能检验非线性，建议增加砝码点" if len(self.calibration_points) == 2 else ""
            self.fit_label.setText(f"零点 {f.config.zero} · 灵敏度 {f.config.grams_per_count:.9g} g/count · RMS {f.rms_mmhg:.3f} mmHg · 最大残差 {f.max_error_mmhg:.3f} mmHg{two}")
            self.render_points()
        except ValueError as exc:
            self.fit = None
            self.tell(str(exc), True)
        self.sync_buttons()

    def apply_fit(self):
        if self.fit:
            self.command(P.SET_CONFIG, self.fit.config.encode(), expected=self.fit.config)

    def export_calibration(self):
        path, _ = W.QFileDialog.getSaveFileName(self, "导出校准记录", "压力校准记录.json", "JSON (*.json)")
        if not path:
            return
        record = {"format":"HTP100-pressure-mass-calibration-v1", "time":datetime.now().astimezone().isoformat(),
                  "area_mm2":self.area.value(), "points":[asdict(p) for p in self.calibration_points],
                  "fit":asdict(self.fit) if self.fit else None,
                  "device_config":asdict(self.device_config) if self.device_config else None}
        try:
            Path(path).write_text(json.dumps(record, ensure_ascii=False, indent=2, allow_nan=False), encoding="utf-8")
            self.tell(f"校准记录已导出：{path}")
        except (OSError, ValueError) as exc:
            self.tell(f"导出失败：{exc}", True)

    def export_history(self):
        path, _ = W.QFileDialog.getSaveFileName(self, "导出峰值结果", "压力峰值结果.csv", "CSV (*.csv)")
        if not path:
            return
        try:
            with open(path, "w", newline="", encoding="utf-8-sig") as output:
                writer = csv.writer(output)
                writer.writerow([self.history_table.horizontalHeaderItem(i).text() for i in range(self.history_table.columnCount())])
                writer.writerows(self.history)
            self.tell(f"峰值结果已导出：{path}")
        except OSError as exc:
            self.tell(f"导出失败：{exc}", True)

    def toggle_recording(self):
        if self.log_file:
            self.stop_recording()
            self.tell("CSV 记录已停止。")
            return
        path, _ = W.QFileDialog.getSaveFileName(self, "持续记录遥测 CSV", "压力实时记录.csv", "CSV (*.csv)")
        if not path:
            return
        try:
            self.log_file = open(path, "w", newline="", encoding="utf-8-sig")
            self.log_writer = csv.writer(self.log_file)
            self.log_writer.writerow(["电脑时间", *P.Telemetry.__dataclass_fields__, "force_n", "mass_g", "filtered_force_n", "filtered_mass_g", "peak_force_n", "peak_mass_g", "zero_counts", "grams_per_count", "config_area_mm2", "telemetry_lost_frames"])
            self.log_file.flush()
            self.log_rows = 0
            self.record_button.setText("停止记录 CSV")
            self.tell(f"正在持续记录：{path}")
        except OSError as exc:
            self.stop_recording()
            self.tell(f"无法记录：{exc}", True)

    def stop_recording(self):
        if self.log_file:
            try:
                self.log_file.close()
            except OSError:
                pass
        self.log_file = self.log_writer = None
        self.record_button.setText("开始记录 CSV")

    def closeEvent(self, event):
        self.disconnect("已关闭")
        event.accept()


def main():
    app = W.QApplication(sys.argv)
    app.setFont(QtGui.QFont("Microsoft YaHei UI", 10))
    window = Window()
    window.show()
    return app.exec_()


if __name__ == "__main__":
    sys.exit(main())
