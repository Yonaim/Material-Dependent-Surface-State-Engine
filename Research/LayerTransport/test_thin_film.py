"""공통 박막 모델의 해석 한계와 두 층의 형상 결합을 확인한다."""

import unittest
from math import cos, hypot, radians, sin, tan

from thin_film import GRAVITY, Material, flux_per_width, transport_step


class TransportChecks(unittest.TestCase):
    def test_water_limit(self) -> None:
        water = Material(1000, 0.001)
        actual = flux_per_width(water, 0.002, 0.05, 0)
        self.assertAlmostEqual(actual, 1000 * GRAVITY * 0.002**3 * 0.05 / 0.003)

    def test_repose_on_rotated_plane(self) -> None:
        grains = Material(1500, 2, repose_degrees=15)
        for rotation in (0, 30, 45, 90):
            angle = radians(rotation)
            for tilt in (14, 16):
                slope = tan(radians(tilt))
                x, y = slope * cos(angle), slope * sin(angle)
                flow = hypot(flux_per_width(grains, 0.01, x, y),
                             flux_per_width(grains, 0.01, y, x))
                expected = max(0, slope - tan(radians(15))) * 1500 * GRAVITY * 0.01**3 / 6
                self.assertAlmostEqual(flow, expected, places=15)

    def test_combined_threshold(self) -> None:
        film = Material(1000, 1, yield_stress=10, repose_degrees=10)
        slope = tan(radians(10)) + 0.1
        self.assertEqual(flux_per_width(film, 0.01, slope, 0), 0)
        self.assertGreater(flux_per_width(film, 0.02, slope, 0), 0)

    def test_diagonal_grid_transport(self) -> None:
        # 두 축 성분은 각각 문턱 아래지만 전체 경사는 문턱을 넘는다.
        # 모서리의 두 유출 면에서 빠진 질량을 해석값과 비교한다.
        grains = Material(1500, 2, repose_degrees=15)
        pitch, depth, seconds = 0.02, 0.012, 0.0001
        for component in (0.15, 0.20):
            bed = [-component * pitch * (x + y) for y in range(4) for x in range(4)]
            mass = [grains.density * depth * pitch**2] * len(bed)
            after, = transport_step(bed, 4, pitch, seconds, [grains], [mass])
            slope = hypot(component, component)
            excess = max(0.0, slope - tan(radians(15)))
            flow = grains.density * GRAVITY * depth**3 * excess / (3 * grains.viscosity)
            expected = 2 * pitch * seconds * grains.density * flow * component / slope
            self.assertAlmostEqual(mass[0] - after[0], expected, places=14)
            self.assertAlmostEqual(sum(after), sum(mass), places=14)

    def test_barrier_and_conservation(self) -> None:
        # 네모 격자의 중앙 열 전체를 턱으로 사용한다. 오른쪽 끝 행·열은 닫혀 있다.
        bed = [0.] * 9
        materials = [Material(900, 1, yield_stress=1e6), Material(1000, 0.001)]
        water = [1., 0., 0.] * 3
        for height, should_open in ((0.2, False), (0.005, True)):
            ridge = [0., height * 900 * 0.1**2, 0.] * 3
            result = transport_step(bed, 3, 0.1, 0.01, materials, [ridge, water])
            self.assertEqual(result[0], ridge)
            self.assertEqual(result[1][1] > 0, should_open)
            self.assertAlmostEqual(sum(result[1]), sum(water))
            self.assertGreaterEqual(min(result[1]), 0)

    def test_multiple_moving_layers(self) -> None:
        layers = [Material(1100, 1), Material(1000, 0.1)]
        start = [[0.3, 0., 0.], [0.2, 0., 0.]]
        end = transport_step([0.] * 3, 3, 0.1, 0.001, layers, start)
        for before, after in zip(start, end):
            self.assertGreater(after[1], 0)
            self.assertAlmostEqual(sum(before), sum(after))
            self.assertEqual(after[2], 0)


if __name__ == "__main__":
    unittest.main()
