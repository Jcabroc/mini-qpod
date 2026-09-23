"""Two-minute read-only serial stability run; never sends ARM or MOVE."""
import time
from pathlib import Path
import serial
from console import ServoMappingConsole

port = serial.Serial("COM9", 115200, timeout=.1)
console = ServoMappingConsole(port, Path("C:/Users/José M Caballero/Desktop/jRobot/Codex/hardware_backups/mini-qpod/servo_mapping_console_stability.log"))
console.start()
started = time.monotonic(); next_status = started + 10; status_turn = False
try:
    while time.monotonic() - started < 120:
        now = time.monotonic()
        if now >= next_status:
            console.send("IMU" if status_turn else "STATUS")
            status_turn = not status_turn; next_status += 10
        time.sleep(.05)
finally:
    console.send("X")
    console.close()
print("stability_seconds=120")
print(f"ping_sent={console.ping_sent}")
print(f"anomalies={len(console.anomalies)}")
