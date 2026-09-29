import copy
import json
import math
import tempfile
import unittest
from anastasis_map_intelligence import core as c


def grid(nx=20,ny=20,height=lambda x,y:0.,step=1.):
    s=c.Settings(step_m=step,relief_radius_m=2.,min_settlement_area_m2=10.,
        min_agriculture_area_m2=5.,min_settlement_width_m=2.)
    g=c.Grid.covering((0,0,nx*step,ny*step),s)
    g.z=[height(*g.xy(i)) for i in range(nx*ny)]
    return g,s


class MapIntelligenceTests(unittest.TestCase):
    def test_known_plane_slope_and_detrended_roughness(self):
        g,s=grid(height=lambda x,y:.1*x+.2*y)
        slopes,rough,relief=c.metrics(g,s)
        self.assertAlmostEqual(slopes[210],math.degrees(math.atan(math.sqrt(.05))),8)
        self.assertLess(max(rough),1e-12)
        self.assertAlmostEqual(relief[210],1.2,8)

    def test_large_flat_region_area_and_units(self):
        g,s=grid(); r=c.analyze(g,s)
        self.assertEqual(r['statistics']['significant_basins'],1)
        self.assertEqual(r['regions'][0]['area_m2'],400.)
        self.assertEqual(r['regions'][0]['area_ha'],.04)
        self.assertEqual(r['statistics']['habitat_pct'],100.)

    def test_rough_land_is_not_settlement(self):
        g,s=grid(height=lambda x,y:10*((int(x)+int(y))%2))
        r=c.analyze(g,s)
        self.assertEqual(r['statistics']['habitat_pct'],0.)
        self.assertGreater(r['statistics']['steep_slope_pct'],90.)
        self.assertGreater(max(r['cells']['roughness_m']),3.)

    def test_diagonal_contact_does_not_merge(self):
        g,s=grid(2,2)
        labels,groups=c.components(g,[True,False,False,True])
        self.assertEqual(sorted(map(len,groups)),[1,1])

    def test_tiny_platform_filtered(self):
        g,s=grid(3,3); r=c.analyze(g,s)
        self.assertEqual(r['statistics']['habitat_fragments_before_area_filter'],1)
        self.assertEqual(r['statistics']['significant_basins'],0)

    def test_hole_not_filled_and_coverage_reported(self):
        g,s=grid(); g.z[210]=None
        r=c.analyze(g,s)
        self.assertEqual(r['cells']['category'][210],'UNKNOWN')
        self.assertEqual(r['statistics']['sampled_terrain_m2'],399)

    def test_cliff_discontinuity_splits_basins(self):
        g,s=grid(height=lambda x,y:0 if x<10 else 30)
        r=c.analyze(g,s)
        self.assertEqual(r['statistics']['significant_basins'],2)
        self.assertEqual(r['statistics']['isolated_basins'],2)
        self.assertEqual(r['corridors'],[])

    def test_water_excludes_housing_and_distance(self):
        g,s=grid(); g.water_z=[1 if i%g.nx<4 else None for i in range(len(g.z))]
        r=c.analyze(g,s)
        self.assertEqual(r['cells']['category'][0],'WATER_OR_MARGIN')
        self.assertAlmostEqual(r['cells']['water_distance_m'][10],7)
        self.assertEqual(r['statistics']['habitat_pct'],80)

    def test_unknown_navigation_is_not_zero(self):
        g,s=grid(); r=c.analyze(g,s)
        self.assertIsNone(r['statistics']['navigable_pct'])
        self.assertIsNone(r['candidates'][0]['subscores']['connectivity'])
        self.assertLess(r['candidates'][0]['score_evidence_weight_fraction'],1.)

    def test_nav_adjacent_but_blocked_is_two_components(self):
        g,s=grid(2,2); g.nav=[True]*4; g.nav_edges={(0,2),(1,3)}
        r=c.analyze(g,s)
        self.assertEqual(r['statistics']['nav_components'],2)

    def test_paired_snapshot_comparison_and_mismatch_rejection(self):
        g,s=grid(); before=c.analyze(g,s)
        g.z=[v+10 for v in g.z]; after=c.analyze(g,s)
        result=c.compare(before,after)
        self.assertEqual(result['statistics']['habitat_pct']['delta'],0.)
        self.assertNotEqual(before['sample_sha256'],after['sample_sha256'])
        s.settlement_slope_deg=7
        with self.assertRaisesRegex(ValueError,'INCOMPARABLE'): c.compare(before,c.analyze(g,s))

    def test_export_json_and_human_report_no_overwrite(self):
        g,s=grid(); r=c.analyze(g,s)
        with tempfile.TemporaryDirectory() as d:
            paths=c.export(r,d,'Baseline')
            with open(paths[0],encoding='utf-8') as f: self.assertEqual(json.load(f)['comparison_key'],r['comparison_key'])
            with open(paths[1],encoding='utf-8') as f: self.assertIn('Global statistics',f.read())
            with self.assertRaises(FileExistsError): c.export(r,d,'Baseline')
            with self.assertRaises(ValueError): c.export(r,d,'../escape')

    def test_triangle_rasterization_world_units_and_rotation(self):
        g,s=grid(4,4)
        g.z=[None]*16
        c.rasterize(g,[(0,0,0),(4,0,4),(4,4,4),(0,4,0)],[0,1,2,0,2,3])
        for i,z in enumerate(g.z): self.assertAlmostEqual(z,g.xy(i)[0])
        slopes,rough,_=c.metrics(g,s)
        self.assertAlmostEqual(slopes[5],45.)
        self.assertLess(max(rough),1e-12)

    def test_grid_covers_exact_extent(self):
        s=c.Settings(step_m=8)
        g=c.Grid.covering((.5,.5,95.5,95.5),s)
        self.assertAlmostEqual(len(g.z)*g.dx*g.dy,9025.)
        self.assertLessEqual(g.dx,s.step_m)

    def test_invalid_parameters_and_budget(self):
        for kwargs in ({'step_m':0},{'step_m':float('nan')},{'max_cells':.5},{'settlement_slope_deg':50}):
            with self.assertRaises(ValueError): c.Settings(**kwargs).validate()
        with self.assertRaises(ValueError): c.Grid.covering((0,0,10000,10000),c.Settings())

    def test_corridor_between_flat_basins(self):
        g,s=grid(30,10,height=lambda x,y:0. if x<10 else (2. if x>20 else (x-10)*.2))
        s.relief_radius_m=1.; s.settlement_roughness_m=.5; s.min_settlement_area_m2=20
        r=c.analyze(g,s)
        self.assertEqual(r['statistics']['significant_basins'],2)
        self.assertEqual(r['statistics']['isolated_basins'],0)
        self.assertEqual(len(r['corridors']),1)
        self.assertGreater(r['corridors'][0]['length_m'],8)
        self.assertFalse(r['corridors'][0]['nav_verified'])

    def test_missing_all_samples_rejected(self):
        g,s=grid(); g.z=[None]*len(g.z)
        with self.assertRaises(ValueError): c.analyze(g,s)

    def test_water_above_and_below_terrain(self):
        g,s=grid(); g.z=[2.]*len(g.z); g.water_z=[1.]*len(g.z)
        r=c.analyze(g,s)
        self.assertEqual(r['statistics']['habitat_pct'],100.)
        self.assertIsNone(r['regions'][0]['water_distance_m'])

    def test_changed_loaded_landscape_coverage_rejected(self):
        g,s=grid()
        a=c.analyze(g,s,{'targets':[{'path':'Landscape_A'},{'path':'Landscape_B'}]})
        b=c.analyze(g,s,{'targets':[{'path':'Landscape_A'}]})
        with self.assertRaisesRegex(ValueError,'INCOMPARABLE'): c.compare(a,b)

    def test_neighbourhood_budget_prevents_unbounded_work(self):
        g,s=grid(); s.max_neighborhood_visits=1
        with self.assertRaisesRegex(ValueError,'budget exceeded'): c.analyze(g,s)

if __name__=='__main__': unittest.main(verbosity=2)
