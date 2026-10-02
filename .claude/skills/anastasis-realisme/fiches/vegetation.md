# Végétation : arbres, sous-bois, herbe

## Unreal

- **Instanciation** : le même mesh dessiné des milliers de fois en un seul appel. `ISM` instancie ;
  `HISM` (Hierarchical ISM) ajoute un arbre de culling et le choix du LOD par grappe. C'est la base de
  toute forêt temps réel.
- **LOD** : des versions de moins en moins détaillées, choisies selon la taille à l'écran
  (`screen size`). Au loin, un arbre ne doit garder que sa silhouette et sa masse d'ombre.
- **Nanite** : géométrie virtualisée, détail illimité sans LOD manuel. Il gère le feuillage depuis 5.7
  (Nanite Foliage), mais ce n'est pas un interrupteur : feuillage masqué, WPO de vent et ombres ont un
  coût propre sous Nanite, et Nanite impose les VSM.
- **Placement** : Foliage Tool (à la main), Procedural Foliage (simulation de semis dans un volume), PCG
  (graphes procéduraux), Landscape Grass (herbe générée par le matériau d'un Landscape).
- **Création** : SpeedTree (outil externe sous licence), Megaplants (Fab, Nanite), Procedural
  Vegetation Editor (PVE, **expérimental**, 5.7–5.8 : pousse les arbres dans l'éditeur).
- Ce qui rend une forêt crédible : variété d'âges et de tailles, groupes et clairières, réaction au relief
  et à l'eau, lisières, sous-bois, couleur du feuillage qui varie, translucidité des feuilles à
  contre-jour, et mouvement. Une forêt à espacement constant se lit en champ planté.

## ANÁSTASIS aujourd'hui

Pas de PCG, de Procedural Foliage, de Foliage Tool, de Landscape Grass, de SpeedTree ni de Megascans.

| Quoi | Mécanisme / valeur | Où |
|---|---|---|
| Arbres (assets) | GeometryScript : grille pontique + 7 essences × 3 formes (pin d'Alep, cyprès, chêne vert, olivier, platane, pin noir, sapin de Céphalonie) | `tools/unreal/create_tree_asset.py`, régénérés à chaque run |
| LOD des arbres | 3 LOD, screen size 1,0 / 0,22 / 0,055 ; 100 % / 50 % / enveloppe de couronne fermée | idem |
| Nanite | **off** : « le pipeline est HISM + LOD, et la direction artistique refuse une feature qui n'a pas gagné son existence » | idem |
| Placement | C++ déterministe (graine) : `AnastasisEcologicalDressing::Build` puis `AnastasisForestStructure` (âges, groupes, clairières) ; HISM transitoires, non sauvés dans la map | `FOREST_STRUCTURE_001.md` |
| Forêt | 16 120 arbres, 6 HISM, hauteurs 7,70 / 14,17 / 27,08 m (`FOREST_STRUCTURE_001`) ; au 2026-10-01, l'inventaire de `vegetation-cost-capture.ps1` compte 5 688 arbres en 266 composants | idem |
| Coût GPU (2026-10-01, RTX 3060, viewport éditeur 1280×720) | arbres 0,3 à **3,75 ms** (intérieur de forêt) ; herbe 0,1 à **3,75 ms** (vallée B) ; sous-bois non mesurable (< 0,4 ms, dans le bruit) ; toute la végétation 2,2 à 6,2 ms sur 10,3 à 14,2 ms par vue | `handoffs/forest-cost-001.md` |
| Vent | WPO sinusoïdal dans `M_AnastasisVegetation`, sans `WindDirectionalSource` | `handoffs/forest-canopy-wind-sway.md` |
| Herbe | `SM_Grass_*` + `M_AnastasisGrass` ; environ 988 000 touffes en 683 HISM sur des tuiles de 160 m ; proche 40 → 55 m avec ombres, lointain 70 → 105 m ; +1,5 à 2 ms | `GROUND_COVER_001.md`, `handoffs/ground-cover-001.md` |
| Sous-bois, rives | `AnastasisUnderstory` (maquis, ronces, roches), `AnastasisMicroEcology`, rive (`anastasis.Dressing.Riverbank`) | `MICRO_ECOLOGY_001.md`, `RIVERBANK_LIFE_001.md` |

CVars (1 par défaut) : `anastasis.Dressing.Ecology`, `.MacroForest`, `.TreeSpecies`, `.GroundCover`,
`.Understory`, `.MicroEcology`, `.Riverbank`. Leurs variantes `*.InAutomation` sont à 0 : un million de
touffes faisait tomber la suite de tests par manque de mémoire.

## Règles

- **VEG-01** — Toute végétation est instanciée en HISM par le C++, avec une graine. Pas d'acteur par
  arbre, pas de peinture à la main dans la map.
- **VEG-02** — Un asset de végétation se change dans son script d'autorité (`create_tree_asset.py`,
  `create-ground-cover.py`), jamais dans l'éditeur.
- **VEG-03** — Une HISM est **tuilée** : l'herbe en une HISM par tuile de 160 m. Six HISM à l'échelle de
  la carte coûtaient +12 ms dans toutes les vues.
- **VEG-04** — Le LOD lointain garde la masse : enveloppe de couronne fermée sous 5,5 % de hauteur
  d'écran, pour que la forêt garde sa couverture au loin.
- **VEG-05** — La structure prime sur le nombre : âges, groupes, clairières, lisières. La passe forêt
  enlève des arbres, elle n'en ajoute pas.
- **VEG-06** — Albédo du feuillage et de l'écorce dans le réel : feuillage sombre (aiguilles autour de
  0,04–0,09), écorces 0,05–0,11 (valeurs de `create_tree_asset.py`).
- **VEG-07** — Toute densité nouvelle se mesure en ms, vue par vue, avant d'être revendiquée.

## Vérifier

| Quoi | Comment |
|---|---|
| Stature, essences | `capture-tree-lineup.ps1` (`-Set species`) |
| Forêt vue du sol | `capture-forest-walk.py`, `capture-macro-forest.py` |
| Coût réel (triangles, LOD, instances soumises) | `measure-tree-cost.ps1` |
| Coût GPU par strate, vue par vue | `vegetation-cost-capture.ps1` (strates masquées à l'exécution, témoin `all2`) |
| Herbe | `capture-ground-cover.ps1 -States on,off,notint,noshadow,on2` (frame p50/p95 et GPU par vue) |

## Ne pas faire

- Activer Nanite sur le feuillage : feuillage en cartes masquées + WPO, incompatible avec un simple
  « activer Nanite » (`AAA_VISUAL_TARGET_LAB.md`). Nanite Foliage (RU-002-13) ne lève pas cet interdit :
  il anime par des os (plugin `DynamicWind`), pas par le WPO, donc il demande des arbres refaits, pas une
  case cochée.
- Une HISM pour toute la carte (+12 ms).
- Couper les ombres de l'herbe pour gagner : aucun gain mesurable.
- Passer `enable_recompute_normals=True` à la création d'un mesh : jette les normales écrites, sans
  erreur.

## Ouvert

- Coût GPU de la forêt : mesuré le 2026-10-01 (`forest-cost-001`), dans un viewport d'éditeur de
  1280×720 ; le jeu en 1080p plein écran reste à mesurer.
- Vent : estimé, pas mesuré, et pas de `WindDirectionalSource`.
- PVE (5.8) ou Nanite Foliage pour des arbres « héros » : pas de mandat. Faits à connaître avant d'en
  proposer un (RU-002-13, RU-002-14) : tout est expérimental ; `r.Nanite.Foliage` est en lecture seule
  (0 par défaut, réglage de projet et redémarrage) ; vent global seulement, pas de collision ; les assets
  PVE de 5.7 ne s'ouvrent pas en 5.8, ce qui heurte VEG-02 (assets régénérables par script). Un premier
  essai se ferait dans le labo isolé (`aaa-visual-lab.ps1`), jamais sur la carte du jeu.
- Essences méditerranéennes contre direction pontique : voir le skill, section 6.
