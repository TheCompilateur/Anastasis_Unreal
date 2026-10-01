# Micro-implantation humaine, isolee de Lvl_AnastasisSlice.
# Ouvre l'editeur deja incarne, pose le prototype, enregistre un NOUVEAU niveau,
# capture, quitte. Ne lance pas Unreal hors de Start-AnastasisEditor.
param(
    [Parameter(Mandatory = $true)][string]$Out
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
if ($root -eq 'C:\dev\ANASTASIS_UNREAL') {
    throw 'Prototype belongs in an isolated worktree, never the integration root.'
}
$evidenceDir = [IO.Path]::GetFullPath($Out)
New-Item -ItemType Directory -Path $evidenceDir -Force | Out-Null
$scriptPath = (Join-Path $PSScriptRoot 'human-occupation-001.py').Replace('\', '/')
$wrapper = Join-Path $evidenceDir 'run-human-occupation.py'
@"
import unreal, traceback
try:
    with open(r'$scriptPath', encoding='utf-8-sig') as f:
        exec(compile(f.read(), r'$scriptPath', 'exec'), globals())
except Exception:
    unreal.log_error(traceback.format_exc())
    unreal.SystemLibrary.quit_editor()
"@ | Set-Content -LiteralPath $wrapper -Encoding utf8
$env:HO01_OUT = $evidenceDir.Replace('\', '/')
$log = Join-Path $evidenceDir 'occupation.log'
$argsList = @(
    ('"' + $root + '\Anastasis_UnrealV2.uproject"'),
    '-windowed', '-resx=1600', '-resy=900',
    '-unattended', '-nosplash', '-nosound', '-NoLiveCoding',
    '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
    ('-abslog="' + $log + '"'),
    ('-ExecCmds="py ' + $wrapper.Replace('\', '/') + '"')
)
$p = Start-AnastasisEditor 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' $argsList
Write-Output ('HUMAN_OCCUPATION_PID::' + $p.Id)
$deadline = (Get-Date).AddMinutes(28)
while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 3
    $p.Refresh()
}
if (-not $p.HasExited) {
    Stop-Process -Id $p.Id -Force
    throw 'Dedicated occupation editor timed out.'
}
if (-not (Select-String -LiteralPath $log -Pattern 'HUMAN_OCCUPATION_001_COMPLETE' -Quiet)) {
    throw ('Incomplete run: ' + $log)
}
Write-Output ('HUMAN_OCCUPATION::PASS ' + $log)
