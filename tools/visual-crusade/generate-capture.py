"""Generate our tree assets and immediately capture the existing slice, without a proxy module."""
import os
root=os.path.abspath(os.path.join(os.path.dirname(__file__),'../..'))
path=os.path.join(root,'tools/visual-crusade/generate.py')
with open(path,encoding='utf-8-sig') as f:
    code=f.read().replace('unreal.SystemLibrary.quit_editor()','')
exec(compile(code,path,'exec'),{'__name__':'__main__','__file__':path})
os.environ['ANASTASIS_CAPTURE_VIEWS']='prairie_eye,lisiere_eye,sousbois_eye,oblique,riviere_eye'
path=os.path.join(root,'tools/unreal/ground-cover-capture.py')
with open(path,encoding='utf-8-sig') as f:
    code=f.read()
exec(compile(code,path,'exec'),{'__name__':'__main__','__file__':path})
