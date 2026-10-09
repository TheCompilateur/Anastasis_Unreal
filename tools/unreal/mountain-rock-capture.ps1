# PONTIC_MOUNTAIN_PRESENCE_003 : one-editor 0/1/0 material comparison at S045.
param([string]$Label='mountain-rock', [int]$TimeoutSec=300)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root "Saved\MountainRockEvidence\$Label"
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'capture.log'
if(Test-Path $log){Remove-Item -LiteralPath $log}
$env:ANASTASIS_MOUNTAIN_ROCK_OUT=$dir
$py=(Join-Path $Root 'tools\unreal\mountain-rock-capture.py').Replace('\','/')
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
 '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
 ('-abslog="'+$log+'"'),
 ('-ExecCmds="py '+$py+'"')
)
$p=Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force; throw "MOUNTAIN_ROCK::FAIL editeur bloque pid=$($p.Id)" }
$status=Select-String -Path $log -Pattern 'MOUNTAIN_ROCK_(ACTORS|VIEW|SKY|SHOT|COMPLETE|FAIL)|ANASTASIS_TERRAIN_HORIZON' | ForEach-Object {$_.Line}
$status | ForEach-Object {Write-Output $_}
if(-not ($status | Where-Object {$_ -match 'MOUNTAIN_ROCK_COMPLETE'})){throw 'MOUNTAIN_ROCK::FAIL completion absente'}
foreach($tag in @('A','B','A2')){
 $path=Join-Path $dir "S045_$tag.png"
 if(!(Test-Path $path)){throw "MOUNTAIN_ROCK::FAIL image absente $path"}
}
Write-Output "MOUNTAIN_ROCK::CAPTURE_COMPLETE $dir"
