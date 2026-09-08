"""Versioned pose files, independent from the desktop widgets."""
from dataclasses import asdict, dataclass
import json
import math

from tools.ik_simulator.kinematics import MOUNTS
from tools.ik_simulator.validation import validate_target


@dataclass(frozen=True)
class Pose:
    height: float = 80
    spread: float = 75
    advance: float = 0
    yaw: float = 0
    knee_sign: int = -1

    def __post_init__(self):
        for key, value in asdict(self).items():
            if type(value) not in (int, float) or not math.isfinite(value):
                raise ValueError(f"{key}: número finito requerido")
        if self.height < 0 or self.spread < 0:
            raise ValueError("Altura y apertura deben ser no negativas")
        if self.knee_sign not in (-1, 1):
            raise ValueError("Rama de rodilla: -1 o +1")

    def targets(self):
        result = {}
        for leg, m in MOUNTS.items():
            # Symmetric radial fan; advance is a subsequent body +Y offset.
            theta = m.yaw + math.radians(self.yaw) * (1 if m.x*m.y > 0 else -1)
            radius = math.sqrt(2)*self.spread
            result[leg] = (m.x+radius*math.cos(theta),
                           m.y+radius*math.sin(theta)+self.advance, -self.height)
        return result


PRESETS = {"READY": Pose(), "X_NEUTRAL": Pose(height=70, spread=85)}


def dumps(pose, export=False):
    data = {"schema": "mini-qpod-pose", "version": 1, "pose": asdict(pose)}
    if export:
        data["simulation"] = {"units": "mm/rad", "frame": "+X right, +Y front, +Z up",
                              "physical_approval": False, "legs": {}}
        for leg, point in pose.targets().items():
            report = validate_target(leg, point, pose.knee_sign)
            data["simulation"]["legs"][leg] = {
                "target": point, "angles": asdict(report.angles) if report.angles else None,
                "checks": [asdict(c) for c in report.checks], "warnings": report.warnings}
    return json.dumps(data, indent=2, ensure_ascii=False, allow_nan=False)


def loads(text):
    data = json.loads(text)
    if not isinstance(data, dict) or data.get("schema") != "mini-qpod-pose" or type(data.get("version")) is not int or data["version"] != 1:
        raise ValueError("Formato o versión de pose no compatible")
    if not isinstance(data.get("pose"), dict) or set(data["pose"]) != set(asdict(Pose())):
        raise ValueError("Campos de pose incorrectos")
    return Pose(**data["pose"])
