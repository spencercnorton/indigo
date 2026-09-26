#!/usr/bin/env python3
"""controller_outline.py -- regenerate the Library emblem's silhouette table.

Prints the `controllerOutline` table used by gui/indigo_background.c: the
union of a GameCube controller's structural parts (two main lobes, the bridge
that carries Start, the D-pad and C-stick lobes and the flared grips), traced
on a raster, corner-cut so every junction is round enough for a mitred band,
simplified, and ordered clockwise in face units (v up). Needs Pillow.
"""
import math

from PIL import Image, ImageDraw

U0, U1, V0, V1 = -0.72, 0.72, -0.66, 0.50
RES = 1400  # raster pixels per face unit


def parts():
    return [
        ("circle", ((-0.335, 0.075), 0.235)), ("circle", ((0.335, 0.075), 0.235)),
        ("poly", [(-0.34, -0.15), (-0.34, 0.262), (-0.17, 0.280), (0.0, 0.284),
                  (0.17, 0.280), (0.34, 0.262), (0.34, -0.15)]),
        ("circle", ((-0.165, -0.19), 0.13)), ("circle", ((0.165, -0.19), 0.13)),
        ("hull", ((-0.45, -0.05), 0.14, (-0.565, -0.335), 0.105)),
        ("hull", ((0.45, -0.05), 0.14, (0.565, -0.335), 0.105)),
    ]


def to_px(u, v):
    return ((u - U0) * RES, (V1 - v) * RES)


def hull(c0, r0, c1, r1, n=40):
    pts = sorted({(c[0] + r * math.cos(2 * math.pi * i / n), c[1] + r * math.sin(2 * math.pi * i / n))
                  for (c, r) in ((c0, r0), (c1, r1)) for i in range(n)})
    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]


def union_mask():
    mask = Image.new("L", (int((U1 - U0) * RES), int((V1 - V0) * RES)), 0)
    draw = ImageDraw.Draw(mask)
    for kind, p in parts():
        if kind == "circle":
            (cx, cy), r = p
            x, y = to_px(cx, cy)
            draw.ellipse([x - r * RES, y - r * RES, x + r * RES, y + r * RES], fill=255)
        else:
            draw.polygon([to_px(*q) for q in (p if kind == "poly" else hull(*p))], fill=255)
    return mask


def trace(mask):
    """Moore-neighbour trace of the single blob's boundary."""
    width, height = mask.size
    px = mask.load()
    inside = lambda x, y: 0 <= x < width and 0 <= y < height and px[x, y] > 127
    start = next((x, y) for y in range(height) for x in range(width) if inside(x, y))
    around = [(-1, 0), (-1, -1), (0, -1), (1, -1), (1, 0), (1, 1), (0, 1), (-1, 1)]
    boundary, current, back = [start], start, 0
    while True:
        for k in range(8):
            index = (back + k) % 8
            step = (current[0] + around[index][0], current[1] + around[index][1])
            if inside(*step):
                back, current = (index + 5) % 8, step
                break
        if current == start:
            return boundary
        boundary.append(current)


def douglas_peucker(points, eps):
    a, b = points[0], points[-1]
    dx, dy = b[0] - a[0], b[1] - a[1]
    length = math.hypot(dx, dy) or 1e-9
    best, index = 0.0, 0
    for i in range(1, len(points) - 1):
        d = abs(dy * (points[i][0] - a[0]) - dx * (points[i][1] - a[1])) / length
        if d > best:
            best, index = d, i
    if best > eps:
        return douglas_peucker(points[:index + 1], eps)[:-1] + douglas_peucker(points[index:], eps)
    return [a, b]


def closed_simplify(points, eps):
    far = max(range(len(points)), key=lambda i: math.dist(points[i], points[0]))
    return (douglas_peucker(points[:far + 1], eps)[:-1] +
            douglas_peucker(points[far:] + [points[0]], eps)[:-1])


def corner_cut(points, rounds):
    for _ in range(rounds):
        points = [q for i, p in enumerate(points) for q in (
            (0.75 * p[0] + 0.25 * points[(i + 1) % len(points)][0], 0.75 * p[1] + 0.25 * points[(i + 1) % len(points)][1]),
            (0.25 * p[0] + 0.75 * points[(i + 1) % len(points)][0], 0.25 * p[1] + 0.75 * points[(i + 1) % len(points)][1]))]
    return points


def outline():
    raw = [(x / RES + U0, V1 - y / RES) for (x, y) in trace(union_mask())]
    loop = closed_simplify(corner_cut(closed_simplify(raw, 0.004), 3), 0.004)
    area = sum(p[0] * q[1] - q[0] * p[1] for p, q in zip(loop, loop[1:] + loop[:1]))
    return loop if area < 0 else loop[::-1]


def literal(value):
    text = f"{value:.3f}".rstrip("0").rstrip(".")
    text = "0" if text in ("-0", "") else text
    return (text if "." in text else text + ".0") + "f"


if __name__ == "__main__":
    points = [(round(u, 3), round(v, 3)) for u, v in outline()]
    rows = ["\t\t" + ", ".join("{%s, %s}" % (literal(u), literal(v)) for u, v in points[k:k + 5])
            for k in range(0, len(points), 5)]
    print(",\n".join(rows))
