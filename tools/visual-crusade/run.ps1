param([ValidateSet('before','after')][string]$Phase='before')
$ErrorActionPreference='Stop'
$env:ANASTASIS_EDITOR_MAX='1'
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $Root 'tools/unreal/editor-launch.ps1')
$out=Join-Path $Root ('Saved/VisualCrusade/'+$Phase)
New-Item -ItemType Directory -Force $out | Out-Null
$jobs=@(
 @{name='ground';script=(Join-Path $Root 'tools/unreal/ground-cover-capture.py');timeout=600;env=@{ANASTASIS_GROUND_OUT=(Join-Path $out 'ground');ANASTASIS_GROUND_STATES='on';ANASTASIS_CAPTURE_VIEWS='prairie_eye,lisiere_eye,sousbois_eye,oblique'}},
 @{name='river';script=(Join-Path $Root 'tools/unreal/riverbank-capture.py');timeout=600;env=@{ANASTASIS_RIVERBANK_OUT=(Join-Path $out 'river');ANASTASIS_RIVERBANK_STATES='water';ANASTASIS_CAPTURE_VIEWS='calme_plaine,rive_lac'}}
)
if ($Phase -eq 'after') {
  $jobs = @(@{name='generate';script=(Join-Path $Root 'tools/visual-crusade/generate.py');timeout=900;env=@{}}) + $jobs + @(@{name='village';script=(Join-Path $Root 'tools/visual-crusade/village.py');timeout=120;env=@{}})
}
$jobsFile=Join-Path $out 'jobs.json'
ConvertTo-Json -InputObject $jobs -Depth 5 | Set-Content $jobsFile -Encoding utf8
$env:ANASTASIS_EDITOR_BATCH_JOBS=$jobsFile
$py=(Join-Path $Root 'tools/unreal/editor-batch.py').Replace('\','/')
$log=Join-Path $out 'capture.log'
$argsEditor=@(('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),'-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding','-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',('-abslog="'+$log+'"'),('-ExecCmds="py '+$py+'"'))
$p=Start-AnastasisEditor 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' $argsEditor
Write-Output ('VISUAL_CRUSADE_PID::'+$p.Id)
$p | Wait-Process
Select-String -Path $log -Pattern 'EDITOR_BATCH_|CAPTURE_COMPLETE|CAPTURE_FAIL|Error:' | Select-Object -Last 25
