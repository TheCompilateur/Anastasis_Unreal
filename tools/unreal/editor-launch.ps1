# Lancement discret d'un editeur Unreal par un agent. A dot-sourcer.
#
# Le 2026-09-29, les editeurs des agents s'ouvraient au premier plan pendant
# qu'Alexandre travaillait ; il les fermait, et tuait sans le savoir la preuve en
# cours d'un agent (six fermetures, dont deux report-tests). Deux mecanismes ici :
#
#   1. Le processus est cree avec STARTF_USESHOWWINDOW + SW_SHOWNOACTIVATE : Windows
#      applique cet etat a la premiere fenetre affichee, qui ne prend donc pas le focus.
#      Start-Process ne sait pas passer cette valeur, d'ou CreateProcess.
#      Exception : UnrealEditor-Cmd.exe (sous-systeme console) garde son lancement
#      d'origine, console cachee (Start-Process -WindowStyle Hidden). Essaye le
#      2026-09-30 avec CREATE_NO_WINDOW : le Build.bat ValidatePlatforms que l'editeur
#      lance au demarrage s'est bloque dans cette console sans fenetre, et report-tests
#      a expire a 15 min sans un seul test. Le gardien s'applique quand meme.
#   2. editor-window-guard.ps1, processus cache, envoie chaque fenetre du processus
#      hors de l'ecran, au fond de la pile, sans activation -- y compris celles
#      ouvertes plus tard (PIE). Pas de minimisation : Slate ne dessine plus une
#      fenetre minimisee, et les captures en dependent.
#
# Le processus est cree suspendu, ouvert en System.Diagnostics.Process, puis relance :
# l'objet rendu garde son handle, donc Wait-Process, HasExited, ExitCode et Modules
# fonctionnent comme avec Start-Process -PassThru.
#
# ANASTASIS_EDITOR_VISIBLE=1 : comportement d'avant (fenetre normale, pas de gardien),
# pour regarder un editeur travailler.
#
# Porte memoire (Invoke-AnastasisEditorGated). Le 2026-09-30, douze alertes Windows
# "memoire virtuelle insuffisante" en une soiree : chaque fois deux ou trois editeurs
# d'agents a 8-13 Go chacun, sur une machine de 16 Go, plus une compilation. Tout gelait,
# puis un editeur mourait faute de memoire, et l'agent cherchait une regression. Un
# lancement attend donc que la machine puisse le porter : moins de ANASTASIS_EDITOR_MAX
# editeurs Unreal ouverts (2, l'editeur d'Alexandre compte), au moins
# ANASTASIS_EDITOR_MIN_RAM_GB de RAM disponible (3) et ANASTASIS_EDITOR_MIN_COMMIT_GB de
# marge avant la limite de memoire engagee (8). Au-dela de ANASTASIS_EDITOR_WAIT_MIN
# minutes (45), EDITOR_GATE::TIMEOUT. Le compte d'editeurs couvre la montee en memoire d'un
# editeur qui vient de demarrer, que la RAM ne montre pas encore. Le verrou nomme rend
# "verifier puis lancer" atomique entre agents. ANASTASIS_EDITOR_GATE=0 : pas de porte.
# Le premier editeur passe toujours : le 2026-10-01, apres un redemarrage, sessions Claude
# et Cursor laissaient 0,6 Go de RAM disponible sans aucun Unreal ouvert. Des seuils de
# memoire appliques au premier editeur n'auraient laisse travailler personne.
#
# Editeur sans rendu (HEADLESS_GATE_001). Un lancement avec -nullrhi (la suite d'automation,
# report-tests.ps1) n'est pas un editeur complet : mesure du 2026-10-08, meme suite de 369 tests,
# 6,4 Go de memoire privee et 313 s sans rendu, contre 11,9 Go et 637 s avec. Il compte donc pour
# ANASTASIS_EDITOR_HEADLESS_WEIGHT (0,5) editeur, ouvert comme a lancer, et a ses propres seuils :
# ANASTASIS_EDITOR_HEADLESS_MIN_RAM_GB (2,5, son pic de RAM mesure) et
# ANASTASIS_EDITOR_HEADLESS_MIN_COMMIT_GB (5). Il peut passer devant un ticket d'agent (priorite 1)
# qui attend depuis moins de ANASTASIS_EDITOR_HEADLESS_OVERTAKE_MIN minutes (15) : cinq minutes de
# suite ne doivent pas attendre derriere un editeur complet que la machine ne peut pas encore porter
# (2026-10-08 : 12 min a la file pour 5 min de suite). Jamais devant un lot d'integration
# (priorite 0), jamais devant une attente plus longue : le retard qu'il impose est borne. Un editeur
# dont la ligne de commande est illisible (lance par un autre compte, eleve) compte pour un editeur entier.

if (-not ('AnastasisLaunch' -as [type])) {
  Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class AnastasisLaunch {
  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
  struct STARTUPINFO {
    public int cb; public string lpReserved; public string lpDesktop; public string lpTitle;
    public int dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
    public short wShowWindow, cbReserved2; public IntPtr lpReserved2, hStdInput, hStdOutput, hStdError;
  }
  [StructLayout(LayoutKind.Sequential)]
  struct PROCESS_INFORMATION { public IntPtr hProcess, hThread; public int dwProcessId, dwThreadId; }
  [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
  static extern bool CreateProcess(string app, System.Text.StringBuilder cmd, IntPtr pa, IntPtr ta, bool inherit,
    uint flags, IntPtr env, string dir, ref STARTUPINFO si, out PROCESS_INFORMATION pi);
  [DllImport("kernel32.dll")] static extern uint ResumeThread(IntPtr h);
  [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
  const uint CREATE_SUSPENDED = 0x4, CREATE_UNICODE_ENVIRONMENT = 0x400;
  const int STARTF_USESHOWWINDOW = 0x1;
  public static System.Diagnostics.Process Start(string exe, string commandLine, string dir, short show) {
    var si = new STARTUPINFO(); si.cb = Marshal.SizeOf(typeof(STARTUPINFO));
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = show;
    PROCESS_INFORMATION pi;
    if (!CreateProcess(exe, new System.Text.StringBuilder(commandLine), IntPtr.Zero, IntPtr.Zero, false,
        CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT, IntPtr.Zero, dir, ref si, out pi))
      throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
    System.Diagnostics.Process p;
    try {
      p = System.Diagnostics.Process.GetProcessById(pi.dwProcessId);
      var keep = p.Handle; // ouvre et garde le handle : ExitCode reste lisible apres la sortie
    } finally {
      ResumeThread(pi.hThread);
      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
    }
    return p;
  }
}
'@
}

if (-not ('AnastasisMemory' -as [type])) {
  Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class AnastasisMemory {
  [StructLayout(LayoutKind.Sequential)]
  struct MEMORYSTATUSEX {
    public uint dwLength, dwMemoryLoad;
    public ulong ullTotalPhys, ullAvailPhys, ullTotalPageFile, ullAvailPageFile,
      ullTotalVirtual, ullAvailVirtual, ullAvailExtendedVirtual;
  }
  [DllImport("kernel32.dll", SetLastError = true)] static extern bool GlobalMemoryStatusEx(ref MEMORYSTATUSEX m);
  // { RAM disponible, marge avant la limite d'engagement }, en octets.
  public static ulong[] Read() {
    var m = new MEMORYSTATUSEX(); m.dwLength = (uint)Marshal.SizeOf(typeof(MEMORYSTATUSEX));
    if (!GlobalMemoryStatusEx(ref m)) throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
    return new ulong[] { m.ullAvailPhys, m.ullAvailPageFile };
  }
}
'@
}

function Get-AnastasisGateSetting([string]$Name, [double]$Default) {
  $v = [Environment]::GetEnvironmentVariable($Name)
  if ($v) { return [double]$v } # cast PowerShell : culture invariante, "2.5" partout
  return $Default
}

# Un lancement d'Unreal sans rendu : -nullrhi parmi ses arguments (ou dans sa ligne de commande).
function Test-AnastasisHeadlessArgs([string]$CommandLine) {
  return ($CommandLine -match '(?i)(^|\s)-nullrhi(\s|$)')
}

# Editeurs ouverts et memoire. Weight : chaque editeur compte 1, un sans rendu HeadlessWeight.
function Get-AnastasisEditorLoad([double]$HeadlessWeight = (Get-AnastasisGateSetting 'ANASTASIS_EDITOR_HEADLESS_WEIGHT' 0.5)) {
  $mem = [AnastasisMemory]::Read()
  $eds = @()
  try {
    $eds = @(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' or Name='UnrealEditor-Cmd.exe'" -ErrorAction Stop |
      ForEach-Object { [pscustomobject]@{ Id = $_.ProcessId; Headless = [bool]($_.CommandLine -and (Test-AnastasisHeadlessArgs $_.CommandLine)) } })
  } catch {
    # Sans WMI, chaque editeur compte entier : la porte reste au moins aussi stricte qu'avant.
    $eds = @(Get-Process UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue | ForEach-Object { [pscustomobject]@{ Id = $_.Id; Headless = $false } })
  }
  $light = @($eds | Where-Object Headless).Count
  [pscustomobject]@{
    Editors  = $eds.Count
    Weight   = ($eds.Count - $light) + $light * $HeadlessWeight
    Pids     = (($eds | ForEach-Object { if ($_.Headless) { "$($_.Id)h" } else { "$($_.Id)" } }) -join ',')
    RamGB    = [math]::Round($mem[0] / 1GB, 1)
    CommitGB = [math]::Round($mem[1] / 1GB, 1)
  }
}

# Lance $Launch quand la machine peut porter un editeur de plus (voir l'en-tete), et rend
# ce qu'il rend. Les messages passent par Write-Host pour ne pas se meler au processus rendu.
function Invoke-AnastasisEditorGated {
  param(
    [Parameter(Mandatory = $true)][scriptblock]$Launch,
    # Le lancement est sans rendu (-nullrhi) : poids, seuils et file d'un editeur sans rendu.
    [switch]$Headless,
    [int]$MaxEditors = (Get-AnastasisGateSetting 'ANASTASIS_EDITOR_MAX' 2),
    [double]$MinRamGB = (Get-AnastasisGateSetting 'ANASTASIS_EDITOR_MIN_RAM_GB' 3),
    [double]$MinCommitGB = (Get-AnastasisGateSetting 'ANASTASIS_EDITOR_MIN_COMMIT_GB' 8),
    [double]$HeadlessWeight = (Get-AnastasisGateSetting 'ANASTASIS_EDITOR_HEADLESS_WEIGHT' 0.5),
    [double]$HeadlessMinRamGB = (Get-AnastasisGateSetting 'ANASTASIS_EDITOR_HEADLESS_MIN_RAM_GB' 2.5),
    [double]$HeadlessMinCommitGB = (Get-AnastasisGateSetting 'ANASTASIS_EDITOR_HEADLESS_MIN_COMMIT_GB' 5),
    [double]$HeadlessOvertakeMinutes = (Get-AnastasisGateSetting 'ANASTASIS_EDITOR_HEADLESS_OVERTAKE_MIN' 15),
    [double]$TimeoutMinutes = (Get-AnastasisGateSetting 'ANASTASIS_EDITOR_WAIT_MIN' 45),
    [int]$PollSeconds = 15,
    # Lecture de la charge ; le banc d'essai la remplace par une charge fixe.
    [scriptblock]$ReadLoad = { Get-AnastasisEditorLoad $HeadlessWeight }
  )
  if ($env:ANASTASIS_EDITOR_GATE -eq '0') { return & $Launch }
  $myWeight = if ($Headless) { $HeadlessWeight } else { 1 }
  if ($Headless) { $MinRamGB = $HeadlessMinRamGB; $MinCommitGB = $HeadlessMinCommitGB }
  $gate = New-Object System.Threading.Mutex($false, 'Global\AnastasisEditorGate')
  $gateStart = Get-Date
  $gateNote = [datetime]::MinValue
  $overtakeNoted = $false
  # File equitable (EDITOR_QUEUE_001). Sans elle, la place liberee allait a qui interrogeait au bon
  # moment : le 2026-10-01 un lot d'integration a attendu 30 min pendant que des suites arrivees apres
  # lui passaient. Un ticket par attente, `<priorite>-<horodatage>-<pid>` ; seul le plus ancien
  # ticket VIVANT passe. Priorite 0 (ANASTASIS_EDITOR_PRIORITY=0) : un lot d'integration, qui sert
  # tous les agents ; 1 sinon. Un lanceur d'une version d'avant (sans ticket) n'attend personne.
  $queueDir = if ($env:ANASTASIS_EDITOR_QUEUE_DIR) { $env:ANASTASIS_EDITOR_QUEUE_DIR } else { 'C:\dev\ANASTASIS_WORKTREES\.editor-gate' }
  $prio = if ($env:ANASTASIS_EDITOR_PRIORITY -eq '0') { '0' } else { '1' }
  $ticket = $null
  try {
    New-Item -ItemType Directory -Force $queueDir | Out-Null
    $ticket = Join-Path $queueDir ('{0}-{1:D19}-{2}.ticket' -f $prio, (Get-Date).ToUniversalTime().Ticks, $PID)
    New-Item -ItemType File -Force $ticket | Out-Null
  } catch { $ticket = $null }
  try {
    while ($true) {
      try { [void]$gate.WaitOne() } catch {
        # Un lanceur tue en tenant le verrou le laisse abandonne : on en herite, c'est voulu.
        $e = $_.Exception
        if (-not ($e -is [System.Threading.AbandonedMutexException] -or
                  $e.InnerException -is [System.Threading.AbandonedMutexException])) { throw }
      }
      $load = & $ReadLoad
      $blocked = @()
      if ($ticket) {
        $ahead = 0
        $passed = 0
        foreach ($t in @(Get-ChildItem $queueDir -Filter '*.ticket' -ErrorAction SilentlyContinue | Sort-Object Name)) {
          if ($t.FullName -eq $ticket) { break }
          $parts = $t.BaseName.Split('-')
          $owner = [int]$parts[-1]
          # Ticket d'un lanceur disparu (tue, machine redemarree) : retire, il ne bloque personne.
          if (-not (Get-Process -Id $owner -ErrorAction SilentlyContinue)) { Remove-Item $t.FullName -Force -ErrorAction SilentlyContinue; continue }
          # Sans rendu : passe devant un agent (priorite 1) qui attend depuis peu, voir l'en-tete.
          if ($Headless -and $parts[0] -eq '1') {
            $waitedMin = ((Get-Date).ToUniversalTime().Ticks - [long]$parts[1]) / [TimeSpan]::TicksPerMinute
            if ($waitedMin -lt $HeadlessOvertakeMinutes) { $passed++; continue }
          }
          $ahead++
        }
        if ($ahead -gt 0) { $blocked += "file=$ahead devant" }
        if ($passed -gt 0 -and -not $overtakeNoted) {
          Write-Host "EDITOR_GATE::SANS_RENDU passe devant $passed attente(s) d'agent de moins de $HeadlessOvertakeMinutes min"
          $overtakeNoted = $true
        }
      }
      if ($load.Weight + $myWeight -gt $MaxEditors) {
        $blocked += "editeurs=$($load.Weight)+$myWeight/$MaxEditors (pids $($load.Pids), h = sans rendu)"
      }
      if ($load.Editors -gt 0) {
        if ($load.RamGB -lt $MinRamGB) { $blocked += "ram_dispo=$($load.RamGB)/$MinRamGB Go" }
        if ($load.CommitGB -lt $MinCommitGB) { $blocked += "marge_engagee=$($load.CommitGB)/$MinCommitGB Go" }
      }
      if (-not $blocked.Count) {
        try { $launched = & $Launch } finally { $gate.ReleaseMutex() }
        $waitedMin = ((Get-Date) - $gateStart).TotalMinutes
        if ($waitedMin -ge 0.25) { Write-Host ('EDITOR_GATE::OPEN apres {0:N1} min' -f $waitedMin) }
        return $launched
      }
      $gate.ReleaseMutex()
      $waitedMin = ((Get-Date) - $gateStart).TotalMinutes
      if ($waitedMin -ge $TimeoutMinutes) {
        throw ("EDITOR_GATE::TIMEOUT apres {0:N0} min : {1}. Machine saturee, pas une regression : " +
          "relancer quand un editeur se ferme ; ANASTASIS_EDITOR_GATE=0 force le lancement.") -f $waitedMin, ($blocked -join ' ')
      }
      if (((Get-Date) - $gateNote).TotalSeconds -ge 60) {
        Write-Host ('EDITOR_GATE::WAIT {0} -- attente {1:N0}/{2:N0} min (editor-launch.ps1)' -f ($blocked -join ' '), $waitedMin, $TimeoutMinutes)
        $gateNote = Get-Date
      }
      Start-Sleep -Seconds $PollSeconds
    }
  } finally {
    if ($ticket) { Remove-Item -LiteralPath $ticket -Force -ErrorAction SilentlyContinue }
    $gate.Dispose()
  }
}

# player-start-001 : appuyer sur Play fait arriver l'habitant-joueur au village (anastasis.Player.AutoArrive 1).
# Un editeur pilote par script (preuve, capture, generateur : `py` dans -ExecCmds, -ExecutePythonScript) demarre
# en observateur, comme avant : une preuve qui accelere le temps avec un joueur incarne change sa reputation et
# le village (AGENTS.md, Temps accelere). Une preuve qui veut le joueur pose `anastasis.Player.AutoArrive 1` en
# console (priorite console > ligne de commande) ; une ligne de commande qui nomme deja la CVar est laissee telle quelle.
function Add-AnastasisScriptedCVars([string[]]$ArgumentList) {
  $joined = $ArgumentList -join ' '
  if ($joined -notmatch '(?i)ExecCmds="?py\s|ExecutePythonScript' -or $joined -match '(?i)anastasis\.Player\.AutoArrive') {
    return $ArgumentList
  }
  $out = @()
  $added = $false
  foreach ($a in $ArgumentList) {
    if (-not $added -and $a -match '(?i)^-dpcvars=') { $out += ($a + ',anastasis.Player.AutoArrive=0'); $added = $true }
    else { $out += $a }
  }
  if (-not $added) { $out += '-dpcvars=anastasis.Player.AutoArrive=0' }
  return $out
}

function Start-AnastasisEditor {
  param(
    [Parameter(Mandatory = $true)][string]$FilePath,
    [string[]]$ArgumentList = @()
  )
  $ArgumentList = Add-AnastasisScriptedCVars $ArgumentList
  $p = Invoke-AnastasisEditorGated -Headless:(Test-AnastasisHeadlessArgs ($ArgumentList -join ' ')) {
    if ($env:ANASTASIS_EDITOR_VISIBLE -eq '1') {
      Start-Process $FilePath -ArgumentList $ArgumentList -PassThru
    } elseif ($FilePath -match '-Cmd\.exe$') {
      Start-Process $FilePath -ArgumentList $ArgumentList -WindowStyle Hidden -PassThru
    } else {
      $SW_SHOWNOACTIVATE = 4
      $cmd = '"' + $FilePath + '" ' + ($ArgumentList -join ' ')
      [AnastasisLaunch]::Start($FilePath, $cmd, (Get-Location).ProviderPath, $SW_SHOWNOACTIVATE)
    }
  }
  if ($env:ANASTASIS_EDITOR_VISIBLE -eq '1') { return $p }
  $guard = Join-Path $PSScriptRoot 'editor-window-guard.ps1'
  $guardArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-WindowStyle', 'Hidden',
    '-File', ('"' + $guard + '"'), '-ProcessId', $p.Id)
  # Journal du gardien (fenetres vues, deplacees, premier plan) : diagnostic seulement.
  if ($env:ANASTASIS_EDITOR_GUARD_LOG) { $guardArgs += @('-Log', ('"' + $env:ANASTASIS_EDITOR_GUARD_LOG + '"')) }
  Start-Process powershell.exe -WindowStyle Hidden -ArgumentList $guardArgs | Out-Null
  return $p
}

# Editeurs Unreal encore ouverts sur le projet de $Root. Appele par `agent-worktree.ps1 finish` :
# un editeur interactif (`anastasis-unreal.ps1 editor`) ne se ferme pas seul, il reste hors ecran
# avec ses 8-13 Go apres la fin de la mission, et la porte memoire retient alors tous les autres.
# Le separateur final evite qu'une mission en couvre une autre (`x-001` / `x-001b`).
function Find-WorktreeEditor([string]$Root) {
  $needle = ($Root.TrimEnd('\', '/') + '\').Replace('/', '\')
  Get-CimInstance Win32_Process -Filter "Name like 'UnrealEditor%'" | Where-Object {
    $_.CommandLine -and $_.CommandLine.Replace('/', '\').IndexOf($needle, [StringComparison]::OrdinalIgnoreCase) -ge 0
  } | ForEach-Object {
    $p = Get-Process -Id $_.ProcessId -ErrorAction SilentlyContinue
    '{0} pid={1} depuis {2} engage={3:N1} Go' -f $_.Name, $_.ProcessId, $(if ($p) { $p.StartTime.ToString('HH:mm') } else { '?' }),
      $(if ($p) { $p.PrivateMemorySize64 / 1GB } else { 0 })
  }
}

# Lancements d'Unreal qui contournent Start-AnastasisEditor dans tools/unreal/. Appele par
# `agent-worktree.ps1 finish`, qui refuse la passation s'il en trouve : une heure apres
# l'ecriture de cette regle, capture-horizon.ps1 arrivait sur main avec un Start-Process.
function Find-RawEditorLaunch([string]$Root) {
  $skip = @('editor-launch.ps1', 'editor-window-guard.ps1')
  # Toute ligne Start-Process qui nomme UnrealEditor ou $Editor : chemin entre guillemets (avec
  # espaces), chemin nu, (Join-Path $engine '...UnrealEditor.exe'), variable. Large expres : un
  # faux positif se corrige en une ligne, un faux negatif rouvre des fenetres devant Alexandre.
  $pattern = 'Start-Process\b.*(UnrealEditor|\$editor\b)'
  foreach ($f in Get-ChildItem (Join-Path $Root 'tools\unreal') -Filter *.ps1 -File) {
    if ($skip -contains $f.Name) { continue }
    Select-String -LiteralPath $f.FullName -Pattern $pattern | ForEach-Object { "$($f.Name):$($_.LineNumber)" }
  }
}
