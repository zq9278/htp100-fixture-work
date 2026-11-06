"""Compile/run production measurement C on emulated ARM, decode its wire trace."""
from pathlib import Path
import shutil
import subprocess
import sys
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_PC

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "tools/pressure_sensor_host"))
import protocol as P

project = root / "9C-U_HTP100_Main_vscode-200"
output = project / "build/pressure-tests"
output.mkdir(parents=True, exist_ok=True)
elf_path = output / "pressure-tests.elf"
subprocess.run([shutil.which("arm-none-eabi-gcc"), "-mcpu=cortex-m0", "-mthumb", "-O1", "-g",
                "-nostartfiles", "--specs=nosys.specs", "-Wl,-Ttext=0x10000", "-Wl,-Tdata=0x20000000",
                "-Wl,-e,main", "-I"+str(project / "App/Inc"), str(project / "tests/test_pressure_monitor.c"),
                str(project / "App/Src/pressure_monitor.c"), "-lm", "-o", str(elf_path)], check=True)
u = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
u.mem_map(0, 0x200000)
u.mem_map(0x20000000, 0x100000)
with elf_path.open("rb") as source:
    elf = ELFFile(source)
    for segment in elf.iter_segments():
        if segment["p_type"] == "PT_LOAD":
            u.mem_write(segment["p_vaddr"], segment.data())
    symbols = elf.get_section_by_name(".symtab")
    def address(name):
        return symbols.get_symbol_by_name(name)[0]["st_value"]
    entry, trace, trace_length = (address(n) for n in ("main", "pressure_test_trace", "pressure_test_trace_length"))
u.reg_write(UC_ARM_REG_SP, 0x200F0000)
u.reg_write(UC_ARM_REG_LR, 0x8001)
u.emu_start(entry | 1, 0x8000, count=100000000)
result = u.reg_read(UC_ARM_REG_R0)
assert result == 0, f"C assertion failed at line {result}"
length = int.from_bytes(u.mem_read(trace_length, 4), "little")
decoder = P.Decoder()
packets = decoder.feed(bytes(u.mem_read(trace, length)))
assert not decoder.buffer and decoder.bad_frames == 0
counts = {}
for packet in packets:
    counts[packet.command] = counts.get(packet.command, 0)+1
    if packet.command == P.STATUS:
        P.Telemetry.decode(packet.payload)
    elif packet.command == P.CONFIG:
        P.Config.decode(packet.payload)
    elif packet.command == P.POINT_RESULT:
        P.PointSample.decode(packet.payload)
assert counts[P.STATUS] > 50 and counts[P.POINT_RESULT] >= 2
print(f"PASS: ARM measurement engine and PC decoder ({len(packets)} frames): continuous raw maximum, no one-hour deadline, clear/restart, duplicate-clear idempotency, negative maximum, stale/saturation, calibration during capture, CRC, persistence/power-cut fallback, long-uptime saturation and timer/generation wrap.")

uart_elf = output / "pressure-uart-tests.elf"
subprocess.run([shutil.which("arm-none-eabi-gcc"), "-mcpu=cortex-m0", "-mthumb", "-O1", "-nostartfiles",
                "--specs=nosys.specs", "-DPRESSURE_MONITOR_BUILD=1", "-Wl,-Ttext=0x10000",
                "-Wl,-Tdata=0x20000000", "-Wl,-e,main", "-I"+str(project / "tests/pressure_uart_stubs"),
                "-I"+str(project / "Bsp/Inc"), str(project / "tests/test_pressure_uart.c"),
                str(project / "Bsp/Src/screen_uart.c"), str(project / "Bsp/Src/pc_uart.c"), "-o", str(uart_elf)], check=True, capture_output=True)
uart = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
uart.mem_map(0, 0x200000)
uart.mem_map(0x20000000, 0x100000)
with uart_elf.open("rb") as source:
    elf = ELFFile(source)
    for segment in elf.iter_segments():
        if segment["p_type"] == "PT_LOAD":
            uart.mem_write(segment["p_vaddr"], segment.data())
    entry = elf.get_section_by_name(".symtab").get_symbol_by_name("main")[0]["st_value"]
uart.reg_write(UC_ARM_REG_SP, 0x200F0000)
uart.reg_write(UC_ARM_REG_LR, 0x8001)
uart.emu_start(entry | 1, 0x8000, count=10000000)
result = uart.reg_read(UC_ARM_REG_R0)
assert result == 0, f"UART C assertion failed at line {result}"
print("PASS: production UART DMA queue: buffer lifetime, ordered frames, circular wrap, atomic overflow rejection, timeout recovery.")

# Production H7 controller linked directly to the production G0 core.
screen = root / "9C-U_HTP100_SCREEN_Picture -100"
two_elf = output / "pressure-two-boards.elf"
subprocess.run([shutil.which("arm-none-eabi-gcc"), "-mcpu=cortex-m0", "-mthumb", "-O1", "-nostartfiles",
                "--specs=nosys.specs", "-Wl,-Ttext=0x10000", "-Wl,-Tdata=0x20000000", "-Wl,-e,main",
                "-I"+str(project / "App/Inc"), "-I"+str(screen / "HardWare/Inc"),
                str(project / "tests/test_pressure_two_boards.c"), str(project / "App/Src/pressure_monitor.c"),
                str(screen / "HardWare/Src/pressure_client.c"), "-lm", "-o", str(two_elf)], check=True, capture_output=True)
two = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
two.mem_map(0, 0x200000)
two.mem_map(0x20000000, 0x100000)
with two_elf.open("rb") as source:
    elf = ELFFile(source)
    for segment in elf.iter_segments():
        if segment["p_type"] == "PT_LOAD":
            two.mem_write(segment["p_vaddr"], segment.data())
    entry = elf.get_section_by_name(".symtab").get_symbol_by_name("main")[0]["st_value"]
two.reg_write(UC_ARM_REG_SP, 0x200F0000)
two.reg_write(UC_ARM_REG_LR, 0x8001)
two.emu_start(entry | 1, 0x8000, count=800000000)
assert two.reg_read(UC_ARM_REG_PC)==0x8000, "Two-board test exceeded its instruction budget"
result = two.reg_read(UC_ARM_REG_R0)
assert result == 0, f"Two-board C assertion failed at line {result}"
print("PASS: real H7 client + G0 engine: discovery, fit/apply/save, peak/reconnect, independent UARTs; guided six-point calibration corrects 1.86% gain error, three up/down verification cycles, EEPROM reboot, hysteresis/repeatability rejection, save gating, parameter-change and link interruption.")
