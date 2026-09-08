import json
import unittest

from tools.ik_pose_lab.pose import Pose, PRESETS, dumps, loads
from tools.ik_simulator.validation import validate_target


class PoseTests(unittest.TestCase):
    def test_roundtrip_presets_and_export(self):
        for pose in (*PRESETS.values(), Pose(100, 55, -10, 25, 1)):
            for export in (False, True):
                self.assertEqual(loads(dumps(pose, export)), pose)

    def test_export_agrees_with_shared_ik(self):
        p = Pose()
        data = json.loads(dumps(p, True))['simulation']
        self.assertFalse(data['physical_approval'])
        for leg, target in p.targets().items():
            report = validate_target(leg, target, p.knee_sign)
            self.assertEqual(data['legs'][leg]['angles']['knee'], report.angles.knee)

    def test_invalid_documents(self):
        for text in ('[]', '{}', '{', dumps(Pose()).replace('"version": 1','"version": 2')):
            with self.assertRaises(ValueError): loads(text)
        for value in (float('nan'), float('inf'), True, '80'):
            data = json.loads(dumps(Pose()))
            data['pose']['height'] = value
            with self.assertRaises(ValueError): loads(json.dumps(data))
        for kw in ({'height':-1},{'spread':-1},{'knee_sign':0}):
            with self.assertRaises(ValueError): Pose(**kw)

    def test_unreachable_export(self):
        data = json.loads(dumps(Pose(height=1000), True))
        self.assertTrue(all(v['angles'] is None for v in data['simulation']['legs'].values()))

    def test_nominal_symmetry(self):
        points = PRESETS['X_NEUTRAL'].targets()
        self.assertAlmostEqual(points['L1'][0], -points['R1'][0])
        self.assertAlmostEqual(points['L1'][1], -points['L2'][1])


if __name__ == '__main__': unittest.main()
