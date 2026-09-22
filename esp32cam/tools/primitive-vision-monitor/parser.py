"""Parser for ESP32-CAM primitive-vision diagnostic lines."""
from dataclasses import dataclass
import re
from typing import Optional

_MOTION = re.compile(
    r"^MOTION\s+state=(?P<state>[A-Za-z_]+)\s+"
    r"x=(?P<x>-?\d+)\s+y=(?P<y>-?\d+)\s+"
    r"area=(?P<area>\d+)\s+score=(?P<score>\d+)"
    r"(?:\s+threshold=(?P<threshold>\d+))?"
    r"(?:\s+luminance=(?P<luminance>\d+)\s+light=(?P<light>[A-Za-z_]+))?\s*$"
)

@dataclass
class MotionSample:
    state: str
    x: int
    y: int
    area: int
    score: int
    threshold: Optional[int] = None
    confidence: Optional[int] = None
    light: Optional[int] = None
    light_state: Optional[str] = None

def parse_line(line: str) -> Optional[MotionSample]:
    """Return a sample for a known line; silently ignore unknown/malformed lines."""
    match = _MOTION.match(line.strip())
    if not match:
        return None
    values = match.groupdict()
    return MotionSample(
        state=values["state"].upper(),
        x=int(values["x"]), y=int(values["y"]), area=int(values["area"]),
        score=int(values["score"]),
        threshold=int(values["threshold"]) if values["threshold"] else None,
        light=int(values["luminance"]) if values["luminance"] else None,
        light_state=values["light"].upper() if values["light"] else None,
    )
