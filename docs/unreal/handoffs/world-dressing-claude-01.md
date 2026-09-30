# HANDOFF: world-dressing-claude-01

## MISSION

WORLD DRESSING 01 — passe créative : faire émerger des lieux reconnaissables de la géographie
existante (graine 12345, Human_Geography_V2, échelle 5 = 1,9 km), avec les assets déjà présents
dans le dépôt. Sans toucher à la topographie, au simulateur PNJ ni au village.

Worktree `C:\dev\ANASTASIS_WORKTREES\world-dressing-claude-01` (branche `agent/world-dressing-claude-01`,
base `main` 50a66d8). Le nom `world-dressing-01` a d'abord été créé pour cette mission, puis un autre
agent (git `Codex`) y a travaillé en même temps (avance rapide sur `main` à 19:02, `AnastasisSiteDressing.*`
non commités). Ce worktree lui a été laissé tel quel : **deux propositions concurrentes de la même
mission existent**. Il contient aussi les 60 assets importés ci-dessous, en index, non commités.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisPlaces.{h,cpp}` : planificateur pur + incarnation HISM
- `Source/Anastasis_UnrealV2/WorldView/AnastasisPlacesTests.cpp`
- `tools/unreal/capture-places.ps1` + `tools/unreal/places-capture.py` (indexés dans `AGENTS.md`)
- `docs/visual/world-dressing-01/` (15 captures, LFS)
- Assets **importés tels quels** depuis des branches jamais intégrées (blobs identiques, aucun code de
  ces branches) : `Content/Anastasis/Ecotone/*` + `M_AnastasisStone` (`agent/ecotone-forge-001` fb68398),
  `Content/Anastasis/Lithos/*` + `M_AnastasisLithos` (`agent/lithos_forge_001` 55bfd02),
  `Content/Anastasis/Rock/*` et les 18 `Architecture/SM_Ruin_{Angle,Enclos,Foyer,Mur,Reemploi,Soubassement}_0N`
  (`claude/anastasis-rock-grammar-e67075` fc81d57). `SM_Ruin_Generic_01` non touché (blob identique).
  Recettes de fabrication : sur ces branches (`create_ecotone_assets.py`, `create_lithos_asset.py`,
  `create_rock_assets.py`, `create_ruin_grammar.py`), non copiées.
- Touché hors de mes fichiers :
  - `AnastasisWorldEmbodiment.{h,cpp}` : CVar `anastasis.Dressing.Places`, `bComposePlaces`, `GetPlaceReport`,
    composition avant le dressing par tuile.
  - Deux tests d'autrui, une ligne chacun :
    - `AnastasisTerrainSurfaceTests.cpp` : `DressingOnGround` coupe les lieux comme il coupait déjà la forêt.
    - `AnastasisHumanGeographyTests.cpp` : les composants `Place_*` sont exclus du garde « ×20 ». Leur
      échelle est une taille en mètres sur des meshes de 1 m, pas `SpatialScale`.

## COMMIT

Le commit qui porte cette fiche sur `agent/world-dressing-claude-01`. Non intégré, non poussé.

## MEC

- BUILD : `BUILD::PASS` (UE 5.8.2 / CL 56702186, Editor Win64 Development), `tools\unreal\anastasis-unreal.ps1 build`.
- TESTS, suite complète `tools\unreal\report-tests.ps1` : **PASS 111, KNOWN_EXPECTED_FAILURE 4** (les 4 du
  registre), **FAIL 0**, 115/115 annoncés.
- Nouveaux tests (4 PASS) :
  - `Anastasis.Places.CanonicalDeterminismAndReserve` : deux plans identiques pièce par pièce ; les 8 lieux
    nommés trouvés ; toute pièce au sec, hors vallée (> 0,3) et hors bassin (10 tuiles), le col seul jusqu'à
    0,8 ; seuls hameau et vestiges remplacent des tuiles ; snapshot non muté.
  - `Anastasis.Places.WithoutHumanGeography` : sans géographie humaine, source, col, marais et chêne sont
    déclarés absents et non inventés ; guet, hameau, hautes pierres et vieille forêt restent.
  - `Anastasis.Places.RejectsInvalidInput`
  - `Anastasis.Places.MeshesResolve` : chaque famille × variante se charge et a un volume.
- Éditeur réel (log `ANASTASIS_PLACES`) : `places=20 pieces=859 instances=859 components=63 ungrounded=0
  missing_meshes=0 superseded_ruin_tiles=74`. Seul `bois_5` est vide (reporté, pas inventé).

## SCN

Captures A/B (lieux actifs puis coupés, mêmes caméras, même session) : `tools\unreal\capture-places.ps1 -Label final`
→ `Saved/PlacesEvidence/final/` (27 vues × 2 états), sélection dans `docs/visual/world-dressing-01/`.

| Lieu | Tuile | Lecture |
|---|---|---|
| **Le Guet** | (67.8, 12.8) | tor de blocs ~20 m sur le point haut de la forge + tour de guet effondrée côté vallée. Lisible de loin. |
| **La Source aux Pierres** | (30.1, 75.9) | jusqu'à 11 pierres levées (2 couchées ; celles qui tomberaient dans l'eau ne sont pas posées) autour de l'étang d'où naît la rivière, vieil arbre de 17 m en amont, roselière. Le lieu étrange. |
| **Le Col** | (42.8, 32.8) | selle entre les deux vallées : deux pierres de ~10 m à la lèvre du passage, cairn, blocs sur les épaules, pin penché. Le fond reste libre (future route). |
| **Les Hautes Pierres** | (80.5, 6.5) | chaos de tors de blocs arrondis au cœur le plus dense de la pierre (pas l'anneau de bordure). Presque impraticable. |
| **L'Ancien Hameau** | (15.5, 71.5) | maisons ruinées orientées sur la pente, murets d'enclos, pierres de remploi, buissons et un arbre dans une maison ; remplace 74 moignons génériques. |
| **Le Marais** | (63.7, 64.9) | embouchure trouvée en descendant la rivière depuis la source : roseaux, bois flotté, souches. Se lit au sol, **pas de loin**. |
| **Le Vieux Chêne** | (31.5, 36.5) | chêne de 21 m, seul, au rebord de la vallée ; rien autour, volontairement. |
| **La Vieille Forêt** | (76.9, 18.3) | vétérans de 15–19 m au cœur du massif, bois mort au sol, lisière de buissons. |

Secondaires : 5 affleurements, 3 vestiges, 4 bois (quelques pièces).

## PLY

UNKNOWN. Collision sur rochers, ruines, troncs, souches (`BlockAll`, comme le dressing existant) ;
aucune contribution à la navigation. Coût : 859 instances en 63 HISM, non mesuré en GPU.

## INTEGRATION_RISK

- `AnastasisWorldEmbodiment.{h,cpp}` est chaud : l'agent concurrent y a aussi un diff non commité.
- Source, col, marais et chêne lisent `AnastasisHumanGeography::Evaluate`. Si Human_Geography_V2 bouge,
  ils bougent avec elle ; sans elle, ils se déclarent absents (testé).
- Les scripts d'intégration des branches `ecotone`, `lithos` et `rock-grammar` réimporteront les mêmes
  blobs : aucun conflit binaire tant que ces assets ne sont pas refabriqués.
- Rock et ruines n'ont pas de couleur de sommet : teinte via `BasicShapeMaterial` (comme Ruin dans le
  registre). Lithos agrandi lit comme des caisses : cantonné aux éboulis.
- Les moignons `SM_Ruin_Generic_01` restent hors du hameau et des vestiges (388 tuiles).

## STOP

Pas de modification du relief, de l'eau, de la simulation, des PNJ ni du village ; aucune vallée ni le bassin
habité n'est occupé. Pas de nouvel asset fabriqué. Pas de verdict artistique final, pas de mesure de
performance, pas de PLAYER, pas d'intégration dans `main`.
