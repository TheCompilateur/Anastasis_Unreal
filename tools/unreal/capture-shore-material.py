"""Material factorial review: baseline / ground only / water only / both.
Reuses the retained 21 reeds and ShoreProfile=1 geometry. No asset or map saves.
ANASTASIS_MATERIAL_HIDE_WATER=1 selects the section-visibility diagnostic.
"""
from array import array
import ast
import hashlib
import importlib.util
import json
import os
import sys
import unreal
sys.dont_write_bytecode=True
path=os.path.join(unreal.Paths.project_dir(),'tools','unreal','capture-shore-contact.py')
spec=importlib.util.spec_from_file_location('shore_contact_review',path)
s=importlib.util.module_from_spec(spec)
spec.loader.exec_module(s)
r=s.r
mel=unreal.MaterialEditingLibrary
assets=unreal.AssetToolsHelpers.get_asset_tools()
ground_changes={
    'DetailTiling':1.0/12.0, 'DetailContrast':0.14, 'BumpStrength':0.07,
    'MesoContrast':0.12, 'DampRoughness':0.26, 'RoughnessGrain':0.08,
}
states={'bare':{'ground':False,'water':False},
        'A':{'ground':True,'water':False},
        'B':{'ground':False,'water':True},
        'C':{'ground':True,'water':True}}
diagnostic=os.environ.get('ANASTASIS_MATERIAL_HIDE_WATER','0')=='1'
if diagnostic:
    states['B']={'ground':False,'water':False,'hide_water':True}
    states['C']={'ground':True,'water':False,'hide_water':True}
keepalive=[]
comp=None
ground=None
water=None
ground_candidate=None
water_candidate=None
geometry_hash=None


def signature():
    digest=hashlib.sha256()
    for section in (0,1):
        data=unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(comp,section)
        verts,indices,normals,uvs,tangents=data
        for values in (verts,normals):
            digest.update(array('d',(v for p in values for v in (p.x,p.y,p.z))).tobytes())
        digest.update(array('i',indices).tobytes())
        digest.update(array('d',(v for p in uvs for v in (p.x,p.y))).tobytes())
        digest.update(array('d',(v for p in tangents for v in
            (p.tangent_x.x,p.tangent_x.y,p.tangent_x.z,float(p.flip_tangent_y)))).tobytes())
    return digest.hexdigest()


def setup():
    global comp,ground,water,ground_candidate,water_candidate,geometry_hash
    s.setup()
    s.pose('C',0)
    comp=s.actor.get_components_by_class(unreal.ProceduralMeshComponent)[0]
    ground=comp.get_material(0)
    water=comp.get_material(1)
    assert ground.get_path_name()=='/Game/Anastasis/Materials/MI_AnastasisGround.MI_AnastasisGround'
    assert water.get_path_name()=='/Game/Anastasis/Materials/M_AnastasisShoreWater.M_AnastasisShoreWater'
    originals={'ground':ground.get_path_name(),'water':water.get_path_name()}
    params={str(n):mel.get_material_instance_scalar_parameter_value(ground,n)
            for n in mel.get_scalar_parameter_names(ground)}
    r.log('MATERIAL_SOURCE '+json.dumps(dict(materials=originals,ground_parameters=params)))
    # Duplicate to unsaved review packages, without changing the source assets.
    pkg='/Game/Anastasis/ShoreMaterialReview007'
    assert not unreal.EditorAssetLibrary.does_directory_exist(pkg), 'Review path already exists'
    ground_candidate=assets.duplicate_asset('MI_Ground_Review',pkg,ground)
    water_candidate=assets.duplicate_asset('M_Water_Review',pkg,water)
    assert ground_candidate and water_candidate
    keepalive.extend([ground,water,ground_candidate,water_candidate])
    for name,value in ground_changes.items():
        assert name in params, name
        # UE 5.8.2 returns false unconditionally here; verify the readback instead.
        mel.set_material_instance_scalar_parameter_value(ground_candidate,name,value)
        actual=mel.get_material_instance_scalar_parameter_value(ground_candidate,name)
        assert abs(actual-value)<1e-6, (name,actual,value)
    mel.update_material_instance(ground_candidate)
    color=mel.get_material_property_input_node(water_candidate,unreal.MaterialProperty.MP_BASE_COLOR)
    assert isinstance(color,unreal.MaterialExpressionCustom),type(color)
    original_code=color.get_editor_property('code')
    r.log('WATER_SOURCE_CODE '+original_code)
    # Read only the recipe's literal HLSL assignments, without its save/quit entrypoint.
    recipe_path=os.path.join(unreal.Paths.project_dir(),'tools','unreal','shore-water.py')
    with open(recipe_path,'r',encoding='utf-8-sig') as f: recipe_source=f.read()
    constants={}
    def literal(expr):
        if isinstance(expr,ast.Constant) and isinstance(expr.value,str): return expr.value
        if isinstance(expr,ast.Name): return constants[expr.id]
        if isinstance(expr,ast.BinOp) and isinstance(expr.op,ast.Add):
            return literal(expr.left)+literal(expr.right)
        raise ValueError('Nonliteral HLSL recipe')
    for node in ast.parse(recipe_source).body:
        if isinstance(node,ast.Assign) and len(node.targets)==1 and isinstance(node.targets[0],ast.Name):
            name=node.targets[0].id
            if name in ('GENOME','BASE_COLOR_HLSL','SURFACE_HLSL'): constants[name]=literal(node.value)
    code=constants['BASE_COLOR_HLSL']
    opacity=mel.get_material_property_input_node(water_candidate,unreal.MaterialProperty.MP_OPACITY)
    inputs=list(mel.get_inputs_for_material_expression(water_candidate,opacity))
    assert len(inputs)==1 and isinstance(inputs[0],unreal.MaterialExpressionCustom),inputs
    surface=inputs[0]
    original_surface=surface.get_editor_property('code')
    color.set_editor_property('code',code)
    surface.set_editor_property('code',constants['SURFACE_HLSL'])
    errors=mel.recompile_material(water_candidate)
    assert not errors, list(errors)
    geometry_hash=signature()
    r.manifest.update(base='643738a447c47bd70f77633c8a50bbaf4f209377',
        comparison=('bare=original; A=fine ground; B=original with water hidden; C=fine ground with water hidden' if diagnostic else
                    'same ShoreProfile=1 geometry, reeds, cameras and light; bare=original materials, A=ground only, B=water only, C=both'),
        states=states, diagnostic_hide_water=diagnostic, saved_assets=False, material_sources=originals,
        ground_parameters_before=params, ground_parameters_after=ground_changes,
        water_recipe=recipe_path, water_recipe_sha256=hashlib.sha256(recipe_source.encode()).hexdigest(),
        water_source_hlsl=original_code, water_source_surface_hlsl=original_surface,
        water_recipe_drift={'color':original_code!=code,'surface':original_surface!=constants['SURFACE_HLSL']},
        water_candidate_surface_hlsl=constants['SURFACE_HLSL'],
        water_candidate_hlsl=code, geometry_sha256=geometry_hash,
        geometry_hash_scope='both sections: positions, triangles, normals, UV0, tangents exposed by Python',
        material_bindings=[], shader_compile_errors=list(errors or []))
    r.write_manifest()


def pose(mode,view):
    state=states[mode]
    s.base_pose('C',view)
    comp.set_material(0,ground_candidate if state['ground'] else ground)
    comp.set_material(1,water_candidate if state['water'] else water)
    comp.set_mesh_section_visible(1,not state.get('hide_water',False))
    assert comp.is_mesh_section_visible(1)==(not state.get('hide_water',False))
    assert signature()==geometry_hash, 'geometry changed during material comparison'
    bindings=[comp.get_material(i).get_path_name() for i in (0,1)]
    assert bindings==[(ground_candidate if state['ground'] else ground).get_path_name(),
                      (water_candidate if state['water'] else water).get_path_name()]
    r.manifest['material_bindings'].append(dict(mode=mode,view=r.manifest['views'][view]['name'],materials=bindings,water_visible=comp.is_mesh_section_visible(1)))
    r.write_manifest()
    r.log('MATERIAL_POSE '+json.dumps(r.manifest['material_bindings'][-1]))
    r.les.editor_invalidate_viewports()


r.setup=setup
r.pose=pose
r.start_capture()
