import unittest
import math
from anastasis_terrain_access import Surface, inspect_path, observed_segment


def plane(z=0., slope=0.):
    return Surface([[0,0,z],[10,0,z+10*slope],[10,10,z+10*slope],[0,10,z]], [0,1,2,0,2,3])

class TerrainAccessTests(unittest.TestCase):
    def test_flat(self):
        r=inspect_path([[1,1],[9,9]],plane(),[])
        self.assertEqual(r['status'],'NO_ANOMALY_AT_SAMPLES')
        self.assertAlmostEqual(r['length_m'],math.sqrt(128))
    def test_slope(self):
        r=inspect_path([[1,1],[9,1]],plane(slope=1),[])
        self.assertAlmostEqual(r['max_slope_deg'],45)
        self.assertIn('slope_over_policy',r['flags'])
    def test_exposed_water(self):
        self.assertIn('water_or_low_freeboard',inspect_path([[1,1]],plane(),[plane(.5)])['flags'])
    def test_buried_ribbon(self):
        self.assertEqual(inspect_path([[1,1]],plane(1),[plane()])['flags'],[])
    def test_missing(self):
        self.assertEqual(inspect_path([[20,20]],plane(),[])['status'],'UNKNOWN')
    def test_between_waypoints(self):
        water=Surface([[4,0,1],[6,0,1],[6,10,1],[4,10,1]],[0,1,2,0,2,3])
        self.assertIn('water_or_low_freeboard',inspect_path([[1,1],[9,1]],plane(),[water])['flags'])
    def test_highest_surface(self):
        s=Surface([[0,0,0],[10,0,0],[0,10,0],[0,0,3],[10,0,3],[0,10,3]],[0,1,2,3,4,5])
        self.assertEqual(s.sample(1,1)[0],3)
    def test_step_between_flat_surfaces(self):
        ground=Surface([[0,0,0],[5,0,0],[5,10,0],[0,10,0],
                        [5,0,3],[10,0,3],[10,10,3],[5,10,3]],
                       [0,1,2,0,2,3,4,5,6,4,6,7])
        r=inspect_path([[1,1],[9,1]],ground,[])
        self.assertEqual(r['max_slope_deg'],0)
        self.assertIn('grade_over_policy',r['flags'])
        self.assertEqual(r['sampled_ascent_m'],3)
    def test_invalid_mesh(self):
        with self.assertRaises(ValueError): Surface([[0,0,0]],[0,1,2])
    def test_invalid_step(self):
        with self.assertRaises(ValueError): inspect_path([[1,1]],plane(),[],step=0)
    def test_gap(self):
        a={'id':'n','inside':False,'position':{'x':.1,'y':.1}}
        self.assertEqual(observed_segment({'time':0,'actor':a},{'time':1,'actor':a},20,plane(),[])['status'],'UNKNOWN')
    def test_observed(self):
        a={'id':'n','inside':False,'position':{'x':.1,'y':.1}}
        b=dict(a,position={'x':.11,'y':.1})
        self.assertEqual(observed_segment({'time':0,'actor':a},{'time':.05,'actor':b},20,plane(),[])['status'],'NO_ANOMALY_AT_SAMPLES')

if __name__=='__main__': unittest.main()
