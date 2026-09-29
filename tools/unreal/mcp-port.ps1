# Port MCP de l'editeur d'une racine ANASTASIS. A dot-sourcer, pas un point d'entree.
#
# Chaque worktree lance son propre editeur, et le plugin ModelContextProtocol vise
# le port 8000 par defaut : le premier editeur leve le garde, et un agent qui croit
# inspecter SON editeur parle a celui d'un autre. Un port par racine supprime ce
# partage : 8000 pour le canonique, un port stable derive du nom de mission pour
# chaque worktree. Stable = meme mission, meme port, dans tous les processus :
# FNV-1a, pas GetHashCode (non garanti stable).
#
# Deux missions peuvent tomber sur le meme port (800 valeurs). C'est pourquoi
# get_session_snapshot renvoie project_dir : le controle reste obligatoire.

function Get-AnastasisMcpPort([string]$Root) {
  $full = [IO.Path]::GetFullPath($Root).TrimEnd('\')
  if ($full -ieq 'C:\dev\ANASTASIS_UNREAL') { return 8000 }
  $mission = (Split-Path $full -Leaf).ToLowerInvariant()
  [uint32]$hash = 2166136261
  foreach ($b in [Text.Encoding]::UTF8.GetBytes($mission)) {
    $hash = [uint32](($hash -bxor $b) * [uint64]16777619 % 4294967296)
  }
  return 8100 + [int]($hash % 800)
}
