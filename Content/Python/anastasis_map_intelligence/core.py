"""Map Intelligence v1: metres, projected XY areas, explicit missing evidence.

Four-neighbour connectivity; centre samples are resolution-dependent estimates.
No terrain or engine objects are mutated by this module.
"""
from dataclasses import dataclass, asdict, field
from collections import deque
from pathlib import Path
import hashlib
import heapq
import json
import math
import re
import time

VERSION = 1

@dataclass
class Settings:
    step_m: float = 2.0
    relief_radius_m: float = 8.0
    roughness_radius_m: float = 2.0
    settlement_slope_deg: float = 8.0
    agriculture_slope_deg: float = 12.0
    pasture_slope_deg: float = 20.0
    traversable_slope_deg: float = 30.0
    difficult_slope_deg: float = 45.0
    settlement_roughness_m: float = 0.35
    agriculture_roughness_m: float = 0.7
    min_settlement_area_m2: float = 200.0
    min_agriculture_area_m2: float = 100.0
    min_settlement_width_m: float = 8.0
    water_margin_m: float = 0.25
    water_good_distance_m: float = 100.0
    agriculture_radius_m: float = 100.0
    target_settlement_area_m2: float = 10000.0
    target_agriculture_area_m2: float = 20000.0
    max_cells: int = 100000
    max_radius_cells: int = 32
    max_neighborhood_visits: int = 20000000
    max_candidates: int = 12
    max_corridors: int = 64
    max_draw_cells: int = 2500
    max_nav_queries: int = 200000
    nav_extent_xy_m: float = 0.5
    nav_extent_z_m: float = 1.0
    terrain_component: str = 'ExperimentalTerrain'
    terrain_section: int = 0
    water_section: int = 1
    weights: dict = field(default_factory=lambda: dict(flatness=1.0, continuous_area=1.0,
        water_access=1.0, connectivity=1.0, agricultural_area=1.0, terrain_isolation=1.0))

    def validate(self):
        for k, v in asdict(self).items():
            if isinstance(v, (int, float)) and (not math.isfinite(v) or v < 0):
                raise ValueError(f'{k} must be finite and nonnegative')
        for k in ('step_m', 'water_good_distance_m', 'target_settlement_area_m2',
                  'target_agriculture_area_m2', 'max_cells', 'max_radius_cells',
                  'max_candidates', 'max_corridors', 'max_draw_cells', 'max_nav_queries', 'max_neighborhood_visits'):
            if getattr(self, k) <= 0:
                raise ValueError(f'{k} must be positive')
        for k in ('max_cells','max_radius_cells','max_candidates','max_corridors',
                  'max_draw_cells','max_nav_queries','max_neighborhood_visits','terrain_section','water_section'):
            if type(getattr(self, k)) is not int:
                raise ValueError(f'{k} must be an integer')
        slopes = [self.settlement_slope_deg, self.agriculture_slope_deg,
                  self.pasture_slope_deg, self.traversable_slope_deg, self.difficult_slope_deg]
        if slopes != sorted(slopes) or slopes[-1] >= 90:
            raise ValueError('Slope thresholds must be ordered and below 90 degrees')
        expected = {'flatness','continuous_area','water_access','connectivity',
                    'agricultural_area','terrain_isolation'}
        if set(self.weights) != expected or not all(math.isfinite(v) and v >= 0 for v in self.weights.values()) or sum(self.weights.values()) <= 0:
            raise ValueError('Six named finite nonnegative weights required; sum must be positive')
        return self

@dataclass
class Grid:
    nx: int
    ny: int
    x0: float
    y0: float
    dx: float
    dy: float
    z: list
    gradients: list
    water_z: list
    nav: list
    nav_edges: set = field(default_factory=set)

    @classmethod
    def covering(cls, bounds, settings):
        settings.validate()
        x0, y0, x1, y1 = bounds
        if not all(math.isfinite(v) for v in bounds) or x1 <= x0 or y1 <= y0:
            raise ValueError('Nonempty finite XY bounds required')
        nx, ny = math.ceil((x1-x0)/settings.step_m), math.ceil((y1-y0)/settings.step_m)
        if nx*ny > settings.max_cells:
            raise ValueError(f'{nx*ny} cells exceeds max_cells={settings.max_cells}; increase step_m')
        n = nx*ny
        return cls(nx, ny, x0, y0, (x1-x0)/nx, (y1-y0)/ny,
                   [None]*n, [None]*n, [None]*n, [None]*n)

    def xy(self, i):
        return self.x0+(i%self.nx+.5)*self.dx, self.y0+(i//self.nx+.5)*self.dy

    def neighbours(self, i):
        x, y = i%self.nx, i//self.nx
        if x: yield i-1
        if x+1 < self.nx: yield i+1
        if y: yield i-self.nx
        if y+1 < self.ny: yield i+self.nx

    def distance(self, a, b):
        ax, ay = self.xy(a); bx, by = self.xy(b)
        return math.hypot(ax-bx, ay-by)

    def identity(self):
        return {k: getattr(self, k) for k in ('nx','ny','x0','y0','dx','dy')}


def rasterize(grid, vertices, triangles, water=False):
    """Barycentric centre sampling of world-space triangles in metres.
    Highest surface wins, triangle derivatives retained (no vertex-normal smoothing).
    Missing coverage remains None; vertical triangles have no projected area.
    """
    target = grid.water_z if water else grid.z
    for t in range(0, len(triangles), 3):
        a,b,c = (vertices[triangles[t+j]] for j in range(3))
        ux,uy,uz = (b[j]-a[j] for j in range(3))
        vx,vy,vz = (c[j]-a[j] for j in range(3))
        det = ux*vy-uy*vx
        if abs(det) < 1e-12: continue
        gx,gy = (uz*vy-uy*vz)/det, (ux*vz-uz*vx)/det
        lo_x = max(0, math.ceil((min(a[0],b[0],c[0])-grid.x0)/grid.dx-.5-1e-8))
        hi_x = min(grid.nx-1, math.floor((max(a[0],b[0],c[0])-grid.x0)/grid.dx-.5+1e-8))
        lo_y = max(0, math.ceil((min(a[1],b[1],c[1])-grid.y0)/grid.dy-.5-1e-8))
        hi_y = min(grid.ny-1, math.floor((max(a[1],b[1],c[1])-grid.y0)/grid.dy-.5+1e-8))
        for y in range(lo_y, hi_y+1):
            for x in range(lo_x, hi_x+1):
                i = y*grid.nx+x
                px,py = grid.xy(i); px-=a[0]; py-=a[1]
                u,v = (px*vy-py*vx)/det, (ux*py-uy*px)/det
                if u < -1e-7 or v < -1e-7 or u+v > 1+1e-7: continue
                z = a[2]+u*uz+v*vz
                if target[i] is None or z > target[i]:
                    target[i] = z
                    if not water: grid.gradients[i] = (gx,gy)


def edge_ok(g, slopes, a, b, threshold):
    return (slopes[a] is not None and slopes[b] is not None and
            max(slopes[a],slopes[b], math.degrees(math.atan2(abs(g.z[a]-g.z[b]),g.distance(a,b)))) <= threshold)


def components(g, mask, edge=None):
    labels = [-1]*len(mask); groups=[]
    for start, allowed in enumerate(mask):
        if not allowed or labels[start]>=0: continue
        idx=len(groups); labels[start]=idx; group=[]; queue=deque([start])
        while queue:
            i=queue.popleft(); group.append(i)
            for j in g.neighbours(i):
                if mask[j] and labels[j]<0 and (edge is None or edge(i,j)):
                    labels[j]=idx; queue.append(j)
        groups.append(group)
    return labels,groups


def distance_field(g, sources):
    """8-neighbour grid approximation of horizontal distance; not shoreline precision."""
    dist=[math.inf]*len(g.z); heap=[]
    for i in sources: dist[i]=0.; heap.append((0.,i))
    heapq.heapify(heap)
    while heap:
        d,i=heapq.heappop(heap)
        if d!=dist[i]: continue
        x,y=i%g.nx,i//g.nx
        for ox,oy in ((1,0),(-1,0),(0,1),(0,-1),(1,1),(-1,1),(1,-1),(-1,-1)):
            if 0<=x+ox<g.nx and 0<=y+oy<g.ny:
                j=i+ox+oy*g.nx; nd=d+math.hypot(ox*g.dx,oy*g.dy)
                if nd<dist[j]: dist[j]=nd; heapq.heappush(heap,(nd,j))
    return [d if math.isfinite(d) else None for d in dist]


def metrics(g,s):
    n=len(g.z); slopes=[None]*n; rough=[None]*n; relief=[None]*n
    radius=max(s.relief_radius_m,s.roughness_radius_m)
    rx,ry=math.ceil(radius/g.dx),math.ceil(radius/g.dy)
    if n*(2*rx+1)*(2*ry+1)>s.max_neighborhood_visits:
        raise ValueError('Neighbourhood work budget exceeded; increase step or reduce radii')
    if max(rx,ry)>s.max_radius_cells:
        raise ValueError('Relief neighbourhood exceeds max_radius_cells; increase step or reduce radius')
    gradients=list(g.gradients)
    for i,z in enumerate(g.z):
        if z is None: continue
        x,y=i%g.nx,i//g.nx
        if gradients[i] is None:
            derivatives=[]
            for lo,hi,step in ((i-1 if x else None,i+1 if x+1<g.nx else None,g.dx),
                (i-g.nx if y else None,i+g.nx if y+1<g.ny else None,g.dy)):
                lo=lo if lo is not None and g.z[lo] is not None else i
                hi=hi if hi is not None and g.z[hi] is not None else i
                derivatives.append((g.z[hi]-g.z[lo])/(step*((lo!=i)+(hi!=i))) if lo!=hi else None)
            if None in derivatives: continue
            gradients[i]=derivatives
        gx,gy=gradients[i]
        slopes[i]=math.degrees(math.atan(math.hypot(gx,gy)))
        neighbours=[j for j in g.neighbours(i) if g.z[j] is not None]
        # Maximum edge grade catches discontinuities hidden by central derivatives.
        if neighbours:
            slopes[i]=max(slopes[i], max(math.degrees(math.atan2(abs(g.z[j]-z),g.distance(i,j))) for j in neighbours))
        px,py=g.xy(i); residual=[]; heights=[]
        for yy in range(max(0,y-ry), min(g.ny,y+ry+1)):
            for xx in range(max(0,x-rx),min(g.nx,x+rx+1)):
                j=yy*g.nx+xx
                if g.z[j] is None: continue
                qx,qy=g.xy(j)
                distance=max(abs(qx-px),abs(qy-py))
                if distance<=s.relief_radius_m+1e-8: heights.append(g.z[j])
                if distance<=s.roughness_radius_m+1e-8: residual.append(g.z[j]-z-gx*(qx-px)-gy*(qy-py))
        rough[i]=math.sqrt(sum(v*v for v in residual)/len(residual)) if residual else 0.
        relief[i]=max(heights)-min(heights) if heights else 0.
    return slopes,rough,relief


def region(g, cells, slopes, rough, water_dist, nav_labels):
    area=len(cells)*g.dx*g.dy; xs=[g.xy(i)[0] for i in cells]; ys=[g.xy(i)[1] for i in cells]
    cx,cy=sum(xs)/len(xs),sum(ys)/len(ys)
    length=max(max(xs)-min(xs)+g.dx, max(ys)-min(ys)+g.dy)
    water=[water_dist[i] for i in cells if water_dist[i] is not None]
    nav_ids=sorted({nav_labels[i] for i in cells if nav_labels[i]>=0})
    return dict(area_m2=area, area_ha=area/10000, length_m=length,
        approximate_width_m=area/length, centre_m=[sum(xs)/len(xs),sum(ys)/len(ys),sum(g.z[i] for i in cells)/len(cells)],
        slope_mean_deg=sum(slopes[i] for i in cells)/len(cells), slope_max_deg=max(slopes[i] for i in cells),
        roughness_mean_m=sum(rough[i] for i in cells)/len(cells),
        water_distance_m=min(water) if water else None,
        nav_sample_fraction=sum(g.nav[i] is True for i in cells)/len(cells) if any(g.nav[i] is not None for i in cells) else None,
        nav_components=nav_ids, representative_cell=min(cells,key=lambda i:(g.xy(i)[0]-cx)**2+(g.xy(i)[1]-cy)**2))


def corridors(g,s,labels,walk,slopes):
    """Multi-source least-cost grid fronts. Reports potential corridors, never roads.
    Cost = length * (1 + slope / traversable_threshold)^2.
    No diagonal shortcuts; includes edge grade checks.
    """
    n=len(g.z); owner=[-1]*n; costs=[math.inf]*n; parent=[-1]*n; heap=[]; meetings={}
    for i,label in enumerate(labels):
        if label>=0: owner[i]=label; costs[i]=0.; heap.append((0.,i))
    heapq.heapify(heap)
    while heap:
        cost,i=heapq.heappop(heap)
        if cost!=costs[i]: continue
        for j in g.neighbours(i):
            if not walk[j] or not edge_ok(g,slopes,i,j,s.traversable_slope_deg): continue
            length=g.distance(i,j)
            w=length*(1+max(slopes[i],slopes[j])/max(s.traversable_slope_deg,.001))**2
            if owner[j]>=0 and owner[j]!=owner[i]:
                key=tuple(sorted((owner[i],owner[j]))); total=cost+costs[j]+w
                if key not in meetings or total<meetings[key][0]: meetings[key]=(total,i,j)
            elif cost+w<costs[j]:
                costs[j]=cost+w; owner[j]=owner[i]; parent[j]=i; heapq.heappush(heap,(costs[j],j))
    # Grid distance to non-traversable centres or map edge; width is approximate.
    blocked=[i for i,v in enumerate(walk) if not v or i%g.nx in (0,g.nx-1) or i//g.nx in (0,g.ny-1)]
    clearance=distance_field(g,blocked)
    found=[]
    for key,(cost,a,b) in sorted(meetings.items(),key=lambda p:(p[1][0],p[0]))[:s.max_corridors]:
        def chain(i):
            path=[]
            while i>=0: path.append(i); i=parent[i]
            return path
        path=list(reversed(chain(a)))+chain(b)
        bottleneck=min(path,key=lambda i:clearance[i] if clearance[i] is not None else math.inf)
        found.append(dict(basins=list(key), weighted_cost_m=cost, path_cells=path,
            length_m=sum(g.distance(a,b) for a,b in zip(path,path[1:])),
            maximum_elevation_m=max(g.z[i] for i in path), maximum_slope_deg=max(slopes[i] for i in path),
            bottleneck_cell=bottleneck, approximate_bottleneck_width_m=2*(clearance[bottleneck] or 0),
            nav_verified=all(g.nav[i] is True for i in path) and all(tuple(sorted((a,b))) in g.nav_edges for a,b in zip(path,path[1:])),
            evidence='potential terrain-grid corridor; no road or pass classification'))
    return found, len(meetings)>s.max_corridors


def analyze(g,s,provenance=None):
    start=time.perf_counter(); s.validate(); n=len(g.z)
    if n!=g.nx*g.ny or any(len(a)!=n for a in (g.gradients,g.water_z,g.nav)):
        raise ValueError('Grid shape mismatch')
    if n>s.max_cells: raise ValueError('max_cells exceeded')
    if not any(z is not None for z in g.z): raise ValueError('No terrain samples')
    if any(v is not None and not math.isfinite(v) for v in g.z+g.water_z): raise ValueError('Nonfinite sample')
    slopes,rough,relief=metrics(g,s)
    dry=[z is not None and (g.water_z[i] is None or z>=g.water_z[i]+s.water_margin_m) for i,z in enumerate(g.z)]
    known=[dry[i] and slopes[i] is not None for i in range(n)]
    water_sources=[i for i in range(n) if g.water_z[i] is not None and g.z[i] is not None and g.water_z[i]>=g.z[i]]
    water_dist=distance_field(g,water_sources)
    nav_labels,nav_groups=components(g,[v is True for v in g.nav],lambda a,b:tuple(sorted((a,b))) in g.nav_edges)
    habitat=[known[i] and slopes[i]<=s.settlement_slope_deg and rough[i]<=s.settlement_roughness_m for i in range(n)]
    agriculture=[known[i] and slopes[i]<=s.agriculture_slope_deg and rough[i]<=s.agriculture_roughness_m for i in range(n)]
    walk=[known[i] and slopes[i]<=s.traversable_slope_deg for i in range(n)]
    _,raw_groups=components(g,habitat,lambda a,b:edge_ok(g,slopes,a,b,s.settlement_slope_deg))
    ag_labels,ag_groups=components(g,agriculture,lambda a,b:edge_ok(g,slopes,a,b,s.agriculture_slope_deg))
    for cells in ag_groups:
        if len(cells)*g.dx*g.dy<s.min_agriculture_area_m2:
            for i in cells: agriculture[i]=False
    groups=[]; regions=[]; labels=[-1]*n
    for cells in raw_groups:
        r=region(g,cells,slopes,rough,water_dist,nav_labels)
        if r['area_m2']<s.min_settlement_area_m2 or r['approximate_width_m']<s.min_settlement_width_m: continue
        r['id']=len(groups)
        for i in cells: labels[i]=r['id']
        groups.append(cells); regions.append(r)
    walk_labels,walk_groups=components(g,walk,lambda a,b:edge_ok(g,slopes,a,b,s.traversable_slope_deg))
    paths,truncated=corridors(g,s,labels,walk,slopes)
    by_walk={}
    for r,cells in zip(regions,groups): by_walk.setdefault(walk_labels[cells[0]],[]).append(r['id'])
    # Number of separate boundary portals, not number of neighbours or boundary cells.
    for r,cells in zip(regions,groups):
        boundary=set(j for i in cells for j in g.neighbours(i) if walk[j] and labels[j]!=r['id'] and edge_ok(g,slopes,i,j,s.traversable_slope_deg))
        count=0
        while boundary:
            count+=1; todo=[boundary.pop()]
            while todo:
                for j in g.neighbours(todo.pop()):
                    if j in boundary: boundary.remove(j); todo.append(j)
        r['natural_exit_portals']=count
        r['other_basins_reachable_by_terrain']=len(by_walk[walk_labels[cells[0]]])-1
    categories=[]
    for i in range(n):
        if g.z[i] is None or slopes[i] is None: category='UNKNOWN'
        elif not dry[i]: category='WATER_OR_MARGIN'
        elif labels[i]>=0: category='PRIME_SETTLEMENT'
        elif agriculture[i]: category='AGRICULTURAL_CANDIDATE'
        elif slopes[i]<=s.pasture_slope_deg: category='PASTURE_SECONDARY'
        elif walk[i]: category='GENERAL_TRAVERSABLE'
        elif slopes[i]<=s.difficult_slope_deg: category='DIFFICULT'
        else: category='SEVERE'
        categories.append(category)
    candidates=[]; candidate_visits=0
    for r,cells in zip(regions,groups):
        cx,cy,_=r['centre_m']; radius=s.agriculture_radius_m
        xlo=max(0,int((cx-radius-g.x0)/g.dx)); xhi=min(g.nx-1,int((cx+radius-g.x0)/g.dx))
        ylo=max(0,int((cy-radius-g.y0)/g.dy)); yhi=min(g.ny-1,int((cy+radius-g.y0)/g.dy))
        candidate_visits+=(xhi-xlo+1)*(yhi-ylo+1)
        if candidate_visits>s.max_neighborhood_visits: raise ValueError('Candidate work budget exceeded; increase step or minimum basin area')
        ag=sum(g.dx*g.dy for y in range(ylo,yhi+1) for x in range(xlo,xhi+1)
            if agriculture[y*g.nx+x] and labels[y*g.nx+x]!=r['id'] and math.hypot(g.xy(y*g.nx+x)[0]-cx,g.xy(y*g.nx+x)[1]-cy)<=radius)
        reachable=r['other_basins_reachable_by_terrain']; nav_fraction=r['nav_sample_fraction']
        nav_connected=None if nav_fraction is None else max((len(nav_groups[k])/max(1,sum(v is True for v in g.nav)) for k in r['nav_components']),default=0.)
        scores=dict(flatness=max(0.,1-r['slope_mean_deg']/max(s.settlement_slope_deg,.001)),
            continuous_area=min(1.,r['area_m2']/s.target_settlement_area_m2),
            water_access=None if r['water_distance_m'] is None else max(0.,1-r['water_distance_m']/s.water_good_distance_m),
            connectivity=nav_connected, agricultural_area=min(1.,ag/s.target_agriculture_area_m2),
            terrain_isolation=reachable/(len(regions)-1) if len(regions)>1 else None)
        active=sum(s.weights[k] for k,v in scores.items() if v is not None)
        constraints=[f'{k}: UNKNOWN' for k,v in scores.items() if v is None]
        if reachable==0: constraints.append('No other qualifying basin reachable on sampled terrain graph')
        if ag<s.target_agriculture_area_m2: constraints.append('Surrounding agriculture below design target')
        candidates.append(dict(**r, agricultural_area_around_m2=ag, subscores=scores,
            score=None if active==0 else sum(v*s.weights[k] for k,v in scores.items() if v is not None)/active*100,
            score_evidence_weight_fraction=active/sum(s.weights.values()), constraints=constraints))
    candidates.sort(key=lambda c:(-(c['score'] if c['score'] is not None else -1),-c['area_m2'],c['id']))
    for rank,c in enumerate(candidates): c['name']=f'Candidate_{rank+1:02}'
    sampled=sum(z is not None for z in g.z); valid=sum(known); cell_area=g.dx*g.dy
    def pct(count): return 100*count/sampled if sampled else None
    stats=dict(footprint_m2=n*cell_area, sampled_terrain_m2=sampled*cell_area,
        sample_coverage_fraction=sampled/n, dry_analyzable_m2=valid*cell_area,
        water_or_margin_m2=sum(z is not None and not dry[i] for i,z in enumerate(g.z))*cell_area,
        unknown_slope_m2=sum(g.z[i] is not None and slopes[i] is None for i in range(n))*cell_area,
        low_slope_pct=pct(sum(known[i] and slopes[i]<=s.settlement_slope_deg for i in range(n))),
        medium_slope_pct=pct(sum(known[i] and s.settlement_slope_deg<slopes[i]<=s.traversable_slope_deg for i in range(n))),
        steep_slope_pct=pct(sum(known[i] and slopes[i]>s.traversable_slope_deg for i in range(n))),
        navigable_pct=pct(sum(v is True for v in g.nav)) if any(v is not None for v in g.nav) else None,
        habitat_pct=pct(sum(i>=0 for i in labels)), agriculture_pct=pct(sum(agriculture)),
        terrain_traversable_pct=pct(sum(walk)), largest_habitable_area_m2=max((r['area_m2'] for r in regions),default=0),
        significant_basins=len(regions), habitat_fragments_before_area_filter=len(raw_groups),
        isolated_basins=sum(r['other_basins_reachable_by_terrain']==0 for r in regions),
        terrain_components=len(walk_groups), nav_components=len(nav_groups) if any(v is not None for v in g.nav) else None,
        largest_terrain_component_m2=max((len(c)*cell_area for c in walk_groups),default=0))
    provenance=provenance or {}
    identity=dict(version=VERSION,grid=g.identity(),settings=asdict(s),
        map=provenance.get('map'),source=provenance.get('source'),
        sampling=provenance.get('sampling'),coverage=provenance.get('coverage','loaded_only'),
        loaded_target_paths=sorted(t['path'] for t in provenance.get('targets',[])),
        engine=provenance.get('engine'),world_partition=provenance.get('world_partition'),
        nav_agent=provenance.get('navigation',{}).get('agent'))
    key=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()
    sample_hash=hashlib.sha256(json.dumps(dict(z=g.z,gradients=g.gradients,water=g.water_z,nav=g.nav,edges=sorted(g.nav_edges)),sort_keys=True).encode()).hexdigest()
    return dict(version=VERSION,comparison_key=key,comparison_identity=identity,sample_sha256=sample_hash,
        provenance=provenance,settings=asdict(s),grid=g.identity(),statistics=stats,regions=regions,
        candidates=candidates[:s.max_candidates],corridors=paths,corridors_truncated=truncated,
        cells=dict(elevation_m=g.z,slope_deg=slopes,roughness_m=rough,relief_m=relief,
            water_distance_m=water_dist,water_surface_m=g.water_z,navigable=g.nav,habitat_region=labels,
            terrain_component=walk_labels,nav_component=nav_labels,agriculture=agriculture,category=categories),
        analysis_seconds=time.perf_counter()-start,
        limitations=['Projected XY areas; configurable design thresholds, not human carrying capacity.',
            'Centre-sampled terrain; sub-grid obstacles, soil, fertility, flood risk and ownership are not assessed.',
            'Only loaded actors/collision are observed. No automatic World Partition loading.',
            'Terrain connectivity is a grid hypothesis; navigation uses the loaded default nav agent.',
            'Water distance is an 8-neighbour grid approximation from observed wet samples.',
            'Scores renormalize available weights; compare evidence coverage as well as score.'])


def compare(before,after):
    if before['version']!=VERSION or after['version']!=VERSION or before['comparison_key']!=after['comparison_key']:
        raise ValueError('INCOMPARABLE: map, coverage, sampling, grid or settings differ; rerun both with identical parameters')
    a,b=before['statistics'],after['statistics']
    rows={k:dict(before=a[k],after=b[k],delta=None if a[k] is None or b[k] is None else b[k]-a[k]) for k in a}
    return dict(version=VERSION,comparison_key=before['comparison_key'],
        before_sample_sha256=before['sample_sha256'],after_sample_sha256=after['sample_sha256'],statistics=rows,
        caveat='Paired descriptive comparison; does not establish a causal treatment effect or gameplay viability.')


def markdown(report):
    lines=['# ANASTASIS Map Intelligence','', 'Geometric suitability estimates. UNKNOWN remains unknown.', '']
    if 'candidates' not in report:
        lines+=['| Metric | Before | After | Delta |','|---|---:|---:|---:|']
        for k,v in report['statistics'].items(): lines.append(f"| {k} | {v['before']} | {v['after']} | {v['delta']} |")
        lines+=['',report['caveat']]
    else:
        lines+=['## Global statistics','','Percentages use sampled terrain XY area as denominator. Water/margin and unknown slope are excluded from slope bins.','', '| Metric | Value |','|---|---:|']
        for k,v in report['statistics'].items(): lines.append(f'| {k} | {v if v is not None else "UNKNOWN"} |')
        for c in report['candidates']:
            lines+=['',f"## {c['name']}",'',f"Score: {c['score']} / 100; evidence weight coverage: {c['score_evidence_weight_fraction']:.1%}",'']
            for k,v in c.items(): lines.append(f'- {k}: {v}')
        lines+=['','## Corridors','',f"{len(report['corridors'])} potential corridors; truncated={report['corridors_truncated']}",'','## Limits','']
        lines += ['- '+v for v in report['limitations']]
        lines+=['','## Provenance','', '```json',json.dumps(report['provenance'],indent=2,ensure_ascii=False),'```', '',f"Comparison key: {report['comparison_key']}",f"Samples SHA256: {report['sample_sha256']}", f"Core analysis: {report['analysis_seconds']:.3f} s"]
    return '\n'.join(lines)+'\n'


def export(report,directory,name,overwrite=False):
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,100}',name): raise ValueError('Invalid snapshot name')
    directory=Path(directory); directory.mkdir(parents=True,exist_ok=True)
    paths=[directory/(name+'.json'),directory/(name+'.md')]
    if not overwrite and any(p.exists() for p in paths): raise FileExistsError('Snapshot exists; choose a new name')
    # Serialize before opening either output, and reject NaN/Infinity.
    payload=json.dumps(report,ensure_ascii=False,indent=2,allow_nan=False)
    prose=markdown(report)
    for p,data in zip(paths,(payload,prose)):
        with p.open('w' if overwrite else 'x',encoding='utf-8') as f: f.write(data)
    return [str(p) for p in paths]
