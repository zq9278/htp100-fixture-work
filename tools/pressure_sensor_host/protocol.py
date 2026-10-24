"""HTP100 pressure monitor v1 wire format (little endian, CRC16/MODBUS)."""
from dataclasses import dataclass
import math
import struct

HELLO, GET_CONFIG, TARE, POINT, SET_CONFIG, SAVE, START, STOP = range(1, 9)
INFO, CONFIG, ACK, POINT_RESULT, STATUS = 0x81, 0x82, 0x83, 0x84, 0x90
VALID, CALIBRATED, ZEROED, CHARGING, FULL, POWER_VALID, DIRTY, SATURATED, ADC_READY = (1 << i for i in range(9))
ERRORS = {0: "成功", 1: "命令格式错误或固件不兼容", 2: "设备忙，请先结束当前采集",
          3: "传感器无有效数据／采集缺样", 4: "受力不稳定，请静置后重试",
          5: "EEPROM 保存或回读失败，旧校准仍保留", 6: "请先空载去皮或完成校准", 7: "参数超出范围"}


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
    return crc


def frame(command: int, sequence: int, payload: bytes = b"") -> bytes:
    if len(payload) > 64:
        raise ValueError("payload too large")
    head = struct.pack("<2sBBHB", b"\x7b\xb7", 1, command, sequence, len(payload)) + payload
    return head + struct.pack("<H", crc16(head)) + b"\x0a"


@dataclass(frozen=True)
class Frame:
    command: int
    sequence: int
    payload: bytes


class Decoder:
    def __init__(self):
        self.buffer = bytearray()
        self.bad_frames = 0

    def feed(self, data: bytes) -> list[Frame]:
        self.buffer.extend(data)
        output = []
        while len(self.buffer) >= 2:
            if self.buffer[:2] != b"\x7b\xb7":
                del self.buffer[0]
                continue
            if len(self.buffer) < 7:
                break
            n = self.buffer[6]
            if self.buffer[2] != 1 or n > 64:
                self.bad_frames += 1
                del self.buffer[0]
                continue
            size = n + 10
            if len(self.buffer) < size:
                break
            packet = bytes(self.buffer[:size])
            if packet[-1] != 10 or crc16(packet[:-3]) != struct.unpack_from("<H", packet, size - 3)[0]:
                self.bad_frames += 1
                del self.buffer[0]
                continue
            output.append(Frame(packet[3], struct.unpack_from("<H", packet, 4)[0], packet[7:-3]))
            del self.buffer[:size]
        return output


@dataclass(frozen=True)
class Config:
    zero: int
    grams_per_count: float
    area: float
    flags: int
    generation: int = 0
    dirty: bool = False

    @classmethod
    def decode(cls, payload: bytes):
        if len(payload) != 18:
            raise ValueError("校准参数长度不匹配")
        zero, scale, area, flags, generation, dirty = struct.unpack("<iffBIB", payload)
        if not (-8388608 <= zero <= 8388607 and math.isfinite(scale) and 1e-7 <= abs(scale) <= 100
                and math.isfinite(area) and 1 <= area <= 100000 and flags in (0, 2, 3) and dirty in (0, 1)):
            raise ValueError("设备返回无效校准参数")
        return cls(zero, scale, area, flags, generation, bool(dirty))

    def encode(self) -> bytes:
        return struct.pack("<iffB", self.zero, self.grams_per_count, self.area, self.flags)


@dataclass(frozen=True)
class PointSample:
    mean: float
    stddev: float
    minimum: int
    maximum: int
    count: int

    @classmethod
    def decode(cls, payload: bytes):
        if len(payload) != 20:
            raise ValueError("校准采样长度不匹配")
        result = cls(*struct.unpack("<ffiiI", payload))
        if not (math.isfinite(result.mean) and math.isfinite(result.stddev) and result.stddev >= 0
                and result.count >= 30 and -8388608 <= result.minimum <= result.mean <= result.maximum <= 8388607):
            raise ValueError("设备返回无效校准采样")
        return result


@dataclass(frozen=True)
class Telemetry:
    ms: int
    raw: int
    pressure: float
    filtered: float
    peak: float
    minimum: float
    elapsed_ms: int
    duration_ms: int
    count: int
    flags: int
    state: int
    soc: int
    battery_mv: int
    errors: int
    age_ms: int
    peak_at_ms: int

    @classmethod
    def decode(cls, payload: bytes):
        if len(payload) != 52:
            raise ValueError("遥测长度不匹配")
        result = cls(*struct.unpack("<IiffffIIIHBBHIHI", payload))
        if result.state > 3 or result.soc not in (*range(101), 255):
            raise ValueError("无效设备状态")
        return result
