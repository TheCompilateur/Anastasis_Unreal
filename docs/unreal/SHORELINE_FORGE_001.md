# SHORELINE_FORGE_001 — la rencontre de l'eau et de la terre

Mission `anastasis-shoreline-grammar`, branchée sur `main` à `1cbeef6`. Additive.
Aucun fichier de `Source/AnastasisSim/` n'est ouvert en écriture.

## Gate 0 — ce que l'ordre de mission suppose, et ce que le projet contient

L'ordre de mission prescrit huit outils Unreal par ordre de priorité : Water
System, Landscape Edit Layers, Landscape Patch System, Landscape Materials,
RVT, PCG Framework, Foliage Mode, Decals.

**Aucun des huit n'existe dans ce projet.** Ce n'est pas une opinion, c'est une
lecture :

| Outil prescrit | État réel | Vérification |
|---|---|---|
| Water System | plugin absent | `Anastasis_UnrealV2.uproject` — aucun `Water` dans `Plugins` |
| Landscape Edit Layers | aucun `ALandscape` | aucune occurrence de `Landscape` dans `Source/` ni `Config/` |
| Landscape Patch System | plugin absent | idem |
| Landscape Materials / Material Layers | sans objet | il n'y a pas de Landscape à matérialiser |
| RVT | aucune `URuntimeVirtualTexture` | aucune occurrence dans `Source/`, `Config/`, `Content/` |
| PCG Framework | plugin absent | `.uproject` — aucun `PCG` |
| Foliage Mode | aucun `InstancedFoliageActor` | le dressing passe par des HISM pilotés en C++ (`AnastasisEcologicalDressing`) |
| Decals | aucun | — |

Le monde d'ANÁSTASIS n'est pas un Landscape peuplé à la main. C'est une
**projection du simulateur**, rebâtie à chaque incarnation :

```
AnastasisWorld::GenerateWorld(seed, 96, 96)     tuiles : Type, Alt, Shore, Wetness, FlowAmt
      -> AnastasisWorldView::CaptureSnapshot     snapshot sémantique
      -> AnastasisTerrainSurface::Build          UProceduralMeshComponent
             section 0 = relief      (couleur de sommet + UV morphologiques)
             section 1 = nappe d'eau (plate, à AnastasisWorld::SeaLevel)
      -> AAnastasisWorldEmbodiment               + HISM de dressing
```

Il n'y a donc pas de « berge » à sculpter avec un Patch, ni de couche d'édition
où isoler une retouche : **la berge est un résultat calculé.** La mission garde
tout son sens — le contact eau/terre est bien trop binaire — mais elle se joue
là où ce contact est produit, pas dans des outils qui ne sont pas montés.

`ShoreDepthSpan`, `FShorelineVertex` et `M_AnastasisShoreWater` sont donc les
équivalents fonctionnels, dans cette architecture, de ce que l'ordre de mission
appelait Landscape Material Layers et Water System.

## SHORELINE_OWNER_MAP

Relevé à `1cbeef6`, avec `git worktree list` et les diffs de chaque branche
d'agent vivante contre `main`.

| Surface | Propriétaire | État |
|---|---|---|
| `CURRENT_WATER_SYSTEM_OWNER` | aucun — pas de Water plugin. La nappe d'eau est `FGeometry::Water*` dans `AnastasisTerrainSurface` | **libre, repris par cette mission** |
| `CURRENT_LANDSCAPE_OWNER` | aucun — pas de Landscape | sans objet |
| `CURRENT_GROUND_MATERIAL_OWNER` | `claude/anastasis-ground-materials-727cc2` (GROUND_SURFACE_001, commit `620a35a`, **non intégré à `main`**) | **occupé — non touché** |
| `CURRENT_SHORE_MATERIAL_OWNER` côté TERRE | même agent : `ShoreSand`, le lerp `Shore` dans `TileColor`, `M_/MI_AnastasisGround` | **occupé — non touché** |
| `CURRENT_SHORE_MATERIAL_OWNER` côté EAU | personne | **libre, repris par cette mission** |
| `CURRENT_PCG_OWNER` | aucun — pas de PCG | sans objet |
| `CURRENT_RVT_OWNER` | aucun — pas de RVT | sans objet |
| `CURRENT_HYDROLOGY_ASSETS` | `AnastasisSim` (`AnastasisHydrology`, `AnastasisWorld`) — **lecture seule pour tout agent de présentation** | lu, jamais écrit |
| `CURRENT_LANDSCAPE_LAYERS` | aucune | sans objet |

Agents concurrents relevés, et ce qu'ils touchent :

| Branche | Fichiers partagés avec cette mission |
|---|---|
| `claude/anastasis-ground-materials-727cc2` | `AnastasisTerrainSurface.{h,cpp}`, `AnastasisTerrainSurfaceTests.cpp`, `AnastasisWorldEmbodiment.{h,cpp}` |
| `claude/anastasis-tree-visuals-136553` | `AnastasisWorldEmbodiment.cpp` (dressing) |
| `claude/anastasis-rock-grammar-e67075` | rien encore (branche au niveau de `main`) |
| `agent/multi-agent-control-001`, `agent/astral-env-001` | aucun fichier commun |

### Le recouvrement, et pourquoi il n'a pas déclenché un STOP

GROUND_SURFACE_001 écrit dans les mêmes fichiers. La condition d'arrêt « un
autre agent modifie les mêmes matériaux » a donc été examinée ligne à ligne
avant toute écriture — et c'est cet agent lui-même qui a tracé la frontière.
Son diff sépare explicitement les deux sections du maillage et écrit :

> « Section 0 = le sol, section 1 = la nappe d'eau. Elles ne portent PLUS le
> même matériau […] **l'eau n'est pas de cette mission.** »

Il a aussi justifié de ne pas exporter `Shore` : « c'est le même champ de
distance à l'eau que `Wetness` […] il est déjà peint dans la couleur de sommet ».

La frontière est donc nette, et elle est la sienne :

```
SECTION 0  relief, albédo, familles de surface, humidité    GROUND_SURFACE_001
SECTION 1  nappe d'eau, canaux de rive, matériau d'eau      SHORELINE_FORGE_001
```

**Rien de la section 0 n'est modifié ici** : ni `SurfaceTypeColor`, ni
`TileColor`, ni `ShoreSand`, ni `FSurfaceMix`, ni `UV0`/`UV1` du relief, ni
`M_AnastasisGround`. La seule fonction que les deux missions modifient est
`AAnastasisWorldEmbodiment::EmbodyCrop`, et elles y écrivent des lignes
différentes pour des sections différentes.

**Ordre d'intégration recommandé : GROUND_SURFACE_001 d'abord, puis celle-ci.**
Le conflit textuel attendu porte sur un seul bloc — l'appel
`CreateMeshSection_LinearColor` de la section 1 et l'affectation des matériaux —
et la résolution est additive : garder le sol de GROUND_SURFACE_001 pour la
section 0, garder `ResolveWaterMaterial()` de cette mission pour la section 1.
Les deux branches déclarent la **même méthode** `ResolveWaterMaterial()` ; la
leur renvoie le matériau de tranche, la nôtre `M_AnastasisShoreWater`. C'est
délibéré : les deux versions se rejoignent au lieu de se heurter.

## Gate 1 — le défaut réellement observé

Le défaut ne se déduit pas du code seul ; il se voit dans les preuves déjà
versées au dépôt, prises avant cette mission :

- `docs/visual/slice-006/B_slice_surface.png` — la tranche scellée 32×32. La
  baie du quart sud-ouest rencontre la terre sur **un trait net**, cyan contre
  crème, sans aucune épaisseur.
- `docs/visual/terrain-extent/C_world_surface.png` — le monde entier vu du ciel.
  Chaque plan d'eau est une tache cyan **détourée**.

Diagnostic, en reprenant le vocabulaire de l'ordre de mission — et **seulement**
les défauts qu'on voit vraiment :

| Code | Constat |
|---|---|
| `TOO_BINARY_TRANSITION` | oui — deux matières, aucune troisième |
| `NO_WET_MARGIN` | oui — aucune bande intermédiaire |
| `NO_MATERIAL_GRADATION` | côté eau : oui. Côté terre : **non**, `TileColor` dégrade déjà vers `ShoreSand` sur ~5 tuiles, et cette gradation appartient à GROUND_SURFACE_001 |
| `TOO_SHARP_EDGE` | oui, et la cause est précise : la nappe est opaque, donc son arête est visible telle quelle |
| `UNNATURAL_BANK_SLOPE` | **non constaté** — la pente vient du simulateur et le trait de côte est déjà l'intersection continue d'un plan avec un relief triangulé, pas un escalier de tuiles |
| `NO_ECOLOGICAL_BAND` | vrai, mais hors périmètre : le dressing appartient à `AnastasisEcologicalDressing` et à la mission arbres |

La cause racine tient en une phrase : **la nappe d'eau jetait la seule grandeur
qui décrit une berge.** Elle était peinte d'une couleur unique
`(0.043, 0.176, 0.290, 1)` — la profondeur d'eau au-dessus de chaque sommet,
qui est pourtant déjà dans la géométrie, n'était lue par personne.

## Gate 2 — SHORELINE_GENOME

Trois nombres par sommet de nappe, et rien de plus. Chacun est **dérivé** de ce
que le monde contient déjà ; aucun n'est une simulation nouvelle.

```
FShorelineVertex
{
    Depth      (WaterPlaneZ - Z_relief) / ShoreDepthSpan,  [0,1]
    Flatness   normale Z du relief au meme sommet,          [0,1]
    Flow       FlowAmt de la tuile source,                  [0,1]
}
```

| Variable | Origine | Effet visible |
|---|---|---|
| `Depth` | le relief déjà bâti et `AnastasisWorld::SeaLevel` | la gradation marge → eau peu profonde → eau franche, et l'effacement de l'arête |
| `Flatness` | la normale déjà accumulée par `Build` | la **largeur** de la marge et de l'ourlet : plate = étalée, abrupte = nette |
| `Flow` | `AnastasisHydrology::StampWaterFlowFields` | limon dormant ↔ graviers lavés d'un chenal, et un ourlet renforcé |

`ShoreDepthSpan` est le seul réglage d'échelle, et il a été **mesuré, pas
choisi** — après s'être trompé une fois, ce qui vaut d'être écrit.

La première valeur, 120 uu, venait d'un raisonnement correct : le pas de tuile
fait 100 uu, `AltitudeScale` vaut 1000, donc 120 uu est la marche qu'une berge de
pente ≈ 1:1 franchit en une tuile. Le raisonnement était juste et la valeur était
fausse. Le test a mesuré le monde canonique : **sa fosse la plus profonde fait
91 uu.** Avec un span de 120, aucun sommet du monde n'atteignait jamais l'eau
franche — tout était marge, donc plus rien n'était une marge. Le test a échoué
exactement là-dessus (`l'eau franche existe aussi`).

`ShoreDepthSpan = 60 uu`, les deux tiers de la profondeur maximale relevée : la
nappe atteint son état plein bien avant le point le plus creux du monde, donc
« eau franche » est un état réellement occupé. La marge occupe 0.16 à 0.55 de ce
span selon `Flatness`, soit les 10 à 33 premiers uu d'eau. Sa largeur **au sol**
n'est réglée nulle part : elle sort de la pente, une berge douce étalant la même
tranche de profondeur sur beaucoup plus de terrain.

Le marqueur `TERRAIN_SHORELINE_DEPTHS` sort désormais les déciles de profondeur à
chaque exécution : changer cette constante sans les relire, c'est refaire
l'erreur de 120.

Ce qui a été refusé au génome, et pourquoi :
- `Shore` et `Wetness` — déjà lus par GROUND_SURFACE_001, côté terre.
- `FlowX`/`FlowZ` (direction du courant) — aucune matière ne s'en sert
  aujourd'hui ; ce serait un canal sans effet.
- Une largeur de marge réglable par l'artiste — elle sort de la pente. Un
  réglage de plus aurait permis de contredire la topographie.

## Gate 4 — la forme d'abord, et ce que la forme ne pouvait pas être

`FORM_FIRST` a été pris au sérieux, et c'est ce qui a dicté le périmètre.

La forme de la berge **ne pouvait pas** bouger :
- le relief est scellé par `TERRAIN_CONTRACT` (1024 sommets, 1922 triangles,
  `max_error = 0.000000000`) ;
- `AnastasisTerrainSurface::SampleHeight` doit rester d'accord avec `Build`,
  sinon tout ce qui est posé au sol flotte ou s'enfonce ;
- le test scellé `Anastasis.Terrain.Semantics` exige que chaque sommet de nappe
  reste **exactement** à `WaterPlaneZ` et aligné en XY sur le relief.

Déplacer un sommet aurait donc cassé le sceau `WORLD_SLICE_006`, c'est-à-dire la
condition d'arrêt `WORLD_STRUCTURE_MUST_SURVIVE`. **Aucun sommet n'a bougé.**

Mais le constat de la Gate 1 est que la forme n'était pas le défaut : le trait de
côte est déjà l'intersection continue d'un plan avec un relief triangulé. Ce qui
manquait n'était pas une pente, c'était une **lecture** de la pente sous l'eau.
La forme du fond immergé existait et n'était affichée par rien.

Ordre effectivement suivi :

```
1. forme      constatee, mesuree, NON MODIFIEE (et c'est la conclusion, pas un raccourci)
2. matiere    profondeur / platitude / courant -> trois etats de matiere de rive
3. transition l'effacement de la nappe avant son arete
4. ecologie   NON FAITE -- appartient au dressing, cf. Limites
```

## Gate 5 — les familles de matière de rive

Trois états, portés par `M_AnastasisShoreWater`, tous fonction du génome :

| Famille | Condition | Rendu |
|---|---|---|
| `SHORE_WET` | `Depth` sous `marginEnd` | limon sombre (`0.140, 0.115, 0.082`), mat (roughness 0.80), spéculaire faible (0.20) |
| `SHORE_MINERAL` | idem, mais `Flow > 0` | graviers lavés (`0.168, 0.172, 0.160`), plus clairs et plus froids, ourlet renforcé |
| `SHORE_TRANSITION` | `Depth` entre `marginEnd` et l'eau franche | interpolation vers `ShallowWater`, opacité et spécularité croissantes |
| *(eau franche)* | `Depth → 1` | `DeepWater`, opacité 0.95, spéculaire 1.0 — la valeur historique |

Et, transversal aux trois : l'**ourlet**, une bande posée à mi-profondeur
d'ourlet, dont la largeur suit `Flatness` et l'intensité `Flow`.

Les deux teintes d'eau (`ShallowWater`, `DeepWater`) sont **reprises telles
quelles** de `AnastasisTerrainSurface::TileColor` : la nappe ne réinvente pas la
couleur de l'eau, elle la dégrade.

## Preuve — build et tests

Build sur sources quiescentes du worktree, puis la suite `Anastasis.Terrain`
complete :

```
BUILD::PASS
Anastasis.Terrain.Contract         Success
Anastasis.Terrain.DressingOnGround Success
Anastasis.Terrain.Fallback         Success
Anastasis.Terrain.SampleHeight     Success
Anastasis.Terrain.Semantics        Success
Anastasis.Terrain.Shoreline        Success   <- nouveau
Anastasis.Terrain.WorldExtent      Success
...Automation Test Queue Empty 7 tests performed.
```

**Les marqueurs scellés sont inchangés, caractère pour caractère** — c'est la
preuve que la géométrie n'a pas bougé :

| Marqueur | Valeur | Attendu |
|---|---|---|
| `TERRAIN_CONTRACT` | `vertices=1024 triangles=1922 max_error=0.000000000 boundary_edges=124` | identique à WORLD_SLICE_006 |
| `TERRAIN_SEMANTICS` | `water_tiles=151 land_tiles=873 shore_tiles=158 water_quads=187` | identique |
| `TERRAIN_EXTENT` | `world=96x96 vertices=9216 triangles=18050 boundary_edges=380` | identique |

Valeurs nouvelles, sorties du test et non affirmées :

```
TERRAIN_SHORELINE        water_vertices=1198 margin=1152 full_depth=46 flowing=389
                         depth_uu_min=0.0 depth_uu_max=91.0 span_uu=60 max_flow=1.000
TERRAIN_SHORELINE_DEPTHS uu n=1198 p10=9.0 p20=14.0 p30=18.0 p40=20.0 p50=23.0
                         p60=27.0 p70=31.0 p80=37.0 p90=48.0
TERRAIN_SHORELINE_ROUNDING  water_tiles_at_sealevel=6/1198
TERRAIN_SHORELINE_CROP      border_flatness_diff=62
TERRAIN_SHORELINE_FAMILIES WORLD soft=243 steep=51 flowing=304
TERRAIN_SHORELINE_FAMILIES CROP  soft=43  steep=1  flowing=17
```

### Trois choses que le test a trouvées et qu'on aurait sinon affirmées à tort

**1. `ShoreDepthSpan = 120` était faux, et le test l'a dit.** `full_depth=0` :
aucun sommet du monde n'atteignait l'eau franche. Corrigé à 60 (voir Gate 2).

**2. Six tuiles d'eau n'ont aucune profondeur, et c'est normal.** Le TYPE d'une
tuile est décidé sur l'altitude brute (`Alt < SeaLevel`), l'altitude STOCKÉE est
arrondie à trois décimales (`AnastasisWorld.cpp:414`). Une tuile d'eau à 0.2746
se range donc à 0.275 = `SeaLevel` exactement, et la nappe n'a plus d'épaisseur
au-dessus d'elle. **Le trait de côte rendu est celui de l'altitude arrondie, pas
celui de la classification** — un écart borné à 0.0005, soit 0.5 uu, sur 6 tuiles
sur 1198. Le test compte les deux cas séparément et refuserait une vraie
régression.

**3. La platitude de berge est relative à l'emprise, aux bords.** 62 sommets du
bord intérieur de la tranche 32×32 portent une `Flatness` différente de celle du
monde 96×96 : leur normale n'a pas les faces voisines que le monde a. Ce n'est
pas une divergence à corriger, c'est ce qu'une emprise veut dire — même classe de
fait que « la palette est relative à l'emprise » dans `TERRAIN_SURFACE_EXTENT`.
La **profondeur** et le **courant**, eux, sont identiques dans les deux emprises,
et le test l'exige.

## Défaut trouvé dans l'outillage partagé, signalé et NON corrigé

`tools/unreal/report-tests.ps1` a rapporté **deux fois de suite** :

```
PASS                  : 0
FAIL                  : 0
TOTAL                 : 0
TESTS::PASS (echecs connus exclus, jamais comptes comme PASS)
```

Zéro test exécuté, et le script sort `TESTS::PASS` avec le code 0. **Un portail
de tests qui dit PASS quand rien n'a tourné est pire qu'un portail absent.**

Cause reproduite : `-ExecCmds="Automation RunTests <filtre>;Quit"` met en file la
commande de tests *puis* `Quit` (`Automation: Quit Command Queued.` au log), et
sur un cache d'AssetRegistry chaud le `Quit` gagne la course. En retirant `;Quit`
et en laissant `-testexit="Automation Test Queue Empty"` fermer l'éditeur, les
7 tests tournent — c'est ainsi que la preuve ci-dessus a été obtenue.

Ce fichier est l'outillage de test de **toutes** les missions et n'appartient pas
à celle-ci : **il n'est pas modifié ici.** Le correctif tient en un mot retiré, et
la décision revient à Alexandre. Deux ajouts seraient à faire ensemble : refuser
`TESTS::PASS` quand `TOTAL` vaut 0.

## Assets créés

| Chemin | Rôle |
|---|---|
| `Content/Anastasis/Materials/M_AnastasisShoreWater.uasset` | matériau de la nappe d'eau (section 1) |
| `tools/unreal/shore-water.py` / `.ps1` | source d'autorité du matériau |
| `tools/unreal/shore-capture.py` / `.ps1` | rig de capture A/B des vues de rive |
| `docs/unreal/SHORELINE_FORGE_001.md` | ce document |
| `docs/visual/shoreline-001/` | captures baseline et candidate |

## Fichiers modifiés

| Fichier | Nature |
|---|---|
| `AnastasisTerrainSurface.h` | `ShoreDepthSpan`, `FShorelineVertex`, `ShorelineDepthAt`, `FGeometry::WaterUV0/WaterUV1` |
| `AnastasisTerrainSurface.cpp` | `ShorelineAt`, remplissage des canaux après normalisation |
| `AnastasisTerrainSurfaceTests.cpp` | `Anastasis.Terrain.Shoreline` (additif, aucun test existant touché) |
| `AnastasisWorldEmbodiment.h/.cpp` | CVar `anastasis.Terrain.Shoreline`, `ResolveWaterMaterial()`, canaux sur la section 1, log `ANASTASIS_SHORELINE` |

Aucun fichier de `Source/AnastasisSim/` n'est modifié. Aucune fonction de la
section 0 n'est touchée.

## Outils Unreal réellement utilisés

| Outil | Utilisé | Pourquoi |
|---|---|---|
| Water System | **non** | plugin absent du projet |
| Landscape Edit Layers | **non** | aucun Landscape |
| Landscape Patch System | **non** | plugin absent |
| Landscape Materials / Layers | **non** | aucun Landscape |
| RVT | **non** | aucune infrastructure ; l'ouvrir aurait été le gouffre architectural que les conditions d'arrêt interdisent |
| PCG Framework | **non** | plugin absent |
| Foliage Mode | **non** | le dressing est du HISM piloté en C++ |
| Decals | **non** | non nécessaire |
| **ProceduralMeshComponent** | oui | la surface du monde, sections 0 et 1 |
| **Material Editor (Custom HLSL)** | oui | `M_AnastasisShoreWater`, deux nœuds `Custom` |
| **Translucent / Surface Per-Pixel Lighting** | oui | c'est ce qui efface l'arête de la nappe |
| **Console variables** | oui | `anastasis.Terrain.Shoreline` — le levier A/B |
| **Automation** | oui | `Anastasis.Terrain.Shoreline` |
| **Python éditeur** | oui | forge d'asset et rig de capture |


## Preuve visuelle

Toutes les captures : `docs/visual/shoreline-001/`, avec leur README, leurs
caméras, leurs octets et leurs SHA. Chaque paire est prise au même uu de caméra,
sous le même soleil, à la même exposition ; seule change
`anastasis.Terrain.Shoreline`, et la géométrie est identique des deux côtés
(`vertices=9216 triangles=18050 water_triangles=3544`).

| Vue | Gain constaté |
|---|---|
| `A_close` | **fort** — la droite nette du bord d'eau disparaît, remplacée par un plateau gradué |
| `B_mid` | **réel mais modéré** — les bassins gagnent un liseré et un cœur |
| `C_gameplay` | **faible** — en incidence rasante la marge se comprime ; reste l'absence d'arête |
| `D_aerial` | **fort** — les plans d'eau cessent d'être des autocollants cyan uniformes |
| `E_flowing` / `F_steep` | **variantes distinctes** — ourlet marqué sur un chenal, liseré étroit sur une berge raide |

Le gain n'est donc **pas uniforme selon la distance**, et le dire fait partie du
résultat : la mission exigeait qu'une amélioration seulement en gros plan, ou
seulement vue du ciel, soit jugée insuffisante. Ici les deux extrêmes gagnent, le
milieu gagne peu, et la vue à hauteur d'œil gagne le moins.

**Aucun élément décoratif n'a été ajouté.** Pas une plante, pas une pierre. Le
gain visible vient entièrement de la forme lue et de la matière, ce que la
condition d'arrêt « le gain dépend seulement de plantes décoratives » exigeait.

## Limites connues, assumées

1. **Le fond immergé reste peint en bleu.** `TileColor` peint toute tuile d'eau
   en `ShallowWater`/`DeepWater`, y compris sous la marge. La translucidité de la
   nappe ne révèle donc pas du limon : elle révèle du bleu. C'est pourquoi la
   marge porte sa propre matière, presque opaque, et pourquoi la transparence ne
   sert ici qu'à une chose — effacer la nappe juste avant son arête. Repeindre le
   fond immergé demanderait de toucher `TileColor`, qui appartient à
   GROUND_SURFACE_001. **C'est la première chose à faire quand les deux missions
   seront intégrées**, et c'est une ligne, pas un chantier.
2. **Aucune bande écologique.** Gate 6 n'est pas faite : ni roseaux, ni pierres
   de rive, ni herbes humides. Le dressing appartient à
   `AnastasisEcologicalDressing` et à la mission arbres, tous deux vivants. En
   contrepartie, le gain rapporté ici **ne dépend d'aucune plante** — c'est
   exactement ce que l'ordre de mission exigeait (`FORM_FIRST`, et la condition
   d'arrêt « le gain dépend seulement de plantes décoratives »).
3. **Aucune écume animée, aucune vague.** `KNOWN_VISUAL_LIMITS` de
   WORLD_SLICE_006 reste vrai mot pour mot. L'ourlet est statique et dérivé de la
   profondeur, pas d'un temps.
4. **La nappe n'a pas de collision** (`bCreateCollision=false`, inchangé) : rien
   ne flotte, rien ne nage. Hors périmètre.
5. **`Flow` n'a pas de direction visible.** `FlowX`/`FlowZ` existent et ne sont
   pas exportés : aucune matière ne saurait quoi en faire aujourd'hui.
6. **La translucidité a un coût non mesuré ici.** La section 1 passe d'un
   matériau opaque à un matériau translucide à éclairage par pixel, sur jusqu'à
   3544 triangles en mode 2. Aucun budget GPU n'a été relevé — ce n'est pas une
   régression constatée, c'est une mesure qui manque.

## Défaut trouvé chez le voisin, signalé et NON corrigé

`UAnastasisWorldProbeSubsystem::EnsureDefaultBookmarks` compose le signet
`SHORE` ainsi (`AnastasisWorldProbeSubsystem.cpp`, ~l. 519) :

```cpp
// SHORE: land tile with the smallest Shore distance to water (i.e. the coastline).
if (Tile.Shore < BestShore) { BestShore = Tile.Shore; BestIndex = Index; }
```

Le commentaire dit « coastline », le code prend le **minimum** de `Tile.Shore`.
Or `Shore` vaut 1 au bord de l'eau et décroît vers 0 en s'éloignant
(`AnastasisWorld.cpp:349`). Le minimum est donc la tuile la **plus éloignée** de
l'eau. Le signet `SHORE` vise l'intérieur des terres, pas une rive.

Ce n'est pas une correction d'un caractère prise à la légère : c'est du code de
l'observatoire, il a des captures scellées derrière lui
(`docs/visual/atmosphere-002/`), et il n'appartient pas à cette mission. **Il
n'est pas corrigé ici.** C'est pour cela que la preuve visuelle de cette mission
ne s'appuie pas sur ce signet : elle se cadre sur `TERRAIN_SHORELINE_SITE`, que
le test mesure lui-même.

## Risques

1. **Conflit de fusion avec GROUND_SURFACE_001**, certain et borné : un bloc de
   `EmbodyCrop`. Résolution décrite dans l'OWNER_MAP. Intégrer le sol d'abord.
2. **Coût GPU de la translucidité non mesuré.** La section 1 passe d'un matériau
   opaque à un translucide à éclairage par pixel, sur jusqu'à 3544 triangles.
   Aucun budget relevé — c'est une mesure qui manque, pas une régression connue.
3. **Le tri de la translucidité n'a pas été éprouvé** contre le futur dressing de
   rive : une plante à moitié immergée traversera la nappe. Aucun asset de ce
   type n'existe aujourd'hui.
4. **`ShoreDepthSpan` est calé sur la graine 12345.** Il est calé sur une
   *distribution*, pas sur un maximum, donc il devrait tenir sur d'autres graines
   du même générateur — mais cela n'a pas été vérifié. `TERRAIN_SHORELINE_DEPTHS`
   le dit à chaque exécution.
5. **La lecture « limon » n'est pas atteinte** (cf. Limites n°1). Si GROUND_SURFACE_001
   change l'albédo du sol, le réglage de la marge sera à revoir — c'est une
   dépendance réelle entre les deux missions.

## Prochaine étape recommandée

Une seule, et elle est petite : **repeindre le relief immergé**. Une tuile d'eau
est aujourd'hui peinte en bleu par `TileColor`, y compris sous la marge, ce qui
empêche la bande de rive de montrer du sédiment. La correction est une ligne dans
la branche eau de `TileColor` — mais cette fonction appartient à
GROUND_SURFACE_001, donc **elle se fait après l'intégration des deux missions, pas
avant**. C'est le point qui transformerait le « plateau d'eau peu profonde »
actuel en vraie marge humide.

Ensuite seulement, et seulement si la forme convainc : GATE 6, une bande
écologique clairsemée.

## Verdict

```
SHORELINE_FORGE_001 :: KEEP
```

Ce qui est tenu, et vérifiable :

1. une zone de rive ne ressemble plus à une coupure eau/terrain — `A_close` ;
2. une bande transitionnelle lisible existe, et sa largeur sort de la pente ;
3. la rive possède une gradation de matière : marge, eau peu profonde, eau franche ;
4. l'intégration reste cohérente — aucune texture, aucun pack, aucun asset externe ;
5. la solution fonctionne de près et du ciel ; **à hauteur d'œil elle fonctionne peu** ;
6. six zones de test, trois familles de rive, toutes mesurées et non choisies ;
7. aucune refonte : la géométrie du monde est identique au sommet près ;
8. travail non destructif — CVar à 0 et le monde rend exactement comme avant ;
9. les outils Unreal réellement utilisés sont identifiés, y compris les huit prescrits
   qui n'existent pas dans ce projet ;
10. la comparaison baseline/candidate est contrôlée et versée avec ses hachages.

Ce qui n'est **pas** tenu, et qu'on ne présentera pas comme tenu : la marge ne se
lit pas comme du limon (Limites n°1), aucune bande écologique n'a été posée
(Limites n°2), et le gain à hauteur d'œil est faible (Preuve visuelle).
