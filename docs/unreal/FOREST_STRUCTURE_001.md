# Forêt structurelle — audit et passe parallèle

Mission `forest-structure-001`. Branche `agent/forest-structure-001`.
Worktree `C:\dev\ANASTASIS_WORKTREES\forest-structure-001`.

La passe ne reconstruit pas le placement. Elle restructure le plan déjà produit
par `AnastasisEcologicalDressing::Build` : moins d'arbres, pas davantage, répartis
en âges, groupes, clairières et réactions au relief.

Le branchement est dans `AAnastasisWorldEmbodiment`, juste après `Build` et avant
les HISM. `Shape` écrit `Maturity`, que la passe P1 multiplie par la hauteur
réelle de l'essence, et la couche d'âge. Les arbres isolés (`bLone`) ne bougent pas.

## FOREST_SYSTEM

Pas de PCG, pas de Procedural Foliage, pas de Foliage Tool, pas d'Instanced Foliage
Actor. La map `Lvl_AnastasisSlice` ne peint pas les arbres.

Le placement est C++, dans `AnastasisEcologicalDressing::Build`, instancié en HISM
par `AAnastasisWorldEmbodiment` (`Dressing_<archétype>_v<index>`). Les instances sont
transitoires : elles ne sont pas sérialisées dans le `.umap`.

Graine : `FWorldVisualSnapshot::Seed`, hash déterministe. Même graine, même plan.
CVar `anastasis.Dressing.MacroForest` (défaut 1) choisit la grammaire macro, qui
échantillonne le relief rendu (`AnastasisTerrainForge`) plutôt que la tuile sémantique.

Macro, réglages actuels : masse corrélée sur 14 tuiles, espacement des troncs 500 cm,
hauteur ×3,5, au plus 128 candidats par tuile, pente max 48°, bassin ouvert 8 tuiles
plus un fondu de 4, vallées et rivière lues dans `AnastasisHumanGeography`,
ripisylve éclaircie via `AnastasisDrainage::RiparianAt`.

La « clairière » actuelle est un seuil sur un bruit de valeur (`ClearingThreshold`
0,28). C'est une absence de tirage, pas une forme. Les trois couches Young /
Secondary / Canopy existent, et 18 % des canopées deviennent des émergents au moment
du mesh (`StatureForLayer`). En macro, la couche jeune n'est jamais tirée : le plan
est une majorité de canopées à espacement constant. C'est le champ uniforme décrit
par la mission.

## AVAILABLE_SPECIES

Huit meshes, deux familles, quatre statures. Pas de mort, de chablis, ni de souche
dans cette grammaire.

| Mesh | Stature | Famille | Biais d'échelle |
|---|---|---|---|
| `SM_Tree_Conifer_Understory_01` | Understory | Conifer | 1,00 |
| `SM_Tree_Broadleaf_Understory_01` | Understory | Broadleaf | 0,90 |
| `SM_Tree_Conifer_Subcanopy_01` | Subcanopy | Conifer | 1,00 |
| `SM_Tree_Broadleaf_Subcanopy_01` | Subcanopy | Broadleaf | 0,92 |
| `SM_Tree_Conifer_Canopy_01` | Canopy | Conifer | 1,00 |
| `SM_Tree_Broadleaf_Canopy_01` | Canopy | Broadleaf | 0,85 |
| `SM_Tree_Conifer_Emergent_01` | Emergent | Conifer | 1,35 |
| `SM_Tree_Broadleaf_Emergent_01` | Emergent | Broadleaf | 1,12 |

`SM_Tree_Generic_01` est un alias du conifère canopée. L'espèce vient du site
(`SelectFoliageFamily` : ombre, humidité), pas d'un tirage aveugle.

Enveloppes d'échelle du plan, avant biais de mesh : jeune 0,25–0,45, secondaire
0,50–0,78, canopée 0,95–1,20, puis ×3,5 en macro. La passe nouvelle reste dans
ces enveloppes. Le garde-fou physique existant est 30 m
(`Anastasis.Terrain.HumanGeography.CollisionAndDressing`).

`agent/ecotone-forge-001` (non intégré) possède `SM_Ecotone_FallenLog_01`,
`SM_Ecotone_Stump_01`, `SM_Ecotone_Sapling_01`. Ils ne sont pas utilisés ici.

## CURRENT_RULES

Hiérarchie déjà en place, dans l'ordre du `Build` macro :

1. Habitat : forêt, herbe, broussailles, et roche si le relief rendu le permet.
2. Exclusion : eau rendue, falaise, champ, ruine, bassin, vallée, rivière, ripisylve.
3. Bruit de masse (`MassSpan` 14) comparé à `ClearingThreshold`.
4. Densité × support × couverture × ouverture × pénalité d'humidité.
5. Espacement dur, 500 cm, identique partout.
6. Couche : en macro, canopée si le tirage est sous `0,65 + 0,25 × maturité`, sinon secondaire.

## PERFORMANCE_CONSTRAINTS

Mesure historique de la macro-forêt, pas une mesure de cette passe :
16 120 instances, six HISM, hauteurs 7,70 / 14,17 / 27,08 m. Nanite est off.
Trois LOD par mesh (écran 1,0 / 0,22 / 0,055) ; le LOD lointain est une enveloppe
autorée, pas une réduction à 7 %. Le HISM choisit le LOD. Pas de distance de
culling par arbre. Ombres portées allumées sur le HISM de dressing.

Collisions inspectées, non modifiées. Le HISM de dressing est
`QueryAndPhysics` / `BlockAll`, et le mesh reçoit une collision simple NDOP10
autour de la couronne entière (`create_tree_asset.py`). La navigation est déjà
`bCanEverAffectNavigation = false`. Un corps physique peut donc être arrêté par
le feuillage. Le resserrer sur le tronc demanderait le composant ou la recette
de mesh, tous deux occupés ailleurs.

La passe nouvelle ne fait que retirer, déplacer de moins de 4,2 m, et
réétiqueter. Le nombre d'instances baisse.

## SAFE_FILES

Fichiers créés par cette mission, absents du travail concurrent :

- `Source/Anastasis_UnrealV2/WorldView/AnastasisForestStructure.h`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisForestStructure.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisForestStructureTests.cpp`
- `docs/unreal/FOREST_STRUCTURE_001.md`
- `docs/unreal/handoffs/forest-structure-001.md`

## Agent déjà sur la forêt

`agent/forest-terrain-yhszr2` (`C:\dev\ANASTASIS_WORKTREES\forest-terrain-yhszr2`)
travaillait en même temps, éditeur ouvert, meshes en cours d'écriture. Ses commits,
absents de `main`, couvrent déjà une partie de cette mission :

| Passe | Ce qu'elle fait | Fichiers |
|---|---|---|
| P1 | essences par altitude, maturité, hauteurs réelles | registre, résolveur, `FPlacement::Maturity`, `create_tree_asset.py` |
| P2 | lisière en dégradé, densité variable, clairières par bruit, arbres isolés | `AnastasisEcologicalDressing.*`, embodiment |
| P3 | maquis, ronces, ourlet | `AnastasisUnderstory.*`, couvert |
| P4 | sol méditerranéen | terrain, matériaux de sol |

Au moment de l'audit, le worktree avait en plus les neuf `SM_Tree_*` existants modifiés
et quinze meshes neufs non suivis (pin d'Alep, cyprès, chêne vert, olivier, platane).
Aucun de ces fichiers n'est touché ici.

`Shape` n'est pas un second placement à empiler sur P2. P2 change déjà le plan.
L'empiler éclaircirait deux fois. On ne branche qu'après avoir relu leur `Build`,
et seulement pour ce que P2 ne fait pas : contour de clairière irrégulier avec
ourlet jeune et quelques isolés, classes d'âge explicites, crêtes plus basses,
couloir qui n'est pas une route.

## CONFLICT_FILES

Ne pas les éditer tant que le travail suivant n'est pas versé.

| Fichier | Qui |
|---|---|
| `AnastasisEcologicalDressing.cpp/.h/.Tests.cpp` | `agent/macro-forest-001`, diff non commité : `BuildGroundMask` (litière), pas le placement |
| `AnastasisWorldEmbodiment.cpp/.h` | le même, et `agent/understory-001` (couvert au sol) |
| `AnastasisGroundCover.*`, `create-ground-cover.py` | `understory-001` |
| `M_AnastasisGround`, `MI_AnastasisGround`, `ground-material.py` | `macro-forest-001` |
| `capture-forest-walk.py` | `macro-forest-001` |
| Assets et code `Ecotone/*` | `ecotone-forge-001`, 1 commit non intégré |
| `SM_Tree_*`, matériaux d'arbre | recette de la passe macro ; pas de diff ouvert, mais ne pas régénérer |
| `Lvl_AnastasisSlice.umap` | évité |

## Règles ajoutées

Chaque champ a un rôle. Aucun n'est un bruit ajouté pour « faire du détail ».

| Champ | Rôle |
|---|---|
| Âge, octave 26 tuiles + octave 11 | jeunes / mature / vieille forêt, transition continue |
| Perturbation, octave 7, seuil haut | poche rare : ouverture et recrû, sans chablis |
| Groupes, octave 4,2 | garde les pics, éclaircit les creux, attire les troncs vers le pic |
| Clairières | disques irréguliers, 1 à 2 grandes, plusieurs moyennes, petites nombreuses |
| Bord de clairière | jeunes arbres, pas un trou net |
| Intérieur | 1, 2 ou 3 individus isolés selon la taille |
| Crête | au-dessus de la moyenne d'altitude locale et pente modérée : moins d'arbres, plus petits |
| Pente > 34° | éclaircie et échelle ramenée vers le bas de l'enveloppe |
| Couloir | bande étroite de deux champs allongés et croisés : ouverture, pas une route |

Classes visuelles, avec les meshes existants seulement :

- jeune : surtout sous-bois, échelle basse de l'enveloppe jeune ;
- mature : mélange, canopée majoritaire ;
- ancien : canopées hautes de l'enveloppe, espacement plus large, quelques jeunes ;
- perturbé : densité basse, mélange de jeunes et de survivants ;
- déclin : non créé. Aucun mesh mort dans la grammaire d'arbres.

Graine : la même que le snapshot. Deux `Shape` sur le même plan frais coïncident.
Un second `Shape` sur un plan déjà structuré l'éclaircirait encore : un seul appel.

## Intégration, quand les fichiers chauds sont libres

Après un `Build` réussi, avant la boucle HISM :

```cpp
AnastasisForestStructure::FReport StructureReport;
TArray<AnastasisForestStructure::FNote> StructureNotes;
FString StructureError;
if (!AnastasisForestStructure::Shape(CanonicalSource, ForestDressing, bMacro,
        ForestPlan, StructureNotes, StructureReport, StructureError))
{
    UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_FOREST_STRUCTURE rejected=%s"), *StructureError);
}
```

`bMacro` est déjà calculé dans `PlaceDressing`. Le sol est rééchantillonné plus
bas à partir de `P.Ground`, donc un déplacement XY reste ancré.

## Non fait

- Captures avant/après, coût GPU.
- Arbres morts, troncs au sol, souches.
- Collision réduite au tronc.
- Végétation basse, eau, ciel, bâtiments, PNJ, Landscape, matériaux de sol.
