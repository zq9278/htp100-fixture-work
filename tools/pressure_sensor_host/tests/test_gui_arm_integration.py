"""Drive the real Qt GUI through a fake serial cable to production C on ARM."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import time
import math
import pytest
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_R1

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from PyQt5.QtWidgets import QApplication
from PyQt5.QtGui import QFontDatabase
import pressure_sensor_host as H
import protocol as P
from calibration import mass_to_mmhg


class ArmSerial:
    def __init__(self,pc_port=True):
        root = Path(__file__).resolve().parents[3]
        project = root / "9C-U_HTP100_Main_vscode-200"
        output = project / "build/pressure-tests"
        output.mkdir(parents=True, exist_ok=True)
        target = output / "pressure-simulator.elf"
        subprocess.run([shutil.which("arm-none-eabi-gcc"), "-mcpu=cortex-m0", "-mthumb", "-O1", "-nostartfiles",
                        "--specs=nosys.specs", "-Wl,-Ttext=0x10000", "-Wl,-Tdata=0x20000000", "-Wl,-e,main",
                        "-I"+str(project / "App/Inc"), str(project / "tests/pressure_device_simulator.c"),
                        str(project / "App/Src/pressure_monitor.c"), "-lm", "-o", str(target)], check=True, capture_output=True)
        self.u = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        self.u.mem_map(0, 0x200000)
        self.u.mem_map(0x20000000, 0x100000)
        with target.open("rb") as file:
            elf = ELFFile(file)
            for segment in elf.iter_segments():
                if segment["p_type"] == "PT_LOAD":
                    self.u.mem_write(segment["p_vaddr"], segment.data())
            self.symbols = {s.name:s["st_value"] for s in elf.get_section_by_name(".symtab").iter_symbols()}
        self.clock = 0
        self.call("Sim_Init")
        self.pc_port=pc_port
        self.call("Sim_PcPort",int(pc_port))

    def call(self, name, arg0=0, arg1=0):
        self.u.reg_write(UC_ARM_REG_SP, 0x200F0000)
        self.u.reg_write(UC_ARM_REG_LR, 0x8001)
        self.u.reg_write(UC_ARM_REG_R0, arg0 & 0xFFFFFFFF)
        self.u.reg_write(UC_ARM_REG_R1, arg1 & 0xFFFFFFFF)
        self.u.emu_start(self.symbols[name] | 1, 0x8000, count=5000000)

    def advance(self, duration, grams=0):
        for _ in range(duration//5):
            self.clock += 5
            self.call("Sim_Step", self.clock, round(1_000_000-grams*400))

    @property
    def in_waiting(self):
        return int.from_bytes(self.u.mem_read(self.symbols["sim_tx_length"], 4), "little")

    def reset_input_buffer(self):
        self.u.mem_write(self.symbols["sim_tx_length"], b"\0"*4)

    def read(self, n):
        length = self.in_waiting
        data = bytes(self.u.mem_read(self.symbols["sim_tx"], length))
        self.reset_input_buffer()
        if n < length:
            self.u.mem_write(self.symbols["sim_tx"], data[n:])
            self.u.mem_write(self.symbols["sim_tx_length"], (length-n).to_bytes(4, "little"))
        return data[:n]

    def write(self, data):
        self.u.mem_write(self.symbols["sim_rx"], data)
        self.call("Sim_FeedPc" if self.pc_port else "Sim_Feed", len(data))
        return len(data)

    def close(self):
        pass


def test_gui_to_real_firmware(monkeypatch):
    cable = ArmSerial()
    app = QApplication.instance() or QApplication([])
    # Windows' offscreen Qt plugin does not discover system fonts itself.
    if not QFontDatabase().families():
        QFontDatabase.addApplicationFont(str(Path(os.environ.get("WINDIR", "C:/Windows")) / "Fonts/msyh.ttc"))
    monkeypatch.setattr(H.serial, "Serial", lambda *a, **k:cable)
    window = H.Window()
    window.timer.stop()
    window.ports.clear()
    window.ports.addItem("ARM test device", "SIM")
    window.toggle_connection()
    window.poll()
    assert window.identified and window.device_config.flags == 0
    cable.advance(50)
    window.poll()
    assert window.tare_button.isEnabled() and not window.start_button.isEnabled()
    for mass in (0, 100, 300, 500):
        window.mass.setValue(mass)
        window.capture_point()
        cable.advance(1000, mass)
        window.poll()
        assert window.pending is None
    assert len(window.calibration_points) == 4
    window.calculate_fit()
    assert window.fit is not None and window.apply_button.isEnabled()
    window.apply_fit()
    window.poll()
    assert window.device_config.flags == 3
    assert window.device_config.zero == 1_000_000
    assert window.device_config.grams_per_count == pytest.approx(-0.0025)
    window.command(P.SAVE)
    window.poll()
    assert window.device_config.generation == 1 and not window.device_config.dirty
    window.duration.setValue(0.2)
    window.start_button.click()
    window.poll()
    cable.advance(50, 100)
    cable.advance(5, 500)  # A 5ms spike between two PC telemetry frames.
    cable.advance(195, 100)
    window.poll()
    assert window.telemetry.state == 2 and window.telemetry.errors == 0
    assert window.telemetry.peak == pytest.approx(mass_to_mmhg(500, 84.8), abs=0.01)
    assert window.telemetry.pressure == pytest.approx(mass_to_mmhg(100, 84.8), abs=0.01)
    assert window.telemetry.peak_at_ms == 55 and len(window.history) == 1
    assert "433.7" in window.peak_label.text()
    for step in range(120):
        cable.advance(250, 220+180*math.sin(step/7))
        window.poll()
    window.connection.setText("ARM 仿真联调 · 非实测数据")
    window.show()
    app.processEvents()
    preview = Path(__file__).resolve().parents[3] / "交付/压力传感器检测/PC辅助界面（仿真数据）.png"
    assert window.grab().save(str(preview))
    # A config query must invalidate a fit made with another area.
    window.area.setValue(100)
    window.calculate_fit()
    assert window.fit.config.area == 100
    window.command(P.GET_CONFIG)
    window.poll()
    assert window.fit is None and window.area.value() == pytest.approx(84.8, abs=0.001)
    # Expired data cannot remain displayed as current pressure.
    window.last_rx = time.monotonic()-2
    window.poll()
    assert window.pressure_label.text() == "— mmHg" and not window.start_button.isEnabled()
    # Reboot keeps calibration; reconnect discards old point/command state.
    cable.call("Sim_Reset")
    cable.clock = 0
    cable.advance(50)
    window.poll()
    assert window.serial is None
    window.toggle_connection()
    window.poll()
    assert window.device_config.generation == 1 and window.device_config.flags == 3
    assert window.calibration_points == [] and window.pending is None
    window.close()
