"""Validate an opening water survey. Acquisition success is not geographic agreement."""

GROUPS = ('both_dry', 'both_water', 'simulation_only', 'render_only', 'unknown')


def validate(report):
    c = report['water_concordance']
    w, h = c['width'], c['height']
    if type(w) is not int or type(h) is not int or min(w, h) <= 0:
        raise ValueError('Invalid survey dimensions')
    total = w * h
    seen = set()
    for name in GROUPS:
        for index in c[name]:
            if type(index) is not int or not 0 <= index < total or index in seen:
                raise ValueError('Water partition overlaps or has invalid indices')
            seen.add(index)
    if len(seen) != total:
        raise ValueError('Water partition is incomplete')
    compared = total - len(c['unknown'])
    mismatch = len(c['simulation_only']) + len(c['render_only'])
    agreement = ('UNKNOWN' if not compared else 'MISMATCH' if mismatch else
                 'PARTIAL' if c['unknown'] else 'AGREEMENT_AT_CENTRES')
    if c['compared_cells'] != compared or c['mismatch_cells'] != mismatch:
        raise ValueError('Water counts disagree with their raw indices')
    if c['agreement'] != agreement or c['status'] != ('SAMPLED' if compared else 'UNKNOWN'):
        raise ValueError('Water verdict overstates its evidence')
    return dict(total=total, compared=compared, mismatch=mismatch,
                agreement=agreement, counts={k:len(c[k]) for k in GROUPS})


def svg(report):
    """Diagnostic tile-centre map; no terrain interpolation or inferred shoreline."""
    validate(report)
    c = report['water_concordance']
    colors = dict(both_dry='#e2dfd1', both_water='#147a9c', simulation_only='#b04716',
                  render_only='#873ec4', unknown='#565d67')
    scale = min(8, 768 / max(c['width'], c['height']))
    width, height = max(700, c['width']*scale), c['height']*scale
    rows = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height+170}" viewBox="0 0 {width} {height+170}">',
            '<rect width="100%" height="100%" fill="#fff"/>']
    for name in GROUPS:
        for i in c[name]:
            rows.append(f'<rect x="{(i%c["width"])*scale}" y="{(i//c["width"])*scale}" width="{scale}" height="{scale}" fill="{colors[name]}"/>')
    rows.append(f'<text x="10" y="{height+22}" font-family="sans-serif" font-size="14">Water concordance: tile centres, +X right / +Y down; no geographic north assumed.</text>')
    for n, name in enumerate(GROUPS):
        y=height+44+n*20
        rows.append(f'<rect x="10" y="{y-11}" width="12" height="12" fill="{colors[name]}"/><text x="30" y="{y}" font-family="sans-serif" font-size="14">{name}: {len(c[name])}</text>')
    rows.append('</svg>')
    return '\n'.join(rows)
