# HANDOFF: pontic-common-ground-003 — travail en cours

## MISSION

La planche de sols fournie par Alexandre montre notamment la terre brune, le sol
herbeux et le sol forestier. Renforcer les deux surfaces courantes sous l'herbe
entre la fin de la photo PBR proche (20 m) et la disparition des HISM (55/105 m).
La planche est une reference visuelle, pas une carte PBR.

## RELAIS

RELAIS: pontic-ground-context-002

Cette branche part du commit `6bc8c3c29` qui contient les scans acceptes de
chemin et de gravier. Si ce commit n'est pas encore dans `main`, l'integrateur
admet ce relais apres la mission parente ou par son protocole de relais.

## FILES_OWNED

- `tools/unreal/ground-textures.py`, `ground-material.py`, `ground-cover-capture.py`, `proofs.txt`
- `Content/Anastasis/Materials/GroundTextures/T_Ground_{MeadowDistance,ForestDistance}_AH.uasset` (a importer)
- `Content/Anastasis/Materials/M_AnastasisGround.uasset`, `MI_AnastasisGround.uasset` (a regenerer)
- cette fiche

## COMMIT

Le premier commit conserve les scripts et la procedure de reprise. Les `.uasset`
et les preuves seront produits dans un commit suivant avant `finish`.

## MEC

- Build initial du worktree depuis `agent/pontic-ground-context-002` : `BUILD::PASS`,
  19 actions, 301,73 s. Aucun C++ de cette mission n'a ete change.
- Syntaxe des trois scripts Python : `PY_SYNTAX::PASS` via `ast.parse`.
- Sources CC0 : [Leafy Grass](https://polyhaven.com/a/leafy_grass) et
  [Forest Leaves 02](https://polyhaven.com/a/forest_leaves_02) (Poly Haven).
- `ground-textures.py MeadowDistance ForestDistance` : PASS, 2048x2048 ;
  passe-haut 75 cm et 100 cm, pixels albédos écrêtés 0,002 % et 0,316 %.
  Sources, URL et SHA256 dans `Saved/GroundTextures/manifest.json` (local, ignore).
- Le HLSL emploie un sampler Wrap partage fourni par UE 5.8, avec dérivées
  explicites dans les branches. **Compilation Unreal et nombre de samplers UNKNOWN**.
- `ground-material.ps1 -Rebuild -TimeoutSec 900` a attendu la porte memoire
  (`EDITOR_GATE::WAIT`, deux editeurs deja ouverts, 0,7 a 1,1 Go libres).
  Ce processus d'attente a ete interrompu avant de lancer un editeur.

## PROOFS

PROOFS: pontic-path-capture, pontic-gravel-capture, pontic-common-ground-capture

## SCN

UNKNOWN : les deux PNG empaquetes ont ete inspectes, mais aucune image du
materiau dans la scene n'a ete capturee. Regarder les vues prairie, lisiere et
oblique en A/B/A ; rejeter un damier, une couleur de photo trop forte ou un
ecart inferieur a la variance du temoin.

## PLY

UNKNOWN : aucune marche joueur.

## REPRISE

1. Depuis `C:\dev\ANASTASIS_WORKTREES\pontic-common-ground-003`, verifier
   `MAIN_LOCK::libre` et la file editeur ; ne fermer aucun editeur etranger.
2. `tools\unreal\ground-material.ps1 -Rebuild -TimeoutSec 900` : importe les
   deux textures, regenere le master et l'instance, exige `GROUND_MATERIAL::PASS`
   et inspecte les statistiques materiau (16 samplers maximum auparavant).
3. `tools\unreal\editor-batch.ps1 -Proofs pontic-common-ground-capture` :
   A/B/A identique a 11 h, puis inspecter tous les PNG et mesurer avec
   `.claude\skills\anastasis-capture\compare.py` contre le temoin.
4. Si le rendu est a garder, mettre a jour cette fiche, la fiche de sol du skill
   et l'index AGENTS si la procedure change ; commiter les `.uasset` par LFS.
5. `tools\unreal\agent-worktree.ps1 finish -Mission pontic-common-ground-003`.
   S'arreter a `HANDOFF_READY::YES` ; seul l'integrateur verse dans `main`.

## INTEGRATION_RISK

- Cette branche porte la mission parente non encore versee au moment du depart.
- Le materiau partage a deja 16 samplers ; le nouveau sampler Wrap partage doit
  etre controle par la compilation, pas suppose fonctionnel.
- Les textures produites sous `Saved/` ne sont pas les assets du jeu. Aucun
  `.uasset` de cette mission n'existe tant que l'etape 2 n'a pas passe.

## ECARTS

AUCUN : `Source/AnastasisSim/` inchange.

## STOP

Pas de `HANDOFF_READY`, pas d'integration, pas de revendication visuelle.
