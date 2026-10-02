"""Same world/cameras: 14h A/B + humid morning + overcast. Capture completion is not artistic PASS."""
import os, runpy
os.environ['ANASTASIS_WEATHER_CONTINUE'] = '1'
runpy.run_path(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'weather-contract.py'), run_name='weather_contract')
os.environ.pop('ANASTASIS_WEATHER_CONTINUE', None)
os.environ['ANASTASIS_SKY_VIEWS'] = 'ov_sw,valley_long,ridge_long,riviere_eye,sousbois_eye'
os.environ['ANASTASIS_SKY_STATES'] = (
    'before=anastasis.Sky.Day 1;anastasis.Sky.Hour 14;anastasis.Sky.Humidity 0.45;anastasis.Sky.Wind 0.3;anastasis.Sky.Cover 0.25;anastasis.Atmosphere.Coupling 0|'
    'reference=anastasis.Atmosphere.Coupling 1|'
    'reference2=anastasis.Atmosphere.Coupling 1|'
    'morning=anastasis.Sky.Hour 7;anastasis.Sky.Humidity 0.85;anastasis.Sky.Wind 0.12;anastasis.Sky.Cover 0.4|'
    'overcast=anastasis.Sky.Hour 14;anastasis.Sky.Humidity 0.72;anastasis.Sky.Wind 0.5;anastasis.Sky.Cover 0.9')
p = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'capture-sky.py')
exec(compile(open(p, encoding='utf-8').read(), p, 'exec'), globals())

# Exact fixed poses from visual-crusade-001/before/ground/cameras.json; no foliage-bounds recentering.
for label, position, target in [['riviere_eye', [98484.375, 109825.0, 1017.5780487060547], [100473.95833333333, 116191.66666666666, 432.9681396484375]], ['sousbois_eye', [173136.07116057913, 100281.35934773751, 5791.802806854248], [171150.25030995623, 100159.05894091477, 5337.982692718506]]]:
    views.append((label, V(*position), V(*target)))
