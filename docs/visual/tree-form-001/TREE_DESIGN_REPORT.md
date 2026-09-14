# TREE_DESIGN_REPORT — ANASTASIS_UNREAL_TREE_FORM_001

Mission : transformer les arbres de la scène — de cônes de proxy en silhouettes
conçues, avec hauteur, stature, masse et tronc. Périmètre : présentation uniquement.
Ni terrain, ni hydrologie, ni simulation, ni redistribution écologique.

Worktree `anastasis-tree-visuals-136553`, branche `claude/anastasis-tree-visuals-136553`,
base `main` à `1cbeef6`. `main` n'a pas bougé pendant la mission (vérifié au début et à la fin).

---

## 1. État initial découvert

Le pipeline réel, lu avant toute mutation :

```
AnastasisSim::GenerateWorld(seed, 96, 96)            vérité de simulation
  -> AnastasisWorldView::CaptureCanonicalWorld        adaptateur, aucune donnée nouvelle
  -> AnastasisEcologicalDressing::Build               OÙ pousse un arbre + sa strate
  -> AnastasisPresentationResolver                    À QUOI il ressemble + sa transform
  -> HISM par (archétype, variante)                   instanciation
  -> SM_Tree_Generic_01                               un seul mesh
```

Les arbres n'étaient pas des `StaticMeshActor` : ce sont des instances
`UHierarchicalInstancedStaticMeshComponent`, un composant par variante résolue. Le
placement était déjà déterministe, déjà écologique (lisière, clairières, pente,
humidité), et déjà stratifié en trois couches `Young / Secondary / Canopy`.

**Le défaut n'était donc pas le placement. C'était l'incarnation.** Les trois strates
étaient le MÊME mesh à trois échelles uniformes — exactement ce que la loi de mission
interdit. Et ce mesh unique, produit par `tools/unreal/create_tree_asset.py`, était un
cylindre de tronc (rayon 9) surmonté d'un cône (rayon de base 34) : vue de dessus à 32°,
la jupe du cône mangeait intégralement le tronc. La lecture finale était `GREEN_CONE`.

### Mesure, pas impression

Sonde de lecture seule sur l'incarnation réelle (seed 12345, monde 96×96, mode surface) :

| | avant |
|---|---|
| instances d'arbres | 438 |
| instances de ruines | 448 |
| hauteur d'arbre min / médiane / max | **49 / 104 / 237 uu** |
| hauteur de ruine min / médiane / max | 65 / 91 / 118 uu |
| silhouettes distinctes | **1** |

Une tuile de simulation vaut 100 uu. **L'arbre médian mesurait donc 1,04 m** — la hauteur
d'un mètre de terrain, et à peine plus que le moignon de ruine posé à côté. Ce n'était
pas une forêt : c'était un champ de marqueurs.

## 2. Fichiers / assets dont je prends la propriété

| Fichier | Rôle | Action |
|---|---|---|
| `tools/unreal/create_tree_asset.py` | autorité des meshes d'arbres | **réécrit** — produit désormais 6 silhouettes + 1 alias + le matériau |
| `tools/unreal/set_tree_grammar.py` | autorité du câblage FOREST | **créé** |
| `tools/unreal/capture-tree-lineup.{py,ps1}` | planche de stature (test anti-arnaque) | **créés** |
| `Content/Anastasis/Vegetation/SM_Tree_*.uasset` | 6 meshes + alias | **créés / régénéré** |
| `Content/Anastasis/Materials/M_AnastasisVegetation.uasset` | lecture des couleurs de sommet | **créé** |
| `Source/.../AnastasisPresentationRegistry.{h,cpp}` | données de présentation | axe de stature + biais + inclinaison |
| `Source/.../AnastasisPresentationResolver.{h,cpp}` | choix de variante et transform | sélection par stature, fail-open |
| `Source/.../AnastasisWorldEmbodiment.cpp` | pose des instances | strate → stature, journal de profil |
| `Source/.../AnastasisPresentationResolverTests.cpp` | tests | 3 tests ajoutés |

**Fichiers partagés touchés, et pourquoi** (diff concurrent inspecté avant) :

- `tools/unreal/set_presentation_meshes.py` — la ligne `FOREST` en est **retirée**. Cet
  outil ne sait écrire que `variants[0].mesh` ; le relancer sur Forest écraserait
  silencieusement la variante *understory* par le mesh générique et casserait la
  grammaire. Ruin, qui appartient à un autre chantier, est laissé intact.
- `tools/unreal/presentation-registry.py` — la graine `FOREST` pointait encore sur
  `/Engine/BasicShapes/Cone.Cone`. Un registre recréé de zéro repartait donc sur le cône
  même que cette mission retire. Corrigé pour Forest seulement.
- `tools/unreal/observe-slice.py` + `capture-slice.ps1` — **ajout additif** de deux
  variables d'environnement de caméra (`ANASTASIS_SLICE_CAM_LOC` / `_ROT`). Absentes,
  rien ne change et les captures scellées de WORLD_SLICE_006 restent reproductibles
  telles quelles. Elles existent parce qu'une preuve de silhouette se juge de près : à
  la distance de la caméra monde, un arbre fait dix pixels.

**Aucun fichier de `Source/AnastasisSim/` n'est touché. `AnastasisEcologicalDressing`
n'est pas touché** — la distribution écologique est le travail d'un autre agent
(`bb883e6`) et reste exactement ce qu'elle était.

## 3. Références utilisées

Autorité principale : `docs/visual/reference/pontique-etat-zero-3-stratification-forestiere.png`.
Sa coupe écologique nomme les strates, et c'est d'elle que vient le vocabulaire du code :

```
STRATE ÉMERGENTE   grands arbres, arbres anciens
CANOPÉE            feuillus matures / conifères dominants (sapins, épicéas, pins)
SOUS-CANOPÉE       jeunes arbres, arbres intermédiaires (érables, charmes)
STRATE ARBUSTIVE   jeunes semis
```

Sa checklist PCG demande explicitement « variation d'âge des arbres (jeunes, matures,
anciens, sénescents) » et « hiérarchie de densité et de strates ».

Complément : `docs/visual/P1_6_PONTIC_BYZANTINE_ART_DIRECTION.md` — « la forêt n'est pas
un tampon d'arbre répété », « lire l'échelle par la hauteur des arbres », palette retenue,
pas de compensation par bloom ou fog.

## 4. Échelle du monde constatée

```
1 tuile              = 100 uu  (gelé P1.5, "Lot 4.5 ne choisit pas d'échelle joueur")
monde canonique      = 96 x 96 tuiles = 9600 uu ≈ 96 m
niveau de la mer     = 275 uu
ruine (mesurée)      = 65–118 uu, médiane 91
adulte (repère)      = 180 uu
arbre AVANT (mesuré) = 49–237 uu, médiane 104
```

### La tension, énoncée plutôt que masquée

Si l'on prend `1 tuile = 1 m` au pied de la lettre, un conifère pontique mature mesure
10 à 25 m, soit **10 à 25 tuiles**. Sa couronne couvrirait alors 4 à 6 tuiles, alors que
l'espacement minimal du dressing écologique est de 0,55 tuile. Atteindre la hauteur
littérale exigerait de diviser la densité forestière par un ordre de grandeur — c'est-à-dire
**redessiner la distribution écologique, ce que cette mission s'interdit explicitement**.

Choix retenu, et c'en est un : porter la stature au maximum que la distribution scellée
peut accueillir sans perdre la lisibilité macro, et **traiter les RAPPORTS entre classes
comme la vraie livraison**. La hauteur absolue littérale est documentée en NEXT TARGET,
pas bricolée ici.

## 5. Familles créées

Six silhouettes construites, une par case de la grille (strate × conifère/feuillu) que la
planche de référence autorise. Chaque mesh est normalisé sur Z = [−50, +50] : **seules les
proportions INTERNES changent d'une stature à l'autre**, jamais une simple mise à l'échelle.

| Mesh | Strate | Fût dégagé | Largeur/hauteur | Signature |
|---|---|---|---|---|
| `SM_Tree_Conifer_Understory_01` | arbustive | 22 % | **0,27** | flèche fine, feuillue jusqu'en bas |
| `SM_Tree_Conifer_Subcanopy_01` | sous-canopée | 30 % | **0,35** | 3 étages, tronc lisible |
| `SM_Tree_Broadleaf_Subcanopy_01` | sous-canopée | 34 % | **0,47** | fourche + petit dôme asymétrique |
| `SM_Tree_Conifer_Canopy_01` | canopée | 44 % | **0,39** | épicéa d'Orient, 4 étages à décrochement |
| `SM_Tree_Broadleaf_Canopy_01` | canopée | 42 % | **0,57** | hêtre, 3 branches maîtresses, large dôme |
| `SM_Tree_Conifer_Emergent_01` | émergente | 54 % | **0,44** | fût massif, cime **émoussée** (sénescence) |

`SM_Tree_Generic_01` survit comme alias géométrique de la silhouette de canopée : c'est le
chemin que code en dur le repli de `AnastasisPresentationRegistry.cpp`, et il devait rester
valide sans devenir un septième dessin.

Deux règles de dessin portent tout le reste :

- **Décrochement d'étage.** Chaque tronçon de couronne *repart plus large* que ne finit
  celui du dessous. C'est ce décrochement, pas le chevauchement, qui fait une jupe visible
  à moyenne distance. La première itération chevauchait sans décrocher : les étages se
  fondaient et la silhouette redevenait un cône. Corrigé et re-capturé.
- **Six à sept lobes pour un feuillu.** Trois gros lobes se lisent comme trois boules
  empilées — un brocoli, pas un hêtre. Au-delà de cinq, les masses fusionnent et seul le
  bord reste irrégulier, ce qui est exactement l'effet cherché.

Couleur : chaque sous-partie porte une couleur de sommet (écorce / aiguilles / feuillage)
et `M_AnastasisVegetation` la lit — même langage que `M_AnastasisSlice` pour le sol, la
couleur EST la sémantique. Sans ce matériau, la teinte unique de l'archétype peindrait
aussi le tronc en vert et le tronc cesserait d'exister. L'écorce a été assombrie après
lecture de la planche anti-arnaque : sous le soleil neutre du banc, un linéaire de 0,13
remonte à ~0,40 en sRGB, c'est-à-dire du beige.

## 6. Hauteurs retenues — mesurées dans le monde

`hauteur = 100 uu × enveloppe d'entrée × facteur de strate × biais de variante`

Enveloppe portée de `1.6–2.4` à `3.6–5.0`. Les facteurs de strate appartiennent à
`FAnastasisForestDressingSettings` et **ne sont pas touchés**.

| | avant | après |
|---|---|---|
| hauteur min | 49 uu (0,5 m) | **107 uu (1,07 m)** |
| hauteur max | 237 uu (2,4 m) | **668 uu (6,7 m)** |
| amplitude | ×4,8 | **×6,2** |
| silhouettes | 1 | **6** |
| classes de stature | 3 (échelles du même mesh) | **4 (meshes distincts)** |
| instances | 886 | **886 — inchangé** |

Profil journalisé à l'incarnation, mesuré et non affirmé :

```
ANASTASIS_TREE_STATURE understory=180 subcanopy=129 canopy=103 emergent=26 height_uu=[107,668]
ANASTASIS_ECOLOGY      young=180 secondary=129 canopy=129 full_plan=438   (identique à avant)
```

La forme de la distribution est celle qu'une forêt réelle a : beaucoup de semis, une
majorité d'arbres moyens, un peuplement dominant, et **26 émergents seulement** qui donnent
sa ligne de ciel au couvert.

## 7. Méthode de variation

Trois axes, tous déterministes, tous pilotables par la donnée :

1. **Stature → mesh.** La strate écologique décidée par `AnastasisEcologicalDressing`
   devient une demande de stature. Le résolveur ne sert que les variantes construites pour
   elle. La quatrième strate — les émergents — n'est pas une décision écologique nouvelle :
   c'est la queue la plus âgée de la canopée, séparée **en présentation** par le même hash
   déterministe, à hauteur de 18 %. Aucun arbre ne se déplace.
2. **Biais d'échelle par variante.** Un émergent est plus haut qu'un arbre de canopée de
   la même espèce (×1,35) ; un feuillu est plus bas qu'une flèche de conifère (×0,85–0,92).
   Un seul archétype couvre ainsi plus de hauteur qu'une paire Min/Max ne sait exprimer.
3. **Inclinaison.** Enveloppe de 5° par entrée, tirée puis **élevée au carré** : la plupart
   des instances restent d'aplomb et quelques-unes penchent franchement. Un tirage uniforme
   donnerait à tout un peuplement la même ivresse moyenne — du bruit, pas des individus.
   `MaxLeanDegrees = 0` (la valeur par défaut, celle de Ruin) reproduit la transform
   historique **exactement** : un test le vérifie à 1e-6.

Tout est reproductible : même seed, même monde ⇒ même arbre au même endroit, à chaque
lancement.

## 8. Intégration Unreal

L'architecture n'a **pas** été réécrite. HISM reste le motif ; il y a maintenant un
composant par variante réellement utilisée, ce que le code faisait déjà par conception
(`Dressing_<Archetype>_v<N>`). Pas de nouvel Actor, pas de PCG, pas de Nanite, pas de
nouveau framework végétal.

`ANASTASIS_ECOLOGY_COST enabled=1 generation_ms=19–35 components=7 instances=886`

La contrainte structurante, tenue et verrouillée : **deux formules de lift coexistent** dans
`AnastasisWorldEmbodiment` — le chemin hérité utilise `0.5 × EngineBasicShapeSize × Scale`,
le chemin écologique `−MeshBounds.Min.Z × Scale`. Elles ne coïncident que si chaque mesh
couvre Z = [−50, +50]. Un mesh reconstruit hors convention ferait flotter ou enfoncer les
arbres, **et dans un seul des deux chemins**. Le générateur vérifie donc la normalisation au
lieu de la supposer (il échoue au-delà de 0,01 uu), et un nouveau test la verrouille côté
moteur.

La donnée fait autorité, pas le code : `ANASTASIS_PRESENTATION_REGISTRY source=asset entries=2`.
Le repli code dit la même chose, pour qu'un checkout sans data asset rende la même grammaire.

## 9. Tests / build

```
BUILD::PASS                       Anastasis_UnrealV2Editor Win64 Development
PASS                   : 58
KNOWN_EXPECTED_FAILURE : 4        (les 4 du registre, aucune nouvelle)
FAIL                   : 0
TOTAL                  : 62
```

Trois tests ajoutés :

- `Anastasis.Presentation.Stature` — une stature étiquetée atteint sa propre variante sur
  chaque tuile ; le choix est reproductible ; une demande `Any` couvre encore plusieurs
  looks ; **fail-open** : une donnée sans look pour la stature demandée dessine quand même
  l'archétype (la présence est vérité de simulation, la stature n'est qu'un habillage) ;
  une variante non étiquetée sert toutes les demandes, donc la donnée écrite avant cet axe
  rend à l'identique ; le lift suit le biais d'échelle.
- `Anastasis.Presentation.Lean` — `MaxLeanDegrees=0` reste d'aplomb à 1e-6
  (`worst_plumb=0.000000000`), une enveloppe incline réellement, et aucune instance ne
  dépasse l'enveloppe déclarée (`worst_lean=4.976`).
- `Anastasis.Presentation.TreePivotConvention` — les 6 variantes chargent et couvrent
  Z = [−50, +50] (`TREE_PIVOT variants_checked=6`).

Et surtout, l'invariant scellé qui aurait pu casser en silence tient exactement :

```
DRESSING_ON_GROUND surface=1162 slice=126 slab=1178 ground_error=0.000000000 slab_error=0.000000000
```

## 10. Captures avant / après

Toutes au même banc (`Lvl_AnastasisSlice` : soleil 75000 lux, EV100 figé à 14), seed 12345,
monde 96×96, mode surface. Aucun fog, aucun coucher de soleil, aucun étalonnage.

| Fichier | Caméra | Contenu |
|---|---|---|
| `A_close_before.png` | `(3050,4250,1100)` pitch −15 yaw 45 | **AVANT** — cônes identiques, aucun tronc |
| `C_close_after.png` | **identique** | **APRÈS** — même monde, même caméra |
| `B_stature_board.png` | banc dédié, arc | les 6 silhouettes + ruine 0,9 m + repère 1,8 m |
| `A_before_cone.png` | caméra monde scellée | avant, lecture macro |
| `C_wide_after.png` | **identique** | après, lecture macro |

La planche `B` place ses sujets sur un **arc centré sur la caméra**, pas en rang. En rang,
les sujets des extrémités sont plus loin que celui du milieu et la perspective les
rapetisse — à 1250 uu de recul pour un rang de 2100, le sujet du bord perd 23 % de sa
taille apparente. La planche aurait alors menti sur exactement ce qu'elle prétend montrer.

## 11. Fichiers modifiés

```
M  Content/Anastasis/Presentation/DA_AnastasisPresentation.uasset
M  Content/Anastasis/Vegetation/SM_Tree_Generic_01.uasset
A  Content/Anastasis/Materials/M_AnastasisVegetation.uasset
A  Content/Anastasis/Vegetation/SM_Tree_Broadleaf_Canopy_01.uasset
A  Content/Anastasis/Vegetation/SM_Tree_Broadleaf_Subcanopy_01.uasset
A  Content/Anastasis/Vegetation/SM_Tree_Conifer_Canopy_01.uasset
A  Content/Anastasis/Vegetation/SM_Tree_Conifer_Emergent_01.uasset
A  Content/Anastasis/Vegetation/SM_Tree_Conifer_Subcanopy_01.uasset
A  Content/Anastasis/Vegetation/SM_Tree_Conifer_Understory_01.uasset
M  Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationRegistry.{h,cpp}
M  Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationResolver.{h,cpp}
M  Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationResolverTests.cpp
M  Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp
M  tools/unreal/create_tree_asset.py
A  tools/unreal/set_tree_grammar.py
A  tools/unreal/capture-tree-lineup.{py,ps1}
M  tools/unreal/capture-slice.ps1
M  tools/unreal/observe-slice.py
M  tools/unreal/presentation-registry.py
M  tools/unreal/set_presentation_meshes.py
A  docs/visual/tree-form-001/  (rapport + 5 captures)
```

## 12. Limites restantes

Constatées, pas corrigées — elles sortent du mandat ou méritent leur propre passe.

1. **La hauteur littérale n'est pas atteinte.** 6,7 m au maximum contre les 10–25 m d'un
   conifère pontique réel. Bloqué par la densité de la distribution scellée (§4).
2. **Pas de LOD.** `create_new_static_mesh_asset_from_mesh` ne produit que le LOD0. À 438
   instances c'est sans effet ; à la densité d'une vraie forêt, il en faudra.
3. **Collision NDOP10 sur l'arbre entier**, héritée de l'asset précédent. Sur un arbre haut,
   le volume est gros ; seul le fût devrait bloquer. Comportement inchangé, donc pas une
   régression — mais c'est désormais plus visible.
4. **Aucune micro-texture.** Voulu : la mission portait sur la silhouette. De près, l'écorce
   et le feuillage sont des aplats de couleur de sommet.
5. **Le mélange d'espèces n'est pas écologique.** Conifère ou feuillu sort du hash de
   variante. La planche de référence demande un mélange « selon l'humidité, l'altitude et
   l'exposition » — et `FVisualTile` porte déjà `Wetness`, `Alt` et `Shade`. C'est faisable
   sans toucher la simulation, mais c'était une seconde mission.
6. **La graine `RUIN` de `presentation-registry.py` pointe encore sur
   `/Engine/BasicShapes/Cylinder.Cylinder`** alors que l'asset réel porte
   `SM_Ruin_Generic_01`. Dérive préexistante, chantier d'un autre agent : signalée, pas
   corrigée.
7. **Le sol reste pâle et lavé** sous le soleil du banc, ce qui affaiblit le contraste des
   troncs. `M_AnastasisSlice` appartient au chantier matériaux en cours.

## 13. NEXT TARGET recommandé — non exécuté

**Le mélange d'espèces piloté par l'écologie.** C'est la suite la moins chère et la plus
rentable : `Wetness`, `Alt` et `Shade` existent déjà sur `FVisualTile`, la grammaire a déjà
ses deux familles, et le résolveur a déjà un axe de filtrage. Un feuillu en fond de vallon
humide et un conifère sur la croupe sèche feraient lire la topographie *à travers* la forêt
— sans déplacer un seul arbre, sans toucher `AnastasisSim`.

Candidat suivant, symétrique et de même maturité : **`Stone` → rochers**. Le type de tuile
existe, il n'a aucun dressing, et l'ASSET_MAP le désigne déjà comme le successeur naturel.

---

### Verdict

```
[x] les cônes ne sont plus la silhouette finale        6 silhouettes construites
[x] le tronc est perceptible                           fût dégagé de 22 % à 54 % selon l'âge
[x] plusieurs niveaux de stature                       4 classes, meshes distincts
[x] hauteur cohérente avec l'échelle du monde          107–668 uu contre 91 uu de ruine
[x] les arbres ne paraissent pas clonés                6 meshes x biais x inclinaison, déterministes
[x] lecture claire à distance                          C_wide_after.png, caméra scellée
[x] résultat stylisé                                   pas de photoréalisme, pas de Nanite
[x] aucune refonte terrain                             AnastasisTerrainSurface intact
[x] aucune refonte simulation                          Source/AnastasisSim/ intact
[x] aucun travail concurrent écrasé                    main immobile, dressing écologique intact
[x] le projet compile                                  BUILD::PASS
[x] le résultat existe dans Unreal                     886 instances, source=asset
[x] preuve visuelle finale                             A/B/C au même banc
```
