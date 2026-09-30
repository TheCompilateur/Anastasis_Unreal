param(
  [Parameter(Mandatory=$true)][string]$Out,
  [ValidateSet('Preview','Save','Verify')][string]$Mode='Preview',
  [switch]$TerrainTests
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
if($root -eq 'C:\dev\ANASTASIS_UNREAL' -and $Mode -ne 'Verify'){throw 'Use an isolated worktree for authored map changes.'}
$evidenceDir=[IO.Path]::GetFullPath($Out)
New-Item -ItemType Directory -Path $evidenceDir -Force | Out-Null
$scriptPath=(Join-Path $PSScriptRoot 'world-dressing-01.py').Replace('\','/')
$wrapper=Join-Path $evidenceDir 'run-world-dressing.py'
@"
import unreal, traceback
try:
    with open(r'$scriptPath', encoding='utf-8-sig') as f:
        exec(compile(f.read(), r'$scriptPath', 'exec'), globals())
except Exception:
    unreal.log_error(traceback.format_exc())
    unreal.SystemLibrary.quit_editor()
"@ | Set-Content -LiteralPath $wrapper -Encoding utf8
$env:WD01_OUT=$evidenceDir.Replace('\','/')
$env:WD01_SAVE=if($Mode -eq 'Save'){'1'}else{'0'}
$env:WD01_VERIFY=if($Mode -eq 'Verify'){'1'}else{'0'}
$env:WD01_TESTS=if($TerrainTests){'1'}else{'0'}
if($TerrainTests -and $Mode -ne 'Verify'){throw 'TerrainTests requires Verify.'}
$log=Join-Path $evidenceDir ($Mode.ToLower()+'.log')
$argsList=@(('"'+$root+'\Anastasis_UnrealV2.uproject"'),'-windowed','-resx=1600','-resy=900','-unattended','-nosplash','-nosound','-NoLiveCoding','-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',('-abslog="'+$log+'"'),('-ExecCmds="py '+$wrapper.Replace('\','/')+'"'))
if($TerrainTests){$argsList+=@('-testexit="Automation Test Queue Empty"','-LogCmds="LogAutomationTest Log"')}
$p=Start-Process 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList $argsList -WindowStyle Hidden -PassThru
Write-Output ('WORLD_DRESSING_PID::'+$p.Id)
$deadline=(Get-Date).AddMinutes(12)
while(-not $p.HasExited -and (Get-Date) -lt $deadline){Start-Sleep -Seconds 2;$p.Refresh()}
if(-not $p.HasExited){Stop-Process -Id $p.Id;throw 'Dedicated dressing editor timed out.'}
if(-not (Select-String -LiteralPath $log -Pattern 'WORLD_DRESSING_01_COMPLETE' -Quiet)){throw ('Incomplete run: '+$log)}
if($TerrainTests){
  . (Join-Path $PSScriptRoot 'automation-log.ps1')
  $known=Read-KnownExpectedFailures (Join-Path $PSScriptRoot 'known-expected-failures.txt')
  $run=Read-AutomationLog -LogPath $log -Known $known -LauncherExitCode $p.ExitCode
  Write-Output ('PASS='+$run.Pass.Count+' KNOWN_EXPECTED_FAILURE='+$run.Expected.Count+' FAIL='+$run.FailCount+' TOTAL='+$run.Total)
  if($run.FailCount -gt 0 -or $run.Incomplete.Count -gt 0 -or $run.Total -eq 0){throw 'Terrain tests failed or incomplete.'}
}
Write-Output ('WORLD_DRESSING::'+$Mode.ToUpper()+'::PASS '+$log)
