"""Shore contact A/B: delivered terrain vs bounded profile, fixed site/cameras.
Eight diagnostic views: bare=before, A=after, B=before with reeds, C=after with reeds.
Requires the retained composition manifest through ANASTASIS_SHORE_PLACEMENTS.
"""
import hashlib
import importlib.util
import json
import math
import os
import sys
import time
import unreal
sys.dont_write_bytecode=True
path=os.path.join(unreal.Paths.project_dir(),'tools','unreal','capture-reed-form.py')
spec=importlib.util.spec_from_file_location('shore_capture_base',path)
r=importlib.util.module_from_spec(spec)
spec.loader.exec_module(r)
base_setup=r.setup
base_pose=r.pose
source=os.environ['ANASTASIS_SHORE_PLACEMENTS']
with open(source,'r',encoding='utf-8-sig') as f: prior=json.load(f)
placements=prior['variants']['B']
actor=None
baseline_centres=None


def centres(comp):
    verts=unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(comp,0)[0]
    covered=set(unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(comp,1)[1])
    return {(round(p.x),round(p.y)):i in covered and p.z<r.water_z-0.0001
            for i,p in enumerate(verts) if (i%len(r.xs))%4==0 and (i//len(r.xs))%4==0}


def setup():
    global actor,baseline_centres
    r.cmd('anastasis.Terrain.Forge.ShoreProfile 0')
    existing_assets=all(unreal.EditorAssetLibrary.does_asset_exist(p) for p in (
        "/Game/Anastasis/EcotoneReview004/SM_Reed_Curved_01",
        "/Game/Anastasis/EcotoneReview004/M_Reed_Static_01"))
    base_setup()
    assert r.manifest['views']==prior['views'], 'fixed cameras changed'
    assert len(r.spawned)==len(placements)==21
    cls=unreal.load_class(None,'/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
    actor=list(unreal.GameplayStatics.get_all_actors_of_class(r.ues.get_editor_world(),cls))[0]
    baseline_centres=centres(actor.get_components_by_class(unreal.ProceduralMeshComponent)[0])
    with open(source,'rb') as source_file: source_hash=hashlib.sha256(source_file.read()).hexdigest()
    r.manifest.update(comparison='bare/B: delivered terrain; A/C: ShoreProfile 1; B/C add the same 21 grouped reeds',
        states={'bare':{'profile':0,'reeds':False},'A':{'profile':1,'reeds':False},
                'B':{'profile':0,'reeds':True},'C':{'profile':1,'reeds':True}},
        placements_source_sha256=source_hash,
        root_xy_yaw_scale_source=source, saved_assets=not existing_assets,profile_placements={},centre_coverage_changes={},embody_seconds=[])
    r.write_manifest()


def pose(mode,view):
    state=r.manifest['states'][mode]
    r.cmd('anastasis.Terrain.Forge.ShoreProfile %d'%state['profile'])
    started=time.perf_counter()
    assert actor.call_method('EmbodyCanonical',args=(r.SEED,))
    r.manifest['embody_seconds'].append(dict(profile=state['profile'],seconds=time.perf_counter()-started))
    comp=actor.get_components_by_class(unreal.ProceduralMeshComponent)[0]
    current_centres=centres(comp)
    changed=[list(xy) for xy,before in baseline_centres.items() if current_centres[xy]!=before]
    r.manifest['centre_coverage_changes'][str(state['profile'])]=changed
    vertices=unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(comp,0)[0]
    r.heights={(p.x,p.y):p.z for p in vertices}
    base_pose('C',view)
    current=[]
    for a,p in zip(r.spawned,placements):
        x,y=p['location'][:2]
        ground=r.height(x,y)
        support=[ground]+[r.height(x+16*math.cos(t*math.tau/12),y+16*math.sin(t*math.tau/12)) for t in range(12)]
        z=min(support)-r.curved_mesh.get_bounding_box().min.z
        a.static_mesh_component.set_visibility(state['reeds'])
        a.set_actor_location(unreal.Vector(x,y,z),False,False)
        a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=p['yaw'],roll=0),False)
        loc=a.get_actor_location()
        assert max(abs(loc.x-x),abs(loc.y-y),abs(loc.z-z))<.01
        current.append(dict(location=[x,y,z],yaw=p['yaw'],scale=[1,1,1],support_min=min(support),support_max=max(support)))
    r.manifest['profile_placements'][str(state['profile'])]=current
    r.write_manifest()
    r.log('SHORE_POSE profile=%d reeds=%s view=%s'%(state['profile'],state['reeds'],r.manifest['views'][view]['name']))
    r.les.editor_invalidate_viewports()


r.setup=setup
r.pose=pose
if __name__ == '__main__':
    r.start_capture()
