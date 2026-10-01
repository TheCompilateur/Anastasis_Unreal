param([Parameter(Mandatory=$true)][string]$OutDir,[int]$TimeoutSec=1500)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
. (Join-Path $PSScriptRoot 'mcp-port.ps1')
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$project=Join-Path $root 'Anastasis_UnrealV2.uproject'
$engine='C:\Program Files\Epic Games\UE_5.8'
$version=Get-Content (Join-Path $engine 'Engine/Build/Build.version') -Raw | ConvertFrom-Json
if($version.PatchVersion -ne 2 -or $version.Changelist -ne 56702186){throw 'Unexpected engine version'}
$availableMb=(Get-CimInstance Win32_PerfFormattedData_PerfOS_Memory).AvailableMBytes
if($availableMb -lt 4000){throw "RAM disponible $availableMb Mo. Seuil 4000 Mo : LookDev non lance."}
$outPath=[IO.Path]::GetFullPath($OutDir)
if(Test-Path -LiteralPath $outPath){throw 'Choose a new evidence directory'}
New-Item -ItemType Directory -Path $outPath | Out-Null
$env:ANASTASIS_AAA_LAB_OUT=$outPath
$script=(Join-Path $PSScriptRoot 'aaa-visual-lab.py').Replace('\','/')
$log=Join-Path $outPath 'lab.log'
# Entry, not Lvl_AnastasisSlice: this lab must not open the game map.
$mcpPort=Get-AnastasisMcpPort $root
$argsList=@(('"'+$project+'"'),'/Engine/Maps/Entry','-windowed','-resx=1920','-resy=1080','-nosplash','-NoLiveCoding',
 "-ModelContextProtocolPort=$mcpPort",
 '-unattended','-NoSound','-RenderOffscreen',
 '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
 ('-abslog="'+$log+'"'),('-ExecCmds="py '+$script+'"'))
$previousSdkSkip=$env:UE_SKIP_UBT_SDK_SETUP
try {
 $env:UE_SKIP_UBT_SDK_SETUP='1'
 $p=Start-AnastasisEditor (Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor.exe') $argsList
} finally { $env:UE_SKIP_UBT_SDK_SETUP=$previousSdkSkip }
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){Stop-Process -Id $p.Id;throw 'Own lookdev editor timed out'}
if(-not (Select-String -LiteralPath $log -SimpleMatch 'AAA_LAB_COMPLETE')){throw 'Lookdev incomplete; inspect log'}
$images=@(Get-ChildItem -LiteralPath $outPath -Filter 'cam_*.png')
if($images.Count -ne 3){throw 'Wrong PNG capture count'}
Write-Output "AAA_LAB::CAPTURE_COMPLETE $outPath"
