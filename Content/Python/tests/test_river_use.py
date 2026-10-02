import copy
import math
import unittest
import anastasis_river_use as p


def rectangle(x0,y0,x1,y1,z):
    return p.Mesh([(x0,y0,z),(x1,y0,z),(x1,y1,z),(x0,y1,z)],[0,1,2,0,2,3])


def journey():
    samples=[]
    for i in range(61):
        samples.append(dict(time=i*.02,position_m=[i*.4,0],tile_m=20,
            physical=dict(dry=True,slope=2,z=1),river_distance_m=1 if i==60 else None,
            drinks=1 if i==60 else 0,thirst=50 if i==60 else 80,
            autonomous=True,building_count=0,npc_count=1,foot_blocked=False,
            at_drink_spot=i==60,goal='drink' if i else 'idle',has_target=i!=0,source='shore'))
    return samples,dict(bank_m=[24,0],bank_radius_m=20,river_tolerance_m=10)


class GeometryTests(unittest.TestCase):
    def test_triangle_surface_and_boundaries(self):
        m=rectangle(0,0,10,10,2)
        self.assertEqual(m.sample(5,5),(2,0))
        self.assertIsNone(m.sample(11,5))
        self.assertEqual(len(m.boundary),4) # no diagonal masquerading as bank
        self.assertEqual(m.near(12,5),2)
        self.assertIsNone(m.near(21,5))

    def test_highest_overlapping_surface(self):
        m=p.Mesh([(0,0,0),(4,0,0),(0,4,0),(0,0,2),(4,0,2),(0,4,2)],list(range(6)))
        self.assertEqual(m.sample(1,1),(2,0))

    def test_flooded_and_unknown_ground(self):
        g=rectangle(0,0,10,10,0); water=rectangle(0,0,5,5,1)
        self.assertFalse(p.physical(g,(water,),2,2)['dry'])
        self.assertTrue(p.physical(g,(water,),8,8)['dry'])
        self.assertIsNone(p.physical(g,(water,),11,11)['dry'])

    def test_steep_triangle_measured_in_degrees(self):
        m=p.Mesh([(0,0,0),(1,0,1),(0,1,0)],[0,1,2])
        self.assertAlmostEqual(m.sample(.2,.2)[1],45)

    def test_no_river_never_falls_back_to_lake(self):
        ctx=dict(site={'selected':dict(eligible=True,x=2,y=2)},tile_m=20,width=5,height=5,cells=[])
        with self.assertRaisesRegex(ValueError,'No dry gentle river bank'):
            p.select(ctx,rectangle(0,0,100,100,1),rectangle(0,0,20,20,2),p.Mesh([],[]))

    def test_buried_ribbon_edges_do_not_hide_actual_shoreline(self):
        class Ground:
            def sample(self,x,y):
                return (0.,0.) if 4<=x<=6 else (2.,0.)
        ctx=dict(site={'selected':dict(eligible=True,x=2,y=2)},tile_m=20,width=6,height=6,
                 cells=[dict(walkable=True,drinkable=False) for _ in range(36)])
        choice=p.select(ctx,Ground(),p.Mesh([],[]),rectangle(0,0,10,100,1))
        x,y=choice['water_m']
        self.assertTrue(4<=x<=6)
        self.assertTrue(p.physical(Ground(),(rectangle(0,0,10,100,1),),*choice['bank_m'])['dry'])

    def test_fully_buried_river_is_not_a_visible_source(self):
        ctx=dict(site={'selected':dict(eligible=True,x=2,y=2)},tile_m=20,width=5,height=5,cells=[])
        with self.assertRaisesRegex(ValueError,'No dry gentle river bank'):
            p.select(ctx,rectangle(-50,-50,200,200,2),p.Mesh([],[]),rectangle(0,0,10,100,1))


class VerdictTests(unittest.TestCase):
    def test_complete_autonomous_journey(self):
        s,c=journey(); self.assertEqual(p.verdict(s,c),('PASS','autonomous_river_journey_and_drink'))

    def test_well_or_player_contaminates(self):
        for key,value in [('building_count',1),('autonomous',False),('npc_count',2)]:
            s,c=journey(); s[10][key]=value
            self.assertEqual(p.verdict(s,c),('FAIL','scenario_contaminated'))

    def test_counter_alone_is_not_proof(self):
        for key,value,reason in [('thirst',79,'drink_without_need_relief'),
                                 ('river_distance_m',None,'drink_away_from_visible_river')]:
            s,c=journey(); s[-1][key]=value
            self.assertEqual(p.verdict(s,c),('FAIL',reason))

    def test_other_bank_does_not_pass(self):
        s,c=journey(); c['bank_m']=[100,100]
        self.assertEqual(p.verdict(s,c),('FAIL','different_bank_selected'))

    def test_missing_samples_never_pass(self):
        s,c=journey(); s=s[::5]
        self.assertEqual(p.verdict(s,c)[0],'UNKNOWN')

    def test_teleport_cannot_supply_journey(self):
        s,c=journey(); s[1]['position_m']=[30,0]
        self.assertEqual(p.verdict(s,c),('UNKNOWN','position_jump'))

    def test_recognition_must_be_observed(self):
        s,c=journey()
        for row in s: row['has_target']=False
        self.assertEqual(p.verdict(s,c),('FAIL','recognition_not_observed'))

    def test_wet_or_unknown_route(self):
        for dry,expected in [(False,'FAIL'),(None,'UNKNOWN')]:
            s,c=journey(); s[12]['physical']['dry']=dry
            self.assertEqual(p.verdict(s,c)[0],expected)

    def test_no_drink_and_initial_drinkable(self):
        s,c=journey(); self.assertEqual(p.verdict(s[:-1],c,final=True)[0],'UNKNOWN')
        s[0]['at_drink_spot']=True
        self.assertEqual(p.verdict(s,c),('FAIL','invalid_initial_control'))

    def test_uncertainty_at_water_edge_is_conservative(self):
        s,c=journey(); s[-1]['river_distance_m']=9
        self.assertEqual(p.verdict(s,c),('FAIL','drink_away_from_visible_river'))
