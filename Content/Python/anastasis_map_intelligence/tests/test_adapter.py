"""Contract tests for UE Python return semantics, not live NavMesh/Landscape proof."""
import importlib
import sys
import types
import unittest
from unittest.mock import Mock, patch
from anastasis_map_intelligence import core

class Vector:
    def __init__(self,x=0,y=0,z=0): self.x=x;self.y=y;self.z=z

class NavData: pass
class Landscape: pass

class AdapterTests(unittest.TestCase):
    def setUp(self):
        self.fake=types.ModuleType('unreal')
        self.fake.Vector=Vector; self.fake.NavigationData=NavData
        self.fake.NavigationSystemV1=Mock()
        self.fake.NavigationSystemV1.is_navigation_being_built_or_locked.return_value=False
        self.patch=patch.dict(sys.modules,{'unreal':self.fake}); self.patch.start()
        sys.modules.pop('anastasis_map_intelligence.editor',None)
        self.editor=importlib.import_module('anastasis_map_intelligence.editor')
        self.settings=core.Settings(step_m=1,relief_radius_m=1,roughness_radius_m=1)
        self.grid=core.Grid.covering((0,0,2,2),self.settings); self.grid.z=[0.]*4
        self.progress=Mock(); self.progress.should_cancel.return_value=False

    def tearDown(self):
        self.patch.stop(); sys.modules.pop('anastasis_map_intelligence.editor',None)

    def test_navigation_none_raycast_means_clear(self):
        nav=self.fake.NavigationSystemV1
        nav.project_point_to_navigation.side_effect=lambda world,p,**kwargs:p
        nav.navigation_raycast.return_value=None
        result=self.editor._navigation(object(),self.grid,[NavData()],self.settings,self.progress)
        self.assertEqual(result['status'],'SAMPLED')
        self.assertEqual(self.grid.nav_edges,{(0,1),(0,2),(1,3),(2,3)})
        self.assertEqual(nav.project_point_to_navigation.call_args.kwargs['nav_data'],None)

    def test_navigation_vector_raycast_means_obstruction(self):
        nav=self.fake.NavigationSystemV1
        nav.project_point_to_navigation.return_value=Vector()
        nav.navigation_raycast.return_value=Vector()
        self.editor._navigation(object(),self.grid,[NavData()],self.settings,self.progress)
        self.assertEqual(self.grid.nav_edges,set())

    def test_empty_navigation_keeps_unknown(self):
        self.fake.NavigationSystemV1.project_point_to_navigation.return_value=None
        result=self.editor._navigation(object(),self.grid,[NavData()],self.settings,self.progress)
        self.assertEqual(result['status'],'UNKNOWN')
        self.assertEqual(self.grid.nav,[None]*4)

    def test_landscape_component_trace_contract_and_metres(self):
        self.fake.LandscapeProxy=Landscape
        self.fake.LandscapeHeightfieldCollisionComponent=object
        self.fake.SystemLibrary=Mock()
        self.fake.SystemLibrary.get_component_bounds.return_value=(Vector(100,100,0),Vector(100,100,100),200)
        actor=Landscape(); comp=Mock()
        comp.line_trace_component.side_effect=lambda start,end,*args:(Vector(start.x,start.y,50),Vector(0,0,1),'',object())
        actor.get_components_by_class=lambda cls:[comp]
        g,targets,source,_,_=self.editor._get_grid([actor],self.settings,self.progress)
        self.assertEqual(g.z,[.5]*4)
        self.assertEqual(g.gradients,[(0.,0.)]*4)
        self.assertEqual(g.nx*g.dx,2.)
        self.assertIn('Landscape',source)

    def test_water_requires_collision_footprint(self):
        class Water: pass
        self.fake.WaterBody=Water; body=Water(); component=Mock()
        component.get_collision_components.return_value=[]
        body.get_water_body_component=lambda:component
        body.get_path_name=lambda:'WaterBody_Test'
        evidence=self.editor._water_bodies(self.grid,[body],self.progress)
        self.assertEqual(evidence[0]['status'],'UNKNOWN')
        component.get_water_surface_info_at_location.assert_not_called()
        self.assertEqual(self.grid.water_z,[None]*4)

    def test_water_surface_cannot_leak_outside_collision(self):
        class Water: pass
        self.fake.WaterBody=Water; body=Water(); component=Mock(); collider=Mock()
        self.fake.SystemLibrary=Mock()
        self.fake.SystemLibrary.get_component_bounds.return_value=(Vector(50,50,0),Vector(49,49,100),100)
        component.get_collision_components.return_value=[collider]
        component.get_water_surface_info_at_location.return_value=(Vector(50,50,100),Vector(),Vector(),1.)
        collider.line_trace_component.return_value=(Vector(),Vector(),'name',object())
        body.get_water_body_component=lambda:component; body.get_path_name=lambda:'WaterBody_Test'
        self.editor._water_bodies(self.grid,[body],self.progress)
        self.assertEqual(self.grid.water_z,[1.,None,None,None])

    def test_fingerprint_is_numeric_and_stable(self):
        actor=Mock()
        actor.get_path_name.return_value='Terrain_A'
        actor.get_class.return_value.get_name.return_value='Terrain'
        actor.get_actor_location.return_value=Vector(100,200,300)
        actor.get_actor_scale3d.return_value=Vector(1,1,1)
        actor.get_actor_rotation.return_value=types.SimpleNamespace(pitch=0,yaw=45,roll=0)
        a=self.editor._fingerprint([actor]); b=self.editor._fingerprint([actor])
        self.assertEqual(a,b)
        self.assertEqual(a[0]['transform']['location_m'],[1.,2.,3.])
        actor.get_actor_location.return_value=Vector(100,200,400)
        self.assertNotEqual(a,self.editor._fingerprint([actor]))
