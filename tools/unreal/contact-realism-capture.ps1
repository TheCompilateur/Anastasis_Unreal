# AAA_CONTACT_REALISM_001 -- la peau de contact, avant / apres, aux memes cameras.
# -States "contact,base,contact2" (defaut) : couche active, couche coupee, couche active de nouveau
# (l'ecart contact / contact2 = derive de capture). Voir contact-realism-capture.py.
# Sortie : Saved\ContactRealismEvidence\<Label>\<vue>_<etat>.png + cameras.json + plan.json + metrics.json
# -Profile : un ProfileGPU par vue ; les lignes de passe sont recopiees dans profile.txt.
# -Materials : rejoue d'abord aaa-contact-realism.py DANS CET EDITEUR (avec rendu, donc avec compilation
# reelle des shaders : une erreur HLSL y est visible tout de suite) ; -Rebuild regenere aussi le maitre.
param([string]$Label='latest', [string]$States='contact,base,contact2', [string]$Views='', [int]$TimeoutSec=1500, [switch]$Profile, [switch]$Materials, [switch]$Rebuild)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root "Saved\ContactRealismEvidence\$Label"
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'capture.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_ACR_OUT=$dir
$env:ANASTASIS_ACR_STATES=$States
$env:ANASTASIS_ACR_VIEWS=$Views
$env:ANASTASIS_ACR_PROFILE= if ($Profile) { '1' } else { '0' }
$py=(Join-Path $Root 'tools\unreal\contact-realism-capture.py').Replace('\','/')
$mat=(Join-Path $Root 'tools\unreal\aaa-contact-realism.py').Replace('\','/')
$env:ANASTASIS_ACR_QUIT='0'
$env:ANASTASIS_ACR_REBUILD= if ($Rebuild) { '1' } else { '0' }
$exec= if ($Materials) { 'py '+$mat+',py '+$py } else { 'py '+$py }
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
 # Hors premier plan, l'editeur coupe le rendu des viewports et HighResShot n'est jamais servi.
 '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
 ('-abslog="'+$log+'"'),
 ('-ExecCmds="'+$exec+'"')
)
$p=Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force; throw 'CAPTURE::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'ANASTASIS_CONTACT|ACR_CAPTURE|ACR_SHOT' |
  ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') } | Select-Object -Unique
if(-not (Select-String -Path $log -Pattern 'ACR_CAPTURE_COMPLETE' -Quiet)){ throw 'CAPTURE::FAIL capture incomplete' }
# Un materiau de decalque qui ne compile pas devient le materiau par defaut (rectangle noir opaque) et
# la capture a l'air d'une capture : aaa-contact-realism.ps1 tourne en -nullrhi et ne compile aucun shader.
$all = @(Get-Content $log)
$from = 0
for($i = 0; $i -lt $all.Count; $i++){ if($all[$i] -match 'ACR_MATERIAL_COMPILE'){ $from = $i } }   # le vidage du graphe (-Rebuild) emet des avertissements transitoires
$bad = @($all[$from..($all.Count - 1)] | Where-Object { $_ -match 'M_ACR_Decal.*Failed to compile|Failed to compile Material /Game/Anastasis/AAAContactRealism|\(Node Custom\) Custom material ACR_' } | ForEach-Object { [pscustomobject]@{ Line = $_ } })
if($bad.Count){ throw ('CAPTURE::FAIL materiau de contact non compile : ' + $bad[0].Line.Trim()) }
if($Profile){
 $out=@(); $cur=$null
 foreach($l in Get-Content $log){
  if($l -match 'ACR_PROFILE_BEGIN (\S+)'){ $cur=$Matches[1]; $out+="== $cur"; continue }
  if($l -match 'ACR_SHOT_OK'){ $cur=$null; continue }
  if($cur -and $l -match 'LogRHI'){ $out+=($l -replace '^\[[^\]]*\]\[[ 0-9]*\]','') }
 }
 $out | Set-Content -Encoding utf8 (Join-Path $dir 'profile.txt')
}
Write-Output ('CAPTURE::PASS ' + $dir)
