param([switch]$Rebuild, [switch]$Tests, [int]$TimeoutSec = 900)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
# AAA_CONTACT_REALISM_001 -- forge M_ACR_Decal et les onze MI_ACR_* de /Game/Anastasis/AAAContactRealism.
# Voir aaa-contact-realism.py. Meme mecanique que rain-material.ps1 : -nullrhi, carte neutre,
# l'editeur ecrit les assets puis se ferme. Les instances sont reecrites a chaque lancement ;
# -Rebuild regenere aussi le maitre (ecrase toute retouche manuelle). -Tests enchaine, dans le MEME
# editeur, les tests Anastasis.ContactRealism (une file d'editeur de moins) : PASS / FAIL par test.
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$dir = Join-Path $Root 'Saved\SliceEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log = Join-Path $dir 'aaa-contact-realism.log'
if (Test-Path $log) { Remove-Item $log }
$env:ANASTASIS_ACR_REBUILD = if ($Rebuild) { '1' } else { '0' }
$py = (Join-Path $Root 'tools\unreal\aaa-contact-realism.py').Replace('\', '/')
$env:ANASTASIS_ACR_QUIT = if ($Tests) { '0' } else { '1' }
$exec = if ($Tests) { 'py ' + $py + ',Automation RunTests Anastasis.ContactRealism;Quit' } else { 'py ' + $py }
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '/Engine/Maps/Entry',
  '-nullrhi', '-unattended', '-nosound', '-nosplash', '-NoLiveCoding',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="' + $exec + '"')
)
if ($Tests) { $launchArgs += '-testexit="Automation Test Queue Empty"' }
$p = Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'ACR_MATERIAL::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'ACR_MATERIAL|ACR_INSTANCE' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }
$lines = @(Get-Content $log)
if (@($lines | Where-Object { $_ -match 'LogPython: Error|ACR_MATERIAL_FAIL|ACR_MATERIAL_WIRING_INCOMPLETE|Traceback' }).Count) { throw 'ACR_MATERIAL::FAIL erreur Python (voir le log)' }
$at = -1; for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i] -match 'ACR_MATERIAL_COMPILE') { $at = $i } }
if ($at -ge 0 -and @($lines[$at..($lines.Count - 1)] | Where-Object { $_ -match 'Failed to compile|\[SM[56]\].*error' }).Count) { throw 'ACR_MATERIAL::FAIL compilation du materiau' }
$assets = @('M_ACR_Decal') + @('WetBand', 'Mud', 'StoneWet', 'ReedBed', 'Litter', 'ContactDark', 'RockDirt', 'Deposit', 'Depression', 'Streak', 'Halo' | ForEach-Object { 'MI_ACR_' + $_ })
foreach ($a in $assets) {
  if (!(Test-Path (Join-Path $Root ('Content\Anastasis\AAAContactRealism\' + $a + '.uasset')))) { throw ('ACR_MATERIAL::FAIL asset absent ' + $a) }
}
Write-Output 'ACR_MATERIAL::PASS'
if ($Tests) {
  $res = @($lines | Where-Object { $_ -match 'Test Completed\. Result=\{.*Name=\{|Anastasis\.ContactRealism' } | ForEach-Object { ($_ -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') })
  $res | Select-Object -Unique | ForEach-Object { Write-Output $_ }
  $done = @($lines | Where-Object { $_ -match 'Test Completed\. Result=\{Success\}.*ContactRealism|Test Completed\. Result=\{Success\}.*(ShoreStates|ShorePlan|Anchors)' }).Count
  $bad = @($lines | Where-Object { $_ -match 'Test Completed\. Result=\{Fail' -and $_ -match 'ContactRealism|ShoreStates|ShorePlan|Anchors' }).Count
  if ($bad -gt 0 -or $done -lt 3) { throw ('ACR_TESTS::FAIL succes=' + $done + ' echecs=' + $bad) }
  Write-Output ('ACR_TESTS::PASS ' + $done + '/3')
}

