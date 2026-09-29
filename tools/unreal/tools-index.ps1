# Coherence entre tools/unreal/ et son index dans AGENTS.md. A dot-sourcer.
#
# L'index n'aide un agent que s'il est vrai. Une doc perimee est pire qu'aucune :
# l'agent la croit et ne va pas voir. Ce projet l'a deja paye (AGENTS.md affirmait
# un enregistrement de registre qui n'avait jamais eu lieu). Ce controle rend la
# derive visible au lieu de compter sur la memoire de chaque agent.
#
# Lit la PREMIERE colonne des tableaux de la section « ## Index de `tools/unreal/` ».
# Une cellule `a.ps1` + `.py` designe a.ps1 et a.py.
#
#   MISSING  fichier present dans tools/unreal/, absent de l'index
#   STALE    nom cite par l'index, fichier absent

function Test-AnastasisToolsIndex([string]$Root) {
  $agents = Join-Path $Root 'AGENTS.md'
  $dir = Join-Path $Root 'tools\unreal'
  $indexed = @{}
  $inSection = $false
  foreach ($line in (Get-Content $agents -Encoding UTF8)) {
    if ($line.StartsWith('## ')) { $inSection = $line.StartsWith('## Index de `tools/unreal/`'); continue }
    if (-not $inSection -or -not $line.StartsWith('|')) { continue }
    $cell = ($line.Split('|'))[1]
    $prev = $null
    foreach ($m in [regex]::Matches($cell, '`([^`]+)`')) {
      $name = $m.Groups[1].Value
      if ($name.StartsWith('.') -and $prev) { $name = [IO.Path]::GetFileNameWithoutExtension($prev) + $name }
      if ($name -match '\.(ps1|py|txt)$') { $indexed[$name] = $true; $prev = $name }
    }
  }
  $present = @(Get-ChildItem $dir -File | ForEach-Object Name)
  return [PSCustomObject]@{
    Missing = @($present | Where-Object { -not $indexed.ContainsKey($_) } | Sort-Object)
    Stale   = @($indexed.Keys | Where-Object { $present -notcontains $_ } | Sort-Object)
  }
}
