"""Mass calibration independent of the GUI and serial transport."""
from dataclasses import dataclass
import math
from protocol import Config, PointSample

PASCAL_PER_MMHG = 133.3224
GRAVITY = 9.80665


def mass_to_mmhg(grams: float, area_mm2: float) -> float:
    if not (math.isfinite(grams) and math.isfinite(area_mm2) and area_mm2 > 0):
        raise ValueError("重量和面积必须是有效数值，面积必须大于零")
    return grams * GRAVITY * 1000 / (area_mm2 * PASCAL_PER_MMHG)


def mmhg_to_force_mass(mmhg: float, area_mm2: float) -> tuple[float, float]:
    """Convert one coherent pressure/area sample to signed N and equivalent g."""
    if not (math.isfinite(mmhg) and math.isfinite(area_mm2) and 1 <= area_mm2 <= 100000):
        return math.nan, math.nan
    force = mmhg * PASCAL_PER_MMHG * area_mm2 * 1e-6
    return force, force * 1000 / GRAVITY


@dataclass(frozen=True)
class CalibrationPoint:
    grams: float
    sample: PointSample


@dataclass(frozen=True)
class Fit:
    config: Config
    residuals_mmhg: tuple[float, ...]
    rms_mmhg: float
    max_error_mmhg: float
    span_mmhg: float


def fit_points(points: list[CalibrationPoint], area: float) -> Fit:
    if not math.isfinite(area) or not 1 <= area <= 100000:
        raise ValueError("有效受力面积需为 1～100000 mm²")
    if len(points) < 2 or not any(p.grams == 0 for p in points):
        raise ValueError("至少采集一个 0 g 空载点和一个已知砝码点；推荐 0、100、300、500 g")
    if any(not math.isfinite(p.grams) or p.grams < 0 or p.sample.count < 30
           or not math.isfinite(p.sample.mean) or not math.isfinite(p.sample.stddev)
           or p.sample.stddev < 0 for p in points):
        raise ValueError("校准点包含无效重量或采样")
    masses = [p.grams for p in points]
    values = [p.sample.mean for p in points]
    if max(masses) - min(masses) < 1:
        raise ValueError("砝码重量跨度至少为 1 g")
    span = max(values) - min(values)
    if span < max(100, 10 * max(p.sample.stddev for p in points)):
        raise ValueError("ADC 变化不足或噪声过大，请检查传感器和砝码受力")
    xbar, ybar = sum(values) / len(points), sum(masses) / len(points)
    slope = sum((x-xbar)*(y-ybar) for x, y in zip(values, masses)) / sum((x-xbar)**2 for x in values)
    if not math.isfinite(slope) or not 1e-7 <= abs(slope) <= 100:
        raise ValueError("校准斜率异常，请重新采集")
    zero = round(xbar - ybar / slope)
    if not -8388608 <= zero <= 8388607:
        raise ValueError("拟合零点超出 ADC 范围")
    residuals = tuple(mass_to_mmhg((x-zero)*slope-y, area) for x, y in zip(values, masses))
    rms = math.sqrt(sum(e*e for e in residuals)/len(residuals))
    maximum = max(abs(e) for e in residuals)
    pressure_span = mass_to_mmhg(max(masses)-min(masses), area)
    if maximum > max(2.0, pressure_span * 0.005):
        raise ValueError(f"校准不一致：最大残差 {maximum:.2f} mmHg；请检查偏载、迟滞或砝码录入")
    return Fit(Config(zero, slope, area, 3, dirty=True), residuals, rms, maximum, pressure_span)
