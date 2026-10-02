param([string]$Label='after-v2')
$ErrorActionPreference='Stop'
$env:ANASTASIS_EDITOR_MAX='1'
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $Root 'tools/unreal/editor-launch.ps1')
$out=Join-Path $Root ('Saved/VisualCrusade/'+$Label)
New-Item -ItemType Directory -Force $out | Out-Null
$env:ANASTASIS_GROUND_OUT=Join-Path $out 'ground'
$env:ANASTASIS_GROUND_STATES='on'
$env:ANASTASIS_CAPTURE_VIEWS='prairie_eye,lisiere_eye,sousbois_eye,oblique,riviere_eye'
$env:ANASTASIS_GROUND_CAMERAS=Join-Path $Root 'Saved/VisualCrusade/before/ground/cameras.json'
$py=(Join-Path $PSScriptRoot 'generate-capture.py').Replace('\','/')
$log=Join-Path $out 'capture.log'
$launchArgs=@(('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),'/Engine/Maps/Entry','-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding','-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',('-abslog="'+$log+'"'),('-ExecCmds="py '+$py+'"'))
$p=Start-AnastasisEditor 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' $launchArgs
$p | Wait-Process
$p.Refresh()
if ($p.ExitCode -ne 0) { throw "EDITOR_EXIT::FAIL $($p.ExitCode); images may exist but clean exit is not proved" }
if (-not (Select-String -Path $log -SimpleMatch 'GROUND_CAPTURE_COMPLETE views=5 states=1' -Quiet)) {
    throw 'CAPTURE::INCOMPLETE'
}
Select-String -Path $log -Pattern 'CRUSADE_GENERATION|CAPTURE_COMPLETE|CAPTURE_FAIL|Fatal error' | Select-Object -Last 10
