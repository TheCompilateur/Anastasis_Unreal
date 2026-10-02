"""One-variable fog correction, same two poses and same hour within each pair."""
import os, runpy
os.environ['ANASTASIS_WEATHER_CONTINUE'] = '1'
runpy.run_path(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'weather-contract.py'), run_name='weather_contract')
os.environ.pop('ANASTASIS_WEATHER_CONTINUE', None)
os.environ['ANASTASIS_SKY_VIEWS'] = 'valley_long,riviere_eye'
os.environ['ANASTASIS_SKY_STATES'] = (
    'day_before=anastasis.Sky.Day 1;anastasis.Sky.Hour 14;anastasis.Sky.Humidity 0.45;anastasis.Sky.Wind 0.3;anastasis.Sky.Cover 0.25;anastasis.Atmosphere.Coupling 1;anastasis.Atmosphere.AirVisibility 0|'
    'day_after=anastasis.Atmosphere.AirVisibility 1|'
    'morning_before=anastasis.Sky.Hour 7;anastasis.Sky.Humidity 0.85;anastasis.Sky.Wind 0.12;anastasis.Sky.Cover 0.4;anastasis.Atmosphere.AirVisibility 0|'
    'morning_after=anastasis.Atmosphere.AirVisibility 1')
p = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'capture-sky.py')
exec(compile(open(p, encoding='utf-8').read(), p, 'exec'), globals())
views.append(('riviere_eye', V(98484.375,109825.0,1017.5780487060547), V(100473.95833333333,116191.66666666666,432.9681396484375)))
