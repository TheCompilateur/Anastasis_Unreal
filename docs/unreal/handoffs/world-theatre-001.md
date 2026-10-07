# HANDOFF: world-theatre-001

## MISSION

MEGALOGRAPHIA — mise en scène géographique de la World Map. Lire le monde **réellement exécuté**,
mesurer ce qui le fait lire comme une démo procédurale, poser une couche additive indépendante,
coupable d'un geste, sans toucher aux systèmes des autres agents. Conception complète :
`docs/unreal/world-theatre-001/WORLD_THEATRE_001.md`. Brief d'historicité :
`docs/historicity/briefs/world-theatre-001.json`.

Livré :
1. **Lecture** — `anastasis.Theatre.Read` (`AnastasisWorldReading`) : sol et eau rendus (carte + anneau),
   objets posés par famille ; preuve PIE `world-theatre-read`.
2. **Analyse hors moteur** — `world-theatre-analyze.py` : dérivés, 7 signatures anti-génératives mesurées,
   9 vistas canoniques évaluées (horizon, plans, bruit), rendu logiciel, composition, plan C++ généré.
3. **Couche** — `UAnastasisWorldTheatreSubsystem` + `AnastasisWorldTheatre` : drape le plan
   (`AnastasisWorldTheatrePlan.inl`, généré) sur le sol rendu ; **10 masses forestières** au pied de la
   muraille nord-est (8 318 ha, au-delà de 2,5 km), ouverture sud-ouest laissée vide ; un acteur transitoire,
   couche d'éditeur `DL_WORLD_THEATRE`, `anastasis.Theatre 0/1` à chaud, **0 par défaut**.
4. **Captures** — `world-theatre-capture` : 9 vistas en PIE, Theatre 0 / 1 / 0, ciel à 11 h, simulation gelée.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldTheatre/` (nouveau dossier, 8 fichiers)
- `tools/unreal/world-theatre-read.py`, `world-theatre-analyze.py`, `world-theatre-capture.py`
- `docs/unreal/world-theatre-001/` (conception, vistas), `docs/historicity/briefs/world-theatre-001.json`
- lignes ajoutées : `AGENTS.md` (index), `tools/unreal/proofs.txt` (2 entrées)

## COMMIT

PENDING

## MEC

- BUILD: PASS (`anastasis-unreal.ps1 build`, Editor Win64 Development, worktree).
- TESTS: `report-tests.ps1 -Filter Anastasis.WorldTheatre` -> PASS 4 / KNOWN_EXPECTED_FAILURE 0 / FAIL 0
  (`Geometry`, `Mass`, `Silhouette`, `PlanIsWellFormed`). Run precedent : `Mass` FAIL sur un noeud pose
  exactement sur le contour (ambiguite balayage / rayon), test precise. Suite complete `Anastasis` : non jouee ici (lot).
- Relevé : `WORLD_THEATRE_READ OK near=1295x1295@2000 far=4810x4810@20000 placed=103227 ignored=1200311`
  (`PROOF::PASS world-theatre-read (122.1s)`, `EDITOR_BATCH::PASS 1/1`). La grille lointaine est désormais
  bornée à ±75 km.
- Couche en PIE (run 4, final) : `WORLD_THEATRE built masses=10 (8365 ha) silhouettes=0 rejected=0 triangles=106850 0.18s` ;
  run 1 avec relevé des objets : 4,20 s, retiré du drapage. `PROOF::PASS world-theatre-capture (182.3s)`.
- COMMANDS:
  - `tools\unreal\editor-batch.ps1 -Proofs world-theatre-read`
  - `python tools/unreal/world-theatre-analyze.py Saved/WorldTheatreEvidence/read --vistas docs/unreal/world-theatre-001/vistas.json --resolve-vistas --compose --emit-plan Source/Anastasis_UnrealV2/WorldTheatre/AnastasisWorldTheatrePlan.inl`
  - `tools\unreal\editor-batch.ps1 -Proofs world-theatre-capture`
  - `tools\unreal\report-tests.ps1 -Filter Anastasis.WorldTheatre`

## PROOFS

PROOFS: world-theatre-read

(`world-theatre-capture` est au registre comme outil de capture : COMPLETE = images écrites, pas un verdict.)

## SCN

Run 4 (final), `Saved/WorldTheatreEvidence/capture/` du worktree, 9 vistas, Theatre 0 / 1 / 0, ciel 11 h,
simulation gelée. Pixels changés > 16/255, APRÈS contre témoin, et bruit témoin/avant :

| Vista | Effet | Bruit | Lecture |
|---|---|---|---|
| V8 vue générale | **14,85 %** | 0,27 % | ceinture forestière au pied de la muraille : le passage plaine -> montagne a un plan |
| V1 village -> chaîne | **5,60 %** | 1,67 % | flancs bas vêtus, un plan de plus derrière les arbres de la carte |
| V6 approche | **4,84 %** | 0,67 % | idem, plus net |
| V5 bord est | **3,24 %** | 0,66 % | idem, sous la muraille |
| V2, V3, V4, V7, V9 | 0,5–3,8 % | 0,8–3,0 % | **nul** : rien de la couche dans le cadre (ouverture SO volontairement vide) |

Planche : `Saved/WorldTheatreEvidence/capture/world-theatre-001_avant_apres.png`. Runs 1-3 conservés
(`capture-run1..3`). Verdict visuel : **amélioration réelle et modeste** du plan lointain sur 4 vistas ;
lisières encore « découpées » contre une montagne très voilée (matériau / atmosphère d'autres propriétaires) ;
le « carré posé sur la table » (V8) est **inchangé** — ses causes sont dans la carte.

## PLY

UNKNOWN : la couche est hors de la zone jouable, sans collision ni navigation ; aucune marche joueur.

## ECARTS

AUCUN — `Source/AnastasisSim/` non touché.

## INTEGRATION_RISK

- Couche **désactivée par défaut** (`anastasis.Theatre 0`) : l'intégrer ne change aucune image tant
  qu'Alexandre ne l'allume pas. Elle lit `AAnastasisWorldEmbodiment` (composants `ExperimentalTerrain` /
  `HorizonTerrain` par nom, sections 0/1/2) : un renommage de ces sous-objets la rend muette
  (`WORLD_THEATRE rebuild skipped`), sans crash.
- Le plan est **généré sur le relief de la graine 12345 du 2026-10-07**. Si un agent terrain change l'anneau,
  les masses se redrapent sur le nouveau sol (aucune ne flotte), mais leur placement n'est plus optimal :
  relancer le relevé puis `--compose --emit-plan`.
- Réutilise **en lecture** `M_AnastasisFarTerrain` (propriété CONTINENTAL_001).
- Branches voisines : `worldmap-*` (fiches seules, déjà versées), `lake-shoreline-contour-001`
  (`AnastasisWorldEmbodiment.cpp`, non touché ici). Aucun fichier commun.

## STOP

- Ne revendique **pas** que la carte a perdu sa signature « procedural showcase » : les quatre signatures les
  plus fortes (densité partout, rochers uniformes, variantes équitirées, ruines dans 96 % des quadrats) sont
  **dans la carte**, chez les systèmes végétation / lieux composés ; elles sont mesurées et transmises, pas
  corrigées.
- Repère humain : **refusé** (illisible : 0,21° < 0,35° depuis les vistas). Le monde actuel ne permet aucune
  révélation de repère hors de la carte (canopée de la carte + rebord de la cuvette).
- Masses à moins de 2,5 km du bord : **refusées** (run 1, V5 « bâche verte », V8 bord du carré souligné).
- Densité et ton des masses sur des versants surtout sud-ouest : A_TRANCHER (ECO-01 y attend des
  communautés plus sèches et plus claires).
- Aucune donnée de performance en jeu packagé ; coût GPU non mesuré par vue.
