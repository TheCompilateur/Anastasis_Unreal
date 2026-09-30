"""Read-only PIE proof for Anastasis.Village.FoodSupply. No assets saved.
ANASTASIS_FOOD_OUT selects the evidence directory. Samples conservation each frame,
records pickup/deposit/meal/depletion; missing stages fail, never a narrative PASS.
"""
import json
import os
import time
from pathlib import Path
import unreal
out = Path(os.environ['ANASTASIS_FOOD_OUT'])
out.mkdir(parents=True, exist_ok=True)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
DBG = unreal.AnastasisSimulationDebugLibrary
les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
# Unsaved camera is duplicated with the level into PIE; no hidden spawn API.
probe_camera=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.CameraActor,unreal.Vector(0,0,1000),unreal.Rotator())
probe_camera.set_actor_label('FoodSupplyProofCamera')
t0 = time.monotonic()
phase = 0
handle = None
seen = set()
samples = []
last = None
start = None
camera = None
captured = False
completed_at = None
previous_sample = None
carry_distance = 0.0
finished = False

def finish(ok, reason):
    global finished
    if finished:
        return
    finished=True
    (out / 'food-supply.json').write_text(json.dumps({'pass':ok,'reason':reason,'screenshot':(out/'circuit.png').is_file(),'seen':sorted(seen),'samples':samples},indent=2),encoding='utf-8')
    unreal.log('FOOD_SUPPLY_' + ('PASS ' if ok else 'FAIL ') + reason)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()

def tick(dt):
    global phase, last, start, camera, captured, completed_at, previous_sample, carry_distance
    if time.monotonic()-t0 > 300:
        finish(False, 'wall timeout')
        return
    if phase == 0 and time.monotonic()-t0 > 3:
        phase = 1
        les.editor_request_begin_play()
        return
    if phase == 1 and les.is_in_play_in_editor():
        world = ues.get_game_world()
        if not world or DBG.get_simulation_time(world) < 0:
            return
        unreal.SystemLibrary.execute_console_command(world,'Anastasis.Village.FoodSupply')
        # Place a transient PIE camera above this circuit, using the actual spawned granary.
        buildings = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AnastasisVillageBuilding)
        if buildings:
            target = buildings[0].get_actor_location()
            position = target + unreal.Vector(-12000,-15000,16000)
            rotation = unreal.MathLibrary.find_look_at_rotation(position,target)
            cameras=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CameraActor)
            camera=next(c for c in cameras if c.get_actor_label()=='FoodSupplyProofCamera')
            camera.set_actor_location(position,False,True)
            camera.set_actor_rotation(rotation,True)
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            if pc:
                pc.set_view_target_with_blend(camera,0.0)
        phase = 2
        return
    if phase != 2:
        return
    world = ues.get_game_world()
    s = json.loads(DBG.get_food_supply_status(world))
    if not s or s['initial'] <= 0:
        finish(False,'scenario did not create a finite source')
        return
    if start is None:
        start = s['time']
    if s['initial'] != s['remaining']+s['bag']+s['stock']+s['meals']:
        samples.append(s)
        finish(False,'food conservation broken')
        return
    if previous_sample and previous_sample['bag']>0:
        carry_distance += ((s['x']-previous_sample['x'])**2+(s['y']-previous_sample['y'])**2)**0.5
    s['carry_distance']=carry_distance
    previous_sample=s
    stages = {'carry':carry_distance>0.75,'pickup':s['bag']>0,'deposit':s['delivered']>0,'meal':s['meals']>0,'depletion':s['remaining']==0}
    changed = {k for k,v in stages.items() if v} - seen
    if changed:
        seen.update(changed)
        unreal.log('FOOD_SUPPLY_STAGE ' + ','.join(sorted(changed)) + ' ' + json.dumps(s))
        unreal.SystemLibrary.execute_console_command(world,'Anastasis.Village.Status')
    if changed or last is None or s['time']-last >= 5:
        samples.append(s)
        last=s['time']
    if not captured and len(seen)==5 and camera:
        captured=True
        unreal.SystemLibrary.execute_console_command(world,'anastasis.Sim.Speed 0')
        unreal.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename="'+str(out/'circuit.png').replace('\\','/')+'"')
    if len(seen)==5:
        if completed_at is None:
            completed_at=time.monotonic()
        elif ((out/'circuit.png').is_file() and time.monotonic()-completed_at>3) or time.monotonic()-completed_at>30:
            finish(True,'pickup, actual carrying movement, deposit, meal and finite source depletion observed; sampled conservation holds')
    elif s['time']-start > 300:
        finish(False,'simulation deadline; missing stages '+str(set(stages)-seen))

def guarded_tick(dt):
    try:
        tick(dt)
    except Exception as exc:
        import traceback
        unreal.log_error(traceback.format_exc())
        finish(False, str(exc))

handle=unreal.register_slate_post_tick_callback(guarded_tick)
