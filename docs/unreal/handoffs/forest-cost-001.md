# HANDOFF: forest-cost-001

## MISSION

Mesurer ce que coûte la végétation en ms GPU, strate par strate et vue par vue, sur la machine réelle
(RTX 3060), puis proposer un budget chiffré. Aucune CVar ne retire les arbres : un outil de mesure
nouveau masque les strates à l'exécution, aux mêmes caméras, dans la même session. Le budget lui-même
reste une décision d'Alexandre : la mission l'écrit comme proposition, pas comme règle.

## FILES_OWNED

- `tools/unreal/vegetation-cost-capture.ps1` (nouveau)
- `tools/unreal/vegetation-cost-capture.py` (nouveau)
- `AGENTS.md` (une ligne d'index)
- `.claude/skills/anastasis-realisme/fiches/vegetation.md`, `fiches/performance.md` (mesures, proposition de budget)
- `docs/unreal/handoffs/forest-cost-001.md`

## COMMIT

voir `git log agent/forest-cost-001`

## MEC

- BUILD: `finish` (aucun fichier sous `Source/`, `Config/`, `Content/`)
- TESTS: idem
- COMMANDS:
  - `tools\unreal\vegetation-cost-capture.ps1 -Label run-002` → `VEGCOST_CAPTURE_COMPLETE views=6 states=6`,
    `Saved\VegetationCostEvidence\run-002\vegetation-cost.json` + 36 images
  - run-001 : arrêté à la 12e vue par un `HighResShot` jamais servi ; corrigé (une image manquante est
    signalée, la mesure gardée), relancé en run-002.
- Inventaire (instances visibles, classées par mesh et nom de composant) : arbres 5 688 en 266 composants ;
  herbe 1 268 003 en 1 222 ; sous-bois 25 203 en 31 ; rives 51 782 en 249 ; micro-écologie 20 023 en 1 453 ;
  roches 139 ; autres 575.
- GPU p50 (`stat unit`, ms), viewport éditeur de la fenêtre 1280×720, midi sec épinglé. Coût d'une strate
  = moyenne(all, all2) − état ; témoin = |all − all2| :

  | vue | all | sans arbres | sans sous-bois | sans herbe | sans végétation | all2 | arbres | herbe | végétation | témoin |
  |---|---|---|---|---|---|---|---|---|---|---|
  | intérieur de forêt | 14,24 | 10,48 | 14,26 | 12,85 | 8,06 | 14,22 | **3,75** | 1,38 | **6,17** | 0,02 |
  | lisière | 11,17 | 10,76 | 11,20 | 8,88 | 7,86 | 11,01 | 0,33 | 2,21 | 3,23 | 0,15 |
  | prairie | 12,63 | 11,60 | 12,95 | 9,71 | 8,35 | 12,44 | 0,94 | 2,83 | 4,18 | 0,19 |
  | vallée B | 13,41 | 12,67 | 13,59 | 9,56 | 8,20 | 13,21 | 0,64 | **3,75** | 5,11 | 0,20 |
  | oblique | 10,82 | 9,79 | 10,77 | 9,97 | 8,39 | 10,49 | 0,87 | 0,69 | 2,26 | 0,34 |
  | aérien | 10,26 | 8,85 | 10,48 | 10,04 | 7,95 | 10,04 | 1,30 | 0,11 | 2,20 | 0,22 |

  Sous-bois : −0,03 à −0,41 ms, soit dans le bruit : non mesurable. La scène sans végétation (terrain,
  eau, ciel, Lumen) coûte 7,9 à 8,4 ms.
- Images regardées : `foret_eye_notrees` (la forêt disparaît, restent souches et bois mort de la
  micro-écologie), `vallee_b_eye_nograss` (sol nu, arbres présents) : le masquage retire la bonne strate.
- Budget approuvé par Alexandre le 2026-10-01, écrit en règle PERF-05 (`fiches/performance.md`) : 16,7 ms
  GPU par vue dans ce banc, végétation ≤ 6,5 ms, tout ajout chiffré dans ce banc.

## PROOFS

Preuves PIE que le lot rejoue pour cette mission, noms de `tools/unreal/proofs.txt` (EDITOR_QUEUE_001) :

PROOFS: (aucune)

## SCN

`Lvl_AnastasisSlice` chargée dans un éditeur dédié ; monde incarné (graine 12345), strates masquées par
`set_visibility`. Rien n'est sauvé.

## PLY

`PLAYER` non touché.

## INTEGRATION_RISK

- Le classement des strates repose sur des chemins de meshes et des préfixes de composants
  (`/Vegetation/SM_Tree`, `/Vegetation/Hero/`, `GroundCover_`, `Understory_`, `Riverbank_`, `MicroEco_`) :
  un nouveau mesh d'arbre hors de ces chemins tomberait dans `other` sans être masqué. Le script lit
  l'inventaire et refuse de tourner sans arbre trouvé.
- La sortie de l'éditeur se termine par le crash Python post-capture déjà connu (`Fatal error!` après
  `QUIT_EDITOR`) ; il ne touche pas la mesure, déjà écrite.

## STOP

- Ce ne sont pas des ms de jeu : viewport d'éditeur dans une fenêtre 1280×720, pas un jeu packagé en
  1080p. Les ratios entre strates valent ; les valeurs absolues ne promettent aucune fréquence d'image.
- Une seule heure (midi, sec) : l'aube, le contre-jour ou la pluie peuvent changer le coût des ombres.
- Le budget a été approuvé par Alexandre le 2026-10-01 (PERF-05, `fiches/performance.md`) pour CE banc ; celui du jeu packagé reste à mesurer et à fixer.
