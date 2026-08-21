"""Protocol helpers shared by the provisional Mini Q-POD calibrator.

This module deliberately models only the calibration subset of the Nano protocol.
It contains no POD Station concepts, persistence layer, or web dependencies.
"""

from __future__ import annotations

from dataclasses import dataclass
import math
from typing import Dict, Iterable

SERVO_COUNT = 13
BAUDRATE = 115200
ELECTRICAL_CENTER_DEG = 90.0

SERVO_NAMES = (
    "L1_COXA", "L1_FEMUR", "L1_TIBIA",
    "R1_COXA", "R1_FEMUR", "R1_TIBIA",
    "L2_COXA", "L2_FEMUR", "L2_TIBIA",
    "R2_COXA", "R2_FEMUR", "R2_TIBIA", "CUELLO",
)

# Deliberately excludes STAND, LEG, UNLOCK_WALK, WALK, and DEFAULTS.
ALLOWED_COMMANDS = frozenset(
    {"OFF", "CALIB", "SELECT", "ENABLE", "CENTER", "SERVO", "LIMITS", "SAVE", "LOAD", "STATUS", "IMU", "CONFIG", "PING", "HELP"}
)


def command_name(line: str) -> str:
    tokens = line.strip().split()
    return tokens[0].upper() if tokens else ""


def command_is_allowed(line: str) -> bool:
    return command_name(line) in ALLOWED_COMMANDS


def attempt_safe_off(port: object) -> bool:
    """Best-effort normal-shutdown OFF without opening or probing a port."""
    try:
        port.write(b"OFF\n")  # type: ignore[attr-defined]
    except Exception:
        return False
    return True


def parse_key_values(line: str) -> Dict[str, str]:
    """Parse protocol fields of the form ``PREFIX key=value ...`` strictly."""
    parts = line.strip().split()
    if len(parts) < 2:
        raise ValueError("respuesta sin campos")
    result: Dict[str, str] = {}
    for part in parts[1:]:
        if "=" not in part:
            raise ValueError(f"campo inválido: {part}")
        key, value = part.split("=", 1)
        if not key or not value or key in result:
            raise ValueError(f"campo inválido: {part}")
        result[key] = value
    return result


def parse_int(value: str, *, low: int | None = None, high: int | None = None) -> int:
    if not value or value.strip() != value:
        raise ValueError("entero inválido")
    try:
        parsed = int(value, 10)
    except ValueError as exc:
        raise ValueError("entero inválido") from exc
    if str(parsed) != value and not (value.startswith("+") and str(parsed) == value[1:]):
        raise ValueError("entero inválido")
    if low is not None and parsed < low or high is not None and parsed > high:
        raise ValueError("entero fuera de rango")
    return parsed


def parse_float(value: str, *, low: float | None = None, high: float | None = None) -> float:
    try:
        parsed = float(value)
    except (TypeError, ValueError) as exc:
        raise ValueError("ángulo inválido") from exc
    if not math.isfinite(parsed):
        raise ValueError("ángulo no finito")
    if low is not None and parsed < low or high is not None and parsed > high:
        raise ValueError("ángulo fuera de rango")
    return parsed


@dataclass
class ServoConfig:
    channel: int
    minimum: int
    center: int
    maximum: int
    direction: int
    margin: int = 5

    @property
    def safe_min(self) -> int:
        return self.minimum + self.margin

    @property
    def safe_max(self) -> int:
        return self.maximum - self.margin


def default_configs() -> list[ServoConfig]:
    values: Iterable[tuple[int, int, int, int]] = (
        (50, 90, 150, 1), (10, 95, 180, 1), (0, 100, 180, -1),
        (30, 90, 130, 1), (0, 90, 170, -1), (0, 90, 180, -1),
        (40, 90, 140, 1), (10, 100, 180, 1), (5, 100, 180, -1),
        (50, 90, 130, -1), (15, 90, 180, -1), (0, 90, 180, -1),
        (45, 90, 135, 1),
    )
    return [ServoConfig(index, minimum, center, maximum, direction) for index, (minimum, center, maximum, direction) in enumerate(values)]
