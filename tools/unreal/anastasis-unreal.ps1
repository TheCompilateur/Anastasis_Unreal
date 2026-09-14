param([ValidateSet('status','build','verify','editor','health','build-game')][string]$Command='status',[switch]$Force)
$ErrorActionPreference='Stop'
$Canonical='C:\dev\ANASTASIS_UNREAL'
$WorktreeRoot='C:\dev\ANASTASIS_WORKTREES'
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
# La racine canonique n'est plus le seul lieu legitime : un agent build et teste
# dans son propre worktree sous ANASTASIS_WORKTREES. Tout autre emplacement (une
# copie OneDrive, un dossier ad hoc) reste refuse.
$isCanonical = ($Root -eq $Canonical)
$isWorktree  = $Root.StartsWith($WorktreeRoot + '\', [StringComparison]::OrdinalIgnoreCase)
if(-not ($isCanonical -or $isWorktree)){throw "FAIL: operator must run from $Canonical or a worktree under $WorktreeRoot (got $Root)"}
# Task Scheduler (Anastasis-ScheduledVerify) starts in System32. Root is derived
# from this script's path, not cwd; cd so verify/build work unattended. 2026-09-13
# 03:00 VERIFY_FAIL with TESTS_PASS and no editor-03*.log was this cwd check.
Set-Location -LiteralPath $Root
$Project=Join-Path $Root 'Anastasis_UnrealV2.uproject'
$Engine='C:\Program Files\Epic Games\UE_5.8'
$up=Get-Content $Project -Raw | ConvertFrom-Json
$v=Get-Content "$Engine/Engine/Build/Build.version" -Raw | ConvertFrom-Json
if($up.EngineAssociation -ne '5.8' -or $v.MajorVersion -ne 5 -or $v.MinorVersion -ne 8 -or $v.PatchVersion -ne 2 -or $v.Changelist -ne 56702186){throw 'FAIL: engine identity mismatch'}
foreach($module in @('AnastasisSim','Anastasis_UnrealV2')){if($module -notin $up.Modules.Name){throw "FAIL: missing module $module"}}
$Evidence=Join-Path $Root 'Saved/CanonicalVerification'
New-Item -ItemType Directory -Force $Evidence | Out-Null
$ModuleDlls=@('UnrealEditor-AnastasisSim.dll','UnrealEditor-Anastasis_UnrealV2.dll') | ForEach-Object { Join-Path $Root "Binaries\Win64\$_" }
function FileSha256([string]$Path) {
 $stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
 $sha=[Security.Cryptography.SHA256]::Create()
 try { return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','') }
 finally { $stream.Dispose(); $sha.Dispose() }
}
function Fingerprint {
 $paths=@(Get-ChildItem "$Root/Source","$Root/Config" -Recurse -File)+@(Get-Item $Project)
 $lines=$paths | Sort-Object FullName | ForEach-Object { $_.FullName.Substring($Root.Length)+':'+(FileSha256 $_.FullName) }
 $sha=[Security.Cryptography.SHA256]::Create()
 try { return ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($lines -join "`n")))).Replace('-','') } finally {$sha.Dispose()}
}
# Empreinte des binaires produits. $null si un module manque : on ne peut alors
# rien affirmer et le build reel doit tourner.
function ModuleStamp {
 if(@($ModuleDlls | Where-Object {!(Test-Path -LiteralPath $_)}).Count){return $null}
 return (($ModuleDlls | Sort-Object | ForEach-Object { [IO.Path]::GetFileName($_)+':'+(FileSha256 $_) }) -join "`n")
}
# Deux agents peuvent compiler la meme racine en meme temps. UBT se serialise
# lui-meme (-WaitMutex) ; Tee-Object non. Le second heurtait alors 'le fichier est
# en cours d'utilisation par un autre processus' sur build.log et le gate rendait
# BUILD::FAIL sans qu'aucune compilation ait echoue — un faux negatif qui refuse
# un push parfaitement valide. Observe le 2026-09-13. Chaque run ecrit donc son
# propre journal ; le nom canonique n'est qu'une copie, en meilleur effort, du
# dernier. Effet de bord utile : les journaux de build deviennent un historique.
function RunLog([string]$Name){
 $dir=Join-Path $Evidence 'build-logs'
 New-Item -ItemType Directory -Force $dir | Out-Null
 Get-ChildItem $dir -Filter "$Name-*.log" -File -ErrorAction SilentlyContinue |
  Sort-Object LastWriteTime -Descending | Select-Object -Skip 20 |
  ForEach-Object { try{Remove-Item -LiteralPath $_.FullName -Force -ErrorAction Stop}catch{} }
 return (Join-Path $dir ('{0}-{1:yyyyMMdd-HHmmss}-{2}.log' -f $Name,(Get-Date),$PID))
}
function PublishLog([string]$RunPath,[string]$CanonicalName){
 try { Copy-Item -LiteralPath $RunPath -Destination (Join-Path $Evidence $CanonicalName) -Force -ErrorAction Stop }
 catch { Write-Output "NOTE: $CanonicalName occupe par un autre agent, non mis a jour. Journal de ce run : $RunPath" }
}
function BuildCanonical {
 $before=Fingerprint
 $srcStamp=Join-Path $Evidence 'built-source.sha256'
 $modStamp=Join-Path $Evidence 'built-modules.sha256'
 # Court-circuit : si ni les sources ni les binaires n'ont bouge depuis le dernier
 # build reussi, relinker produirait les memes DLL. Evite que le gate pre-push echoue
 # sur LNK1104 quand un editeur tient la DLL en verrou d'ecriture. -Force force le build.
 if(-not $Force -and (Test-Path $srcStamp) -and (Test-Path $modStamp)){
  $nowMod=ModuleStamp
  if($null -ne $nowMod -and (Get-Content $srcStamp -Raw).Trim() -eq $before -and (Get-Content $modStamp -Raw).Trim() -eq $nowMod.Trim()){
   Write-Output 'BUILD::PASS::CACHED (source and module binaries unchanged since last successful build)'
   return
  }
 }
 $runLog=RunLog 'build'
 & "$Engine/Engine/Build/BatchFiles/Build.bat" Anastasis_UnrealV2Editor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE 2>&1 | Tee-Object $runLog | Out-Host
 $code=$LASTEXITCODE
 PublishLog $runLog 'build.log'
 if($code -ne 0){throw 'BUILD::FAIL'}
 if((Fingerprint) -ne $before){throw 'FAIL: source/config changed during build'}
 $afterMod=ModuleStamp
 if($null -eq $afterMod){throw 'BUILD::FAIL missing module binaries after a successful build'}
 $before | Set-Content $srcStamp
 $afterMod | Set-Content $modStamp
 Write-Output 'BUILD::PASS'
}
function BuildGame {
 $before=Fingerprint
 $runLog=RunLog 'build-game'
 & "$Engine/Engine/Build/BatchFiles/Build.bat" Anastasis_UnrealV2 Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE 2>&1 | Tee-Object $runLog | Out-Host
 $code=$LASTEXITCODE
 PublishLog $runLog 'build-game.log'
 if($code -ne 0){throw 'GAME_BUILD::FAIL'}
 if((Fingerprint) -ne $before){throw 'FAIL: source/config changed during game build'}
 $before | Set-Content "$Evidence/built-game-source.sha256"
 Write-Output 'GAME_BUILD::PASS'
}
try {
 if($Command -eq 'status') {
  if($isCanonical){$roleLabel='CANONICAL_ROOT'}else{$roleLabel='AGENT_WORKTREE'}
  Write-Output "$roleLabel::$Root`nUPROJECT::$Project`nENGINE::5.8.2 CL 56702186`nSOURCE_SHA256::$(Fingerprint)"
  & git -C $Root branch --show-current
  & git -C $Root rev-parse --verify HEAD
  & git -C $Root status --short
  exit 0
 }
 if($Command -eq 'build'){BuildCanonical; exit 0}
 if($Command -eq 'build-game'){BuildGame; exit 0}
 if($Command -eq 'health'){
  & (Join-Path $PSScriptRoot 'project-health.ps1')
  exit $LASTEXITCODE
 }
 if($Command -eq 'editor'){
  Start-Process "$Engine/Engine/Binaries/Win64/UnrealEditor.exe" -ArgumentList ('"'+$Project+'"') -WindowStyle Hidden | Out-Null
  Write-Output 'EDITOR::START_REQUESTED (not a verification)'; exit 0
 }
 # UBT performs the incremental dependency check even when the fingerprint matches.
 BuildCanonical
 $stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
 $log=Join-Path $Evidence "editor-$stamp.log"
 $py=(Join-Path $PSScriptRoot 'smoke-pie.py').Replace('\','/')
 $launchArgs=@(('"'+$Project+'"'),'-unattended','-nosplash','-NoLiveCoding',('-abslog="'+$log+'"'),'-LogCmds="LogAutomationTest Log"',('-ExecCmds="py '+$py+'"'))
 $busy=@(Get-Process UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue)
 if($busy.Count){
  Write-Output ('VERIFY::NOTE concurrent editors pids=' + (($busy | ForEach-Object Id) -join ','))
 }
 $proc=Start-Process "$Engine/Engine/Binaries/Win64/UnrealEditor.exe" -ArgumentList $launchArgs -WindowStyle Hidden -PassThru
 # 4 minutes was enough on a warm canonical DDC (01:28 PASS ~30s session). Cold
 # worktree DDC plus concurrent unattended editors on this machine timed out
 # still loading modules at 4:00 (2026-09-13 worktree verify). 12 minutes
 # covers first-boot shader/DDC without treating load as a project failure.
 $deadline=(Get-Date).AddMinutes(12)
 $modules=@()
 while(!$proc.HasExited -and (Get-Date) -lt $deadline){
  try {$modules=@($proc.Modules | Where-Object ModuleName -in @('UnrealEditor-AnastasisSim.dll','UnrealEditor-Anastasis_UnrealV2.dll') | Select-Object ModuleName,FileName)} catch {}
  Start-Sleep -Seconds 2
  $proc.Refresh()
 }
 if(!$proc.HasExited){Stop-Process -Id $proc.Id; throw 'VERIFY::FAIL timeout; only dedicated process stopped'}
 if($proc.ExitCode -ne 0){throw "VERIFY::FAIL editor exit $($proc.ExitCode)"}
 $body=Get-Content $log -Raw
 foreach($marker in @('CANONICAL_EDITOR_BOOT','CANONICAL_MAP_LOAD=True','CANONICAL_PIE_ACTIVE','CANONICAL_COMPLETE','ANASTASIS_VISUAL_MODE DEBUG','source=96x96 crop=(0,0) 96x96 tiles=9216 instances=9216')){
  if(!$body.Contains($marker)){throw "VERIFY::FAIL missing $marker"}
 }
 if($modules.Count -ne 2 -or @($modules | Where-Object {!$_.FileName.StartsWith($Root+'\',[StringComparison]::OrdinalIgnoreCase)}).Count){throw 'VERIFY::FAIL module origin'}
 $built=(Get-Content "$Evidence/built-source.sha256").Trim()
 if((Fingerprint) -ne $built){throw 'VERIFY::FAIL source changed after build'}
 $dlls=$modules | ForEach-Object {[pscustomobject]@{Path=$_.FileName;SHA256=(FileSha256 $_.FileName)}}
 [ordered]@{VerifiedAt=(Get-Date).ToString('o');Result='PASS';SourceSHA256=$built;Engine='5.8.2 CL 56702186';EditorLog=$log;Modules=$dlls;Scope='Editor map PIE DEBUG; PLAYER unimplemented; not full test suite'} | ConvertTo-Json -Depth 5 | Set-Content "$Evidence/latest.json"
 Write-Output "VERIFY::PASS`nEVIDENCE::$Evidence/latest.json"
} catch {Write-Error $_; exit 1}
