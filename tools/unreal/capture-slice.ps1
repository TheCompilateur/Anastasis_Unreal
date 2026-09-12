param([ValidateSet('0','1')][string]$Mode='1',[Parameter(Mandatory=$true)][string]$Out,[string]$RebuildMaterial='0',[string]$Exposure='12.0')
$ErrorActionPreference='Stop'
$Root='C:\dev\ANASTASIS_UNREAL'
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root 'Saved\SliceEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$shot=Join-Path $dir $Out
if(Test-Path $shot){Remove-Item $shot}
$log=Join-Path $dir ($Out -replace '\.png$','.log')
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_SLICE_SHOT=$shot
$env:ANASTASIS_SLICE_MODE=$Mode
$env:ANASTASIS_SLICE_REBUILD_MATERIAL=$RebuildMaterial
$env:ANASTASIS_SLICE_EV=$Exposure
$launchArgs=@(
 '"C:\dev\ANASTASIS_UNREAL\Anastasis_UnrealV2.uproject"',
 '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
 ('-abslog="'+$log+'"'),
 '-ExecCmds="py C:/dev/ANASTASIS_UNREAL/tools/unreal/observe-slice.py"'
)
$p=Start-Process $Editor -ArgumentList $launchArgs -PassThru
$p | Wait-Process -Timeout 300 -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){
 Stop-Process -Id $p.Id -Force
 throw 'CAPTURE::FAIL editeur bloque'
}
Select-String -Path $log -Pattern 'SLICE_|ANASTASIS_TERRAIN ' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') }
if(!(Test-Path $shot)){throw 'CAPTURE::FAIL pas de capture'}
Write-Output ('CAPTURE::PASS ' + $shot)
