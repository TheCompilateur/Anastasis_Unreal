# ASSET_MAP_002 — du corpus au monde visible

Audit du 28 septembre 2026 (Toronto). Étape 1 : comprendre l'existant. Aucun patch visuel, aucune intégration, aucun build ni test runtime exécuté pour cet audit.

## Verdict

Le patrimoine visuel est réparti entre **16 planches canoniques**, **19 packages ANÁSTASIS dans le checkout ouvert**, des générateurs Python, des consommateurs C++, et **plusieurs bibliothèques candidates conservées dans des branches distinctes**. La priorité est de retrouver et qualifier ces travaux avant de fabriquer de nouveaux assets.

La forêt, le sol et l'eau ont une chaîne de production identifiable. Les écotones et les rochers ne sont pas livrés dans le checkout inspecté, mais des travaux substantiels existent ailleurs dans le même dépôt. Présence dans Git, import Unreal, chargement effectif et qualité visuelle restent quatre états distincts.

## Périmètre et provenance

- Racine lue : `C:/dev/ANASTASIS_UNREAL`.
- Checkout ouvert : branche `atmosphere-cached-lighting-preexposure`, HEAD `ce9ecfa73e8398c24bc1f7b72f044479b6d3238e`.
- `main` observé séparément : `bff3b03093f96f50d8883f2709408543fc047473`. Ce n'est pas le HEAD ouvert. Mêmes 19 noms de packages ANÁSTASIS, mais contenus/code différents, notamment feuillage et terrain. Aucun verdict canonique global.
- Modifications locales préexistantes préservées : `Config/DefaultInput.ini`, `Source/Anastasis_UnrealV2/Anastasis_UnrealV2Character.cpp/.h`, `.claude/` non suivi.
- Rapport isolé dans le worktree géré par l'application `C:/Users/alex_/.codex/worktrees/asset-map-002/ANASTASIS_UNREAL`, base exacte du checkout observé. Seul ce document est ajouté.
- Lecture des sources, inventaires Git et noms de packages ; aucun `.uasset` ou `.umap` interprété comme texte. Les noms donnent un rôle attendu, pas une certification de classe, de LOD, de collision ou de contenu binaire.
- Aucun outil Unreal MCP callable trouvé dans cette session ; aucun processus `UnrealEditor*` retourné par l'inventaire accessible. Pas d'inspection vivante du registre. Ne pas substituer une recette Python à la lecture de l'asset sauvegardé.
- Des logs historiques ont été lus dans `Saved/Logs`, en lecture seule. Les captures existantes sont historiques ; aucune nouvelle capture n'a été prise.
- Les fichiers externes non désignés, achats Fab, bibliothèque de téléchargements et anciens projets OneDrive ne sont pas inventoriés. Aucun ancien projet interdit n'a été ouvert.

### Types de preuve

`OBS-SOURCE` : source ou inventaire lu maintenant. `EVD-HIST` : log ou image historique consulté. `REPORT` : affirmation d'un rapport d'agent, non rejouée. `UNKNOWN` : donnée non vérifiée. Aucun PASS runtime nouveau n'est décerné ici.

## 1. Le corpus de références

Autorité de classement : [index des références](../visual/reference/README.md). Ce sont des images de direction artistique, pas des meshes, textures séparées ou instructions techniques autonomes. Les mentions PCG, Landscape, Nanite ou World Partition sur une planche ne prouvent ni l'usage ni la pertinence de ces systèmes dans le projet.

Les 16 fichiers sont présents. Les planches État Zéro 2, 3 et 5 ont été ouvertes visuellement pour cet audit ; les autres sont indexées par leur README, sans prétendre à une critique visuelle complète.

| Référence, sous `docs/visual/reference/` | Intention et rattachement actuel |
|---|---|
| `pontique-etat-zero-1-biome-fondateur.png` | Macro-territoire ; WorldView → TerrainSurface/Forge. |
| `pontique-etat-zero-2-anatomie-sol.png` | Continuum sec → humus → boue → eau ; matériau Ground et hydrologie. Racines, feuilles et débris demandent aussi une géométrie. |
| `pontique-etat-zero-3-stratification-forestiere.png` | Forêt en strates ; grammaire d'arbres + placement écologique. Arbustes, fougères, mousse et bois mort ne se réduisent pas à de petits arbres. |
| `pontique-etat-zero-4-hydrologie-territoire.png` | Relief et écoulement ; données worldgen et projection de l'eau. |
| `pontique-etat-zero-5-lisieres-transitions.png` | Forêt → lisière → clairière → prairie humide → rive ; forêt et matériaux présents, bibliothèque Ecotone en branche candidate. |
| `pontique-camp-installation-rive.png` | Composition d'un camp ; pas de kit de camp ANÁSTASIS identifié dans le checkout. |
| `pontique-lisiere-defrichement-pcg.png` | Transition et défrichement ; intention humaine non démontrée par le dressing écologique actuel. |
| `pontique-hydrologie-systeme-eau.png` | Eau/berges ; TerrainSurface et ShoreWater. |
| `pontique-tier0-jour3-arrivee.png` | Arrivée, objets sauvés ; pas d'assets dédiés de cette scène identifiés. |
| `pontique-tier1-jour30-implantation.png` | Premiers abris ; pas de kit architectural dédié identifié. |
| `pontique-tier2-an1-village-rhomaios.png` | Village ; fondation d'interaction Village présente, incarnation architecturale non livrée par cette seule présence. |
| `pontique-grammaire-architecturale-batiment.png` | Construction modulaire ; citée par la grammaire de ruines candidate, pas une bibliothèque de maisons livrée. |
| `pontique-props-materiel-refugies.png` | Props ; aucun package ANÁSTASIS dédié identifié. |
| `pontique-usure-materiaux-vieillissement.png` | Vieillissement ; aucun système temporel d'usure validé dans cet audit. |
| `pontique-synthese-lumiere-meteo-canonique.png` | Lumière/météo ; profil d'atmosphère présent, six états visuels non certifiés. |
| `pontique-production-frame-test-integration.png` | Cible de composition ; ce fichier du dossier référence n'est pas une preuve de frame runtime. |

Deux trous de provenance : l'ancien ASSET_MAP_001 mentionne « 6 planches taxonomiques Downloads » sans chemins ni hashes permettant leur identification ; leur correspondance avec les 16 planches reste UNKNOWN. L'index indique que seule la série État Zéro 1 à 5 a été fournie : ne pas inventer une planche 6.

## 2. Ce qui est réellement stocké

`git ls-files Content` : **557 `.uasset` + 4 `.umap` + 6 `.py` = 567 fichiers suivis**. Ce total n'est pas un catalogue de 561 assets artistiques ANÁSTASIS : il inclut personnages, templates, armes, entrées, objets externes de niveaux, etc.

Sous `Content/Anastasis` : **18 `.uasset` + 1 `.umap` = 19 packages**.

| Famille | Packages et rôle attendu | Fabrication et consommation |
|---|---|---|
| Arbres, 9 | `Vegetation/SM_Tree_{Conifer,Broadleaf}_{Understory,Subcanopy,Canopy,Emergent}_01` (8) + `SM_Tree_Generic_01` (1 alias généré) | [create_tree_asset.py](../../tools/unreal/create_tree_asset.py), Geometry Script ; [set_tree_grammar.py](../../tools/unreal/set_tree_grammar.py) câble les 8 variantes. Resolver → HISM. |
| Ruine, 1 | `Architecture/SM_Ruin_Generic_01` | [create_ruin_asset.py](../../tools/unreal/create_ruin_asset.py) assemble cinq volumes ; [set_presentation_meshes.py](../../tools/unreal/set_presentation_meshes.py) câble Ruin. |
| Matériaux, 6 | `M_AnastasisSlice`, `M_AnastasisVegetation`, `M_AnastasisBark`, `M_AnastasisGround`, `MI_AnastasisGround`, `M_AnastasisShoreWater` | Slice : `observe-slice.py`. Végétation/écorce : générateur d'arbres. Ground : `ground-material.py`. Eau : `shore-water.py`. |
| Données, 2 | `Presentation/DA_AnastasisPresentation`, `DA_AnastasisAtmosphere` | Registre de variantes et profil d'éclairage ; recettes `presentation-registry.py` et `atmosphere-profile.py`. |
| Niveau, 1 | `Maps/Lvl_AnastasisSlice` | Map de démarrage configurée ; le code WorldEmbodiment génère la surface et le dressing depuis le snapshot. Inspection des acteurs sauvegardés non effectuée. |

Aucun fichier source `.fbx`, `.obj`, `.glb`, `.gltf`, `.blend`, `.usd`, `.exr` ou `.tga` suivi n'a été trouvé par la requête Git ciblée. Cela ne permet pas de conclure qu'aucun asset Unreal n'a jamais été importé ; son fichier source peut être externe ou absent du dépôt. Les recettes examinées pour les assets propres à ANÁSTASIS sont procédurales.

Les huit silhouettes d'arbres sont des variantes visuelles (deux familles × quatre statures), pas huit espèces botaniques simulées. Le neuvième fichier est un alias de géométrie, pas une neuvième silhouette indépendante. Le générateur normalise la hauteur à 100 uu et désactive explicitement Nanite dans sa recette ; les réglages binaires actuels n'ont pas été inspectés.

## 3. La chaîne qui donne une image

```text
Références PNG ── interprétation humaine/agent ── recettes Python
                                                   │ fabrication éditeur
                                                   v
                                           meshes / matériaux / Data Assets
                                                   │ chargement
GenerateWorld(seed,96,96) → snapshot WorldView ──────┤
    ├─ TerrainSurface + TerrainForge → surface terre/eau
    │      ├─ section terre → MI_AnastasisGround
    │      └─ section eau   → M_AnastasisShoreWater
    └─ plan écologique → stature/famille → PresentationResolver
                         → variante du registre → HISM + matériaux
```

Il n'y a pas de conversion automatique d'une image de référence en contenu du monde dans les sources examinées.

- [WorldView](../../Source/Anastasis_UnrealV2/WorldView/AnastasisWorldView.h) conserve le monde source 96×96 et une échelle diagnostique de 100 uu/tuile ; crop et rendu ne changent pas cette source.
- [EcologicalDressing](../../Source/Anastasis_UnrealV2/WorldView/AnastasisEcologicalDressing.h) produit `Young / Secondary / Canopy`. [WorldEmbodiment](../../Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp) traduit ces placements en statures/familles, ancre sur le terrain et instancie les meshes.
- [PresentationResolver](../../Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationResolver.cpp) charge le Data Asset une fois et le garde en cache. Si le registre manque ou est vide : repli C++. Si un mesh sélectionné ne se charge pas : échec de résolution, pas remplacement garanti de toute instance par une primitive.
- Le sol lit couleurs sémantiques, poids Rock/Litter/Worked, humidité, normale et position ; la matière vient de `ground-material.py`, pas de photographies de la planche collées au terrain.
- L'eau est une section de mesh avec profondeur/platitude/écoulement consommés par `shore-water.py`, pas un WaterBody démontré. Le niveau ne constitue pas à lui seul la vérité du monde.
- `Village/AnastasisVillageBuilding` porte un SmartObject, et le sous-système prévoit House/Well/Workshop. Cela corrige le vieux diagnostic « aucun code village », mais ne prouve ni maison visible ni boucle complète village/PNJ/joueur. PLAYER reste hors périmètre.

## 4. Trois états de présentation qui peuvent diverger

| État | Forest | Ruin | Statut de connaissance |
|---|---|---|---|
| Registre sauvegardé `DA_AnastasisPresentation` | Contenu actuel non relu dans Unreal | Idem | UNKNOWN aujourd'hui ; chargement historique attesté. |
| [CreateCodeDefaults](../../Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationRegistry.cpp) | 8 variantes + matériaux feuillage/écorce | Cylindre moteur | OBS-SOURCE. |
| [presentation-registry.py](../../tools/unreal/presentation-registry.py), création minimale | 1 variante canopée ; ne reconstruit pas seule toute la grammaire | Cylindre moteur | OBS-SOURCE. |
| Scripts de câblage | `set_tree_grammar.py` écrit 8 variantes | `set_presentation_meshes.py` écrit la ruine dédiée | OBS-SOURCE de l'intention des scripts, pas preuve de leur exécution actuelle. |

Conséquence : reconstruire le registre de zéro sans les étapes de câblage peut perdre la diversité et la ruine dédiée. Une modification du repli C++ ne remplace pas le Data Asset chargé. Aucun de ces scripts n'a été exécuté pendant l'audit.

Autre dérive documentaire : les commentaires de certains outils parlent encore de « six variantes » ; la liste exécutable `VARIANTS` en contient huit. `set_ruin_variant.py` est un ancien outil dont la relecture appelle sans garde `unload_assets` ; `set_presentation_meshes.py` décrit et contourne cette limite. Ne pas confondre sauvegarde et vérification disque.

## 5. Les bibliothèques candidates déjà faites

Inventaire des arbres Git de branches, sans merger ni interpréter leurs binaires. Les comptages ne certifient pas que les payloads LFS sont localement matérialisés, chargeables ou artistiquement acceptés. Les refs sont figées ci-dessous par hash abrégé.

| Branche / ref | Contenu distinct retrouvé | Chaîne identifiable et preuve restante |
|---|---|---|
| `claude/anastasis-rock-grammar-e67075` / `fc81d57` | **19 meshes Rock** ; **18 nouvelles ruines**, soit 19 Architecture avec Generic | `create_rock_assets.py` → `set_rock_presentation.py` ; `create_ruin_grammar.py` → `set_ruin_presentation.py` + `AnastasisRuinDressing`. Rapports ROCK_FORGE_001/RUIN_GRAMMAR_001 et captures présents. Qualité/rejeu actuel UNKNOWN. |
| `agent/lithos_forge_001` / `55bfd02` | **10 meshes Lithos** + `M_AnastasisLithos` | `create_lithos_asset.py` + `AnastasisGeologicalDressing`. Rapport : 714 placements, aucun mesh manquant ; capture échouée, PLY UNKNOWN. Ce sont des claims historiques de rapport, pas des mesures refaites ici. |
| `agent/ecotone-forge-001` / `fb68398` | **12 meshes Ecotone** + `M_AnastasisStone` | `create_ecotone_assets.py` + `AnastasisEcotoneDressing`. Rapport : 2271 placements ; SCN PARTIAL, vues in-world à hauteur d'homme manquantes. |

Les 12 Ecotone : BranchPile, BuriedBlock, Bush_Low, Driftwood, ExposedRoots, FallenLog, GrassTuft, Reed, RockCluster, Sapling, ShoreTuft, Stump. Ils couvrent directement une partie des manques apparents des références forêt–sol–rive.

Les 19 Rock : six familles de trois variantes (Boulder, CliffFragment, Low, Massive, Split, Vertical) + Cluster. Les 18 nouvelles ruines : six familles de trois variantes (Angle, Enclos, Foyer, Mur, Reemploi, Soubassement). Les dix Lithos : Cornice, DetachedBlock, Fractured, InclinedWall, Outcrop, Stratum, Summit, TalusCluster, Transition, VerticalWall.

**Ces lots ne sont pas cumulables par simple addition.** Rock et Lithos couvrent en partie le même domaine ; Ecotone contient aussi des fragments rocheux. Les branches reposent sur des états plus anciens et touchent notamment WorldEmbodiment. Une différence complète de branche peut retirer des travaux plus récents : ce n'est pas une liste de fichiers à copier ni une proposition de merge global.

`main` et le checkout ouvert n'ont ni dossier Rock, ni Lithos, ni Ecotone dans `Content/Anastasis`. `agent/hydra-forge-001` (`4908e06`) et `agent/world-dressing-v0` (`4e676d0`) ont aussi été inventoriés par noms ; aucun de ces trois dossiers n'y figure. Audit des candidats borné à ces refs, pas à tous les fichiers sales de tous les worktrees.

## 6. Ce qui a réellement été observé à l'exécution

Source brute lue : `C:/dev/ANASTASIS_UNREAL/Saved/Logs/Anastasis_UnrealV2.log`, session du **18 septembre 2026**. Build moteur : CL 56702186 (ligne 473). Empreinte exacte source/binaire non établie par cet audit ; ces valeurs ne valident pas le HEAD actuel.

| Ligne | Observation historique exacte |
|---|---|
| 1977 | `source=96x96`, `crop=(0,0) 96x96`, `vertices=145161`, `material=MI_AnastasisGround`, `water_material=M_AnastasisShoreWater`. |
| 1978 | `ANASTASIS_SHORELINE enabled=1`, `channels=145161`, `water_vertices=145161`, `span_uu=60`, `forged=1`. |
| 1979 | `ANASTASIS_PRESENTATION_REGISTRY source=asset`, `entries=2`. |
| 1980 | `young=180 secondary=129 canopy=129 full_plan=438`. |
| 1982 | `conifer=225 broadleaf=213`. |
| 1986 | `dressing_instances=886 tree_tiles=716 ruin_tiles=462 source=asset components=9`. |
| 2303 | `ANASTASIS_ATMOSPHERE_PROFILE source=asset`. |

Les tuiles sémantiques ne sont pas les instances rendues : 716 tuiles Forest ne signifient pas 716 arbres. Ces logs confirment historiquement le chemin de chargement, pas la qualité de la scène ni chaque chemin de mesh actuellement enregistré.

Images historiques disponibles : [planche de stature](../visual/tree-form-001/B_stature_board.png), [sol sur terrain forgé](../visual/ground-001/README.md), [rive](../visual/shoreline-001/README.md). Seule la première de ces captures a été ouverte visuellement ici : silhouettes distinctes mais simplifiées, masses de feuillage et troncs géométriques. Ce constat concerne cette image, pas un rendu actuel inspecté. Les planches de référence ouvertes réclament une richesse de sous-bois, de matières et de transitions qui ne peut être certifiée par le simple nombre de variantes.

## 7. Décision issue de l'étape 1

| Famille | Décision proposée | Preuve discriminante suivante |
|---|---|---|
| Arbres | Réutiliser la chaîne de placement et de variantes ; jugement artistique des meshes encore ouvert. | Registre réellement chargé, puis vue proche neutre confrontée à la référence forestière. |
| Sol/eau | Réutiliser les consommateurs et matériaux existants avant toute nouvelle architecture. | Même lieu, mêmes paramètres et éclairage : vérifier continuité forêt–rive et lecture des matières. |
| Sous-bois/rive basse | Évaluer **Ecotone existant** avant de recréer arbustes, bois mort et roseaux. | Une vue proche in-world, sur ref isolée maîtrisée ; cette preuve manque explicitement au handoff. |
| Rochers | Comparer les rôles Rock/Lithos/Ecotone avant toute extraction. | Choisir un propriétaire pour blocs discrets, masses géologiques et débris ; ne pas empiler trois distributions. |
| Ruines | Retrouver le binding réel puis évaluer la grammaire candidate, sans confondre vestiges et kit de maisons. | Capture des sites sur le terrain accepté. |
| Architecture/props | Conserver les références et la frontière fonctionnelle ; ne pas lancer de production massive depuis cet audit. | Mission dédiée seulement quand les consommateurs et le besoin de scène sont définis. |

**NEXT borné : étape 2, confronter le registre réellement chargé et une scène proche forêt–sol–rive aux références, après fixation du terrain retenu avec Claude. Premier candidat à examiner : Ecotone, car son existence invalide déjà l'idée de recréer ces assets depuis zéro.** L'évaluation ne vaut pas autorisation d'intégrer.

## Limite de clôture

Cartographie des sources et candidats : effectuée dans le périmètre déclaré. Audit complet de toutes les propriétés des packages Unreal : non effectué. [MEC] nouveaux tests : non exécutés (documentation seule). [SCN] actuel : UNKNOWN ; preuves historiques identifiées. [PLY] : UNKNOWN. Aucun PASS narratif ne remplace une observation runtime.

Reproduction des inventaires : `git ls-files Content`, `git ls-tree -r --name-only <ref> Content/Anastasis`, lecture des recettes et consommateurs liés ci-dessus. Ancien [ASSET_MAP_001](ASSET_MAP_001.md) conservé comme historique ; ses affirmations « aucun arbre/sol dédié/ruine/code village » ne décrivent plus entièrement les sources examinées.