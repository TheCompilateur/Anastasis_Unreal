"""River-use proof geometry and verdicts. Metres, explicit bounds, no engine imports."""
import math
from collections import defaultdict


def distance(a,b): return math.hypot(a[0]-b[0],a[1]-b[1])


def segment_distance(p,a,b):
    dx,dy=b[0]-a[0],b[1]-a[1]
    t=max(0.,min(1.,((p[0]-a[0])*dx+(p[1]-a[1])*dy)/(dx*dx+dy*dy))) if dx*dx+dy*dy else 0.
    return distance(p,(a[0]+t*dx,a[1]+t*dy))


class Mesh:
    def __init__(self,vertices,indices):
        self.vertices=vertices; self.triangles=[]; self.bins=defaultdict(list)
        edges=defaultdict(int)
        if len(indices)%3: raise ValueError('Malformed triangle indices')
        for i in range(0,len(indices),3):
            ids=indices[i:i+3]; tri=[vertices[j] for j in ids]
            a,b,c=tri
            den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
            if abs(den)<1e-10: continue
            k=len(self.triangles); self.triangles.append((tri,den))
            for x in range(math.floor(min(v[0] for v in tri)/20),math.floor(max(v[0] for v in tri)/20)+1):
                for y in range(math.floor(min(v[1] for v in tri)/20),math.floor(max(v[1] for v in tri)/20)+1):
                    self.bins[x,y].append(k)
            for j in range(3): edges[tuple(sorted((ids[j],ids[(j+1)%3])))]+=1
        self.boundary=[(vertices[a],vertices[b]) for (a,b),n in edges.items() if n==1]

    def sample(self,x,y):
        best=None
        for k in self.bins.get((math.floor(x/20),math.floor(y/20)),[]):
            (a,b,c),d=self.triangles[k]
            u=((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/d
            v=((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/d
            if min(u,v,1-u-v)<-1e-8: continue
            z=u*a[2]+v*b[2]+(1-u-v)*c[2]
            ux,uy,uz=(b[j]-a[j] for j in range(3)); vx,vy,vz=(c[j]-a[j] for j in range(3))
            slope=math.degrees(math.atan2(math.hypot(uy*vz-uz*vy,uz*vx-ux*vz),abs(ux*vy-uy*vx)))
            if best is None or z>best[0]: best=(z,slope)
        return best

    def near(self,x,y,radius=10.):
        keys=set()
        for ix in range(math.floor((x-radius)/20),math.floor((x+radius)/20)+1):
            for iy in range(math.floor((y-radius)/20),math.floor((y+radius)/20)+1): keys.update(self.bins.get((ix,iy),[]))
        if self.sample(x,y) is not None: return 0.
        d=min((segment_distance((x,y),t[j],t[(j+1)%3]) for k in keys for t in [self.triangles[k][0]] for j in range(3)),default=math.inf)
        return d if d<=radius else None


def physical(ground,waters,x,y):
    g=ground.sample(x,y)
    if g is None: return dict(dry=None,slope=None,z=None)
    levels=[v[0] for mesh in waters if (v:=mesh.sample(x,y)) is not None]
    return dict(dry=not levels or g[0]>=max(levels)+0.1,slope=g[1],z=g[0])


def select(context,ground,lake,river):
    """Nearest physical bank to the selected village, then a dry non-drinkable spawn.
    No filtering of the bank by semantic water or existing navigation reachability.
    """
    site=context['site']['selected']
    if not site['eligible']: raise ValueError('No selected village reference')
    tile=context['tile_m']; w,h=context['width'],context['height']
    origin=((site['x']+.5)*tile,(site['y']+.5)*tile)
    edges=sorted(river.boundary,key=lambda e:distance(origin,((e[0][0]+e[1][0])/2,(e[0][1]+e[1][1])/2)))
    bank=None; witness=None
    for a,b in edges:
        m=((a[0]+b[0])/2,(a[1]+b[1])/2); length=distance(a,b)
        if distance(origin,m)>300: break
        if length<.01: continue
        # Ribbons extend beneath the bank. Their mesh edge is NOT the visible shoreline.
        # Look for a dry/wet transition across the edge normal, testing terrain height.
        for side in (-1,1):
            previous=None
            for offset in range(0,26,2):
                q=(m[0]-(b[1]-a[1])*offset*side/length,m[1]+(b[0]-a[0])*offset*side/length)
                g=ground.sample(*q); water=river.sample(*q); cover=lake.sample(*q)
                wet=(g is not None and water is not None and water[0]>=g[0]+.01
                     and (cover is None or cover[0]<water[0]))
                ph=physical(ground,(lake,river),*q)
                if wet and previous is not None:
                    pp,previous_ph=previous
                    if previous_ph['dry'] is True and previous_ph['slope']<=18:
                        bank,witness=pp,q; break
                previous=(q,ph)
            if bank: break
        if bank: break
    if bank is None: raise ValueError('No dry gentle river bank within 300 m of village')
    options=[]
    for i,c in enumerate(context['cells']):
        p=((i%w+.5)*tile,(i//w+.5)*tile); d=distance(p,bank)
        if not (40<=d<=120) or not c['walkable'] or c['drinkable']: continue
        ph=physical(ground,(lake,river),*p)
        if ph['dry'] is True and ph['slope']<=18: options.append((d,i,p))
    if not options: raise ValueError('No controlled spawn near the selected bank')
    _,index,spawn=min(options)
    return dict(bank_m=bank,water_m=witness,spawn_m=spawn,spawn_sim=(spawn[0]/tile,spawn[1]/tile),
                spawn_index=index,village_m=origin,bank_radius_m=20.,river_tolerance_m=10.)


def verdict(samples,selection,final=False):
    if not samples: return ('UNKNOWN','no_samples')
    first=samples[0]
    if first['at_drink_spot'] or first['drinks']!=0: return ('FAIL','invalid_initial_control')
    recognition=False; walked=0.
    for i,s in enumerate(samples):
        if not s['autonomous'] or s['building_count']!=0 or s['npc_count']!=1:
            return ('FAIL','scenario_contaminated')
        if s['physical']['dry'] is False or s['foot_blocked']:
            return ('FAIL','route_crosses_water_or_blocked_tile')
        if s['physical']['dry'] is None: return ('UNKNOWN','ground_not_observed')
        recognition |= s['goal']=='drink' and s['has_target'] and s['source']=='shore'
        if i:
            prev=samples[i-1]; gap=s['time']-prev['time']
            if gap<=0: return ('FAIL','nonmonotonic_time')
            movement=distance(prev['position_m'],s['position_m'])
            if movement>5.2*s['tile_m']*gap+.5: return ('UNKNOWN','position_jump')
            walked+=movement
            # 4 tiles/s with <=1.3 weather multiplier bounds unobserved movement.
            uncertainty=5.2*s['tile_m']*gap
            if gap>.05+1e-9: return ('UNKNOWN','sampling_gap')
            if s['drinks']>prev['drinks']:
                if not recognition: return ('FAIL','recognition_not_observed')
                if walked<20 or distance(first['position_m'],s['position_m'])<20: return ('FAIL','no_meaningful_journey')
                if s['thirst']>first['thirst']-10: return ('FAIL','drink_without_need_relief')
                if s['river_distance_m'] is None or s['river_distance_m']+uncertainty>selection['river_tolerance_m']:
                    return ('FAIL','drink_away_from_visible_river')
                if distance(s['position_m'],selection['bank_m'])+uncertainty>selection['bank_radius_m']:
                    return ('FAIL','different_bank_selected')
                if s['physical']['slope']>18: return ('FAIL','drink_on_steep_bank')
                return ('PASS','autonomous_river_journey_and_drink')
    if final: return ('UNKNOWN','recognition_not_observed' if not recognition else 'arrival_or_consumption_not_observed')
    return ('PENDING','observing')
