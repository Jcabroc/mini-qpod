import math
import unittest

from tools.ik_simulator.kinematics import (
    COXA, FEMUR, TIBIA, MOUNTS, inverse, leg_to_body,
)
from tools.ik_simulator.validation import validate_target


class ValidationTests(unittest.TestCase):
    def report(self, point, sign=-1, leg="R1"):
        return validate_target(leg, leg_to_body(leg, point), sign)

    def test_maximum_extension(self):
        for leg in MOUNTS:
            for sign in (-1, 1):
                result = self.report((COXA+FEMUR+TIBIA, 0, 0), sign, leg)
                self.assertTrue(result.ik_reachable)
                self.assertIn("FULL_EXTENSION_SINGULARITY", result.warnings)
                self.assertIsNotNone(result.angles)

    def test_folded_knee(self):
        for sign in (-1, 1):
            result = self.report((COXA+abs(FEMUR-TIBIA), 0, 0), sign)
            self.assertTrue(result.ik_reachable)
            self.assertAlmostEqual(abs(result.angles.knee), math.pi)
            self.assertIn("FOLDED_KNEE_SINGULARITY", result.warnings)

    def test_unreachable_inner_and_outer(self):
        for d in (abs(FEMUR-TIBIA)-0.01, FEMUR+TIBIA+0.01):
            result = self.report((COXA+d, 0, 0))
            self.assertFalse(result.ik_reachable)
            self.assertEqual(result.checks[0].status, "fail")
            self.assertIsNone(result.angles)

    def test_valid_geometry_is_not_mechanical_approval(self):
        point = leg_to_body("R1", (COXA+FEMUR, 0, -TIBIA))
        result = validate_target("R1", point)
        self.assertEqual(result.angles, inverse("R1", point))
        self.assertTrue(result.ik_reachable)
        self.assertEqual(result.warnings, ("MECHANICAL_ENVELOPE_UNVALIDATED",))
        self.assertEqual([c.category for c in result.checks], [
            "ik", "mechanical_joints", "electrical_channels",
            "chassis_collision", "neighbor_collision",
        ])
        self.assertTrue(all(c.status == "pending" for c in result.checks[1:]))

    def test_cad_annotations_do_not_block(self):
        for degrees in (34, 45, 56, 57, -57):
            yaw = math.radians(degrees)
            result = self.report((100*math.cos(yaw), 100*math.sin(yaw), -80))
            self.assertTrue(result.ik_reachable)
            self.assertIsNotNone(result.angles)

    def test_yaw_axis_and_invalid_input(self):
        result = self.report((0, 0, -100))
        self.assertEqual(result.checks[0].status, "indeterminate")
        self.assertIsNone(result.angles)
        with self.assertRaises(ValueError):
            self.report((float("nan"), 0, 0))


if __name__ == "__main__":
    unittest.main()
