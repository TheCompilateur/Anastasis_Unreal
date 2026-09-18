param(
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [switch]$KeepOpen
)
$ErrorActionPreference='Stop'
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$Out=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$env:ANASTASIS_DRESSING_OUT=$Out
$env:ANASTASIS_DRESSING_KEEP_OPEN=if($KeepOpen){'1'}else{'0'}
$Script=(Join-Path $PSScriptRoot 'capture_preview.py').Replace('\','/')
$LaunchArgs=@(
    ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
    '-windowed','-resx=1600','-resy=900','-nosplash','-NoLiveCoding','-unattended',
    ('-abslog="'+(Join-Path $Out 'capture.log')+'"'),
    ('-ExecCmds="py '+$Script+'"')
)
$Process=Start-Process 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList $LaunchArgs -WindowStyle Hidden -PassThru
Write-Output "WORLD_DRESSING_EDITOR_PID::$($Process.Id)"
Write-Output "EVIDENCE::$Out"
