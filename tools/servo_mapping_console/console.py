"""Minimal safe console for nano_servo_mapping (no EEPROM or motion modes)."""
from __future__ import annotations
import argparse, datetime as dt, re, sys, threading, time
from pathlib import Path
from typing import Optional, TextIO

ALLOWED = {"HELP", "STATUS", "CONFIG", "IMU", "PING", "SELECT", "ARM", "MOVE", "OFF", "X"}
COMMAND_RE = re.compile(r"^(HELP|STATUS|CONFIG|IMU|PING|OFF|X|SELECT\s+\d{1,2}|ARM\s+\d{1,3}\s+[-+]?\d+(?:\.\d+)?|MOVE\s+\d{1,3}\s+[-+]?\d+(?:\.\d+)?)$")

class ServoMappingConsole:
    def __init__(self, serial_port, log_path: Path, *, clock=time.monotonic, wall_clock=dt.datetime.now):
        self.serial = serial_port
        self.log_path = Path(log_path)
        self.clock, self.wall_clock = clock, wall_clock
        self.lock = threading.Lock()
        self.stop_event = threading.Event()
        self.heartbeat_thread: Optional[threading.Thread] = None
        self.reader_thread: Optional[threading.Thread] = None
        self.log: Optional[TextIO] = None
        self.last_status = ""
        self.imu_healthy = True
        self.armed_channel: Optional[int] = None
        self.ready_event = threading.Event()
        self.config_event = threading.Event()
        self.config_channels = set()
        self.anomalies = []
        self.ping_sent = 0

    def _line(self, line: str) -> str:
        stamp = self.wall_clock().isoformat(timespec="milliseconds")
        rendered = f"{stamp} {line}"
        try:
            print(rendered, flush=True)
        except UnicodeEncodeError:
            sys.stdout.buffer.write((rendered + "\n").encode("utf-8", "replace")); sys.stdout.flush()
        if self.log:
            self.log.write(rendered + "\n"); self.log.flush()
        if line.startswith("STATE "):
            self.last_status = line
        if "imu=0" in line or "IMU healthy=0" in line:
            self.imu_healthy = False
            self.emergency()
        if line.startswith("PWM_requested=OFF"):
            self.ready_event.set()
        if "\ufffd" in line or (line.startswith("REPLY cmd=") and "result=0" not in line):
            self.anomalies.append(line)
        m = re.match(r"CONFIG ch=(\d+) min=", line)
        if m:
            self.config_channels.add(int(m.group(1)))
            if len(self.config_channels) == 13:
                self.config_event.set()
        return line

    def send(self, command: str) -> None:
        command = command.strip().upper()
        if command == "X":
            payload = "X\n"
        elif not self.validate(command):
            raise ValueError("Comando no permitido o sintaxis inválida")
        else:
            payload = command + "\n"
        with self.lock:
            # The Nano services USB UART cooperatively while also servicing
            # SoftwareSerial IMU input. Byte pacing prevents dropped characters.
            encoded = payload.encode("ascii")
            if hasattr(self.serial, "writes"):  # deterministic simulated transport
                self.serial.write(encoded)
            else:
                for byte in encoded:
                    self.serial.write(bytes((byte,)))
                    time.sleep(0.002)
            self.serial.flush()
            if command == "PING": self.ping_sent += 1

    @staticmethod
    def validate(command: str) -> bool:
        if not command or command.split()[0] not in ALLOWED:
            return False
        if not COMMAND_RE.fullmatch(command): return False
        if command.split()[0] in {"SELECT", "ARM", "MOVE"} and not 0 <= int(command.split()[1]) <= 12: return False
        return True

    def emergency(self) -> None:
        with self.lock:
            if getattr(self.serial, "is_open", True):
                self.serial.write(b"X\n"); self.serial.flush()

    def _reader(self) -> None:
        while not self.stop_event.is_set():
            try:
                raw = self.serial.readline()
                if raw:
                    self._line(raw.decode("utf-8", "replace").rstrip("\r\n"))
            except Exception as exc:
                self._line(f"[SERIAL ERROR] {exc}")
                try: self.emergency()
                except Exception: pass
                return

    def _heartbeat(self) -> None:
        while not self.stop_event.wait(0.25):
            try: self.send("PING")
            except Exception as exc:
                self._line(f"[HEARTBEAT ERROR] {exc}")
                try: self.emergency()
                except Exception: pass
                return

    def start(self) -> None:
        self.log_path.parent.mkdir(parents=True, exist_ok=True)
        self.log = self.log_path.open("a", encoding="utf-8", buffering=1)
        self.reader_thread = threading.Thread(target=self._reader, daemon=True); self.reader_thread.start()
        # Opening a Nano port toggles reset; wait for the actual identity/off banner,
        # rather than guessing a bootloader delay.
        self.ready_event.wait(3.0)
        time.sleep(0.15)
        for command in ("STATUS", "IMU", "CONFIG"):
            self.send(command)
        if not self.config_event.wait(3.0):
            raise RuntimeError("CONFIG inicial incompleto: no se recibieron las 13 líneas")
        self.heartbeat_thread = threading.Thread(target=self._heartbeat, daemon=True); self.heartbeat_thread.start()

    def arm_ch0(self, angle: float, confirm) -> None:
        if angle != 90.0: raise ValueError("ARM local bloqueado: CH0 solo admite 90 grados en esta fase")
        if not self.imu_healthy: raise ValueError("ARM bloqueado: IMU no saludable")
        prompt = "ARM CH0 90.00; rango seguro 25-110; 310 cuentas; 1512.80 us nominales. ¿Confirmar? [y/N] "
        if not confirm(prompt): raise PermissionError("ARM no confirmado")
        self.send("ARM 0 90")
        self.armed_channel = 0

    def move_ch0(self, angle: float) -> None:
        if self.armed_channel != 0: raise ValueError("MOVE bloqueado: CH0 no está armado")
        if not 89.0 <= angle <= 91.0: raise ValueError("MOVE local bloqueado: intervalo 89-91 grados")
        self.send(f"MOVE 0 {angle:g}")

    def close(self) -> None:
        self.stop_event.set()
        try: self.emergency()
        except Exception: pass
        for t in (self.heartbeat_thread, self.reader_thread):
            if t: t.join(timeout=0.5)
        try: self.serial.close()
        finally:
            if self.log: self.log.close(); self.log = None

def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="Mini Q-POD servo mapping console")
    ap.add_argument("--port", default="COM9"); ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--select", type=int, default=None, choices=range(13), help="seleccionar canal al completar el sondeo")
    ap.add_argument("--log", type=Path, default=Path("local_backups/servo_mapping_console.log"))
    args = ap.parse_args(argv)
    try:
        import serial
        ser = serial.Serial(args.port, args.baud, timeout=0.1)
    except Exception as exc:
        print(f"No se pudo abrir {args.port}: {exc}", file=sys.stderr); return 2
    console = ServoMappingConsole(ser, args.log)
    try:
        console.start()
        print(f"MINI Q-POD SERVO MAPPING\n{args.port} conectado")
        if args.select is not None:
            console.send(f"SELECT {args.select}"); time.sleep(.25); console.send("STATUS"); time.sleep(.25); console.send("IMU")
        print("X=apagado de emergencia. Ctrl+C=cerrar.")
        while True:
            text = input("qpod> ").strip()
            if not text: continue
            if text.upper() == "ARM 0 90": console.arm_ch0(90.0, lambda p: input(p).strip().lower() == "y")
            elif text.upper().startswith("MOVE 0 "): console.move_ch0(float(text.split()[2]))
            else: console.send(text)
    except KeyboardInterrupt: pass
    except Exception as exc: print(f"[ERROR] {exc}", file=sys.stderr)
    finally: console.close()
    return 0

if __name__ == "__main__": raise SystemExit(main())
