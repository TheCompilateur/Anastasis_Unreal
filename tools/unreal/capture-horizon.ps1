# HORIZON_RING_001 -- A/B de l'horizon : anastasis.Terrain.Horizon 0 (A) puis 1 (B),
# memes cameras (voir capture-horizon.py), puis part de pixels "vide" par image.
# Sortie : Saved\HorizonEvidence\<Label>\<vue>_<A|B>.png
#
# -PreCmds 'cmd1;cmd2' : commandes console appliquees aux DEUX etats (etape brume :
# l'anneau reste la seule variable entre A et B, ou fixer -States B pour un seul etat).
#
# -Atmosphere : applique le profil DA_AnastasisAtmosphere comme le GameMode en PIE (soleil,
# ciel, brume, exposition, poches de brume) ; sans lui, l'image est l'eclairage du niveau.
param([string]$Label='ring', [string]$PreCmds='', [ValidateSet('AB','B')][string]$States='AB', [switch]$Atmosphere, [int]$TimeoutSec=480)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root "Saved\HorizonEvidence\$Label"
New-Item -ItemType Directory -Force $dir | Out-Null
$py=(Join-Path $Root 'tools\unreal\capture-horizon.py').Replace('\','/')
$plan = if ($States -eq 'AB') { @(@('0','A'), @('1','B')) } else { @(,@('1','B')) }
foreach ($state in $plan) {
 $log=Join-Path $dir "capture_$($state[1]).log"
 if(Test-Path $log){Remove-Item $log}
 $env:ANASTASIS_HORIZON_OUT=$dir
 $env:ANASTASIS_HORIZON_CVAR=$state[0]
 $env:ANASTASIS_HORIZON_TAG=$state[1]
 $env:ANASTASIS_HORIZON_PRE=$PreCmds
 $env:ANASTASIS_HORIZON_ATMOSPHERE= if ($Atmosphere) { '1' } else { '0' }
 $launchArgs=@(
  ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
  '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
  # Hors premier plan, le viewport ne rend plus et HighResShot n'est jamais servi
  # (voir capture-terrain-relief.ps1).
  '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
  ('-abslog="'+$log+'"'),
  ('-ExecCmds="py '+$py+'"')
 )
 $p=Start-AnastasisEditor $Editor $launchArgs
 $p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
 $p.Refresh()
 if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force; throw "CAPTURE::FAIL editeur bloque ($($state[1]))" }
 Select-String -Path $log -Pattern 'HORIZON_(VIEW|SHOT|COMPLETE|MAP|ATMOSPHERE|FOG|SKY)|ANASTASIS_TERRAIN_HORIZON|ANASTASIS_ATMOSPHERE ' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') }
}

# Vide = le sol de planete du SkyAtmosphere : bleu nuit (16,30,50 a 67,96,129 en
# altitude), quasi-noir au ras du sol (5,8,15). L'eau rendue est bien plus claire,
# l'ombre du sol grise ou verte. Limite connue : le quasi-noir compte aussi les ombres
# les plus denses (sous-bois de H2) -- comparer A et B, pas lire une valeur seule.
Add-Type -AssemblyName System.Drawing
$missing=0
foreach ($view in 'H1_bassin_vers_le_bord','H2_point_haut_vers_l_exterieur','H3_bord_regard_dehors','H4_vue_generale','H5_altitude_vers_le_coin') {
 foreach ($state in $plan) {
  $shot=Join-Path $dir "$($view)_$($state[1]).png"
  if(!(Test-Path $shot)){ Write-Output "CAPTURE::MISSING $($view)_$($state[1]).png"; $missing++; continue }
  $bmp=[System.Drawing.Bitmap]::FromFile($shot)
  $rect=New-Object System.Drawing.Rectangle 0,0,$bmp.Width,$bmp.Height
  $data=$bmp.LockBits($rect,[System.Drawing.Imaging.ImageLockMode]::ReadOnly,[System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
  $bytes=New-Object byte[] ($data.Stride*$bmp.Height)
  [Runtime.InteropServices.Marshal]::Copy($data.Scan0,$bytes,0,$bytes.Length)
  $bmp.UnlockBits($data)
  $n=0;$void=0
  for($y=0;$y -lt $bmp.Height;$y+=2){
   $row=$y*$data.Stride
   for($x=0;$x -lt $bmp.Width;$x+=2){
    $i=$row+3*$x; $b=$bytes[$i]; $g=$bytes[$i+1]; $r=$bytes[$i+2]; $n++
    if(($b -ge 25 -and $b -le 140 -and ($b - $r) -ge 25 -and $b -gt $g -and $r -lt 80) -or ([Math]::Max([Math]::Max($r,$g),$b) -lt 25 -and $b -ge $r)){$void++}
   }
  }
  $bmp.Dispose()
  Write-Output ('HORIZON_VOID {0}_{1} void={2:P1}' -f $view,$state[1],($void/$n))
 }
}
if($missing -gt 0){ throw "CAPTURE::FAIL $missing image(s) manquante(s)" }
Write-Output ('CAPTURE::PASS ' + $dir)
