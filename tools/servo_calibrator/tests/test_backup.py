from __future__ import annotations

from datetime import datetime, timezone
import json
import pathlib
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from backup import (
    DEFAULTS_OR_IDENTICAL_EEPROM,
    EEPROM_LIKELY,
    ActiveConfigCollector,
    BackupGate,
    build_backup,
    compiled_default_rows,
    compare_channels,
    source_assessment,
    write_backup,
)


def config_margin(margin: int = 5) -> str:
    return f"CONFIG margin={margin}"


def config_channel(channel: int, *, minimum: int = 0, center: int = 90, maximum: int = 180, direction: int = 1, margin: int = 5, safe_min: int | None = None, safe_max: int | None = None) -> str:
    safe_min = minimum + margin if safe_min is None else safe_min
    safe_max = maximum - margin if safe_max is None else safe_max
    return f"CONFIG ch={channel} channel={channel} min={minimum} center={center} max={maximum} direction={direction} margin={margin} safeMin={safe_min} safeMax={safe_max}"


def complete_collector() -> ActiveConfigCollector:
    collector = ActiveConfigCollector()
    collector.accept_margin(config_margin())
    for channel in range(13):
        collector.accept_channel(config_channel(channel))
    return collector


class BackupValidationTests(unittest.TestCase):
    def test_incomplete_backup_is_rejected(self) -> None:
        collector = ActiveConfigCollector()
        collector.accept_margin(config_margin())
        collector.accept_channel(config_channel(0))
        with self.assertRaisesRegex(ValueError, "incompleta"):
            collector.rows()

    def test_duplicate_channel_is_rejected(self) -> None:
        collector = ActiveConfigCollector()
        collector.accept_margin(config_margin())
        collector.accept_channel(config_channel(0))
        with self.assertRaisesRegex(ValueError, "duplicado"):
            collector.accept_channel(config_channel(0))

    def test_missing_channel_is_reported(self) -> None:
        collector = ActiveConfigCollector()
        collector.accept_margin(config_margin())
        for channel in range(13):
            if channel != 7:
                collector.accept_channel(config_channel(channel))
        with self.assertRaisesRegex(ValueError, "7"):
            collector.rows()

    def test_invalid_channel_data_is_rejected(self) -> None:
        collector = ActiveConfigCollector()
        collector.accept_margin(config_margin())
        with self.assertRaisesRegex(ValueError, "efectivos"):
            collector.accept_channel(config_channel(0, safe_max=160))

    def test_write_failure_does_not_create_a_backup(self) -> None:
        collector = complete_collector()
        snapshot = build_backup(
            active_rows=collector.rows(),
            global_margin=5,
            port="SIMULATED",
            firmware="TEST",
            timestamp=datetime(2026, 1, 1, tzinfo=timezone.utc),
        )
        with tempfile.TemporaryDirectory() as temporary:
            destination = pathlib.Path(temporary) / "not-a-directory"
            destination.write_text("file", encoding="utf-8")
            with self.assertRaises(OSError):
                write_backup(snapshot, destination)

    def test_json_contains_active_defaults_and_unknown_source(self) -> None:
        collector = complete_collector()
        snapshot = build_backup(
            active_rows=collector.rows(),
            global_margin=5,
            port="SIMULATED",
            firmware="TEST",
            timestamp=datetime(2026, 1, 1, tzinfo=timezone.utc),
        )
        self.assertEqual(snapshot["source"], "ACTIVE_RUNTIME_UNKNOWN_SOURCE")
        self.assertEqual(snapshot["source_assessment"], EEPROM_LIKELY)
        self.assertEqual(len(snapshot["channels"]), 13)
        self.assertEqual(len(snapshot["compiled_default_servos_reference"]), 13)
        with tempfile.TemporaryDirectory() as temporary:
            destination = write_backup(snapshot, pathlib.Path(temporary))
            loaded = json.loads(destination.read_text(encoding="utf-8"))
            self.assertEqual(loaded["source"], "ACTIVE_RUNTIME_UNKNOWN_SOURCE")
            self.assertFalse(loaded["restore_supported"])

    def test_identical_compiled_defaults_are_not_claimed_as_eeprom(self) -> None:
        self.assertEqual(source_assessment(compiled_default_rows()), DEFAULTS_OR_IDENTICAL_EEPROM)

    def test_comparison_reports_only_changed_fields(self) -> None:
        collector = complete_collector()
        previous = collector.rows()
        current = [dict(row) for row in previous]
        current[3]["maximum"] = 170
        current[3]["safe_maximum"] = 165
        self.assertEqual(compare_channels(previous, current), {3: ["maximum", "safe_maximum"]})


class BackupGateTests(unittest.TestCase):
    def test_limits_and_save_stay_blocked_until_save_and_confirmation(self) -> None:
        gate = BackupGate()
        self.assertFalse(gate.allows_modifications)
        with self.assertRaises(ValueError):
            gate.confirm()
        gate.mark_saved(pathlib.Path("backup.json"))
        self.assertFalse(gate.allows_modifications)
        gate.confirm()
        self.assertTrue(gate.allows_modifications)
        gate.reset()
        self.assertFalse(gate.allows_modifications)


if __name__ == "__main__":
    unittest.main()
