param([Parameter(Mandatory=$true)][string]$OutDir,[int]$TimeoutSec=900)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$project=Join-Path $root 'Anastasis_UnrealV2.uproject'
$engine='C:\Program Files\Epic Games\UE_5.8'
$version=Get-Content (Join-Path $engine 'Engine/Build/Build.version') -Raw | ConvertFrom-Json
if($version.PatchVersion -ne 2 -or $version.Changelist -ne 56702186){throw 'Unexpected engine version'}
$outPath=[IO.Path]::GetFullPath($OutDir)
if(Test-Path -LiteralPath $outPath){throw 'Choose a new evidence directory'}
New-Item -ItemType Directory -Path $outPath | Out-Null
$env:ANASTASIS_SHELTER_OUT=$outPath
$expected=3
$script=(Join-Path $PSScriptRoot 'capture-camp-shelter.py').Replace('\','/')
$log=Join-Path $outPath 'capture.log'
$argsList=@(('"'+$project+'"'),'-windowed','-resx=1600','-resy=900','-nosplash','-NoLiveCoding',
 '-unattended','-NoSound','-RenderOffscreen',
 '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
 ('-abslog="'+$log+'"'),('-ExecCmds="py '+$script+'"'))
# Editor already compiled; this is asset rendering, not SDK validation/packaging.
# Engine TargetPlatformManagerModule.cpp:54,223 supports this process-local switch.
$previousSdkSkip=$env:UE_SKIP_UBT_SDK_SETUP
try {
 $env:UE_SKIP_UBT_SDK_SETUP='1'
 $p=Start-AnastasisEditor (Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor.exe') $argsList
} finally { $env:UE_SKIP_UBT_SDK_SETUP=$previousSdkSkip }
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){Stop-Process -Id $p.Id;throw 'Own capture editor timed out'}
if(-not (Select-String -LiteralPath $log -SimpleMatch ("SHELTER009 COMPLETE shots=$expected assets=1"))){throw 'Capture incomplete; inspect log'}
$images=@(Get-ChildItem -LiteralPath $outPath -Filter '*.png')
if($images.Count -ne $expected){throw 'Wrong PNG capture count'}
$images | Get-FileHash -Algorithm SHA256 | ConvertTo-Json | Set-Content (Join-Path $outPath 'capture-hashes.json')
Write-Output "SHELTER009::CAPTURE_COMPLETE $outPath"
