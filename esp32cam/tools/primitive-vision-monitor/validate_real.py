import time
import serial
from parser import parse_line

samples = []
raw = []
with serial.Serial('COM25', 115200, timeout=0.5) as port:
    port.dtr = False
    port.rts = True
    time.sleep(0.1)
    port.rts = False
    until = time.monotonic() + 8
    while time.monotonic() < until:
        line = port.readline().decode('utf-8', errors='replace').strip()
        if line:
            raw.append(line)
            sample = parse_line(line)
            if sample:
                samples.append(sample)
print(f'LINES={len(raw)} MOTION_SAMPLES={len(samples)} STATES={sorted({s.state for s in samples})}')
for line in raw[-12:]: print(line)
