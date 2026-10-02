"""Terrain access diagnostics, metres; no Unreal dependency. Not collision/player proof."""
import math
from collections import defaultdict

class Surface:
    def __init__(self, vertices, indices):
        if len(indices) % 3 or any(not math.isfinite(c) for v in vertices for c in v):
            raise ValueError('invalid mesh')
        if any(i < 0 or i >= len(vertices) for i in indices):
            raise ValueError('invalid mesh index')
        self.triangles = []
        self.bins = defaultdict(list)
        for i in range(0, len(indices), 3):
            a, b, c = [vertices[j] for j in indices[i:i+3]]
            ux, uy, uz = [b[j]-a[j] for j in range(3)]
            vx, vy, vz = [c[j]-a[j] for j in range(3)]
            det = ux*vy-uy*vx
            if abs(det) < 1e-12:
                continue  # Vertical faces have no XY coverage; limitations recorded in protocol.
            gx, gy = (uz*vy-uy*vz)/det, (ux*vz-uz*vx)/det
            k = len(self.triangles)
            self.triangles.append((a, ux, uy, vx, vy, det, gx, gy))
            for x in range(math.floor(min(a[0], b[0], c[0])/20), math.floor(max(a[0], b[0], c[0])/20)+1):
                for y in range(math.floor(min(a[1], b[1], c[1])/20), math.floor(max(a[1], b[1], c[1])/20)+1):
                    self.bins[x, y].append(k)

    def sample(self, x, y):
        best = None
        for k in self.bins.get((math.floor(x/20), math.floor(y/20)), []):
            a, ux, uy, vx, vy, det, gx, gy = self.triangles[k]
            px, py = x-a[0], y-a[1]
            u, v = (px*vy-py*vx)/det, (ux*py-uy*px)/det
            if min(u, v, 1-u-v) < -1e-8:
                continue
            z = a[2]+px*gx+py*gy
            if best is None or z > best[0]:
                best = (z, math.degrees(math.atan(math.hypot(gx, gy))))
        return best


def inspect_path(points, ground, waters, step=.5, slope_limit=18.):
    if not math.isfinite(step) or step <= 0:
        raise ValueError('invalid step')
    if not points or any(len(p) != 2 or not all(math.isfinite(v) for v in p) for p in points):
        raise ValueError('invalid path')
    samples = []
    length = 0.
    for i, p in enumerate(points):
        if i == 0:
            positions = [p]
        else:
            a = points[i-1]
            d = math.dist(a, p)
            length += d
            count = max(1, math.ceil(d/step))
            if len(samples)+count > 100000:
                raise ValueError('path budget exceeded')
            positions = [(a[0]+(p[0]-a[0])*j/count, a[1]+(p[1]-a[1])*j/count) for j in range(1, count+1)]
        for x, y in positions:
            g = ground.sample(x, y)
            levels = [h[0] for w in waters if (h := w.sample(x, y)) is not None]
            # Exposed water versus a ribbon buried beneath a bank. 10 cm is a diagnostic margin.
            flags = ['ground_unknown'] if g is None else []
            if g is not None and levels and max(levels) > g[0]-.1:
                flags.append('water_or_low_freeboard')
            if g is not None and g[1] > slope_limit:
                flags.append('slope_over_policy')
            samples.append(dict(x=x, y=y, z=None if g is None else g[0],
                                slope=None if g is None else g[1], flags=flags))
    # Also catch steps between otherwise flat surfaces. Triangle slope alone misses these.
    grades = []
    ascent = descent = 0.
    for a, b in zip(samples, samples[1:]):
        if a['z'] is None or b['z'] is None:
            continue
        distance = math.hypot(b['x']-a['x'], b['y']-a['y'])
        if distance <= 1e-9:
            continue
        dz = b['z']-a['z']
        grade = math.degrees(math.atan2(abs(dz), distance))
        grades.append(grade)
        ascent += max(0., dz)
        descent += max(0., -dz)
        if grade > slope_limit:
            b['flags'].append('grade_over_policy')
    slopes = [s['slope'] for s in samples if s['slope'] is not None]
    flags = sorted({f for s in samples for f in s['flags']})
    return dict(status='UNKNOWN' if 'ground_unknown' in flags else 'ANOMALY' if flags else 'NO_ANOMALY_AT_SAMPLES',
                length_m=length, sampled_ascent_m=ascent, sampled_descent_m=descent, max_grade_deg=max(grades, default=None), max_slope_deg=max(slopes, default=None), step_m=step,
                slope_limit_deg=slope_limit, flags=flags, samples=samples)


def observed_segment(previous, current, tile_m, ground, waters):
    dt = current['time']-previous['time']
    a, b = previous['actor'], current['actor']
    if a['id'] != b['id'] or dt <= 0 or dt > .1 or a['inside'] or b['inside']:
        return dict(status='UNKNOWN', reason='identity_time_gap_or_interior')
    p, q = [[r['position'][k]*tile_m for k in ('x', 'y')] for r in (a, b)]
    if math.dist(p, q) > 5.:
        return dict(status='UNKNOWN', reason='position_jump')
    result = inspect_path([p, q], ground, waters)
    result['scope'] = 'sampled_simulation_position_chord_not_collision'
    return result
