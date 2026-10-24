from pathlib import Path
import math
import struct
import sys
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from calibration import CalibrationPoint, fit_points, mass_to_mmhg
from protocol import Config, Decoder, PointSample, Telemetry, frame, crc16


def point(grams, raw, stddev=2):
    return CalibrationPoint(grams, PointSample(raw, stddev, int(raw-5), int(raw+5), 250))


def test_mass_area_and_signed_fit():
    assert mass_to_mmhg(100, 84.8) == pytest.approx(86.736, abs=0.01)
    for sign in (-1, 1):
        points = [point(m, 1_234_000+sign*m*400) for m in (0, 100, 300, 500)]
        fit = fit_points(points, 84.8)
        assert fit.config.zero == 1_234_000
        assert fit.config.grams_per_count == pytest.approx(sign/400)
        assert fit.max_error_mmhg < 0.001
        assert fit_points(points, 169.6).span_mmhg == pytest.approx(fit.span_mmhg/2)


@pytest.mark.parametrize("points", [
    [point(100, 500), point(200, 1000)],
    [point(0, 500), point(0, 1000)],
    [point(0, 500), point(100, 505)],
    [point(0, 500), point(100, 1500, 200)],
    [point(0, 100000), point(100, 60000), point(300, 50000)],
    [point(0, 100000), point(math.nan, 50000)],
])
def test_bad_calibration_rejected(points):
    with pytest.raises(ValueError):
        fit_points(points, 84.8)


def test_parser_fragment_crc_recovery():
    assert crc16(b"123456789") == 0x4B37
    good = frame(2, 400, b"abc")
    bad = bytearray(good)
    bad[-3] ^= 1
    wire = b"noise\x7b\x00"+bytes(bad)+good+frame(3, 65535)
    decoder = Decoder()
    packets = []
    for b in wire:
        packets += decoder.feed(bytes([b]))
    assert [(p.command, p.sequence, p.payload) for p in packets] == [(2, 400, b"abc"), (3, 65535, b"")]
    assert decoder.bad_frames >= 1 and not decoder.buffer


def test_wire_config_and_telemetry():
    c = Config.decode(struct.pack("<iffBIB", 123456, -0.0025, 84.8, 3, 123, 1))
    assert c.zero == 123456 and c.dirty
    assert len(c.encode()) == 13
    wire = struct.pack("<IiffffIIIHBBHIHI", 1000, -123, 350., 348., 400., 1., 1000, 1000, 250, 7, 2, 80, 3900, 0, 3, 500)
    telemetry = Telemetry.decode(wire)
    assert telemetry.peak == 400 and telemetry.count == 250 and telemetry.peak_at_ms == 500
    with pytest.raises(ValueError):
        Config.decode(struct.pack("<iffBIB", 0, math.nan, 84.8, 3, 0, 0))
