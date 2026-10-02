import unittest
from anastasis_map_intelligence import concordance


def sample():
    return {'water_concordance':dict(width=3,height=2,compared_cells=4,mismatch_cells=2,
        status='SAMPLED',agreement='MISMATCH',both_dry=[0],both_water=[1],
        simulation_only=[2],render_only=[3],unknown=[4,5])}


class ConcordanceTests(unittest.TestCase):
    def test_directional_mismatches_are_preserved(self):
        self.assertEqual(concordance.validate(sample())['mismatch'],2)
        self.assertIn('simulation_only: 1',concordance.svg(sample()))

    def test_overlapping_or_missing_cells_rejected(self):
        for indices in ([3,5],[5],[-1,5]):
            data=sample(); data['water_concordance']['unknown']=indices
            with self.assertRaises(ValueError): concordance.validate(data)

    def test_false_success_and_false_counts_rejected(self):
        for key,value in [('agreement','AGREEMENT_AT_CENTRES'),('mismatch_cells',0),('status','PASS')]:
            data=sample(); data['water_concordance'][key]=value
            with self.assertRaises(ValueError): concordance.validate(data)

    def test_unknown_is_not_agreement(self):
        data=sample(); c=data['water_concordance']
        for key in concordance.GROUPS: c[key]=[]
        c.update(unknown=list(range(6)),compared_cells=0,mismatch_cells=0,status='UNKNOWN',agreement='UNKNOWN')
        self.assertEqual(concordance.validate(data)['agreement'],'UNKNOWN')
        c['agreement']='AGREEMENT_AT_CENTRES'
        with self.assertRaises(ValueError): concordance.validate(data)
