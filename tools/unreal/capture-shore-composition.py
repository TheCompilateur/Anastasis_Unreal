"""Bounded shore composition study on delivered terrain 82a337b.
A: 21 curved reeds on prior grid; B: same count/mesh/yaws, clustered roots;
C: B plus 28 smaller reeds on supported dry margins. No map saves; missing review assets can be created by the baseline recipe.
"""
import importlib.util
import math
import os
import random
import sys
import unreal

sys.dont_write_bytecode = True
path = os.path.join(unreal.Paths.project_dir(), 'tools', 'unreal', 'capture-reed-form.py')
spec = importlib.util.spec_from_file_location('reed_capture', path)
r = importlib.util.module_from_spec(spec)
spec.loader.exec_module(r)
base_setup = r.setup
base_pose = r.pose


def setup():
    assets_existed = all(unreal.EditorAssetLibrary.does_asset_exist(p) for p in (
        '/Game/Anastasis/EcotoneReview004/SM_Reed_Curved_01',
        '/Game/Anastasis/EcotoneReview004/M_Reed_Static_01'))
    base_setup()
    assert len(r.spawned) == 21
    assert abs(r.manifest['focus'][0] - 5658.461538461538) < .01
    assert abs(r.manifest['focus'][1] - 2630.769230769231) < .01
    # Exact prior cameras are recomputed by the unchanged baseline, then checked.
    eye = r.manifest['views'][0]
    assert max(abs(a-b) for a,b in zip(eye['location'], [6345.011234743799,2767.3324561805207,439.7657715125191])) < .01
    r.manifest.update(comparison='A prior 21 curved reeds; B same 21 mesh/material/yaws at continuous clustered roots; C B plus 28 lower dry-margin reeds',
                      material_wpo='static zero WPO for all variants', saved_assets=not assets_existed,
                      parent_study='reed-form-004-4m-b', baseline_source='d78c68d',
                      transition_scope='composition only; water and ground geometry/material unchanged')
    root_actors=list(unreal.GameplayStatics.get_all_actors_of_class(r.ues.get_editor_world(),unreal.load_class(None,'/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')))
    obstacles=[]
    for c in root_actors[0].get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
        if c.is_visible():
            for i in range(c.get_instance_count()):
                p=c.get_instance_transform(i,world_space=True).translation
                obstacles.append((p.x,p.y))
    baseline=r.manifest['placements']
    ordered=sorted(baseline,key=lambda p:p['location'][0])
    anchors=[ordered[i]['location'][:2] for i in (3,10,17)]
    cx,cy=r.manifest['focus'][:2]
    rng=random.Random(5012345)
    minimum=r.curved_mesh.get_bounding_box().min.z

    def point(x,y,scale,fringe=False):
        if (x-cx)**2+(y-cy)**2 > 430**2: return None
        z=r.height(x,y)
        if not r.water_z-18 <= z <= r.water_z+25: return None
        near=sum(r.wet(x+110*math.cos(a*math.tau/12),y+110*math.sin(a*math.tau/12)) for a in range(12))
        if not 1 <= near <= 11: return None
        if fringe and (r.wet(x,y) or z < r.water_z): return None
        if any((x-ox)**2+(y-oy)**2 < 180**2 for ox,oy in obstacles): return None
        radius=16*max(scale[:2])
        support=[z]+[r.height(x+radius*math.cos(a*math.tau/12),y+radius*math.sin(a*math.tau/12)) for a in range(12)]
        if max(support)-min(support) > 8: return None
        return dict(location=[x,y,min(support)-minimum*scale[2]],scale=scale,
                    ground=z,support_min=min(support),support_max=max(support),
                    support_radius=radius,wet=r.wet(x,y),wet_neighbors=near)

    grouped=[]
    for attempt in range(20000):
        ax,ay=anchors[len(grouped)%3]
        x=rng.gauss(ax,85); y=rng.gauss(ay,65)
        if any((x-p['location'][0])**2+(y-p['location'][1])**2<45**2 for p in grouped): continue
        p=point(x,y,[1,1,1])
        if p is None: continue
        p['yaw']=baseline[len(grouped)]['yaw']
        grouped.append(p)
        if len(grouped)==len(baseline): break
    assert len(grouped)==21, 'cluster sampler exhausted'
    fringe=[]
    for attempt in range(30000):
        anchor=grouped[attempt%len(grouped)]['location']
        x=rng.gauss(anchor[0],90); y=rng.gauss(anchor[1],90)
        if any((x-p['location'][0])**2+(y-p['location'][1])**2<32**2 for p in grouped+fringe): continue
        z=r.height(x,y)
        # Shorter shoots towards higher dry ground; max height 108 cm.
        sz=max(.30,min(.60,.60-max(0,z-r.water_z)*.018))
        p=point(x,y,[.70,.70,sz],True)
        if p is None: continue
        p['yaw']=rng.uniform(0,360)
        fringe.append(p)
        if len(fringe)==28: break
    assert len(fringe)==28, 'dry-margin sampler exhausted'
    for i,p in enumerate(fringe):
        a=r.eas.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p['location']),unreal.Rotator(pitch=0,yaw=p['yaw'],roll=0))
        a.set_actor_label('ShoreFringe_%03d'%i)
        a.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        r.spawned.append(a)
    r.manifest['variants']={'A':baseline,'B':grouped,'C':grouped+fringe}
    r.manifest['cluster_anchors']=anchors
    r.manifest['placement_seed']=5012345
    r.manifest['count_by_variant']={m:len(p) for m,p in r.manifest['variants'].items()}
    r.manifest['triangles_by_variant']={m:3360*len(p) for m,p in r.manifest['variants'].items()}
    r.manifest['max_support_span']=max(p['support_max']-p['support_min'] for p in grouped+fringe)
    r.manifest['fringe_height_range_cm']=[179.933*min(p['scale'][2] for p in fringe),179.933*max(p['scale'][2] for p in fringe)]
    r.write_manifest()
    r.log('COMPOSITION_READY counts=%s support_span=%.3f'%(r.manifest['count_by_variant'],r.manifest['max_support_span']))


def pose(mode,view):
    # Baseline routine retains camera/show-flag assertions. All variants then use C's mesh/material.
    base_pose('C',view)
    placements=[] if mode=='bare' else r.manifest['variants'][mode]
    for i,a in enumerate(r.spawned):
        c=a.static_mesh_component
        c.set_visibility(i<len(placements))
        if i>=len(placements): continue
        p=placements[i]
        c.set_static_mesh(r.curved_mesh)
        c.set_material(0,r.static_mat)
        a.set_actor_scale3d(unreal.Vector(*p['scale']))
        a.set_actor_location(unreal.Vector(*p['location']),False,False)
        a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=p['yaw'],roll=0),False)
        actual=a.get_actor_location(); scale=a.get_actor_scale3d(); yaw=a.get_actor_rotation().yaw
        assert max(abs(a-b) for a,b in zip([actual.x,actual.y,actual.z],p['location']))<.01
        assert max(abs(a-b) for a,b in zip([scale.x,scale.y,scale.z],p['scale']))<.001
        assert abs((yaw-p['yaw']+180)%360-180)<.01
        assert c.get_editor_property('static_mesh').get_path_name()==r.curved_mesh.get_path_name()
        assert c.get_material(0).get_path_name()==r.static_mat.get_path_name()
    r.log('COMPOSITION_POSE_VERIFIED mode=%s roots=%d view=%s'%(mode,len(placements),r.manifest['views'][view]['name']))
    r.les.editor_invalidate_viewports()


r.setup=setup
r.pose=pose
r.start_capture()
