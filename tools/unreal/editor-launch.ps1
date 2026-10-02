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

function Get-AnastasisEditorLoad {
  $mem = [AnastasisMemory]::Read()
  $eds = @(Get-Process UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue)
  [pscustomobject]@{
    Editors  = $eds.Count
    Pids     = (($eds | ForEach-Object Id) -join ',')
    RamGB    = [math]::Round($mem[0] / 1GB, 1)
    CommitGB = [math]::Round($mem[1] / 1GB, 1)
  }
}

function Get-AnastasisGateSetting([string]$Name, [double]$Default) {
  $v = [Environment]::GetEnvironmentVariable($Name)
  if ($v) { return [double]$v } # cast PowerShell : culture invariante, "2.5" partout
  return $Default
}

# Lance $Launch quand la machine peut porter un editeur de plus (voir l'en-tete), et rend
# ce qu'il rend. Les messages passent par Write-Host pour ne pas se meler au processus rendu.
function Invoke-AnastasisEditorGated {
  param(
    [Parameter(Mandatory = $true)][scriptblock]$Launch,
    [int]$MaxEditors = (Get-AnastasisGateSetting 'ANASTASIS_EDITOR_MAX' 2),
    [double]$MinRamGB = (Get-AnastasisGateSetting 'ANASTASIS_EDITOR_MIN_RAM_GB' 3),
    [double]$MinCommitGB = (Get-AnastasisGateSetting 'ANASTASIS_EDITOR_MIN_COMMIT_GB' 8),
    [double]$TimeoutMinutes = (Get-AnastasisGateSetting 'ANASTASIS_EDITOR_WAIT_MIN' 45),
    [int]$PollSeconds = 15
  )
  if ($env:ANASTASIS_EDITOR_GATE -eq '0') { return & $Launch }
  $gate = New-Object System.Threading.Mutex($false, 'Global\AnastasisEditorGate')
  $gateStart = Get-Date
  $gateNote = [datetime]::MinValue
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
      $load = Get-AnastasisEditorLoad
      $blocked = @()
      if ($ticket) {
        $ahead = 0
        foreach ($t in @(Get-ChildItem $queueDir -Filter '*.ticket' -ErrorAction SilentlyContinue | Sort-Object Name)) {
          if ($t.FullName -eq $ticket) { break }
          $owner = [int]($t.BaseName.Split('-')[-1])
          # Ticket d'un lanceur disparu (tue, machine redemarree) : retire, il ne bloque personne.
          if (-not (Get-Process -Id $owner -ErrorAction SilentlyContinue)) { Remove-Item $t.FullName -Force -ErrorAction SilentlyContinue; continue }
          $ahead++
        }
        if ($ahead -gt 0) { $blocked += "file=$ahead devant" }
      }
      if ($load.Editors -ge $MaxEditors) { $blocked += "editeurs=$($load.Editors)/$MaxEditors (pids $($load.Pids))" }
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

function Start-AnastasisEditor {
  param(
    [Parameter(Mandatory = $true)][string]$FilePath,
    [string[]]$ArgumentList = @()
  )
  $p = Invoke-AnastasisEditorGated {
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
