"""REED_FORM_004. Six curved canes with folded lanceolate leaves and open panicles.
Editor recipe. Creates a NEW review mesh/material; never overwrites Ecotone or trees.
Pure geometry stays importable without Unreal for topology validation.
"""
import math
import random

PACKAGE='/Game/Anastasis/EcotoneReview004'
MESH=PACKAGE+'/SM_Reed_Curved_01'
MAT=PACKAGE+'/M_Reed_Static_01'
SOURCE_MAT='/Game/Anastasis/Materials/M_AnastasisVegetation'


def sub(a,b): return tuple(x-y for x,y in zip(a,b))
def cross(a,b): return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def norm(a):
    d=math.sqrt(sum(x*x for x in a))
    assert d>1e-10
    return tuple(x/d for x in a)


def geometry():
    rng=random.Random(4004)
    verts=[]; tris=[]; colors=[]
    def vertex(p,c):
        verts.append(p); colors.append(c); return len(verts)-1
    def tube(points,radii,color,sides=5):
        rings=[]
        for j,p in enumerate(points):
            tangent=norm(sub(points[min(j+1,len(points)-1)],points[max(0,j-1)]))
            ref=(0,0,1) if abs(tangent[2])<0.9 else (1,0,0)
            u=norm(cross(tangent,ref)); v=cross(tangent,u)
            ring=[]
            for k in range(sides):
                t=math.tau*k/sides
                ring.append(vertex(tuple(p[n]+radii[j]*(math.cos(t)*u[n]+math.sin(t)*v[n]) for n in range(3)),color))
            rings.append(ring)
        for j in range(len(rings)-1):
            for k in range(sides):
                a,b=rings[j][k],rings[j][(k+1)%sides]
                c,d=rings[j+1][k],rings[j+1][(k+1)%sides]
                tris.extend(((a,b,c),(b,d,c)))
        for ring,flip,p in ((rings[0],True,points[0]),(rings[-1],False,points[-1])):
            centre=vertex(p,color)
            for k in range(sides):
                a,b=ring[k],ring[(k+1)%sides]
                tris.append((centre,b,a) if flip else (centre,a,b))
    for stem in range(6):
        az=math.tau*stem/6+rng.uniform(-0.2,0.2)
        root_radius=rng.uniform(2,10)
        rx,ry=root_radius*math.cos(az),root_radius*math.sin(az)
        h=rng.uniform(138,181); bend=rng.uniform(10,22)
        direction=az+rng.uniform(-0.65,0.65)
        dx,dy=math.cos(direction),math.sin(direction)
        stem_color=(0.105+rng.uniform(-.012,.012),0.145,0.058,1)
        def axis(t): return (rx+bend*t*t*dx,ry+bend*t*t*dy,h*t)
        tube([axis(j/8) for j in range(9)],[.66*(1-j/11) for j in range(9)],stem_color,6)
        for leaf in range(4):
            node=.20+leaf*.155+rng.uniform(-.025,.025)
            base=axis(node)
            la=az+leaf*2.399+rng.uniform(-.35,.35)
            lx,ly=math.cos(la),math.sin(la)
            length=rng.uniform(32,54)*(1-.1*leaf)
            width=rng.uniform(1.7,2.7)
            rise=rng.uniform(10,19)
            rows=[]
            for j in range(9):
                t=j/8
                w=max(.025,width*math.sin(math.pi*t)**.8)
                z=base[2]+rise*math.sin(math.pi*t)-10*t*t
                row=[]
                for side in (-1,0,1):
                    # A folded central ridge gives the blade volume and readable shading.
                    p=(base[0]+lx*length*t-ly*side*w,
                       base[1]+ly*length*t+lx*side*w,z+(1-abs(side))*1.0*math.sin(math.pi*t))
                    fade=.75 if side else 1.08
                    color=(.09*fade+.025*t,.135*fade+.012*t,.047*fade+.007*t,1)
                    row.append(vertex(p,color))
                rows.append(row)
            for j in range(8):
                for k in range(2):
                    a,b=rows[j][k:k+2]; c,d=rows[j+1][k:k+2]
                    tris.extend(((a,c,b),(b,c,d)))
        # Branched seed head: actual slender geometry, no opaque spherical cap.
        head_color=(.19,.135,.064,1)
        for tier in range(6):
            t=.80+.033*tier
            start=axis(t)
            for branch in range(3):
                a=az+tier*1.37+branch*math.tau/3
                reach=(10-1.1*tier)*rng.uniform(.8,1.15)
                end=(start[0]+reach*math.cos(a),start[1]+reach*math.sin(a),start[2]+6.5)
                mid=tuple((start[k]+end[k])*.5 for k in range(3))
                tube([start,mid,end],[.24,.34,.08],head_color,3)
    # Surface normals and topology checks, before entering the engine.
    normals=[[0.,0.,0.] for _ in verts]
    for a,b,c in tris:
        n=cross(sub(verts[b],verts[a]),sub(verts[c],verts[a]))
        assert sum(x*x for x in n)>1e-12, 'degenerate triangle'
        for i in (a,b,c):
            for k in range(3): normals[i][k]+=n[k]
    normals=[norm(n) for n in normals]
    assert min(p[2] for p in verts)>=-0.15
    return verts,tris,colors,normals


def create():
    import unreal
    if unreal.EditorAssetLibrary.does_asset_exist(MESH):
        raise RuntimeError('Review mesh exists; refuse to overwrite it. Load it for capture.')
    source=unreal.EditorAssetLibrary.load_asset(SOURCE_MAT)
    assert source is not None
    mat=unreal.EditorAssetLibrary.load_asset(MAT)
    if mat is None:
        mat=unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MAT,MAT)
        assert mat is not None
        mel=unreal.MaterialEditingLibrary
        zero=mel.create_material_expression(mat,unreal.MaterialExpressionConstant,300,1050)
        zero.set_editor_property('r',0.0)
        assert mel.connect_material_property(zero,'',unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
        mel.recompile_material(mat)
        assert unreal.EditorAssetLibrary.save_asset(MAT)
    verts,tris,colors,normals=geometry()
    buffers=unreal.GeometryScriptSimpleMeshBuffers()
    buffers.vertices=[unreal.Vector(*p) for p in verts]
    buffers.triangles=[unreal.IntVector(*t) for t in tris]
    buffers.normals=[unreal.Vector(*n) for n in normals]
    buffers.vertex_colors=[unreal.LinearColor(*c) for c in colors]
    buffers.uv0=[unreal.Vector2D(p[0]/100,p[2]/180) for p in verts]
    dyn=unreal.DynamicMesh()
    result=unreal.GeometryScript_MeshEdits.append_buffers_to_mesh(dyn,buffers)
    options=unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    options.enable_recompute_normals=False
    options.enable_recompute_tangents=True
    options.enable_nanite=False
    asset,outcome=unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dyn,MESH,options)
    assert asset is not None, str(outcome)
    asset.set_material(0,mat)
    assert unreal.EditorAssetLibrary.save_asset(MESH)
    bounds=asset.get_bounding_box()
    unreal.log('REED_FORM_CREATED vertices=%d triangles=%d height=%.3f width=%.3f'%(
        len(verts),len(tris),bounds.max.z-bounds.min.z,max(bounds.max.x-bounds.min.x,bounds.max.y-bounds.min.y)))
    return asset,mat


if __name__=='__main__':
    create()
