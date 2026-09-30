"""Four reference-driven refugee props. Run through capture-refugee-props.ps1.
Creates new static meshes in RefugeeProps008; refuses recipe drift on existing assets.
Pure geometry can be validated with ordinary Python. No source asset overwritten.
"""
import hashlib
import math
import random
import os
import json
from pathlib import Path

PKG='/Game/Anastasis/RefugeeProps008'
VERSION='refugee-props-008-v2'
WOOD=(.16,.092,.044,.84)
IRON=(.042,.045,.043,.36)
ROPE=(.29,.22,.12,.92)
CLAY=(.32,.13,.065,.68)
WICKER=(.28,.17,.073,.91)
NAMES=['SM_Amphora_Transport_01','SM_Chest_Travel_01','SM_FishTrap_Wicker_01','SM_Tripod_Cauldron_01']

def add(a,b):return tuple(x+y for x,y in zip(a,b))
def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def mul(a,s):return tuple(x*s for x in a)
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def norm(a):
    d=math.sqrt(sum(x*x for x in a))
    if d<1e-10:raise ValueError('zero normal')
    return mul(a,1/d)
def tint(c,k):return tuple(v*k for v in c[:3])+(c[3],)

class Mesh:
    def __init__(self):self.v=[];self.t=[];self.c=[]
    def vert(self,p,c):
        self.v.append(tuple(p));self.c.append(c);return len(self.v)-1
    def quad(self,a,b,c,d,color):
        ids=[self.vert(p,color) for p in (a,b,c,d)]
        self.t.extend([(ids[0],ids[1],ids[2]),(ids[0],ids[2],ids[3])])
    def tube(self,points,radii,color,sides=8,cap=True):
        rings=[];previous_u=None
        for j,p in enumerate(points):
            tangent=norm(sub(points[min(j+1,len(points)-1)],points[max(0,j-1)]))
            ref=(0,0,1) if abs(tangent[2])<.9 else (1,0,0)
            u=norm(cross(tangent,ref)) if previous_u is None else norm(sub(previous_u,mul(tangent,sum(x*y for x,y in zip(previous_u,tangent)))))
            previous_u=u
            v=cross(tangent,u)
            ring=[]
            for k in range(sides):
                t=math.tau*k/sides
                ring.append(self.vert(add(p,mul(add(mul(u,math.cos(t)),mul(v,math.sin(t))),radii[j])),tint(color,.93+.07*math.cos(t))))
            rings.append(ring)
        for a,b in zip(rings,rings[1:]):
            for k in range(sides):
                l=(k+1)%sides
                self.t.extend([(a[k],a[l],b[k]),(a[l],b[l],b[k])])
        if cap:
            for ring,p,flip in [(rings[0],points[0],True),(rings[-1],points[-1],False)]:
                center=self.vert(p,color)
                for k in range(sides):
                    a,b=ring[k],ring[(k+1)%sides]
                    self.t.append((center,b,a) if flip else (center,a,b))
    def beam(self,a,b,w,h,color,u=None):
        axis=norm(sub(b,a))
        u=norm(u) if u else norm(cross(axis,(0,0,1) if abs(axis[2])<.9 else (1,0,0)))
        v=norm(cross(axis,u))
        corners=[]
        for p in (a,b):
            corners.append([add(p,add(mul(u,x*w/2),mul(v,y*h/2))) for x,y in [(-1,-1),(1,-1),(1,1),(-1,1)]])
        r,s=corners
        self.quad(r[3],r[2],r[1],r[0],color)
        self.quad(s[0],s[1],s[2],s[3],color)
        for k in range(4):
            l=(k+1)%4;self.quad(r[k],r[l],s[l],s[k],tint(color,.94+(.02*k)))
    def lathe(self,profile,color,segments=64,center=(0,0,0),wear=.0):
        rings=[]
        for j,(radius,z) in enumerate(profile):
            ring=[]
            for k in range(segments):
                a=math.tau*k/segments
                factor=1+wear*math.sin(a*7+z*.31)*math.sin(a*3-z*.07)
                c=tint(color,.91+.06*math.sin(z*.35)+.03*math.sin(a*13+z*.9))
                ring.append(self.vert(add(center,(radius*factor*math.cos(a),radius*factor*math.sin(a),z)),c))
            rings.append(ring)
        for a,b in zip(rings,rings[1:]):
            for k in range(segments):
                l=(k+1)%segments;self.t.extend([(a[k],a[l],b[k]),(a[l],b[l],b[k])])
    def finish(self):
        normals=[[0.,0.,0.] for p in self.v]
        for a,b,c in self.t:
            n=cross(sub(self.v[b],self.v[a]),sub(self.v[c],self.v[a]))
            assert sum(x*x for x in n)>1e-13,'degenerate triangle'
            for i in (a,b,c):
                for k in range(3):normals[i][k]+=n[k]
        normals=[norm(n) for n in normals]
        assert all(math.isfinite(x) for p in self.v for x in p)
        # Floor pivot, centred XY. Useful for drag/drop and future instancing.
        lo=[min(p[k] for p in self.v) for k in range(3)]
        hi=[max(p[k] for p in self.v) for k in range(3)]
        offset=((lo[0]+hi[0])/2,(lo[1]+hi[1])/2,lo[2])
        self.v=[sub(p,offset) for p in self.v]
        # Unreal VectorUtil::Normal is edge2.Cross(edge1), not the RH convention
        # used by the generators above. Reverse winding, retain outward normals.
        self.t=[(a,c,b) for a,b,c in self.t]
        for a,b,c in self.t:
            geometric=cross(sub(self.v[c],self.v[a]),sub(self.v[b],self.v[a]))
            summed=add(add(normals[a],normals[b]),normals[c])
            assert sum(x*y for x,y in zip(geometric,summed))>0,('normal/winding disagreement',self.v[a],self.v[b],self.v[c])
        return self.v,self.t,self.c,normals

def circle(center,radius,plane='xy',steps=64):
    out=[]
    for i in range(steps+1):
        a=math.tau*i/steps
        p=(radius*math.cos(a),radius*math.sin(a),0) if plane=='xy' else (radius*math.cos(a),0,radius*math.sin(a))
        out.append(add(center,p))
    return out

def amphora():
    m=Mesh()
    profile=[(.05,0),(7,0),(9,3),(12,8),(16,16),(20,26),(22,35),(22.5,41),(22,47),(20,54),(16,60),(10,65),(8,69),(8,76),(10,77),(10,80),(7.0,80),(6.4,77),(6.2,69),(8,65),(14,59),(18,52),(20,44),(20,35),(17.8,26),(14,17),(10,10),(5,5),(.05,5)]
    m.lathe(profile,CLAY,72,wear=.013)
    # Two open, flattened ear handles connecting neck to shoulder.
    for sign in (-1,1):
        pts=[(sign*x,0,z) for x,z in [(7.5,73),(13,77),(21,76),(28,69),(29,60),(26,53),(20,49)]]
        m.tube(pts,[2.3,2.8,3.0,3.2,3.0,2.6,2.3],tint(CLAY,.85),10)
    # Incised/raised throwing rings and restrained ochre shoulder bands.
    for z,r in [(56,18.9),(58,17.5),(74.5,8.1),(78.5,10.05)]:
        p=circle((0,0,z),r);m.tube(p,[.38]*len(p),(.21,.079,.031,.75),6,False)
    return m.finish()

def chest():
    m=Mesh();rng=random.Random(808)
    # Floor, six front/back boards, side boards; visible seam gaps.
    for x in [-46,-27.5,-9,9.5,28,46.5]:
        m.beam((x,-29,5),(x,29,5),17.6,5,tint(WOOD,rng.uniform(.8,1.18)))
    for z in [10,20,30,40]:
        for y in [-30,30]:m.beam((-55,y,z),(55,y,z),3.4,9.5,tint(WOOD,rng.uniform(.8,1.15)))
        for x in [-54,54]:m.beam((x,-28,z),(x,28,z),3.4,9.5,tint(WOOD,rng.uniform(.8,1.15)))
    for x in [-49,49]:
        for y in [-26,26]:m.beam((x,y,0),(x,y,8),10,10,tint(WOOD,.7))
    # Barrel lid: individual slats follow an elliptical arch.
    for i in range(12):
        a=-math.pi/2+(i+.5)*math.pi/12
        y,z=31*math.sin(a),44+23*math.cos(a)
        u=norm((0,31*math.cos(a),-23*math.sin(a)))
        m.beam((-55,y,z),(55,y,z),7.5,2.7,tint(WOOD,rng.uniform(.82,1.18)),u)
    # End caps assembled from upright boards below the arched lid.
    for x in [-54.5,54.5]:
        for y in [-25,-15,-5,5,15,25]:
            h=23*math.sqrt(max(0,1-(y/31)**2))
            m.beam((x,y,44),(x,y,44+h),2.8,9.4,tint(WOOD,.83),u=(1,0,0))
    for x in [-35,35]:
        pts=[(x,32*math.sin(-math.pi/2+i*math.pi/24),44+24.9*math.cos(-math.pi/2+i*math.pi/24)) for i in range(25)]
        for a,b in zip(pts,pts[1:]):m.beam(a,b,5,.9,IRON,u=(1,0,0))
        for y in [-32,32]:
            m.beam((x,y,6),(x,y,44),5,1.0,IRON,u=(1,0,0))
            for z in [10,23,37]:
                m.tube([(x,y-.3*(1 if y<0 else -1),z),(x,y+1.1*(1 if y>0 else -1),z)],[.9,.9],tint(IRON,1.8),8)
    # Central hasp and staple on front, forged side lifting rings.
    m.beam((0,-32.3,29),(0,-32.3,51),7,1.5,IRON,u=(1,0,0))
    p=[(2.7*math.cos(a),-34,33+4*math.sin(a)) for a in [math.tau*i/24 for i in range(25)]]
    m.tube(p,[.9]*len(p),IRON,7,False)
    for x in [-58,58]:
        p=[(x,6*math.cos(math.tau*i/32),29+6*math.sin(math.tau*i/32)) for i in range(33)]
        m.tube(p,[1]*len(p),IRON,8,False)
        m.beam((x,-4,36),(x,4,36),4,1.5,IRON,u=(1,0,0))
    return m.finish()

def fishtrap():
    m=Mesh()
    # Horizontal wicker body, open mouth at x=-55, closed taper at x=55.
    def radius(x):return 28*(1-((x+55)/110)**2.1)+3
    zc=32
    for k in range(36):
        a=math.tau*k/36
        pts=[]
        for j in range(45):
            x=-55+110*j/44
            r=radius(x)+.48*math.cos(j*math.pi+k*math.pi)
            pts.append((x,r*math.cos(a),zc+r*math.sin(a)))
        m.tube(pts,[.62]*len(pts),tint(WICKER,.82+.15*math.sin(k*1.7)),5)
    for j in range(27):
        x=-55+110*j/26
        pts=[]
        for k in range(145):
            a=math.tau*k/144
            r=radius(x)+.6*(radius(x)/31)**2*math.cos(a*18+j*math.pi)
            pts.append((x,r*math.cos(a),zc+r*math.sin(a)))
        m.tube(pts,[.58*(.4+.6*radius(x)/31)]*len(pts),tint(WICKER,.92+.08*math.sin(j*2.8)),5,False)
    # Inward funnel throat is open and readable from the entrance.
    for k in range(32):
        a=math.tau*k/32
        pts=[(-55+30*t,(30*(1-t)+6*t)*math.cos(a),zc+(30*(1-t)+6*t)*math.sin(a)) for t in [i/12 for i in range(13)]]
        m.tube(pts,[.52]*len(pts),tint(WICKER,.82),5)
    for x,r in [(-55,31),(-52.5,31),(-25,6),(54,3.5)]:
        pts=[(x,r*math.cos(math.tau*k/80),zc+r*math.sin(math.tau*k/80)) for k in range(81)]
        m.tube(pts,[1.1]*len(pts),tint(WICKER,.73),7,False)
    # Carrying cord loop along the upper spine.
    pts=[(-15,0,zc+radius(-15)),(-12,0,76),(4,0,84),(19,0,78),(22,0,zc+radius(22))]
    m.tube(pts,[.9]*len(pts),ROPE,8)
    return m.finish()

def tripod():
    m=Mesh()
    for k in range(3):
        a=math.tau*k/3+.3
        base=(65*math.cos(a),65*math.sin(a),3)
        top=(6*math.cos(a),6*math.sin(a),183+k*2)
        mid=add(mul(base,.46),mul(top,.54))
        m.tube([base,mid,top],[3.2,2.7,1.9],tint(WOOD,.78+k*.1),10)
    # Multiple rope lashings, seated around the converging poles.
    for j in range(7):
        p=circle((0,0,170+j*1.2),9)
        m.tube(p,[.7]*len(p),ROPE,6,False)
    # Open rounded iron cooking pot, with visibly thick rolled rim.
    profile=[(.05,45),(8,45),(17,49),(23,57),(26,66),(26,77),(27.5,79),(27.5,81),(25,81),(24,77),(24,67),(21,59),(15,52),(7,49),(.05,49)]
    m.lathe(profile,IRON,64,wear=.006)
    p=circle((0,0,80),26.5);m.tube(p,[1.5]*len(p),tint(IRON,1.3),8,False)
    # Bail, eyelets, and alternating chain links.
    p=[(27*math.cos(math.pi*i/36),0,80+38*math.sin(math.pi*i/36)) for i in range(37)]
    m.tube(p,[1.05]*len(p),IRON,8)
    for x in [-26,26]:
        p=circle((x,0,79),3.3,'xz',32);m.tube(p,[.9]*len(p),IRON,7,False)
    for j in range(9):
        z=122+j*5.3
        pts=[]
        for k in range(33):
            a=math.tau*k/32
            pts.append((2.0*math.cos(a),0,z+3.4*math.sin(a)) if j%2==0 else (0,2.0*math.cos(a),z+3.4*math.sin(a)))
        m.tube(pts,[.65]*len(pts),tint(IRON,1.2),6,False)
    return m.finish()

def all_geometry():return dict(zip(NAMES,[f() for f in (amphora,chest,fishtrap,tripod)]))

def create():
    import unreal as u
    mel=u.MaterialEditingLibrary;eal=u.EditorAssetLibrary;assets=u.AssetToolsHelpers.get_asset_tools()
    matpath=PKG+'/M_RefugeeProps_Surface'
    mat=eal.load_asset(matpath) if eal.does_asset_exist(matpath) else None
    if not mat:
        mat=assets.create_asset('M_RefugeeProps_Surface',PKG,u.Material,u.MaterialFactoryNew())
        vc=mel.create_material_expression(mat,u.MaterialExpressionVertexColor,-600,0)
        wp=mel.create_material_expression(mat,u.MaterialExpressionWorldPosition,-600,220)
        custom=mel.create_material_expression(mat,u.MaterialExpressionCustom,-280,0)
        custom.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT3)
        inputs=[]
        for name in ['P','C','R']:
            ci=u.CustomInput();ci.set_editor_property('input_name',name);inputs.append(ci)
        custom.set_editor_property('inputs',inputs)
        custom.set_editor_property('code',"""
            float grain=sin(P.x*1.6+sin(P.z*0.9))*sin(P.y*1.9-P.z*1.3);
            float wood=sin(P.x*.24+3*sin(P.z*2.7+sin(P.x*.047)))*sin(P.z*5.2);
            float isWood=step(.80,R)*(1-step(.88,R));
            float n=lerp(grain,wood,isWood);
            return C*(0.88+0.12*n);
        """)
        assert mel.connect_material_expressions(wp,'',custom,'P')
        assert mel.connect_material_expressions(vc,'',custom,'C')
        assert mel.connect_material_expressions(vc,'A',custom,'R')
        assert mel.connect_material_property(custom,'',u.MaterialProperty.MP_BASE_COLOR)
        assert mel.connect_material_property(vc,'A',u.MaterialProperty.MP_ROUGHNESS)
        # Metallic threshold via a second tiny Custom; roughness alpha is the material code.
        metal=mel.create_material_expression(mat,u.MaterialExpressionCustom,-270,330)
        metal.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT1)
        ci=u.CustomInput();ci.set_editor_property('input_name','R');metal.set_editor_property('inputs',[ci])
        metal.set_editor_property('code','return 1-step(0.5,R);')
        assert mel.connect_material_expressions(vc,'A',metal,'R')
        assert mel.connect_material_property(metal,'',u.MaterialProperty.MP_METALLIC)
        errors=list(mel.recompile_material(mat));assert not errors,errors
        eal.set_metadata_tag(mat,'Recipe',VERSION)
        assert eal.save_asset(matpath)
    outputs={};stats={}
    for name,(verts,tris,colors,normals) in all_geometry().items():
        path=PKG+'/'+name
        asset=eal.load_asset(path) if eal.does_asset_exist(path) else None
        if asset and os.environ.get('ANASTASIS_PROPS_REBUILD','0')=='1':
            assert path.startswith(PKG+'/SM_') and name in NAMES
            assert eal.get_metadata_tag(asset,'Recipe') in ('refugee-props-008-v1',VERSION)
            assert eal.delete_asset(path),'Owned asset rebuild failed'
            asset=None
        if asset:
            assert eal.get_metadata_tag(asset,'Recipe')==VERSION,'Existing asset belongs to a different recipe'
            assert asset.get_num_triangles(0)==len(tris),'Existing geometry mismatch'
        else:
            b=u.GeometryScriptSimpleMeshBuffers()
            b.vertices=[u.Vector(*p) for p in verts];b.triangles=[u.IntVector(*t) for t in tris]
            b.normals=[u.Vector(*n) for n in normals];b.vertex_colors=[u.LinearColor(*c) for c in colors]
            b.uv0=[u.Vector2D(p[0]/100,p[2]/100) for p in verts]
            dyn=u.DynamicMesh();u.GeometryScript_MeshEdits.append_buffers_to_mesh(dyn,b)
            opts=u.GeometryScriptCreateNewStaticMeshAssetOptions()
            opts.enable_recompute_normals=False;opts.enable_recompute_tangents=True;opts.enable_nanite=False
            asset,outcome=u.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dyn,path,opts)
            assert asset,str(outcome)
            asset.set_material(0,mat)
            collision=u.get_editor_subsystem(u.StaticMeshEditorSubsystem).add_simple_collisions(asset,u.ScriptCollisionShapeType.NDOP26)
            assert collision>=0,'collision creation failed'
            eal.set_metadata_tag(asset,'Recipe',VERSION)
            assert eal.save_asset(path)
        bounds=asset.get_bounding_box()
        outputs[name]=asset
        stats[name]=dict(path=path,vertices=len(verts),triangles=len(tris),
            size_cm=[bounds.max.x-bounds.min.x,bounds.max.y-bounds.min.y,bounds.max.z-bounds.min.z],
            pivot='bottom centre',material=mat.get_path_name(),collision='single 26-DOP proxy; openings not traversable',lods=asset.get_num_lods())
        u.log('PROPS008 ASSET '+json.dumps(stats[name]))
    return outputs,stats

if __name__=='__main__':
    create()
