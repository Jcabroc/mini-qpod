from __future__ import annotations

import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from protocol import attempt_safe_off, command_is_allowed, parse_float, parse_int
from simulated_serial import SimulatedSerial


def command(port: SimulatedSerial, text: str) -> list[str]:
    port.write((text + "\n").encode("ascii"))
    responses: list[str] = []
    while port.in_waiting:
        response = port.readline().decode("ascii").strip()
        if response:
            responses.append(response)
    return responses


class ProtocolSafetyTests(unittest.TestCase):
    def setUp(self) -> None:
        self.port = SimulatedSerial()
        while self.port.in_waiting:
            self.port.readline()

    def test_calib_and_select_do_not_enable_pwm(self) -> None:
        self.assertEqual(command(self.port, "CALIB"), ["[CALIB] ch=-1 PWM=OFF seleccione un canal con SELECT."])
        self.assertEqual(command(self.port, "SELECT 3"), ["[SELECT] ch=3 PWM=OFF"])
        self.assertEqual(command(self.port, "STATUS"), ["STATUS mode=CALIBRATION servos=0 selected=3 active=-1 imu=1 abort=NONE phase=0 leg=0"])

    def test_enable_only_selected_channel_and_clamps_first_angle(self) -> None:
        command(self.port, "CALIB")
        command(self.port, "SELECT 3")
        self.assertEqual(command(self.port, "ENABLE 2 90"), ["[ERR] Seleccione primero ese canal con SELECT."])
        self.assertEqual(command(self.port, "ENABLE 3 999"), ["[ENABLE] ch=3 angle=125.00"])
        self.assertEqual(command(self.port, "STATUS"), ["STATUS mode=CALIBRATION servos=1 selected=3 active=3 imu=1 abort=NONE phase=0 leg=0"])

    def test_selecting_another_channel_turns_previous_pwm_off(self) -> None:
        command(self.port, "CALIB")
        command(self.port, "SELECT 0")
        command(self.port, "ENABLE 0 90")
        command(self.port, "SELECT 1")
        self.assertEqual(command(self.port, "STATUS"), ["STATUS mode=CALIBRATION servos=0 selected=1 active=-1 imu=1 abort=NONE phase=0 leg=0"])

    def test_center_and_servo_require_enabled_selected_channel(self) -> None:
        command(self.port, "CALIB")
        command(self.port, "SELECT 0")
        self.assertEqual(command(self.port, "CENTER 0"), ["[ERR] CENTER exige canal seleccionado y habilitado."])
        command(self.port, "ENABLE 0 90")
        self.assertEqual(command(self.port, "CENTER 0"), ["[CENTER] ch=0 angle=90.00"])
        self.assertEqual(command(self.port, "SERVO 1 90"), ["[ERR] SERVO exige canal seleccionado y habilitado."])

    def test_off_clears_selection_and_load_save_require_safe_off(self) -> None:
        command(self.port, "CALIB")
        command(self.port, "SELECT 0")
        self.assertEqual(command(self.port, "SAVE"), ["[ERR] Ejecute OFF antes de SAVE."])
        self.assertEqual(command(self.port, "OFF"), ["[OFF] ch=-1 PWM=OFF mode=SAFE_OFF"])
        self.assertEqual(command(self.port, "STATUS"), ["STATUS mode=SAFE_OFF servos=0 selected=-1 active=-1 imu=1 abort=NONE phase=0 leg=0"])
        self.assertEqual(command(self.port, "SAVE"), ["[SAVE] EEPROM OK."])
        self.assertEqual(command(self.port, "LOAD"), ["[LOAD] EEPROM OK."])

    def test_invalid_arguments_are_rejected(self) -> None:
        command(self.port, "CALIB")
        self.assertTrue(command(self.port, "SELECT 1.5")[0].startswith("[ERR]"))
        command(self.port, "SELECT 0")
        self.assertTrue(command(self.port, "ENABLE 0 NaN")[0].startswith("[ERR]"))
        self.assertTrue(command(self.port, "ENABLE 0 90 extra")[0].startswith("[ERR]"))

    def test_imu_abort_blocks_calibration(self) -> None:
        command(self.port, "CALIB")
        command(self.port, "SELECT 0")
        self.port.imu_healthy = False
        self.assertEqual(command(self.port, "ENABLE 0 90"), ["[ERR] ENABLE bloqueado: IMU perdida o inclinacion insegura."])
        self.port.inject_abort()
        self.assertEqual(self.port.readline().decode("ascii").strip(), "[ABORT] IMU_UNSAFE")

    def test_config_reports_existing_and_effective_values(self) -> None:
        config = command(self.port, "CONFIG")
        self.assertEqual(config[0], "CONFIG margin=5")
        self.assertIn("min=50", config[1])
        self.assertIn("center=90", config[1])
        self.assertIn("max=150", config[1])
        self.assertIn("direction=1", config[1])
        self.assertIn("safeMin=55", config[1])
        self.assertIn("safeMax=145", config[1])


class LocalGuardTests(unittest.TestCase):
    def test_terminal_whitelist_excludes_motion_modes_and_defaults(self) -> None:
        for blocked in ("STAND", "LEG 0", "UNLOCK_WALK", "WALK", "DEFAULTS", "UNKNOWN"):
            self.assertFalse(command_is_allowed(blocked))
        for allowed in ("OFF", "STATUS", "IMU", "CONFIG", "PING", "CALIB", "SELECT 0", "ENABLE 0 90", "CENTER 0", "SERVO 0 91", "LIMITS 0 50 90 150", "SAVE", "LOAD"):
            self.assertTrue(command_is_allowed(allowed))

    def test_numeric_helpers_reject_non_finite_or_non_integral_values(self) -> None:
        self.assertEqual(parse_int("+12", low=0, high=12), 12)
        self.assertEqual(parse_float("12.5"), 12.5)
        for value in ("", "1.2", "12x", "NaN", "inf"):
            with self.assertRaises(ValueError):
                parse_int(value)
        for value in ("NaN", "inf", "-inf", "bad"):
            with self.assertRaises(ValueError):
                parse_float(value)


class ManualClock:
    def __init__(self) -> None:
        self.value = 0.0

    def __call__(self) -> float:
        return self.value

    def advance(self, seconds: float) -> None:
        self.value += seconds


class HostWatchdogTests(unittest.TestCase):
    def setUp(self) -> None:
        self.clock = ManualClock()
        self.port = SimulatedSerial(clock=self.clock)
        while self.port.in_waiting:
            self.port.readline()

    def _activate_channel_zero(self) -> None:
        command(self.port, "CALIB")
        command(self.port, "SELECT 0")
        command(self.port, "ENABLE 0 90")

    def test_heartbeat_normal_does_not_abort(self) -> None:
        self._activate_channel_zero()
        self.clock.advance(0.75)
        self.assertEqual(command(self.port, "PING"), ["PING mode=CALIBRATION active=0"])
        self.clock.advance(0.75)
        self.port.tick()
        self.assertFalse(self.port.in_waiting)
        self.assertEqual(command(self.port, "STATUS"), ["STATUS mode=CALIBRATION servos=1 selected=0 active=0 imu=1 abort=NONE phase=0 leg=0"])

    def test_lost_heartbeat_with_active_channel_aborts_to_safe_off(self) -> None:
        self._activate_channel_zero()
        self.clock.advance(1.01)
        self.port.tick()
        self.assertEqual(self.port.readline().decode("ascii").strip(), "[ABORT] HOST_TIMEOUT")
        self.assertEqual(command(self.port, "STATUS"), ["STATUS mode=SAFE_OFF servos=0 selected=-1 active=-1 imu=1 abort=HOST_TIMEOUT phase=0 leg=0"])

    def test_lost_heartbeat_without_active_channel_does_not_abort(self) -> None:
        command(self.port, "CALIB")
        command(self.port, "SELECT 0")
        self.clock.advance(5.0)
        self.port.tick()
        self.assertFalse(self.port.in_waiting)
        self.assertEqual(command(self.port, "STATUS"), ["STATUS mode=CALIBRATION servos=0 selected=0 active=-1 imu=1 abort=NONE phase=0 leg=0"])

    def test_normal_close_attempts_off(self) -> None:
        self._activate_channel_zero()
        self.assertTrue(attempt_safe_off(self.port))
        self.assertEqual(self.port.written_lines[-1], "OFF")
        self.assertEqual(self.port.readline().decode("ascii").strip(), "[OFF] ch=-1 PWM=OFF mode=SAFE_OFF")
        self.assertEqual(command(self.port, "STATUS"), ["STATUS mode=SAFE_OFF servos=0 selected=-1 active=-1 imu=1 abort=NONE phase=0 leg=0"])

    def test_ping_cannot_reactivate_after_timeout(self) -> None:
        self._activate_channel_zero()
        self.clock.advance(1.01)
        self.port.tick()
        self.port.readline()
        self.assertEqual(command(self.port, "PING"), ["PING mode=SAFE_OFF active=-1"])
        self.assertEqual(command(self.port, "STATUS"), ["STATUS mode=SAFE_OFF servos=0 selected=-1 active=-1 imu=1 abort=HOST_TIMEOUT phase=0 leg=0"])


if __name__ == "__main__":
    unittest.main()
