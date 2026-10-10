# HANDOFF: disk-hygiene-001

## MISSION

Mandat d'Alexandre du 2026-10-09 (« fait les 2 »). Le disque C: est retombé de 233 à 72 Go libres en deux
jours. Deux causes ont été mesurées :

- `C:\dev\ANASTASIS_WORKTREES` : 222 Go. 46 worktrees ont été créés en deux jours, chacun avec sa compilation
  complète (~3,4 Go d'`Intermediate/`), et aucun n'a été élagué après son versement.
- Le cache partagé Zen (`AppData\Local\UnrealEngine\Common\Zen`) : 64 → 94 Go. Le moteur garde les entrées
  14 jours et ne nettoie qu'en dessous de 2 Go libres.

Corrections :

1. `integrate` et `integrate-batch` élaguent eux-mêmes chaque mission versée (`AUTO_PRUNE::ELAGUE <m>`).
   Une mission sale, tenue par un processus ou dont un commit manque à `main` est gardée et nommée
   (`AUTO_PRUNE::GARDE <m>`), worktree intact. `ANASTASIS_AUTO_PRUNE=0` désactive l'élagage.
   `prune` refuse désormais aussi un worktree où un processus tourne encore. Avant, `git worktree remove`
   pouvait effacer la moitié des fichiers puis buter sur ceux qui étaient tenus.
2. `Config/DefaultEngine.ini` plafonne Zen : GC dès 40 Go occupés (`--gc-disksize-softlimit`), entrées de
   plus de 5 jours retirées, un passage par heure, nettoyage forcé sous 20 Go libres.

## FILES_OWNED

- tools/unreal/agent-worktree.ps1 (`Invoke-Prune`, `Invoke-AutoPrune`, fin de `integrate` / `integrate-batch`)
- tools/unreal/test-agent-worktree.ps1 (S32 ; `ANASTASIS_AUTO_PRUNE=0` pour les scénarios existants)
- Config/DefaultEngine.ini (`[Zen.AutoLaunch]`)
- AGENTS.md (ligne `prune` de la table du cycle de vie, ligne d'index du banc), .claude/skills/anastasis-mission/SKILL.md (§6)

## COMMIT

voir `git log agent/disk-hygiene-001`

## MEC

- BANC : `tools\unreal\test-agent-worktree.ps1` -> 85 PASS, 0 FAIL (2026-10-09). Le scenario d'elagage a tourne sous le libelle S30, renomme S33 (S30 et S32 sont pris par la porte memoire et le plafond de preuves) ; rejoue apres rebase sur fd8563d4b : 99 PASS, 0 FAIL : ligne `ANASTASIS_AUTO_PRUNE=0` en tete, lot de trois missions (propre elaguee, sale gardee avec son brouillon, tenue par un processus gardee intacte), `integrate` seul qui elague, interrupteur a 0 qui laisse tout
- ZEN : un zenserver jetable (port 18558, dossier de données du scratchpad) lancé avec la nouvelle ligne
  `ExtraArgs` démarre (`/health` = 200) et journalise `0B used, 40G soft limit ... Disk usage GC in 40G`,
  `initializing GC, enabled 'true', interval 1h00m`. Les options sont donc reconnues par le zenserver installé.
- BUILD / TESTS : par `finish` (la branche touche `Config/`)

## PROOFS

PROOFS: (aucune)

## SCN

Aucun changement de scène.

## PLY

Aucun changement joueur.

## ECARTS

AUCUN — la mission ne touche pas `Source/AnastasisSim`.

## INTEGRATION_RISK

- Après le versement de ce lot, l'intégrateur n'a plus à lancer `prune` : le versement le fait.
  `AUTO_PRUNE::GARDE` est normal pour une mission encore sale. C'est un signal à suivre, pas un échec.
- Une session d'agent encore ouverte dans un worktree versé et propre perd son dossier de travail. C'est voulu :
  la mission est finie. Une suite s'ouvre dans une mission neuve.
- Le plafond Zen ne prend effet qu'au prochain démarrage du serveur Zen, c'est-à-dire quand tous les éditeurs
  sont fermés puis qu'un nouveau se lance. Le serveur en cours garde ses anciennes options. Le premier nettoyage
  (94 → 40 Go) se fait au premier passage de GC du nouveau serveur.
- `ExtraArgs` remplace toute la ligne de `BaseEngine.ini` (UE 5.8.2). Si le moteur change cette ligne, il faut la recopier.

## STOP

- Ne revendique pas qu'une mesure du disque ait été faite après un vrai versement. Le comportement d'élagage est
  prouvé sur le dépôt jetable du banc (S32), pas sur `main`.
- Ne revendique pas que Zen soit déjà redescendu à 40 Go : il le sera au premier GC d'un serveur relancé.
- Ne nettoie pas les worktrees existants. La liste à élaguer à la main est donnée à Alexandre à part.
