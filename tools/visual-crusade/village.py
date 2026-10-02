"""Run the existing short village proof in the already loaded slice."""
import os
import unreal
root=os.path.abspath(os.path.join(os.path.dirname(__file__),'../..'))
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if world.get_path_name().split('.')[0] != '/Game/Anastasis/Maps/Lvl_AnastasisSlice':
    raise RuntimeError('The slice must be loaded for the village check')
path=os.path.join(root,'tools/unreal/first-building-pie.py')
with open(path,encoding='utf-8') as f:
    code=f.read()
code=code.replace("les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')",'True')
exec(compile(code,path,'exec'),{'__name__':'__main__','__file__':path})
