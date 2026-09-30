"""Reference-driven static canvas shelter. Run capture-camp-shelter.ps1.
New assets only: /Game/Anastasis/CampShelter009. No map saved.
Uses the proven mesh helpers from create-refugee-props.py, not its asset builder.
"""
import os, math, json, importlib.util
from pathlib import Path
spec=importlib.util.spec_from_file_location('props_mesh',Path(__file__).with_name('create-refugee-props.py'))
g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g)
PKG='/Game/Anastasis/CampShelter009'
VERSION='camp-shelter-009-v2'
NAME='SM_Shelter_PatchedCanvas_01'
WOOD=(.115,.073,.039,.84)
ROPE=(.27,.205,.125,.92)
LINEN=(.37,.315,.23,.97)
PATCH=(.245,.23,.186,.97)
RED=(.17,.065,.04,.97)

def roof(x,y):
    t=abs(x)/216.
    z=268-76*t-9*math.sin(math.pi*t)-12*math.sin(math.pi*(y+172)/344)*(.25+.75*t)
    z+=1.7*math.sin(x*.17+y*.015)*math.sin(math.pi*t)
    return (x,y,z)

def sheet(m,fn,nu,nv,color,thickness=.36):
    grids=[]
    for side in [1,-1]:
        rows=[]
        for j in range(nv+1):
            row=[]
            for i in range(nu+1):
                a=i/nu;b=j/nv;p=fn(a,b)
                du=g.sub(fn(a+.0001,b),fn(a-.0001,b))
                dv=g.sub(fn(a,b+.0001),fn(a,b-.0001))
                n=g.norm(g.cross(du,dv))
                p=g.add(p,g.mul(n,side*thickness/2))
                row.append(m.vert(p,color(a,b)))
            rows.append(row)
        grids.append(rows)
        for j in range(nv):
            for i in range(nu):
                ids=[rows[j][i],rows[j][i+1],rows[j+1][i+1],rows[j+1][i]]
                if side<0:ids.reverse()
                m.t.extend([(ids[0],ids[1],ids[2]),(ids[0],ids[2],ids[3])])
    top,bottom=grids
    perimeter=[(i,0) for i in range(nu+1)]+[(nu,j) for j in range(1,nv+1)]+[(i,nv) for i in range(nu-1,-1,-1)]+[(0,j) for j in range(nv-1,0,-1)]
    for (i,j),(k,l) in zip(perimeter,perimeter[1:]+perimeter[:1]):
        m.quad(m.v[top[j][i]],m.v[bottom[j][i]],m.v[bottom[l][k]],m.v[top[l][k]],color(i/nu,j/nv))

def path(m,fn,steps,radius,color,sides=6):
    p=[fn(i/steps) for i in range(steps+1)]
    m.tube(p,[radius]*(steps+1),color,sides)

def geometry():
    m=g.Mesh()
    # Irregular timber frame: four eaves and two ridge posts, no central interior pole.
    posts=[((-205,-156,0),(-202,-155,201),4.4),((204,-158,0),(206,-154,203),4.6),
           ((-207,153,0),(-202,154,205),4.2),((207,151,0),(204,155,200),4.7),
           ((-3,-157,0),(0,-160,284),5.4),((5,154,0),(0,155,281),5.2)]
    for n,(a,b,r) in enumerate(posts):
        p=[]
        for i in range(9):
            t=i/8
            p.append(tuple(a[k]*(1-t)+b[k]*t for k in range(3)))
            p[-1]=g.add(p[-1],(1.1*math.sin(t*8+n),.8*math.sin(t*6+n),0))
        m.tube(p,[r*(1-.2*i/8) for i in range(9)],g.tint(WOOD,.92+.035*n),9)
        z=b[2]-17
        path(m,lambda t,b=b,z=z,r=r:(b[0]+(r+1)*math.cos(t*math.tau*3),b[1]+(r+1)*math.sin(t*math.tau*3),z+t*5),42,.65,ROPE)
    path(m,lambda t:(.9*math.sin(t*8),-166+330*t,267+2*math.sin(t*7)),18,4.0,WOOD,9)
    for x in [-204,204]:
        path(m,lambda t,x=x:(x,-161+322*t,roof(x,-161+322*t)[2]-4.8),18,3.2,WOOD,8)
    # Roof strips are sewn panels, not uniformly coloured flat primitives.
    def linen(a,b):
        panel=min(5,int(a*6))
        return g.tint(LINEN,[.91,1.03,.97,1.06,.93,1.0][panel]*(.93+.07*math.sin(math.pi*b)))
    sheet(m,lambda a,b:roof(-216+432*a,-172+344*b),60,44,linen)
    # Draped back and partial left wall, bottom held above damp ground.
    def back(a,b):
        x=-204+408*a
        return (x,155+5*math.sin(x*.14)*b,roof(x,155)[2]-5-(roof(x,155)[2]-23)*b)
    def side(a,b):
        y=-24+179*a
        return (-205-4*math.sin(y*.15)*b,y,roof(-205,y)[2]-4-(roof(-205,y)[2]-24)*b)
    sheet(m,back,56,26,lambda a,b:g.tint(LINEN,(.80 if a<.51 else .93)*(1-.17*b)))
    sheet(m,side,28,24,lambda a,b:g.tint(PATCH,.96-.17*b))
    # Reinforced roof seams and hems.
    for x in [-144,-72,0,72,144]:
        path(m,lambda t,x=x:g.add(roof(x,-172+344*t),(0,0,.65)),44,.43,g.tint(ROPE,.85),5)
    for y in [-172,172]:
        path(m,lambda t,y=y:g.add(roof(-216+432*t,y),(0,0,.2)),60,.85,g.tint(LINEN,.66),6)
    for x in [-216,216]:
        path(m,lambda t,x=x:roof(x,-172+344*t),42,.85,g.tint(LINEN,.66),6)
    # Actual raised repair patches and short visible stitch segments.
    for x0,y0,w,h,c in [(-162,-93,72,79,PATCH),(74,37,58,68,g.tint(LINEN,.73)),(-85,81,45,49,RED)]:
        f=lambda a,b,x0=x0,y0=y0,w=w,h=h:g.add(roof(x0+w*a,y0+h*b),(0,0,.8))
        sheet(m,f,12,12,lambda a,b,c=c:c,.22)
        for edge in range(4):
            for j in range(10):
                t=(j+.3)/10
                if edge<2:
                    a=t;b=.025 if edge==0 else .975
                    aa=a;bb=b+.045 if edge==0 else b-.045
                else:
                    a=.025 if edge==2 else .975;b=t
                    aa=a+.045 if edge==2 else a-.045;bb=b
                p=[g.add(f(a,b),(0,0,.25)),g.add(f(aa,bb),(0,0,.25))]
                m.tube(p,[.34,.34],g.tint(ROPE,1.2),5)
    # Back wall repair, contrasting reclaimed textile.
    sheet(m,lambda a,b:g.add(back(.66+.19*a,.39+.26*b),(0,.8,0)),14,14,lambda a,b:RED,.24)
    # Four diagonal guy lines to timber pegs. These belong to the single prop.
    for x,y in [(-204,-154),(204,-154),(-204,154),(204,154)]:
        a=(x,y,191);b=(x*1.42,y*1.42,14)
        path(m,lambda t,a=a,b=b:g.add(tuple(a[k]*(1-t)+b[k]*t for k in range(3)),(0,0,-6*math.sin(math.pi*t))),24,.85,ROPE,7)
        m.tube([(b[0]+4,b[1]+2,0),(b[0]-3,b[1]-2,28)],[2.0,1.6],g.tint(WOOD,.85),7)
        path(m,lambda t,b=b:(b[0]+2.6*math.cos(t*math.tau*2),b[1]+2.6*math.sin(t*math.tau*2),12+t*4),22,.5,ROPE)
    return m.finish()

def create():
    import unreal as u
    e=u.EditorAssetLibrary;mel=u.MaterialEditingLibrary
    matpath=PKG+'/M_Shelter_LinenTimber'
    mat=e.load_asset(matpath) if e.does_asset_exist(matpath) else None
    if not mat:
        mat=u.AssetToolsHelpers.get_asset_tools().create_asset('M_Shelter_LinenTimber',PKG,u.Material,u.MaterialFactoryNew())
        vc=mel.create_material_expression(mat,u.MaterialExpressionVertexColor,-600,0)
        wp=mel.create_material_expression(mat,u.MaterialExpressionWorldPosition,-600,200)
        node=mel.create_material_expression(mat,u.MaterialExpressionCustom,-250,0)
        node.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT3)
        inputs=[]
        for name in ['P','C','R']:
            ci=u.CustomInput();ci.set_editor_property('input_name',name);inputs.append(ci)
        node.set_editor_property('inputs',inputs)
        node.set_editor_property('code',"""
          float cloth=step(.95,R);
          float weave=(sin(P.x*5.7)+sin(P.y*5.9)+sin(P.z*5.5))*.333;
          float stain=sin(P.x*.037+sin(P.z*.031))*sin(P.y*.025+P.z*.019);
          float wood=sin(P.z*.22+sin(P.x*3.1+P.y*2.7))*sin(P.x*1.7+P.y*1.9);
          return C*lerp(.92+.08*wood,.92+.035*weave+.045*stain,cloth);
        """)
        for src,out,dst in [(wp,'','P'),(vc,'','C'),(vc,'A','R')]:
            assert mel.connect_material_expressions(src,out,node,dst)
        assert mel.connect_material_property(node,'',u.MaterialProperty.MP_BASE_COLOR)
        assert mel.connect_material_property(vc,'A',u.MaterialProperty.MP_ROUGHNESS)
        assert not list(mel.recompile_material(mat))
        e.set_metadata_tag(mat,'Recipe',VERSION);assert e.save_asset(matpath)
    verts,tris,colors,normals=geometry()
    path_name=PKG+'/'+NAME
    mesh=e.load_asset(path_name) if e.does_asset_exist(path_name) else None
    if mesh and os.environ.get('ANASTASIS_SHELTER_REBUILD','0')=='1':
        assert path_name==PKG+'/'+NAME
        assert e.get_metadata_tag(mesh,'Recipe') in ('camp-shelter-009-v1',VERSION)
        assert e.delete_asset(path_name)
        mesh=None
    if mesh:
        assert e.get_metadata_tag(mesh,'Recipe')==VERSION
        assert mesh.get_num_triangles(0)==len(tris)
    else:
        b=u.GeometryScriptSimpleMeshBuffers()
        b.vertices=[u.Vector(*p) for p in verts];b.triangles=[u.IntVector(*t) for t in tris]
        b.normals=[u.Vector(*n) for n in normals];b.vertex_colors=[u.LinearColor(*c) for c in colors]
        b.uv0=[u.Vector2D(p[0]/100,p[1]/100) for p in verts]
        dyn=u.DynamicMesh();u.GeometryScript_MeshEdits.append_buffers_to_mesh(dyn,b)
        opts=u.GeometryScriptCreateNewStaticMeshAssetOptions()
        opts.enable_recompute_normals=False;opts.enable_recompute_tangents=True;opts.enable_nanite=False
        mesh,outcome=u.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dyn,path_name,opts)
        assert mesh,str(outcome)
        mesh.set_material(0,mat)
        e.set_metadata_tag(mesh,'Recipe',VERSION)
        assert e.save_asset(path_name)
    bounds=mesh.get_bounding_box()
    stats=dict(path=path_name,vertices=len(verts),triangles=len(tris),size_cm=[bounds.max.x-bounds.min.x,bounds.max.y-bounds.min.y,bounds.max.z-bounds.min.z],
               lods=mesh.get_num_lods(),collision='none; visual prop only',cloth='static, closed thin shell',pivot='bottom centre')
    u.log('SHELTER009 ASSET '+json.dumps(stats))
    return {NAME:mesh},{NAME:stats}

if __name__=='__main__':create()
