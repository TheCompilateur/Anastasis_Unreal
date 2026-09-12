param([ValidateSet('status','build','verify','editor')][string]$Command='status')
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
$cwdPath=(Get-Location).Path
if($cwdPath -ne $Root -and !$cwdPath.StartsWith($Root+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'FAIL: invoke from the canonical root or its subdirectories'}
$Project=Join-Path $Root 'Anastasis_UnrealV2.uproject'
$Engine='C:\Program Files\Epic Games\UE_5.8'
$up=Get-Content $Project -Raw | ConvertFrom-Json
$v=Get-Content "$Engine/Engine/Build/Build.version" -Raw | ConvertFrom-Json
if($up.EngineAssociation -ne '5.8' -or $v.MajorVersion -ne 5 -or $v.MinorVersion -ne 8 -or $v.PatchVersion -ne 2 -or $v.Changelist -ne 56702186){throw 'FAIL: engine identity mismatch'}
foreach($module in @('AnastasisSim','Anastasis_UnrealV2')){if($module -notin $up.Modules.Name){throw "FAIL: missing module $module"}}
$Evidence=Join-Path $Root 'Saved/CanonicalVerification'
New-Item -ItemType Directory -Force $Evidence | Out-Null
function Fingerprint {
 $paths=@(Get-ChildItem "$Root/Source","$Root/Config" -Recurse -File)+@(Get-Item $Project)
 $lines=$paths | Sort-Object FullName | ForEach-Object { $_.FullName.Substring($Root.Length)+':'+(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
 $sha=[Security.Cryptography.SHA256]::Create()
 try { return ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($lines -join "`n")))).Replace('-','') } finally {$sha.Dispose()}
}
function BuildCanonical {
 $before=Fingerprint
 & "$Engine/Engine/Build/BatchFiles/Build.bat" Anastasis_UnrealV2Editor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE 2>&1 | Tee-Object "$Evidence/build.log" | Out-Host
 if($LASTEXITCODE -ne 0){throw 'BUILD::FAIL'}
 if((Fingerprint) -ne $before){throw 'FAIL: source/config changed during build'}
 $before | Set-Content "$Evidence/built-source.sha256"
 Write-Output 'BUILD::PASS'
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
 $proc=Start-Process "$Engine/Engine/Binaries/Win64/UnrealEditor.exe" -ArgumentList $launchArgs -WindowStyle Hidden -PassThru
 $deadline=(Get-Date).AddMinutes(4)
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
 $dlls=$modules | ForEach-Object {[pscustomobject]@{Path=$_.FileName;SHA256=(Get-FileHash -LiteralPath $_.FileName).Hash}}
 [ordered]@{VerifiedAt=(Get-Date).ToString('o');Result='PASS';SourceSHA256=$built;Engine='5.8.2 CL 56702186';EditorLog=$log;Modules=$dlls;Scope='Editor map PIE DEBUG; PLAYER unimplemented; not full test suite'} | ConvertTo-Json -Depth 5 | Set-Content "$Evidence/latest.json"
 Write-Output "VERIFY::PASS`nEVIDENCE::$Evidence/latest.json"
} catch {Write-Error $_; exit 1}
