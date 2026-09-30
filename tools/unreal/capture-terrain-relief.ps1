# TERRAIN_RELIEF_001 -- captures avant / apres d'une etape de la forge.
# Voir terrain-relief-capture.py. Sortie : Saved\TerrainReliefEvidence\step<N>\<vue>_<etat>.png
param([ValidateSet('1','2','3','scale')][string]$Step='scale', [int]$TimeoutSec=420)
$ErrorActionPreference='Stop'
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root "Saved\TerrainReliefEvidence\step$Step"
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'capture.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_RELIEF_OUT=$dir
$env:ANASTASIS_RELIEF_STEP=$Step
$py=(Join-Path $Root 'tools\unreal\terrain-relief-capture.py').Replace('\','/')
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
 # Hors premier plan, UEditorEngine::Tick coupe le rendu des viewports quand
 # bThrottleCPUWhenNotForeground est vrai (EditorEngine.cpp, "Background Process") :
 # ~3 images/s et HighResShot jamais servi. La classe est config=EditorSettings.
 # Surcharge de ligne de commande, en memoire : rien n'est ecrit dans Saved/Config.
 '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
 ('-abslog="'+$log+'"'),
 ('-ExecCmds="py '+$py+'"')
)
$p=Start-Process $Editor -ArgumentList $launchArgs -PassThru
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){
 Stop-Process -Id $p.Id -Force
 throw 'CAPTURE::FAIL editeur bloque'
}
Select-String -Path $log -Pattern 'RELIEF_' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') }
# L'etape 'scale' n'a qu'un etat : l'avant (1 m/tuile) ne se rend plus dans ce build.
$states = if ($Step -eq 'scale') { @('after') } else { @('before','after') }
foreach($view in 'A_overview','B_ground','C_slope'){
 foreach($state in $states){
  $shot=Join-Path $dir "$($view)_$state.png"
  if(!(Test-Path $shot)){throw "CAPTURE::FAIL missing $($view)_$state.png"}
 }
}
Write-Output ('CAPTURE::PASS ' + $dir)
