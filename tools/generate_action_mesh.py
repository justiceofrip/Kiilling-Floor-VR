"""Author an original charging-handle prototype in the proven KF LWO2 format.

No game geometry, textures or exporter code are copied. Coordinates are Unreal
model units (300 per metre): +X forward, +Y right, +Z up. The mounting anchor
stays at the origin; the base extends inward without moving the outer knob.
This presentation mesh has no weapon or ammunition authority.
"""
import argparse
from collections import Counter
import json
import math
from pathlib import Path
import struct

MODEL_UNITS_PER_METRE = 300.0
SIDES = 12
BASE_EXTENSION_MM = 4.0
# (distance from anchor, fore/aft radius, vertical radius), authored in millimetres.
# Extend only the base inward to intersect the receiver; preserve every original
# outward ring and the knob/grasp coordinates. All rings form one closed shell.
RINGS_MM = ((-BASE_EXTENSION_MM, 4, 3), (0, 4, 3), (1, 4, 3),
            (3, 2.6, 2.3), (14, 2.6, 2.3),
            (16.5, 8.5, 5.5), (19, 10, 7), (23, 9.5, 6.5), (25, 7.5, 4.5))


def author():
    vertices, triangles = [], []
    scale = MODEL_UNITS_PER_METRE / 1000.0
    for distance, radius_x, radius_z in RINGS_MM:
        for side in range(SIDES):
            angle = side * 2 * math.pi / SIDES
            vertices.append((radius_x * math.cos(angle) * scale,
                             distance * scale, radius_z * math.sin(angle) * scale))
    for ring in range(len(RINGS_MM) - 1):
        for side in range(SIDES):
            a = ring * SIDES + side
            b = (ring + 1) * SIDES + side
            c = (ring + 1) * SIDES + (side + 1) % SIDES
            d = ring * SIDES + (side + 1) % SIDES
            triangles.extend(((a, b, c), (a, c, d)))
    for ring, base in ((0, True), (len(RINGS_MM) - 1, False)):
        centre = len(vertices)
        vertices.append((0, RINGS_MM[ring][0] * scale, 0))
        for side in range(SIDES):
            a = ring * SIDES + side
            b = ring * SIDES + (side + 1) % SIDES
            triangles.append((centre, a, b) if base else (centre, b, a))
    return vertices, triangles


def validate(vertices, triangles):
    def subtract(a, b):
        return tuple(x - y for x, y in zip(a, b))

    def cross(a, b):
        return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])

    def dot(a, b):
        return sum(x*y for x, y in zip(a, b))

    assert all(math.isfinite(value) for vertex in vertices for value in vertex)
    edges, directed, adjacent = Counter(), Counter(), [set() for _ in vertices]
    volume, minimum_area = 0.0, math.inf
    for triangle in triangles:
        assert len(set(triangle)) == 3 and all(0 <= i < len(vertices) for i in triangle)
        a, b, c = (vertices[i] for i in triangle)
        normal = cross(subtract(b, a), subtract(c, a))
        area = math.sqrt(dot(normal, normal)) / 2
        assert area > 1e-8, "Degenerate triangle"
        minimum_area = min(minimum_area, area)
        volume += dot(a, cross(b, c)) / 6
        for a, b in zip(triangle, triangle[1:] + triangle[:1]):
            edges[tuple(sorted((a, b)))] += 1
            directed[a, b] += 1
            adjacent[a].add(b)
            adjacent[b].add(a)
    assert all(count == 2 for count in edges.values()), "Open or nonmanifold edge"
    assert all(directed[a, b] == directed[b, a] == 1 for a, b in edges), "Inconsistent winding"
    reached, pending = set(), [0]
    while pending:
        current = pending.pop()
        if current in reached:
            continue
        reached.add(current)
        pending.extend(adjacent[current] - reached)
    assert len(reached) == len(vertices), "Disconnected geometry"
    assert len(vertices) - len(edges) + len(triangles) == 2, "Unexpected closed-shell topology"
    assert volume > 0, "Expected outward Unreal-space winding"
    minimum = [min(v[axis] for v in vertices) for axis in range(3)]
    maximum = [max(v[axis] for v in vertices) for axis in range(3)]
    extents_mm = [(b-a) * 1000 / MODEL_UNITS_PER_METRE for a, b in zip(minimum, maximum)]
    assert all(abs(actual-expected) < 1e-6
               for actual, expected in zip(extents_mm, (20, 25 + BASE_EXTENSION_MM, 14)))
    base_y = -BASE_EXTENSION_MM * MODEL_UNITS_PER_METRE / 1000.0
    assert minimum[1] == base_y and vertices[-2] == (0, base_y, 0), "Incorrect inward base extension"
    assert maximum[1] == 7.5 and vertices[-1] == (0, 7.5, 0), "Outer knob moved"
    return dict(original_geometry=True, model_units_per_metre=MODEL_UNITS_PER_METRE,
                vertices=len(vertices), triangles=len(triangles), edges=len(edges),
                closed=True, connected_components=1, euler_characteristic=2,
                outward_unreal_winding=True, minimum_triangle_area=minimum_area,
                signed_volume_model_units=volume, minimum=minimum, maximum=maximum,
                extents_mm=extents_mm, base_extension_mm=BASE_EXTENSION_MM)


def encode(vertices, triangles):
    def be(fmt, *values):
        return struct.pack('>' + fmt, *values)

    def name(value):
        data = value.encode('ascii') + b'\0'
        return data + b'\0' * (len(data) % 2)

    def chunk(tag, data):
        return tag.encode('ascii') + be('I', len(data)) + data + b'\0' * (len(data) % 2)

    def sub(tag, data):
        return tag.encode('ascii') + be('H', len(data)) + data + b'\0' * (len(data) % 2)

    # Installed importer maps LightWave(x,y,z) to Unreal(x,-z,-y).
    # Preserve our outward Unreal-space triangle order while converting points.
    points = [(x, -z, -y) for x, y, z in vertices]
    uv = [((x + 3.0) / 6.0, y / 7.5) for x, y, z in vertices]
    data = b'LWO2' + chunk('TAGS', name('DefaultTexture'))
    data += chunk('LAYR', be('HHfff', 0, 0, 0, 0, 0) + name('OriginalChargingHandle'))
    data += chunk('PNTS', b''.join(be('fff', *v) for v in points))
    data += chunk('VMAP', b'TXUV' + be('H', 2) + name('UVTexture')
                  + b''.join(be('Hff', i, *value) for i, value in enumerate(uv)))
    data += chunk('POLS', b'FACE' + b''.join(be('HHHH', 3, *triangle) for triangle in triangles))
    data += chunk('PTAG', b'SURF' + b''.join(be('HH', i, 0) for i in range(len(triangles))))
    image = sub('IMAP', name('1') + sub('CHAN', b'COLR')) + sub('PROJ', be('H', 5)) + sub('VMAP', name('UVTexture'))
    surface = name('DefaultTexture') + name('') + sub('COLR', be('fffH', .18, .20, .22, 0))
    surface += sub('SIDE', be('H', 1)) + sub('BLOK', image)
    data += chunk('SURF', surface)
    return chunk('FORM', data)


def make(path):
    vertices, triangles = author()
    report = validate(vertices, triangles)
    data = encode(vertices, triangles)
    assert data[:4] == b'FORM' and data[8:12] == b'LWO2'
    assert struct.unpack_from('>I', data, 4)[0] == len(data)-8
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    report.update(path=str(path), bytes=len(data))
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1]
                        / 'unrealscript/KFVRGameplay/Models/KFVRChargingHandle.lwo')
    print(json.dumps(make(parser.parse_args().output), indent=2))
