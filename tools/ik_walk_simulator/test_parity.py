"""Cross-language vectors against the firmware IK/mapping and gait header."""
import importlib.util, json, pathlib, shutil, subprocess, sys, tempfile, unittest
from tools.ik_walk_simulator.walk import READY_FEET, Walker, solve, ready_frame, gait_swing, gait_phase_at, balance_translation

ROOT=pathlib.Path(__file__).resolve().parents[2]

def compiler_command():
    """Return a C++ command, including the bundled Zig fallback when available."""
    native = shutil.which("g++") or shutil.which("clang++")
    if native:
        return [native]
    if importlib.util.find_spec("ziglang"):
        # The ziglang wheel provides a host C++ compiler without requiring a
        # system-wide g++/clang++ installation.
        return [sys.executable, "-m", "ziglang", "c++", "-Wno-nullability-completeness"]
    return None

class FirmwareParity(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.compiler=compiler_command()
        if not cls.compiler: raise unittest.SkipTest("g++/clang++ not installed; run test_parity.py on a host with a C++ compiler")
        cls.tmp=tempfile.TemporaryDirectory(); cls.exe=pathlib.Path(cls.tmp.name)/"parity.exe"
        subprocess.run([*cls.compiler,"-std=c++11",str(ROOT/"mini_qpod_ik_walk_mvp/tests/parity.cpp"),"-o",str(cls.exe)],check=True)
    @classmethod
    def tearDownClass(cls):
        if hasattr(cls,"tmp"): cls.tmp.cleanup()
    def test_ready_pose_targets_and_all_joint_commands(self):
        # Exact firmware READY feet, plus trajectory points visited by lift/swing/drop.
        for i,base in enumerate(READY_FEET):
            for delta in ((0,0,0),(0,0,10),(0,10,10),(0,10,0)):
                p=tuple(base[j]+delta[j] for j in range(3))
                py=solve(i,p)
                out=subprocess.check_output([str(self.exe),str(i),*(f"{v:.9f}" for v in p)],text=True).split()
                if out[0]=="IK_REJECT": self.assertIsNone(py.angles); continue
                if out[0]=="LIMIT_REJECT": self.assertIsNone(py.electrical); continue
                self.assertEqual(out[0],"OK")
                vals=list(map(float,out[1:])); expected=[py.angles.yaw,py.angles.femur,py.angles.knee,*py.electrical]
                for actual,want in zip(vals,expected): self.assertAlmostEqual(actual,want,delta=.0002)
    def test_ready_transition_matches_cpp_full_path(self):
        for i,target in enumerate(READY_FEET):
            start=(target[0]+3,target[1]-4,target[2]+2)
            for t in (0.,.1,.25,.5,.9,1.):
                out=subprocess.check_output([str(self.exe),"Q",str(i),*(str(v) for v in start),str(t)],text=True).split()
                self.assertEqual(out[0],"READY")
                for actual,want in zip(map(float,out[1:]),ready_frame(start,target,t)):self.assertAlmostEqual(actual,want,delta=.0002)

    def test_shared_firmware_trajectory_phases(self):
        start=(12.25,34.5,-81.75)
        walker=Walker(); walker.start=start
        for phase in range(4):
            for direction,turn in ((1,0),(-1,0),(1,1),(1,-1)):
                out=subprocess.check_output([str(self.exe),"T",*(f"{v:.6f}" for v in start),str(phase),str(direction),str(turn)],text=True).split()
                self.assertEqual(out[0],"TARGET")
                walker.phase=phase; walker.direction=("left" if turn==1 else "right" if turn==-1 else "backward" if direction==-1 else "forward")
                py=walker.target()
                for actual,want in zip(map(float,out[1:]),py): self.assertAlmostEqual(actual,want,delta=.00002)
    def test_smooth_gait_curve_matches_firmware(self):
        start=(11.,-20.,-90.); touchdown=(11.,20.,-90.)
        for t in (0.,.1,.25,.5,.75,.9,1.):
            out=subprocess.check_output([str(self.exe),"G",*(str(v) for v in (*start,*touchdown,t,24.))],text=True).split()
            self.assertEqual(out[0],"GAIT")
            for actual,want in zip(map(float,out[1:]),gait_swing(start,touchdown,t,24.)):
                self.assertAlmostEqual(actual,want,delta=.00002)
    def test_gait_phase_and_support_match_firmware(self):
        for phase in (0.,.12,.5,.62,.9,.99):
            for offset in (0.,.25,.5,.75):
                p=gait_phase_at(phase,offset)
                out=subprocess.check_output([str(self.exe),"P",str(phase),str(offset),".62"],text=True).split()
                self.assertAlmostEqual(float(out[1]),p,delta=.00002)
                self.assertEqual(int(out[2]),int(p<.62))
    def test_body_balance_translation_matches_firmware_core(self):
        cases=(
            ([(0.,2.),(40.,0.),(5.,-35.)],(0.,0.),2.),
            ([(-40.,40.),(40.,40.),(40.,-40.),(-40.,-40.)],(55.,4.),2.),
            ([(0.,0.),(20.,0.),(10.,20.)],(10.,5.),1.),
        )
        for points,com,margin in cases:
            py=balance_translation(points,com,margin)
            args=[str(self.exe),"B",str(len(points)),*(f"{v:.7f}" for p in points for v in p),f"{com[0]:.7f}",f"{com[1]:.7f}",f"{margin:.7f}"]
            out=subprocess.check_output(args,text=True).split();self.assertEqual(out[0],"BALANCE")
            for actual,want in zip(map(float,out[1:3]),py):self.assertAlmostEqual(actual,want,delta=.0002)
            if len(points)>=3:self.assertGreaterEqual(float(out[3]),min(margin,2.)-.001)
    def test_control_lite_binary_packet_size_and_direction_mapping(self):
        cases=((0,-127,2,1.,0.,1),(0,127,2,-1.,0.,1),(127,0,2,0.,1.,1),(-127,0,2,0.,-1.,1),(70,-90,2,0.6452,0.4525,1),(0,0,2,0.,0.,1),(0,-127,1,0.,0.,1),(0,-127,2,0.,0.,0))
        for rx,ry,mode,expected_f,expected_t,flags in cases:
            out=subprocess.check_output([str(self.exe),"R",str(rx),str(ry),str(mode),str(flags)],text=True).split()
            self.assertEqual(out[0],"RADIO");self.assertEqual(int(out[1]),9);self.assertAlmostEqual(float(out[2]),expected_f,delta=.001);self.assertAlmostEqual(float(out[3]),expected_t,delta=.001)
    def test_exported_json_parameters_match_nano_table(self):
        data=json.loads((ROOT/"tools/ik_walk_simulator/gait_defaults.json").read_text(encoding="utf-8"))
        for index,(key,p) in enumerate(data["presets"].items()):
            out=subprocess.check_output([str(self.exe),"D",str(index)],text=True).split()
            self.assertEqual(out[0],"PRESET");self.assertEqual(out[1],key)
            expected=[p["stride"],p["lift"],p["turn_degrees"],p["period"],p["duty"],p["start_phase"],*p["phase_offsets"],data["com_estimate"]["x_mm"],data["com_estimate"]["y_mm"],data["com_estimate"]["balance_margin_mm"],p.get("body_height_mm",0),p.get("leg_opening_mm",0)]
            for actual,want in zip(map(float,out[2:]),expected):self.assertAlmostEqual(actual,want,delta=.00001)
    def test_complete_walk_cycles_match_cpp_frame_for_frame(self):
        preset={**json.loads((ROOT/"tools/ik_walk_simulator/gait_defaults.json").read_text(encoding="utf-8"))["presets"]["WALK"],"phase_offsets":tuple(json.loads((ROOT/"tools/ik_walk_simulator/gait_defaults.json").read_text(encoding="utf-8"))["presets"]["WALK"]["phase_offsets"])}
        cases=((.75,0.,1.5,3.,None,0.,0.,1.5,3.),(-.6,0.,0.,0.,None,0.,0.,0.,0.),(.55,.4,1.,2.,None,0.,0.,1.,2.),(.7,0.,0.,0.,100,-.45,.35,1.5,3.))
        frames=round(preset["period"]*3/.04)
        for forward,turn,height,opening,change_at,forward2,turn2,height2,opening2 in cases:
            w=Walker();w.start_cycle(stride=8.,lift=4.,turn_degrees=preset["turn_degrees"],period=preset["period"],duty=preset["duty"],phase_offsets=preset["phase_offsets"],start_phase=0.,body_height_mm=height,leg_opening_mm=opening)
            w.set_drive(forward,turn)
            args=[str(self.exe),"C",str(frames),".04","8","4",str(preset["turn_degrees"]),str(preset["period"]),str(preset["duty"]),*(str(x) for x in preset["phase_offsets"]),str(forward),str(turn),str(height),str(opening),"0","0","2"]
            if change_at is not None:args.extend((str(change_at),str(forward2),str(turn2),str(height2),str(opening2),"0","0","2"))
            rows=subprocess.check_output(args,text=True).splitlines();self.assertEqual(len(rows),frames)
            for frame,line in enumerate(rows):
                if change_at is not None and frame>=change_at:
                    forward_now,turn_now,height_now,opening_now=forward2,turn2,height2,opening2
                    w.set_drive(forward_now,turn_now);w.set_pose_targets(height_now,opening_now)
                w.advance(.04);out=line.split();self.assertEqual(out[0],"FRAME");self.assertEqual(int(out[1]),frame)
                values=list(map(float,out[2:]))
                actual_state=(w.gait_phase,w.body[0],w.body[1],w.body_z,w.body_yaw,w.forward_speed,w.turn_speed,w.spread)
                for j,(actual,want) in enumerate(zip(values[:8],actual_state)):self.assertAlmostEqual(actual,want,delta=.003,msg=f"{forward=}, {turn=}, {height=}, {opening=}, frame={frame}, state={j}, {actual=}, {want=}")
                mask=int(values[8]);py_mask=sum(1<<i for i,o in enumerate(w.phase_offsets) if (w.gait_phase+o)%1.>=w.duty-1e-6)
                self.assertEqual(mask,py_mask,msg=f"{forward=}, {turn=}, {height=}, {opening=}, frame={frame}, phase={w.gait_phase}");self.assertLessEqual(mask.bit_count(),1)
                expected_world=[w._to_world(p) for p in w.feet]
                for i,point in enumerate(expected_world):
                    base=9+i*10
                    for actual,want in zip(values[base:base+3],point):self.assertAlmostEqual(actual,want,delta=.006,msg=f"frame {frame}, foot {i}")
                    result=solve(i,w.feet[i]);self.assertEqual(int(values[base+3]),1 if result.electrical else 0,msg=f"frame {frame}, leg {i}: {result.reason}")
                    if result.electrical:
                        expected=(result.angles.yaw,result.angles.femur,result.angles.knee,*result.electrical)
                        for actual,want in zip(values[base+4:base+10],expected):self.assertAlmostEqual(actual,want,delta=.002,msg=f"frame {frame}, leg {i} joint target")
                self.assertAlmostEqual(values[-2],w.minimum_margin_before or values[-2],delta=.006)
                self.assertAlmostEqual(values[-1],w.minimum_margin_after or values[-1],delta=.006)

if __name__=="__main__":unittest.main()
