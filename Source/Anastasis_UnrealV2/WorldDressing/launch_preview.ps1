param(
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [switch]$KeepOpen,
    [switch]$Edge
)
$ErrorActionPreference='Stop'
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$Out=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$env:ANASTASIS_DRESSING_OUT=$Out
$env:ANASTASIS_DRESSING_KEEP_OPEN=if($KeepOpen){'1'}else{'0'}
$env:ANASTASIS_DRESSING_PROJECT=$Root
$env:ANASTASIS_DRESSING_BASE='394448ce28c03844e2bb20a227747fd3948f117d'
. (Join-Path $Root 'tools/unreal/mcp-port.ps1')
$McpPort=Get-AnastasisMcpPort $Root
$Script=(Join-Path $PSScriptRoot $(if($Edge){'capture_edge.py'}else{'capture_preview.py'})).Replace('\','/')
$LaunchArgs=@(
    ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
    '-windowed','-resx=1600','-resy=900','-nosplash','-NoLiveCoding','-unattended',
    "-ModelContextProtocolPort=$McpPort",
    '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
    ('-abslog="'+(Join-Path $Out 'capture.log')+'"'),
    ('-ExecCmds="py '+$Script+'"')
)
. (Join-Path $Root 'tools/unreal/editor-launch.ps1')
$Process=Start-AnastasisEditor 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList $LaunchArgs
Write-Output "WORLD_DRESSING_EDITOR_PID::$($Process.Id)"
Write-Output "EVIDENCE::$Out"
