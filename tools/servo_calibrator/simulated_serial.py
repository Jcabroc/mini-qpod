"""In-memory serial endpoint used by tests and the GUI's SIMULATED option."""

from __future__ import annotations

from collections import deque
import threading
import time

from protocol import SERVO_COUNT, ServoConfig, default_configs, parse_float, parse_int


class SimulatedSerial:
    """Small deterministic model of the approved calibration protocol.

    It never accesses Windows COM ports and has no hardware side effects.
    """

    HOST_TIMEOUT_S = 1.0

    def __init__(self, *_, timeout: float = 0.1, clock=time.monotonic, **__):
        self.timeout = timeout
        self._clock = clock
        self.is_open = True
        self._incoming: deque[bytes] = deque(
            [b"MINI Q-POD MVP v0.3 SIMULATED\n", b"Arranque seguro: servos OFF.\n"]
        )
        self._lock = threading.Lock()
        self.written_lines: list[str] = []
        self.mode = "SAFE_OFF"
        self.selected = -1
        self.active = -1
        self.abort_cause = "NONE"
        self.last_host_activity = self._clock()
        self.imu_healthy = True
        self.imu_roll = 0.0
        self.imu_pitch = 0.0
        self.configs: list[ServoConfig] = default_configs()

    @property
    def in_waiting(self) -> int:
        with self._lock:
            return sum(len(item) for item in self._incoming)

    def close(self) -> None:
        self.is_open = False

    def reset_input_buffer(self) -> None:
        with self._lock:
            self._incoming.clear()

    def write(self, data: bytes) -> int:
        if not self.is_open:
            raise OSError("puerto simulado cerrado")
        for line in data.decode("ascii", errors="strict").splitlines():
            if line.strip():
                self.written_lines.append(line.strip())
                self._emit_many(self._handle(line.strip()))
        return len(data)

    def readline(self) -> bytes:
        deadline = time.monotonic() + self.timeout
        while self.is_open and time.monotonic() < deadline:
            self.tick()
            with self._lock:
                if self._incoming:
                    return self._incoming.popleft()
            time.sleep(0.001)
        return b""

    def inject_abort(self) -> None:
        self._abort("IMU_UNSAFE")

    def tick(self) -> None:
        """Advance watchdog handling without requiring a new host command."""
        if self.mode == "CALIBRATION" and self.active >= 0 and self._clock() - self.last_host_activity > self.HOST_TIMEOUT_S:
            self._abort("HOST_TIMEOUT")

    def _emit(self, line: str) -> None:
        with self._lock:
            self._incoming.append((line + "\n").encode("ascii"))

    def _emit_many(self, lines: list[str]) -> None:
        for line in lines:
            self._emit(line)

    def _safe(self) -> bool:
        return self.imu_healthy and abs(self.imu_roll) <= 12 and abs(self.imu_pitch) <= 12

    def _refresh_host_activity(self) -> None:
        if self.mode == "CALIBRATION":
            self.last_host_activity = self._clock()

    def _abort(self, cause: str) -> None:
        self.mode, self.selected, self.active, self.abort_cause = "SAFE_OFF", -1, -1, cause
        self._emit(f"[ABORT] {cause}")

    def _config_lines(self) -> list[str]:
        lines = ["CONFIG margin=5"]
        for config in self.configs:
            lines.append(
                f"CONFIG ch={config.channel} channel={config.channel} min={config.minimum} "
                f"center={config.center} max={config.maximum} direction={config.direction} "
                f"margin={config.margin} safeMin={config.safe_min} safeMax={config.safe_max}"
            )
        return lines

    def _handle(self, line: str) -> list[str]:
        self.tick()
        parts = line.split()
        command = parts[0].upper()
        args = parts[1:]
        try:
            if command == "OFF" and not args:
                self.mode, self.selected, self.active = "SAFE_OFF", -1, -1
                return ["[OFF] ch=-1 PWM=OFF mode=SAFE_OFF"]
            if command == "STATUS" and not args:
                return [f"STATUS mode={self.mode} servos={int(self.active >= 0)} selected={self.selected} active={self.active} imu={int(self.imu_healthy)} abort={self.abort_cause} phase=0 leg=0"]
            if command == "IMU" and not args:
                return [f"IMU enabled=1 healthy={int(self.imu_healthy)} roll={self.imu_roll:.2f} pitch={self.imu_pitch:.2f} calibrationTiltAbort=1"]
            if command == "CONFIG" and not args:
                return self._config_lines()
            if command == "CALIB" and not args:
                if not self._safe():
                    return ["[ERR] CALIB bloqueado: IMU perdida o inclinacion insegura."]
                self.mode, self.selected, self.active, self.abort_cause = "CALIBRATION", -1, -1, "NONE"
                self._refresh_host_activity()
                return ["[CALIB] ch=-1 PWM=OFF seleccione un canal con SELECT."]
            if command == "SELECT" and len(args) == 1:
                channel = parse_int(args[0], low=0, high=SERVO_COUNT - 1)
                if self.mode != "CALIBRATION":
                    return ["[ERR] Use CALIB antes de SELECT."]
                self.selected, self.active = channel, -1
                self._refresh_host_activity()
                return [f"[SELECT] ch={channel} PWM=OFF"]
            if command == "ENABLE" and len(args) == 2:
                channel = parse_int(args[0], low=0, high=SERVO_COUNT - 1)
                angle = parse_float(args[1])
                if self.mode != "CALIBRATION" or self.selected != channel:
                    return ["[ERR] Seleccione primero ese canal con SELECT."]
                if not self._safe():
                    return ["[ERR] ENABLE bloqueado: IMU perdida o inclinacion insegura."]
                config = self.configs[channel]
                effective = min(max(angle, config.safe_min), config.safe_max)
                self.active = channel
                self._refresh_host_activity()
                return [f"[ENABLE] ch={channel} angle={effective:.2f}"]
            if command in {"CENTER", "SERVO"}:
                expected = 1 if command == "CENTER" else 2
                if len(args) != expected:
                    return [f"[ERR] Uso: {command} <0..12>{'' if command == 'CENTER' else ' <angulo>'}."]
                channel = parse_int(args[0], low=0, high=SERVO_COUNT - 1)
                if self.mode != "CALIBRATION" or self.selected != channel or self.active != channel:
                    return [f"[ERR] {command} exige canal seleccionado y habilitado."]
                config = self.configs[channel]
                requested = config.center if command == "CENTER" else parse_float(args[1])
                effective = min(max(requested, config.safe_min), config.safe_max)
                self._refresh_host_activity()
                return [f"[{command}] ch={channel} angle={effective:.2f}"]
            if command == "LIMITS" and len(args) == 4:
                channel = parse_int(args[0], low=0, high=SERVO_COUNT - 1)
                minimum, center, maximum = (parse_int(arg) for arg in args[1:])
                if self.mode != "CALIBRATION" or self.active >= 0:
                    return ["[ERR] LIMITS exige CALIB con PWM apagado."]
                if not (0 <= minimum < center < maximum <= 180 and maximum - minimum > 10):
                    return ["[ERR] Limites incoherentes; no aplicados."]
                config = self.configs[channel]
                config.minimum, config.center, config.maximum = minimum, center, maximum
                self._refresh_host_activity()
                return [f"[LIMITS] ch={channel} min={minimum} center={center} max={maximum} safeMin={config.safe_min} safeMax={config.safe_max}"]
            if command == "PING" and not args:
                self._refresh_host_activity()
                return [f"PING mode={self.mode} active={self.active}"]
            if command in {"SAVE", "LOAD"} and not args:
                if self.mode != "SAFE_OFF":
                    return [f"[ERR] Ejecute OFF antes de {command}."]
                return [f"[{command}] EEPROM OK."]
        except ValueError:
            return [f"[ERR] Argumentos invalidos para {command}."]
        return ["[ERR] Comando desconocido. Use HELP."]
