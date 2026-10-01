# CREDIBLE_BANKS_001: owned actors only; no terrain/simulation/shared material writes.
param([Parameter(Mandatory=$true)][string]$Out,[ValidateSet('Preview','Save','Verify')][string]$Mode='Preview',[int]$TimeoutSec=600)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
if($root -eq 'C:\dev\ANASTASIS_UNREAL' -and $Mode -ne 'Verify'){throw 'Use an isolated worktree.'}
$outDir=[IO.Path]::GetFullPath($Out)
New-Item -ItemType Directory -Force $outDir | Out-Null
$recipe=(Join-Path $PSScriptRoot 'credible-banks.py').Replace('\','/')
$wrapper=Join-Path $outDir 'run-banks.py'
@"
import unreal, traceback
try:
    p=r'$recipe'
    with open(p,encoding='utf-8-sig') as f:exec(compile(f.read(),p,'exec'),globals())
except Exception:
    unreal.log_error('BANKS_FAIL '+traceback.format_exc());unreal.SystemLibrary.quit_editor()
"@ | Set-Content -LiteralPath $wrapper -Encoding utf8
$env:BANKS_OUT=$outDir.Replace('\','/')
$env:BANKS_MODE=$Mode.ToLower()
$env:UE_SKIP_UBT_SDK_SETUP='1'
$log=Join-Path $outDir ($Mode.ToLower()+'.log')
# Each mode has distinct filenames; do not accept images from an older execution.
Get-ChildItem -LiteralPath $outDir -Filter ($Mode.ToLower()+'_*.png') -File | Remove-Item
$argsList=@(('"'+$root+'\Anastasis_UnrealV2.uproject"'),'-windowed','-resx=1280','-resy=720','-unattended','-nosplash','-nosound','-NoLiveCoding','-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',('-abslog="'+$log+'"'),('-ExecCmds="py '+$wrapper.Replace('\','/')+'"'))
$p=Start-AnastasisEditor 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' $argsList
Write-Output ('BANKS_PID::'+$p.Id)
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){Stop-Process -Id $p.Id;throw 'Own bank editor timed out.'}
if(-not(Select-String -LiteralPath $log -Pattern 'BANKS_COMPLETE' -Quiet)){throw ('Incomplete bank run: '+$log)}
if(Select-String -LiteralPath $log -Pattern 'BANKS_FAIL|BankDeposit.*Failed to compile Material' -Quiet){throw ('Bank recipe failed: '+$log)}
Write-Output ('BANKS::'+$Mode.ToUpper()+'::PASS '+$log)
exit 0
