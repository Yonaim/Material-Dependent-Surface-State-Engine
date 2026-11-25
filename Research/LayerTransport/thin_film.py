"""수평 정사각 격자에서 층별 kg와 m 단위 높이를 연결하는 CPU 기준 계산."""

from dataclasses import dataclass
from math import hypot, isfinite, radians, tan

GRAVITY = 9.81


# region 00. Material response
@dataclass(frozen=True)
class Material:
    density: float
    viscosity: float
    yield_stress: float = 0.0
    repose_degrees: float = 0.0

    def __post_init__(self) -> None:
        values = (self.density, self.viscosity, self.yield_stress, self.repose_degrees)
        if not all(map(isfinite, values)) or self.density <= 0 or self.viscosity <= 0:
            raise ValueError("density and viscosity must be finite and positive")
        if self.yield_stress < 0 or not 0 <= self.repose_degrees < 90:
            raise ValueError("invalid yield stress or repose angle")


def flux_per_width(material: Material, depth: float, downhill: float, cross: float) -> float:
    """유효 수심 [m]과 전체 수면 구배로부터 면 방향 부피 유량 [m²/s]을 구한다."""
    slope = hypot(downhill, cross)
    net_slope = slope - tan(radians(material.repose_degrees))
    if depth <= 0 or net_slope <= 0:
        return 0.0
    driving_stress = material.density * GRAVITY * depth * net_slope
    if driving_stress <= material.yield_stress:
        return 0.0
    ratio = material.yield_stress / driving_stress
    mobility = (1 - ratio) ** 2 * (1 + ratio / 2)
    return (depth**2 * driving_stress / (3 * material.viscosity)
            * mobility * downhill / slope)
# endregion


# region 01. Conservative layer transport
def transport_step(
    bed: list[float], columns: int, pitch: float, seconds: float,
    materials: list[Material], masses: list[list[float]],
) -> list[list[float]]:
    """닫힌 직교 격자. 층 순서는 아래부터 위이며 모든 면은 스텝 시작 형상을 읽는다."""
    if columns < 1 or not bed or len(bed) % columns or len(materials) != len(masses):
        raise ValueError("inconsistent grid or layer dimensions")
    if not all(map(isfinite, bed)) or not isfinite(pitch) or pitch <= 0:
        raise ValueError("invalid geometry")
    if not isfinite(seconds) or seconds < 0:
        raise ValueError("invalid step duration")
    rows = len(bed) // columns
    support = bed.copy()
    result = []
    for material, mass in zip(materials, masses):
        if len(mass) != len(bed) or any(not isfinite(m) or m < 0 for m in mass):
            raise ValueError("invalid layer mass")
        surface = [z + m / (pitch**2 * material.density) for z, m in zip(support, mass)]

        def gradient(i: int, axis: int) -> float:
            offset, coordinate, count = ((1, i % columns, columns) if axis == 0
                                         else (columns, i // columns, rows))
            left = i - offset if coordinate > 0 else i
            right = i + offset if coordinate + 1 < count else i
            return ((surface[right] - surface[left]) / (pitch * ((right - left) // offset))
                    if left != right else 0.0)

        transfers = []
        outgoing = [0.0] * len(bed)
        for i in range(len(bed)):
            for axis, j in enumerate((i + 1, i + columns)):
                if j >= len(bed) or (axis == 0 and j % columns == 0):
                    continue
                depth = max(surface[i], surface[j]) - max(support[i], support[j])
                downhill = (surface[i] - surface[j]) / pitch
                cross = (gradient(i, 1 - axis) + gradient(j, 1 - axis)) / 2
                q = flux_per_width(material, depth, downhill, cross)
                donor, receiver = (i, j) if q > 0 else (j, i)
                requested = abs(q) * pitch * seconds * material.density
                transfers.append((donor, receiver, requested))
                outgoing[donor] += requested

        scale = [m / out if out > m else 1.0 for m, out in zip(mass, outgoing)]
        incoming = [0.0] * len(bed)
        for donor, receiver, requested in transfers:
            incoming[receiver] += requested * scale[donor]
        result.append([max(0.0, m - out * s) + inc
                       for m, out, s, inc in zip(mass, outgoing, scale, incoming)])
        support = surface
    return result
# endregion
