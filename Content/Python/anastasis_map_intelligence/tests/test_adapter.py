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

    def procedural(self, sections, hidden=()):
        self.fake.LandscapeProxy=Landscape
        self.fake.ProceduralMeshComponent=object
        actor=Mock(); comp=Mock()
        actor.get_components_by_class.return_value=[comp]
        comp.get_name.return_value='ExperimentalTerrain'
        comp.get_num_sections.return_value=3
        comp.is_mesh_section_visible.side_effect=lambda section: section not in hidden
        comp.get_path_name.return_value='Terrain.Mesh'
        comp.get_owner.return_value=actor
        with patch.object(self.editor,'_mesh',side_effect=lambda c,i: sections.get(i,([],[]))) as mesh:
            result=self.editor._get_grid([actor],self.settings,self.progress)
        return result,mesh

    @staticmethod
    def rectangle(x0,y0,x1,y1,z):
        return ([(x0,y0,z),(x1,y0,z),(x1,y1,z),(x0,y1,z)],[0,1,2,0,2,3])

    def test_river_and_lake_union_excludes_dry_habitat(self):
        sections={0:self.rectangle(0,0,2,2,0),1:self.rectangle(0,0,1,1,1),
                  2:self.rectangle(1,1,2,2,2)}
        (g,_,_,_,water),_=self.procedural(sections)
        self.assertEqual(g.water_z,[1.,None,None,2.])
        self.assertEqual([w['section'] for w in water],[1,2])
        report=core.analyze(g,self.settings)
        self.assertEqual(report['cells']['category'][0],'WATER_OR_MARGIN')
        self.assertEqual(report['cells']['category'][3],'WATER_OR_MARGIN')

    def test_hidden_river_is_not_visible_water(self):
        sections={0:self.rectangle(0,0,2,2,0),1:self.rectangle(0,0,1,1,1),
                  2:self.rectangle(1,1,2,2,2)}
        (g,_,_,_,water),_=self.procedural(sections,hidden=[2])
        self.assertEqual(g.water_z,[1.,None,None,None])
        self.assertEqual(water[1]['status'],'HIDDEN_SECTION')

    def test_overlapping_sections_keep_highest_water_once(self):
        self.settings.additional_water_sections=[2,1,2]
        sections={0:self.rectangle(0,0,2,2,0),1:self.rectangle(0,0,2,2,1),
                  2:self.rectangle(0,0,2,2,2)}
        (g,_,_,_,water),mesh=self.procedural(sections)
        self.assertEqual(g.water_z,[2.]*4)
        self.assertEqual(mesh.call_count,3) # ground, lake, river; deduplicated
        self.assertEqual(len(water),2)

    def test_explicit_legacy_only_is_incomparable_to_union(self):
        sections={0:self.rectangle(0,0,2,2,0),1:self.rectangle(0,0,1,1,1),
                  2:self.rectangle(1,1,2,2,2)}
        (g,*_),_=self.procedural(sections)
        before=core.analyze(g,self.settings)
        self.settings.additional_water_sections=[]
        (g,*_),_=self.procedural(sections)
        after=core.analyze(g,self.settings)
        self.assertIsNone(g.water_z[3])
        with self.assertRaisesRegex(ValueError,'INCOMPARABLE'): core.compare(before,after)

    def test_empty_and_absent_water_are_reported(self):
        self.settings.additional_water_sections=[2,3]
        (g,_,_,_,water),_=self.procedural({0:self.rectangle(0,0,2,2,0)})
        self.assertEqual([w['status'] for w in water],['EMPTY_SECTION','EMPTY_SECTION','ABSENT_SECTION'])
        self.assertEqual(g.water_z,[None]*4)

    def test_invalid_water_sections_rejected(self):
        for value in ([0],[-1],[1.5],[True],'2'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                core.Settings(additional_water_sections=value).validate()
