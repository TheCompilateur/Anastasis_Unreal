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

function Start-AnastasisEditor {
  param(
    [Parameter(Mandatory = $true)][string]$FilePath,
    [string[]]$ArgumentList = @()
  )
  if ($env:ANASTASIS_EDITOR_VISIBLE -eq '1') {
    return Start-Process $FilePath -ArgumentList $ArgumentList -PassThru
  }
  if ($FilePath -match '-Cmd\.exe$') {
    $p = Start-Process $FilePath -ArgumentList $ArgumentList -WindowStyle Hidden -PassThru
  } else {
    $SW_SHOWNOACTIVATE = 4
    $cmd = '"' + $FilePath + '" ' + ($ArgumentList -join ' ')
    $p = [AnastasisLaunch]::Start($FilePath, $cmd, (Get-Location).ProviderPath, $SW_SHOWNOACTIVATE)
  }
  $guard = Join-Path $PSScriptRoot 'editor-window-guard.ps1'
  $guardArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-WindowStyle', 'Hidden',
    '-File', ('"' + $guard + '"'), '-ProcessId', $p.Id)
  # Journal du gardien (fenetres vues, deplacees, premier plan) : diagnostic seulement.
  if ($env:ANASTASIS_EDITOR_GUARD_LOG) { $guardArgs += @('-Log', ('"' + $env:ANASTASIS_EDITOR_GUARD_LOG + '"')) }
  Start-Process powershell.exe -WindowStyle Hidden -ArgumentList $guardArgs | Out-Null
  return $p
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
