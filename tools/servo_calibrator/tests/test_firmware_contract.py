from __future__ import annotations

import pathlib
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
FIRMWARE = (REPO / "mini_qpod_mvp" / "mini_qpod_mvp.ino").read_text(encoding="utf-8")
SERVO_CONTROLLER = (REPO / "mini_qpod_mvp" / "servo_control.h").read_text(encoding="utf-8")


class FirmwareContractTests(unittest.TestCase):
    def test_calibration_protocol_symbols_exist(self) -> None:
        for symbol in ("SELECT", "ENABLE", "CENTER <0..12>", "setActiveTarget", "enableOnly", "CALIBRATION_ABORT_ON_TILT", "PING", "HOST_TIMEOUT", "CALIBRATION_HOST_TIMEOUT_MS"):
            self.assertIn(symbol, FIRMWARE + SERVO_CONTROLLER)

    def test_unsafe_parsers_are_not_used(self) -> None:
        self.assertNotIn("atoi(", FIRMWARE)
        self.assertNotIn("atof(", FIRMWARE)
        self.assertIn("parseIntStrict", FIRMWARE)
        self.assertIn("parseFloatStrict", FIRMWARE)

    def test_config_includes_effective_limits_and_direction(self) -> None:
        for field in ("direction=", "margin=", "safeMin=", "safeMax="):
            self.assertIn(field, FIRMWARE)

    def test_off_clears_selection(self) -> None:
        off_section = FIRMWARE.split('else if (!strcmp(cmd, "OFF"))', 1)[1].split('else if (!strcmp(cmd, "CALIB"))', 1)[0]
        self.assertIn("selectedChannel = -1", off_section)
        self.assertIn("servos.disable()", off_section)


if __name__ == "__main__":
    unittest.main()
