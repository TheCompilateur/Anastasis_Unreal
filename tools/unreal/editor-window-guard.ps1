# Gardien des fenetres d'un editeur lance par un agent. Processus cache, detache.
#
# Pourquoi : les editeurs des agents s'ouvraient au premier plan pendant qu'Alexandre
# travaillait. Il les fermait -- et tuait, sans pouvoir le savoir, la preuve en cours
# d'un agent (2026-09-29 : six fermetures, dont deux report-tests). Voir AGENTS.md.
#
# offscreen : chaque fenetre visible du processus part hors de l'ecran, au fond de la
#             pile, SANS activation. Pas de minimisation : une fenetre minimisee n'est
#             plus dessinee par Slate, et les captures (HighResShot) en dependent.
#             Les fenetres creees plus tard (PIE, Journal des messages) sont suivies.
# observe   : ne touche a rien, journalise seulement (experience temoin).
#
# Le gardien sort avec le processus surveille.
param(
  [Parameter(Mandatory = $true)][int]$ProcessId,
  [ValidateSet('offscreen', 'observe')][string]$Mode = 'offscreen',
  [string]$Log = '',
  [int]$PollMs = 150
)
$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class AnastasisWin {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool attach);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  public static uint ThreadOf(IntPtr h) { uint w; return GetWindowThreadProcessId(h, out w); }
  // Rend le premier plan a une fenetre de l'utilisateur. Un processus d'arriere-plan n'a
  // pas le droit de SetForegroundWindow ; attache a la file d'entree du thread qui tient
  // le premier plan, il l'a. Aucune frappe simulee.
  public static bool GiveBack(IntPtr fg, IntPtr target) {
    uint me = GetCurrentThreadId(), fgThread = ThreadOf(fg);
    bool attached = fgThread != 0 && fgThread != me && AttachThreadInput(me, fgThread, true);
    try { return SetForegroundWindow(target); }
    finally { if (attached) AttachThreadInput(me, fgThread, false); }
  }
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  public static List<IntPtr> WindowsOf(uint pid) {
    var list = new List<IntPtr>();
    EnumWindows((h, p) => { uint w; GetWindowThreadProcessId(h, out w); if (w == pid) list.Add(h); return true; }, IntPtr.Zero);
    return list;
  }
  public static string Title(IntPtr h) { var s = new StringBuilder(256); GetWindowText(h, s, 256); return s.ToString(); }
  public static uint PidOf(IntPtr h) { uint w; GetWindowThreadProcessId(h, out w); return w; }
}
'@

function Write-GuardLog($msg) {
  if ($Log) { Add-Content -LiteralPath $Log -Value ((Get-Date -Format 'HH:mm:ss.fff') + ' ' + $msg) -Encoding UTF8 }
}

$HWND_BOTTOM = [IntPtr]1
$SWP_NOSIZE = 0x0001; $SWP_NOACTIVATE = 0x0010; $SWP_NOOWNERZORDER = 0x0200
# Asynchrone : le deplacement est poste au thread de la fenetre. Si ce thread est occupe,
# le gardien n'attend pas -- il continue de surveiller les autres fenetres, et le
# deplacement s'applique des que l'editeur traite ses messages.
$SWP_ASYNCWINDOWPOS = 0x4000
$moveFlags = $SWP_NOSIZE -bor $SWP_NOACTIVATE -bor $SWP_NOOWNERZORDER -bor $SWP_ASYNCWINDOWPOS
# Une fenetre encore a l'ecran (deplacement en attente, ou repositionnee par l'editeur)
# n'est renvoyee qu'une fois toutes les 2 s : sans cela, une file de messages bouchee
# recevait une demande toutes les 150 ms, et le journal une ligne a chaque tour.
$retryAfter = [TimeSpan]::FromSeconds(2)
$lastMove = @{}
# Au-dela du bord gauche de tout le bureau virtuel, quelle que soit la disposition des ecrans.
Add-Type -AssemblyName System.Windows.Forms
$vs = [System.Windows.Forms.SystemInformation]::VirtualScreen
$offX = $vs.Left - 5000
$offY = $vs.Top + 40

$seen = @{}
$lastFg = $null
# Derniere fenetre au premier plan qui n'appartient pas a l'editeur : celle a qui rendre la main.
$userFg = [AnastasisWin]::GetForegroundWindow()
if ([AnastasisWin]::PidOf($userFg) -eq $ProcessId) { $userFg = $null }
Write-GuardLog "START pid=$ProcessId mode=$Mode off=($offX,$offY)"
while ($true) {
  $proc = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
  if (-not $proc) { break }
  foreach ($h in [AnastasisWin]::WindowsOf([uint32]$ProcessId)) {
    if (-not [AnastasisWin]::IsWindowVisible($h)) { continue }
    $key = $h.ToInt64()
    $r = New-Object AnastasisWin+RECT
    [void][AnastasisWin]::GetWindowRect($h, [ref]$r)
    $onScreen = $r.L -gt ($offX + 1000)
    $due = $onScreen -and (-not $lastMove.ContainsKey($key) -or ((Get-Date) - $lastMove[$key]) -ge $retryAfter)
    if (-not $seen.ContainsKey($key) -or ($Mode -eq 'offscreen' -and $due)) {
      $title = [AnastasisWin]::Title($h)
      Write-GuardLog ("WINDOW hwnd=$key title='$title' rect=($($r.L),$($r.T),$($r.R),$($r.B)) iconic=" + [AnastasisWin]::IsIconic($h))
      if ($Mode -eq 'offscreen' -and $due) {
        $ok = [AnastasisWin]::SetWindowPos($h, $HWND_BOTTOM, $offX, $offY, 0, 0, $moveFlags)
        $lastMove[$key] = Get-Date
        Write-GuardLog "MOVED hwnd=$key ok=$ok"
      }
      $seen[$key] = $true
    }
  }
  $fg = [AnastasisWin]::GetForegroundWindow()
  $fgPid = [AnastasisWin]::PidOf($fg)
  if ($fg -ne $lastFg) {
    Write-GuardLog ("FOREGROUND pid=$fgPid editor=" + ($fgPid -eq $ProcessId) + " title='" + [AnastasisWin]::Title($fg) + "'")
    $lastFg = $fg
  }
  if ($fg -ne [IntPtr]::Zero -and $fgPid -ne $ProcessId) {
    $userFg = $fg
  } elseif ($Mode -eq 'offscreen' -and $fgPid -eq $ProcessId -and $userFg -and [AnastasisWin]::IsWindow($userFg)) {
    # Une fenetre de l'editeur a pris le premier plan (PIE, dialogue) : la frappe de
    # l'utilisateur partirait dans une fenetre hors de l'ecran. Rendre la main.
    $ok = [AnastasisWin]::GiveBack($fg, $userFg)
    Write-GuardLog "GIVE_BACK to='$([AnastasisWin]::Title($userFg))' ok=$ok"
  }
  Start-Sleep -Milliseconds $PollMs
}
Write-GuardLog 'EXIT process ended'
