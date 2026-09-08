"""Advisory validation only: unknown physical checks never imply safety."""

from dataclasses import dataclass
import math

from .kinematics import (
    Angles, COXA, FEMUR, TIBIA, SingularTarget, UnreachableTarget,
    body_to_leg, inverse,
)


@dataclass(frozen=True)
class Check:
    category: str
    status: str  # pass, fail, indeterminate, pending
    detail: str


@dataclass(frozen=True)
class ValidationReport:
    angles: Angles | None
    checks: tuple[Check, ...]
    warnings: tuple[str, ...]

    @property
    def ik_reachable(self):
        return self.checks[0].status == "pass"


def validate_target(leg, body_point, knee_sign=-1):
    """Return IK and advisory zones, without applying CAD or servo limits.

    Boundary diagnostics use a numerical 1e-7 mm tolerance, not a mechanical
    clearance. Invalid API inputs still raise ValueError/KeyError.
    """
    local = body_to_leg(leg, body_point)
    warnings = []
    try:
        angles = inverse(leg, body_point, knee_sign)
        ik = Check("ik", "pass", "Ideal geometric solution exists")
    except UnreachableTarget as exc:
        angles = None
        ik = Check("ik", "fail", str(exc))
    except SingularTarget as exc:
        angles = None
        ik = Check("ik", "indeterminate", str(exc))

    if angles is not None:
        distance = math.hypot(math.hypot(*local[:2]) - COXA, local[2])
        if abs(distance - (FEMUR + TIBIA)) <= 1e-7:
            warnings.append("FULL_EXTENSION_SINGULARITY")
        if abs(distance - abs(FEMUR - TIBIA)) <= 1e-7:
            warnings.append("FOLDED_KNEE_SINGULARITY")
        warnings.append("MECHANICAL_ENVELOPE_UNVALIDATED")

    checks = (
        ik,
        Check("mechanical_joints", "pending",
              "CAD envelope is qualitative; joint intervals not validated"),
        Check("electrical_channels", "pending",
              "Physical calibration remains authoritative; mapping not integrated"),
        Check("chassis_collision", "pending",
              "Requires registered solids, offsets and clearance model"),
        Check("neighbor_collision", "pending",
              "Requires simultaneous leg poses and swept solid volumes"),
    )
    return ValidationReport(angles, checks, tuple(warnings))
