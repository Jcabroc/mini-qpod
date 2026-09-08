import math
import random
import unittest

from tools.ik_simulator.kinematics import (
    COXA, FEMUR, TIBIA, MOUNTS, Angles, SingularTarget,
    UnreachableTarget, body_to_leg, leg_to_body, forward_local,
    inverse_local, forward, inverse,
)


class KinematicsTests(unittest.TestCase):
    def assertPoint(self, actual, expected):
        for a, b in zip(actual, expected):
            self.assertAlmostEqual(a, b, places=7)

    def test_step_dimensions(self):
        self.assertEqual((COXA, FEMUR, TIBIA), (42.294, 60.611, 88.714))
        self.assertEqual(MOUNTS["R1"].x - MOUNTS["L1"].x, 93)
        self.assertEqual(MOUNTS["L1"].y - MOUNTS["L2"].y, 93)

    def test_radial_axes(self):
        for leg, sx, sy in (("L1", -1, 1), ("R1", 1, 1),
                            ("L2", -1, -1), ("R2", 1, -1)):
            self.assertPoint(leg_to_body(leg, (math.sqrt(2)*10, 0, -30)),
                             (sx*56.5, sy*56.5, -30))
            self.assertPoint(body_to_leg(leg, (sx*56.5, sy*56.5, -30)),
                             (math.sqrt(2)*10, 0, -30))

    def test_analytic_right_angle(self):
        point = (COXA + FEMUR, 0, -TIBIA)
        result = inverse_local(point)
        self.assertAlmostEqual(result.yaw, 0)
        self.assertAlmostEqual(result.femur, 0)
        self.assertAlmostEqual(result.knee, -math.pi/2)
        self.assertPoint(forward_local(Angles(0, 0, -math.pi/2)), point)

    def test_workspace_boundaries(self):
        for d in (abs(FEMUR-TIBIA), FEMUR+TIBIA):
            for sign in (-1, 1):
                p = (COXA+d, 0, 0)
                self.assertPoint(forward_local(inverse_local(p, sign)), p)
        for d in (abs(FEMUR-TIBIA)-0.001, FEMUR+TIBIA+0.001):
            with self.assertRaises(UnreachableTarget):
                inverse_local((COXA+d, 0, 0))

    def test_invalid_inputs(self):
        with self.assertRaises(SingularTarget):
            inverse_local((0, 0, -100))
        for bad in (float("nan"), float("inf"), -float("inf")):
            with self.assertRaises(ValueError):
                inverse_local((100, bad, 0))
            with self.assertRaises(ValueError):
                forward_local(Angles(bad, 0, 0))
        with self.assertRaises(ValueError):
            inverse_local((100, 0, -80), 0)
        with self.assertRaises(KeyError):
            inverse("unknown", (100, 0, -80))

    def test_seeded_round_trips_both_branches_all_legs(self):
        rng = random.Random(93)
        for leg in MOUNTS:
            for _ in range(250):
                # Sample Cartesian points independently from FK.
                local = (rng.uniform(-150, 150), rng.uniform(-150, 150),
                         rng.uniform(-150, 50))
                d = math.hypot(math.hypot(*local[:2])-COXA, local[2])
                if not abs(FEMUR-TIBIA) < d < FEMUR+TIBIA:
                    continue
                body = leg_to_body(leg, local)
                for sign in (-1, 1):
                    angles = inverse(leg, body, sign)
                    self.assertGreaterEqual(sign*angles.knee, 0)
                    self.assertPoint(forward(leg, angles), body)


if __name__ == "__main__":
    unittest.main()
