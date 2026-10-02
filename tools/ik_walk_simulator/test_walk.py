import unittest
from tools.ik_walk_simulator.walk import Walker, READY_FEET, GAIT_PRESETS, PATTERN_CHOICES, minimum_gait_support, gait_support_range, stride_metrics, solve, estimate_static_stability

class WalkTests(unittest.TestCase):
    def test_firmware_reference_cycle_has_zero_body_advance(self):
        w=Walker(); w.start_cycle(reference=True)
        for _ in range(500):
            if w.motion=="IDLE": break
            w.advance()
        self.assertEqual(w.motion,"IDLE")
        self.assertAlmostEqual(w.body[1],0,places=8)
        for actual,initial in zip(w.feet,READY_FEET):
                for a,b in zip(actual,initial): self.assertAlmostEqual(a,b,places=7)

    def test_ready_uses_firmware_duration_and_smooth_endpoints(self):
        w=Walker();w.feet=[(p[0]+2,p[1]-2,p[2]+1) for p in READY_FEET];w.ready()
        starts=list(w.feet)
        for _ in range(29):w.advance(.04)
        self.assertEqual(w.motion,"READYING")
        w.advance(.04)
        self.assertEqual(w.motion,"IDLE")
        for actual,target in zip(w.feet,READY_FEET):
            for a,b in zip(actual,target):self.assertAlmostEqual(a,b,places=7)

    def test_continuous_trot_advances_smoothly_with_two_legs_in_flight(self):
        w=Walker(); p=GAIT_PRESETS["Trote experimental (diagonal)"]; w.start_cycle(period=p["period"],stride=p["stride"],lift=p["lift"],duty=p["duty"],phase_offsets=p["phase_offsets"],start_phase=p["start_phase"])
        max_flight=0
        for _ in range(180):
            before=set(w.swinging); world={i:(w.feet[i][0]+w.body[0],w.feet[i][1]+w.body[1]) for i in range(4)}
            w.advance()
            after=set(w.swinging); max_flight=max(max_flight,len(after))
            self.assertGreaterEqual(4-len(after),2)
            for i in range(4):
                if i not in before and i not in after:
                    self.assertAlmostEqual(w.feet[i][0]+w.body[0],world[i][0],places=7)
                    self.assertAlmostEqual(w.feet[i][1]+w.body[1],world[i][1],places=7)
        self.assertEqual(w.motion,"GAIT")
        self.assertGreater(w.body[1],35.)
        self.assertEqual(max_flight,2)
        for foot,initial in zip(w.feet,READY_FEET):
            self.assertGreater(foot[1]+w.body[1],initial[1]+45.)

    def test_gait_phases_never_remove_all_but_one_support(self):
        for offsets in ((0.,.5,.5,0.),(0.,.5,.75,.25),(0.,.5,0.,.5)):
            self.assertGreaterEqual(minimum_gait_support(.62,offsets),2)
        with self.assertRaisesRegex(ValueError,"solo 0 patas"):
            Walker().start_cycle(phase_offsets=(0.,0.,0.,0.))

    def test_slow_walk_forward_and_turn_targets_are_reachable(self):
        for direction in ("forward","left","right"):
            w=Walker(); w.start_cycle(direction=direction)
            for _ in range(60):
                w.advance()
                for i,foot in enumerate(w.feet):
                    result=solve(i,foot)
                    self.assertIsNotNone(result.angles, f"{direction}: {result.reason}")
                    self.assertIsNotNone(result.electrical, f"{direction}: {result.reason}")

    def test_slow_walk_reverse_stops_at_existing_channel_limit(self):
        w=Walker();w.start_cycle(direction="backward")
        for _ in range(240):
            if w.motion=="IDLE":break
            w.advance(.01)
        self.assertEqual(w.limit_rejection[0],1)
        self.assertIn("CH3",w.limit_rejection[1])

    def test_presets_have_expected_flight_pairs_and_static_estimate_is_not_two_contact_stable(self):
        slow=GAIT_PRESETS["Caminata lenta"]
        for key in ("Caminata lenta","Onda secuencial"):
            p=GAIT_PRESETS[key]
            self.assertEqual(gait_support_range(p["duty"],p["phase_offsets"]),(3,4),key)
        for key in ("Trote experimental (diagonal)",):
            p={**GAIT_PRESETS[key],"start_phase":.63}; w=Walker(); w.start_cycle(**p)
            w.advance(.03)
            self.assertEqual(len(w.swinging),2,key)
            supports=[w._to_world(w.feet[i])[:2] for i in range(4) if i not in w.swinging]
            self.assertEqual(estimate_static_stability(supports,(0.,0.))["state"],"indeterminate")

    def test_stride_metrics_and_firmware_limit_abort_at_50_mm_without_clamping(self):
        metrics=stride_metrics(50.,.76,"forward")
        self.assertEqual(metrics,{"body_advance":50.,"world_swing":50.,"body_relative_swing":38.})
        p=GAIT_PRESETS["Caminata lenta"]
        w=Walker(); w.start_cycle(stride=50.,duty=p["duty"],period=p["period"],phase_offsets=p["phase_offsets"],start_phase=p["start_phase"])
        for _ in range(200):
            if w.motion=="IDLE":break
            w.advance(.01)
        self.assertEqual(w.motion,"IDLE")
        self.assertIsNotNone(w.limit_rejection)
        leg,reason=w.limit_rejection
        self.assertEqual(leg,1)
        self.assertIn("CH3",reason)
        self.assertLess(abs(w.body[1]),50.)

    def test_slow_walk_advances_net_after_startup_balance_shift(self):
        p=GAIT_PRESETS["Caminata lenta"]
        w=Walker();w.start_cycle(**p)
        for _ in range(round(p["period"]*3/.04)+1):w.advance(.04)
        self.assertIsNone(w.limit_rejection)
        self.assertAlmostEqual(w.last_cycle_advance,p["stride"],delta=1.)
        self.assertGreater(w.body[1],40.)

    def test_mid_cycle_direction_change_is_slewed_without_phase_restart(self):
        p=GAIT_PRESETS["Caminata lenta"];w=Walker();w.start_cycle(**p)
        for _ in range(40):w.advance(.04)
        phase=w.gait_phase;old=w.forward_speed
        w.set_drive(-1.,.8);w.set_pose_targets(4.,4.)
        w.advance(.04)
        self.assertAlmostEqual(w.gait_phase,phase+.01,places=6)
        self.assertLessEqual(abs(w.forward_speed-old),.080001)
        self.assertLessEqual(abs(w.turn_speed),.080001)
        self.assertGreater(w.body_z,0.)
        self.assertGreater(w.spread,0.)
        self.assertEqual(w.motion,"GAIT")

    def test_body_motion_holds_all_world_contacts(self):
        w=Walker(); world=[w._to_world(p) for p in w.feet]
        self.assertTrue(w.start_body_motion("z",6))
        for _ in range(30):w.advance(.04)
        for before,foot in zip(world,w.feet):
            after=w._to_world(foot)
            for a,b in zip(before,after):self.assertAlmostEqual(a,b,places=7)

    def test_prioritized_gaits_keep_requested_com_margin_over_full_cycle(self):
        self.assertEqual(PATTERN_CHOICES[:3],("Caminata lenta","Onda secuencial","Trote experimental (diagonal)"))
        for name in ("Caminata lenta","Onda secuencial"):
            p=GAIT_PRESETS[name];w=Walker();w.start_cycle(**p);max_airborne=0
            for _ in range(round(p["period"]*3/.04)):
                before=set(w.swinging);planted={i:w._to_world(w.feet[i]) for i in range(4) if i not in before}
                w.advance(.04);after=set(w.swinging);max_airborne=max(max_airborne,len(after))
                for i,world in planted.items():
                    if i not in after:
                        for a,b in zip(w._to_world(w.feet[i]),world):self.assertAlmostEqual(a,b,delta=1e-6)
            self.assertIsNone(w.limit_rejection)
            self.assertEqual(max_airborne,1)
            self.assertIsNotNone(w.minimum_margin_before)
            self.assertLess(w.minimum_margin_before,2.)
            self.assertGreaterEqual(w.minimum_margin_after,1.99)
            self.assertAlmostEqual(w.last_cycle_advance or 0.,p["stride"],delta=1.)
            self.assertGreater(w.body[1],40.)

    def test_selected_leg_smooth_step_keeps_other_world_contacts(self):
        for leg in range(4):
            w=Walker(); w.start_single_step(leg,stride=10,lift=8,period=4,turn_degrees=4)
            max_airborne=0;fixed={i:w._to_world(w.feet[i]) for i in range(4)}
            for _ in range(180):
                if w.motion=="IDLE":break
                w.advance()
                max_airborne=max(max_airborne,len(w.swinging))
                for i,world in fixed.items():
                    # The selected foot is allowed to establish its new contact
                    # location after touchdown; the other three remain planted.
                    if i!=leg and i not in w.swinging:
                        for actual,want in zip(w._to_world(w.feet[i]),world):self.assertAlmostEqual(actual,want,delta=1e-6)
                for i,foot in enumerate(w.feet):
                    result=solve(i,foot)
                    self.assertIsNotNone(result.electrical,f"leg {leg}: {result.reason}")
            self.assertEqual(w.motion,"IDLE");self.assertEqual(max_airborne,1)
            self.assertEqual(w.phase_offsets,GAIT_PRESETS["Caminata lenta"]["phase_offsets"])

if __name__=="__main__": unittest.main()
