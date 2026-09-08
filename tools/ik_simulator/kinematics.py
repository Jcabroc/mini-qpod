"""Millimetres and radians; mechanical angles, never servo commands."""

from dataclasses import dataclass
import math
from types import MappingProxyType

COXA = 42.294
FEMUR = 60.611
TIBIA = 88.714


@dataclass(frozen=True)
class Mount:
    x: float
    y: float
    yaw: float


MOUNTS = MappingProxyType({
    "L1": Mount(-46.5, 46.5, math.radians(135)),
    "R1": Mount(46.5, 46.5, math.radians(45)),
    "L2": Mount(-46.5, -46.5, math.radians(-135)),
    "R2": Mount(46.5, -46.5, math.radians(-45)),
})


@dataclass(frozen=True)
class Angles:
    yaw: float
    femur: float
    knee: float


class UnreachableTarget(ValueError):
    """Target lies outside the mathematical workspace."""


class SingularTarget(ValueError):
    """Target does not determine a unique coxa yaw."""


def _finite(*values):
    if not all(math.isfinite(v) for v in values):
        raise ValueError("Coordinates and angles must be finite")


def body_to_leg(leg, point):
    """Rotate/translate body coordinates to the radial mount frame."""
    x, y, z = point
    _finite(x, y, z)
    mount = MOUNTS[leg]
    c, s = math.cos(mount.yaw), math.sin(mount.yaw)
    dx, dy = x - mount.x, y - mount.y
    return c * dx + s * dy, -s * dx + c * dy, z


def leg_to_body(leg, point):
    x, y, z = point
    _finite(x, y, z)
    mount = MOUNTS[leg]
    c, s = math.cos(mount.yaw), math.sin(mount.yaw)
    return mount.x + c * x - s * y, mount.y + s * x + c * y, z


def forward_local(angles):
    """FK: femur elevation from horizontal; knee relative to femur."""
    q0, q1, q2 = angles.yaw, angles.femur, angles.knee
    _finite(q0, q1, q2)
    radius = COXA + FEMUR * math.cos(q1) + TIBIA * math.cos(q1 + q2)
    z = FEMUR * math.sin(q1) + TIBIA * math.sin(q1 + q2)
    return radius * math.cos(q0), radius * math.sin(q0), z


def inverse_local(point, knee_sign=-1):
    """Select outward coxa and one of the two planar knee branches.

    Fully extended/folded boundaries are accepted; yaw-axis singularities
    are rejected. This function does not enforce physical joint limits.
    """
    x, y, z = point
    _finite(x, y, z)
    if knee_sign not in (-1, 1):
        raise ValueError("knee_sign must be -1 or +1")
    radius = math.hypot(x, y)
    if radius <= 1e-9:
        raise SingularTarget("Coxa yaw is undefined on its vertical axis")
    horizontal = radius - COXA
    distance = math.hypot(horizontal, z)
    if not abs(FEMUR - TIBIA) - 1e-9 <= distance <= FEMUR + TIBIA + 1e-9:
        raise UnreachableTarget("Target is outside the femur/tibia annulus")
    cosine = (distance**2 - FEMUR**2 - TIBIA**2) / (2 * FEMUR * TIBIA)
    knee = knee_sign * math.acos(max(-1.0, min(1.0, cosine)))
    femur = math.atan2(z, horizontal) - math.atan2(
        TIBIA * math.sin(knee), FEMUR + TIBIA * math.cos(knee)
    )
    return Angles(math.atan2(y, x), femur, knee)


def inverse(leg, body_point, knee_sign=-1):
    return inverse_local(body_to_leg(leg, body_point), knee_sign)


def forward(leg, angles):
    return leg_to_body(leg, forward_local(angles))
