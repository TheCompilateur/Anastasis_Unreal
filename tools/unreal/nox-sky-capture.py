"""NOX_001: same level, seed, sky and player-height pose for the night profile triptych.

Editor batch proof. The shared capture-sky.py owns camera construction, frame settling,
GPU sampling and quit_editor; this script supplies only the controlled states.
SKY_CAPTURE_COMPLETE is an instrumental result, not a visual or player verdict.
"""
import os
import unreal

root = unreal.Paths.project_dir()
os.environ['ANASTASIS_SKY_OUT'] = os.path.join(unreal.Paths.project_saved_dir(), 'NoxEvidence', 'batch')
os.environ['ANASTASIS_SKY_VIEWS'] = 'valley_long,ridge_long'
os.environ['ANASTASIS_SKY_STATES'] = '|'.join((
    'day_base=anastasis.Sky.Day 1;anastasis.Sky.Hour 12;anastasis.Sky.Cover 0;anastasis.Nox.Profile 0',
    'day_nox=anastasis.Nox.Profile 1',
    'full_base=anastasis.Sky.Hour 0;anastasis.Nox.Profile 0',
    'full_austere=anastasis.Nox.Profile 1;anastasis.Nox.MoonFraction 1',
    'full_radical=anastasis.Nox.Profile 2',
    'full_controlled=anastasis.Nox.Profile 3',
    'dark_austere=anastasis.Nox.Profile 1;anastasis.Nox.MoonFraction 0',
    'dark_radical=anastasis.Nox.Profile 2',
    'dark_controlled=anastasis.Nox.Profile 3',
    'full_base_repeat=anastasis.Nox.Profile 0;anastasis.Nox.MoonFraction 1',
))
script = os.path.join(root, 'tools', 'unreal', 'capture-sky.py')
with open(script, encoding='utf-8') as source:
    exec(compile(source.read(), script, 'exec'), {'__name__': '__main__', '__file__': script})
