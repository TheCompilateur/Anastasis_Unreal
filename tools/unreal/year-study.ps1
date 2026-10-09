# annee-valmire-001 -- une annee a Valmire, mesuree, sans rendu.
#
# Lance l'editeur de CE worktree sans rendu (-nullrhi, /Engine/Maps/Entry, porte memoire d'un demi-editeur), joue
# `Anastasis.Etude.Annee <Days> <Every> <Seeds>` puis quitte. Quatre scenarios du village du lancement, meme graine :
# sans joueur, joueur qui ne fait que survivre, joueur qui travaille, sans joueur dans un village ferme au monde exterieur.
# Sorties : Saved/YearStudy/<horodatage>/seed-<graine>/ (un csv par scenario, sa chronique, summary.json). Aucun asset ecrit.
#
#   tools\unreal\year-study.ps1                      # deux annees du jeu (240 jours), releve tous les 10 jours
#   tools\unreal\year-study.ps1 -Days 120 -Every 5
#   tools\unreal\year-study.ps1 -Seeds 12345,7,42,99,2026   # plusieurs mondes : une difference ne compte que si elle se repete
#
# Verdict : YEAR_STUDY COMPLETE (fichiers ecrits, chaque village pose) ; c'est un instrument, il ne juge pas.
param(
  [int]$Days = 240,
  [int]$Every = 10,
  [string]$Seeds = '12345',
  [int]$TimeoutSec = 1800
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Engine = 'C:\Program Files\Epic Games\UE_5.8'
$Evidence = Join-Path $Root 'Saved/YearStudy'
New-Item -ItemType Directory -Force $Evidence | Out-Null
$log = Join-Path $Evidence 'year-study.log'
if (Test-Path $log) { Remove-Item $log }

$launchArgs = @(
  ('"' + $Root + '\Anastasis_UnrealV2.uproject"'),
  '/Engine/Maps/Entry', '-nullrhi', '-nosound',
  '-unattended', '-nopause', '-nosplash', '-NoLiveCoding',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="Anastasis.Etude.Annee ' + $Days + ' ' + $Every + ' ' + (($Seeds.Trim()) -replace '[, ]+', '+') + ' quit"')
)
$p = Start-AnastasisEditor "$Engine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $launchArgs
$t0 = Get-Date
while (-not $p.HasExited) {
  if (((Get-Date) - $t0).TotalSeconds -ge $TimeoutSec) { Stop-Process -Id $p.Id -Force; throw 'YEAR_STUDY FAIL lanceur bloque' }
  Start-Sleep -Seconds 2
}
Write-Output ('YEAR_STUDY_RUN duree={0:N0}s log={1}' -f ((Get-Date) - $t0).TotalSeconds, $log)
$lines = @(Select-String -Path $log -Pattern 'YEAR_STUDY ' | ForEach-Object { $_.Line -replace '^.*?(YEAR_STUDY .*)$', '$1' })
$lines | ForEach-Object { Write-Output $_ }
if (-not ($lines | Where-Object { $_ -like 'YEAR_STUDY COMPLETE*' })) { Write-Output 'YEAR_STUDY FAIL (pas de COMPLETE au log)'; exit 1 }
exit 0
