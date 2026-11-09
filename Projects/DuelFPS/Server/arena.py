"""Load the duel's collision boxes and weapon values from its shipped assets."""
import math
import re
from pathlib import Path

ASSETS = Path(__file__).resolve().parents[1] / "Assets"


def weapons():
    result = {}
    for path in (ASSETS / "Scripts/Weapons").glob("*.lua"):
        text = path.read_text(encoding="utf-8")
        weapon = re.match(r"""\s*return\s*\{\s*id\s*=\s*['"]([^'"]+)""", text)
        if weapon:
            values = {k: float(v) for k, v in re.findall(
                r"\b(damage|range|fireInterval|pellets|spread)\s*=\s*([\d.]+)", text)}
            result[weapon[1]] = values
    return result


def cover(map_id):
    scene = ("Arena", "Depot")[map_id - 1]
    boxes = []
    for entity in (ASSETS / f"Scenes/{scene}.scene").read_text().split("\nEntity ")[1:]:
        fields = dict(line.split(" ", 1) for line in entity.splitlines()[1:] if " " in line)
        collider = fields["Collider"].split()
        if collider[0] != "1" or fields["Player"].startswith("1"):
            continue
        center = tuple(map(float, fields["Position"].split()))
        scale = map(float, fields["Scale"].split())
        half = tuple(abs(s * float(c)) / 2 for s, c in zip(scale, collider[1:]))
        pitch, yaw, roll = map(float, fields["Rotation"].split())
        if pitch or roll or fields["Parent"] != "0":
            raise ValueError(f"Unsupported static collider in {scene}: {fields['Name']}")
        boxes.append((center, half, math.cos(yaw), math.sin(yaw)))
    return boxes


def distance(box, origin, direction, limit):
    center, half, c, s = box
    x, y, z = (a-b for a, b in zip(origin, center))
    dx, dy, dz = direction
    near, far = 0, limit
    for p, d, extent in zip((c*x-s*z, y, s*x+c*z), (c*dx-s*dz, dy, s*dx+c*dz), half):
        if abs(d) < 1e-8:
            if abs(p) > extent:
                return math.inf
        else:
            a, b = (-extent-p)/d, (extent-p)/d
            near, far = max(near, min(a, b)), min(far, max(a, b))
            if far < near:
                return math.inf
    return near


def pellets(direction, weapon):
    x, y, z = (v / math.hypot(*direction) for v in direction)
    count = int(weapon.get("pellets", 1))
    if count == 1:
        yield (x, y, z)
        return
    length = math.hypot(x, z)
    rx, rz = (-z/length, x/length) if length >= .01 else (1, 0)
    for i in range(1, count+1):
        radius = math.sqrt((i-.5)/count) * weapon.get("spread", .055)
        a = math.cos(i*2.399963229728653)*radius
        b = math.sin(i*2.399963229728653)*radius
        ray = (x+rx*a-rz*y*b, y+(rz*x-rx*z)*b, z+rz*a+rx*y*b)
        norm = math.hypot(*ray)
        yield tuple(v/norm for v in ray)
