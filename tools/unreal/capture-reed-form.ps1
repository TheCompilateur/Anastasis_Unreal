param([Parameter(Mandatory=$true)][string]$OutDir,[int]$TimeoutSec=720)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$project=Join-Path $root 'Anastasis_UnrealV2.uproject'
$editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$outPath=[IO.Path]::GetFullPath($OutDir)
if(Test-Path -LiteralPath $outPath){throw 'Choose a new output directory; preserve prior evidence.'}
New-Item -ItemType Directory -Path $outPath | Out-Null
$env:ANASTASIS_REED_FORM_OUT=$outPath
$script=(Join-Path $PSScriptRoot 'capture-reed-form.py').Replace('\','/')
$log=Join-Path $outPath 'capture.log'
$argsList=@(('"'+$project+'"'),'-windowed','-resx=1600','-resy=900','-nosplash','-NoLiveCoding',
 '-unattended','-NoSound','-RenderOffscreen',
 '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
 ('-abslog="'+$log+'"'),('-ExecCmds="py '+$script+'"'))
$p=Start-AnastasisEditor $editor $argsList
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){Stop-Process -Id $p.Id; throw 'Dedicated capture timed out.'}
if(-not (Select-String -LiteralPath $log -SimpleMatch 'REED_REVIEW COMPLETE shots=8')){throw 'Capture incomplete; inspect log.'}
$hashes=@()
foreach($view in @('eye','context')){
 foreach($mode in @('bare','A','B','C')){
  $png=Join-Path $outPath ($mode+'_'+$view+'.png')
  if(!(Test-Path -LiteralPath $png) -or (Get-Item -LiteralPath $png).Length -lt 1000){throw "Missing capture: $png"}
  $hashes+=Get-FileHash -LiteralPath $png -Algorithm SHA256
 }
}
$hashes | ConvertTo-Json | Set-Content (Join-Path $outPath 'capture-hashes.json')
Write-Output "CAPTURE::COMPLETE $outPath (visual acceptance requires image review)"
