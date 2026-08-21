"""Read-only backup and comparison helpers for active Mini Q-POD configuration."""

from __future__ import annotations

from datetime import datetime, timezone
import json
from pathlib import Path
from typing import Mapping

from protocol import SERVO_COUNT, SERVO_NAMES, default_configs, parse_int, parse_key_values

BACKUP_SOURCE = "ACTIVE_RUNTIME_UNKNOWN_SOURCE"
DEFAULTS_OR_IDENTICAL_EEPROM = "DEFAULTS_OR_IDENTICAL_EEPROM"
EEPROM_LIKELY = "EEPROM_LIKELY"

_REQUIRED_CONFIG_FIELDS = frozenset({"ch", "channel", "min", "center", "max", "direction", "margin", "safeMin", "safeMax"})


class BackupGate:
    """Requires a saved, explicitly confirmed snapshot before edits can persist."""

    def __init__(self) -> None:
        self.saved_path: Path | None = None
        self.confirmed = False

    @property
    def allows_modifications(self) -> bool:
        return self.saved_path is not None and self.confirmed

    def reset(self) -> None:
        self.saved_path = None
        self.confirmed = False

    def mark_saved(self, path: Path) -> None:
        self.saved_path = path
        self.confirmed = False

    def confirm(self) -> None:
        if self.saved_path is None:
            raise ValueError("no existe respaldo para confirmar")
        self.confirmed = True


class ActiveConfigCollector:
    """Strictly collects one complete CONFIG response without accepting duplicates."""

    def __init__(self) -> None:
        self.reset()

    def reset(self) -> None:
        self.global_margin: int | None = None
        self._channels: dict[int, dict[str, str]] = {}

    def accept_margin(self, line: str) -> None:
        data = parse_key_values(line)
        if set(data) != {"margin"}:
            raise ValueError("margen global CONFIG inválido")
        margin = parse_int(data["margin"], low=0, high=20)
        if self.global_margin is not None and self.global_margin != margin:
            raise ValueError("margen global CONFIG inconsistente")
        self.global_margin = margin

    def accept_channel(self, line: str) -> None:
        data = parse_key_values(line)
        if not _REQUIRED_CONFIG_FIELDS <= data.keys():
            raise ValueError("canal CONFIG incompleto")
        channel = parse_int(data["ch"], low=0, high=SERVO_COUNT - 1)
        if channel in self._channels:
            raise ValueError(f"canal CONFIG duplicado: {channel}")
        if parse_int(data["channel"], low=0, high=15) != channel:
            raise ValueError("canal lógico y PCA no coinciden")
        minimum = parse_int(data["min"], low=0, high=180)
        center = parse_int(data["center"], low=0, high=180)
        maximum = parse_int(data["max"], low=0, high=180)
        direction = parse_int(data["direction"], low=-1, high=1)
        margin = parse_int(data["margin"], low=0, high=20)
        safe_min = parse_int(data["safeMin"], low=0, high=180)
        safe_max = parse_int(data["safeMax"], low=0, high=180)
        if direction not in {-1, 1} or not minimum < center < maximum:
            raise ValueError("límites o dirección CONFIG inválidos")
        if maximum - minimum <= 2 * margin:
            raise ValueError("rango CONFIG menor que el margen")
        if safe_min != minimum + margin or safe_max != maximum - margin:
            raise ValueError("límites efectivos CONFIG inconsistentes")
        if self.global_margin is not None and margin != self.global_margin:
            raise ValueError("margen por canal no coincide con margen global")
        self._channels[channel] = data

    @property
    def complete(self) -> bool:
        return self.global_margin is not None and set(self._channels) == set(range(SERVO_COUNT))

    def rows(self) -> list[dict[str, int | str]]:
        if not self.complete:
            missing = sorted(set(range(SERVO_COUNT)) - set(self._channels))
            raise ValueError(f"CONFIG incompleta; faltan canales: {missing}")
        rows: list[dict[str, int | str]] = []
        for channel in range(SERVO_COUNT):
            data = self._channels[channel]
            rows.append({
                "channel": channel,
                "name": SERVO_NAMES[channel],
                "minimum": parse_int(data["min"]),
                "center": parse_int(data["center"]),
                "maximum": parse_int(data["max"]),
                "direction": parse_int(data["direction"]),
                "margin": parse_int(data["margin"]),
                "safe_minimum": parse_int(data["safeMin"]),
                "safe_maximum": parse_int(data["safeMax"]),
            })
        return rows


def compiled_default_rows() -> list[dict[str, int | str]]:
    rows: list[dict[str, int | str]] = []
    for config in default_configs():
        rows.append({
            "channel": config.channel,
            "name": SERVO_NAMES[config.channel],
            "minimum": config.minimum,
            "center": config.center,
            "maximum": config.maximum,
            "direction": config.direction,
            "margin": config.margin,
            "safe_minimum": config.safe_min,
            "safe_maximum": config.safe_max,
        })
    return rows


def source_assessment(active_rows: list[Mapping[str, int | str]]) -> str:
    return DEFAULTS_OR_IDENTICAL_EEPROM if active_rows == compiled_default_rows() else EEPROM_LIKELY


def build_backup(*, active_rows: list[dict[str, int | str]], global_margin: int, port: str, firmware: str, timestamp: datetime | None = None) -> dict[str, object]:
    if len(active_rows) != SERVO_COUNT:
        raise ValueError("no se puede respaldar una configuración incompleta")
    moment = timestamp or datetime.now(timezone.utc)
    return {
        "schema": "mini-qpod-active-config-backup-v1",
        "timestamp": moment.isoformat(),
        "port": port,
        "firmware_identification": firmware or "UNKNOWN",
        "source": BACKUP_SOURCE,
        "source_assessment": source_assessment(active_rows),
        "global_margin": global_margin,
        "channels": active_rows,
        "compiled_default_servos_reference": compiled_default_rows(),
        "restore_supported": False,
    }


def write_backup(snapshot: Mapping[str, object], directory: Path) -> Path:
    directory.mkdir(parents=True, exist_ok=True)
    stamp = str(snapshot["timestamp"]).replace(":", "-").replace("+", "_")
    destination = directory / f"active_config_backup_{stamp}.json"
    destination.write_text(json.dumps(snapshot, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return destination


def compare_channels(previous_rows: list[Mapping[str, int | str]], current_rows: list[Mapping[str, int | str]]) -> dict[int, list[str]]:
    previous = {int(row["channel"]): row for row in previous_rows}
    current = {int(row["channel"]): row for row in current_rows}
    changes: dict[int, list[str]] = {}
    for channel in sorted(set(previous) | set(current)):
        before, after = previous.get(channel), current.get(channel)
        if before is None or after is None:
            changes[channel] = ["canal agregado o ausente"]
            continue
        fields = ("name", "minimum", "center", "maximum", "direction", "margin", "safe_minimum", "safe_maximum")
        changed = [field for field in fields if before.get(field) != after.get(field)]
        if changed:
            changes[channel] = changed
    return changes
