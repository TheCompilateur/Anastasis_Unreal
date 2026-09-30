param([Parameter(Mandatory=$true)][string]$OutDir,[int]$TimeoutSec=900,[switch]$FinishOnly)
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
$env:ANASTASIS_PROPS_OUT=$outPath
$env:ANASTASIS_PROPS_FINISH_ONLY=if($FinishOnly){'1'}else{'0'}
$expected=if($FinishOnly){3}else{9}
$script=(Join-Path $PSScriptRoot 'capture-refugee-props.py').Replace('\','/')
$log=Join-Path $outPath 'capture.log'
$argsList=@(('"'+$project+'"'),'-windowed','-resx=1600','-resy=1000','-nosplash','-NoLiveCoding',
 '-unattended','-NoSound','-RenderOffscreen',
 '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
 ('-abslog="'+$log+'"'),('-ExecCmds="py '+$script+'"'))
$p=Start-AnastasisEditor (Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor.exe') $argsList
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){Stop-Process -Id $p.Id;throw 'Own capture editor timed out'}
if(-not (Select-String -LiteralPath $log -SimpleMatch ("PROPS008 COMPLETE shots=$expected assets=4"))){throw 'Capture incomplete; inspect log'}
$images=@(Get-ChildItem -LiteralPath $outPath -Filter '*.png')
if($images.Count -ne $expected){throw 'Wrong PNG capture count'}
$images | Get-FileHash -Algorithm SHA256 | ConvertTo-Json | Set-Content (Join-Path $outPath 'capture-hashes.json')
Write-Output "PROPS008::CAPTURE_COMPLETE $outPath"
