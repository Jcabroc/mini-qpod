import io, tempfile, time, unittest
from pathlib import Path
try:
    from .console import ServoMappingConsole
except ImportError:
    from console import ServoMappingConsole

class FakeSerial:
    def __init__(self): self.writes=[]; self.is_open=True; self.lines=[]
    def write(self,b): self.writes.append(b); return len(b)
    def flush(self): pass
    def readline(self): return self.lines.pop(0) if self.lines else b''
    def close(self): self.is_open=False

class ConsoleTests(unittest.TestCase):
    def make(self):
        self.tmp=tempfile.TemporaryDirectory(); self.ser=FakeSerial()
        self.ser.lines=[b'MINI Q-POD SERVO MAPPING version=1.0.0 build=test\n',b'PWM_requested=OFF prescale=121\n']
        self.ser.lines += [f'CONFIG ch={i} min=0 center=90 max=180 direction=1 margin=5 safeMin=5 safeMax=175\n'.encode() for i in range(13)]
        return ServoMappingConsole(self.ser, Path(self.tmp.name)/'log.txt')
    def test_whitelist_and_arm_confirmation(self):
        c=self.make(); self.assertTrue(c.validate('STATUS')); self.assertFalse(c.validate('WALK')); self.assertFalse(c.validate('SAVE'))
        with self.assertRaises(ValueError): c.arm_ch0(91, lambda _: True)
        with self.assertRaises(PermissionError): c.arm_ch0(90, lambda _: False)
        c.imu_healthy=True; c.arm_ch0(90, lambda _: True); self.assertEqual(self.ser.writes[-1],b'ARM 0 90\n')
    def test_move_limits_and_no_interleaving(self):
        c=self.make(); c.armed_channel=0
        with self.assertRaises(ValueError): c.move_ch0(92)
        c.move_ch0(89); self.assertEqual(self.ser.writes[-1],b'MOVE 0 89\n')
        c.send('STATUS'); self.assertTrue(all(x in (b'MOVE 0 89\n',b'STATUS\n') for x in self.ser.writes))
    def test_heartbeat_and_close_send_lines(self):
        c=self.make(); c.start(); time.sleep(.27); c.close()
        self.assertIn(b'PING\n',self.ser.writes); self.assertEqual(self.ser.writes[-1],b'X\n')
        self.assertTrue((Path(c.log_path)).exists())
    def test_imu_loss_emergency(self):
        c=self.make(); c._line('IMU healthy=0 roll=13 pitch=0'); self.assertEqual(self.ser.writes[-1],b'X\n')
    def test_reject_numbers_and_commands(self):
        for cmd in ('MOVE 0 nan','SELECT 13','CONFIG extra','EXPAND','WALK','DEFAULTS'):
            self.assertFalse(ServoMappingConsole.validate(cmd))
    def test_emergency_is_atomic_line(self):
        c=self.make(); c.emergency(); self.assertEqual(self.ser.writes,[b'X\n'])

if __name__=='__main__': unittest.main()
