# macro-forest-001 — première passe de masses forestières

MISSION: Donner à la vallée une canopée dominante, en réutilisant le terrain, les arbres et les HISM existants. Ne modifier ni la topographie ni AnastasisSim.

FILES_OWNED:
- Source/Anastasis_UnrealV2/WorldView/AnastasisEcologicalDressing.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisEcologicalDressing.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisEcologicalDressingTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp
- tools/unreal/capture-macro-forest.py
- docs/unreal/handoffs/macro-forest-001.md

COMMIT: PENDING

## Inspection et choix

Base: `ce9ecfa73e8398c24bc1f7b72f044479b6d3238e`, branche canonique observée `atmosphere-cached-lighting-preexposure`.
Worktree géré par Codex: `C:\Users\alex_\.codex\worktrees\macro-forest-001\ANASTASIS_UNREAL`, branche `agent/macro-forest-001`.
Le chemin est celui du gestionnaire de worktrees Codex; l'opérateur historique refuse ce préfixe. Compilation via le même Build.bat UE 5.8.2, sans modifier l'opérateur.

Map inspectée dans un éditeur: `/Game/Anastasis/Maps/Lvl_AnastasisSlice`, un `AnastasisWorldEmbodiment`, aucun acteur Landscape/PCG/foliage peint. La surface est reconstruite depuis le snapshot canonique 96×96 puis TerrainForge. Le registre fournit huit variantes conifère/feuillu × quatre statures, déjà présentes et chargées. Le placement écologique et les HISM fonctionnent; aucun nouvel asset n'est nécessaire.

État initial observé: 438 arbres, 180 jeunes, 129 secondaires, 103 canopées, 26 émergents; hauteur 97–627 UU. Les tuiles Stone sont exclues et le placement utilise la pente du terrain sémantique avant projection sur le relief forgé. Référence Three.js consultée en lecture seule: `forestGenerator3d.js` et `landForest3d.js` (masses, clairières, silhouettes); aucun port de renderer.

## Changement

Extension de `AnastasisEcologicalDressing::Build` avec un échantillonneur du relief déjà rendu. Les crêtes plantables et épaules rocheuses deviennent éligibles; les falaises, l'eau, champs et ruines restent exclus. Le bassin possède une réserve de 8 tuiles avec transition de 4 tuiles. Les prairies basses et plates restent ouvertes.

Masses corrélées sur 14 tuiles, espacement minimal des troncs 2,4 tuiles, hauteur multipliée par 2,5; canopée majoritaire et émergents issus du registre existant. Troncs verticaux, azimut variable, racines sur le relief rendu. Ces arbres sont de la présentation, sans création de ressources de simulation.

Activation par défaut à l'ouverture et en BeginPlay. `anastasis.Dressing.MacroForest 0` + reconstruction retrouve l'ancienne grammaire; `1` réactive la passe. Réglages dans `ForestDressing`, catégorie Macro. Le commutateur ne change aucune géométrie de terrain ni matériau.

MEC: BUILD::PASS (source/config stables pendant le build). Tests ciblés complets: 27 PASS, 0 KNOWN_EXPECTED_FAILURE, 0 FAIL, 0 absent, sortie 0. Exécution `-nullrhi`, classification via `tools/unreal/automation-log.ps1` et le registre officiel. Cela ne constitue pas une preuve visuelle.

Nouveaux tests: `Anastasis.Ecology.MacroForestRenderedHabitat` (hauteur rendue, eau, falaises, bassin, champs, ruines, espacement, répétabilité) et `Anastasis.Ecology.MacroForestCanonicalRelief` (trois graines réelles). Les 3 tests écologiques existants, 9 tests Presentation et 13 tests Terrain passent également.

| Graine | Arbres | Canopées, émergents compris | Sur tuiles Stone |
|---|---:|---:|---:|
| 12345 | 260 | 213 | 81 |
| 42 | 385 | 319 | 135 |
| 98765 | 196 | 167 | 28 |

Le nombre total d'arbres baisse volontairement: on remplace les petits sujets serrés par des arbres dominants espacés. La couverture et la silhouette se jugent sur les images, pas sur ce compteur.

Preuves brutes: `C:\Users\alex_\.codex\visualizations\2026\09\29\01a0ef4d-f7a0-72c0-8f4d-cc4c8234f6c4\build-final.log`, `automation.log`, `tests-summary.json`. Une première exécution globale était incomplète alors que les assets LFS du worktree étaient encore des pointeurs; elle est exclue du verdict. `git lfs checkout` a matérialisé les 620 objets depuis le cache local; aucun asset modifié dans le diff final.

SCN: VALIDATION_PENDING

PLY: UNKNOWN — aucune validation humaine ni revendication de performance joueur.

INTEGRATION_RISK: `AnastasisWorldEmbodiment.cpp` est un fichier partagé entre chantiers. La racine canonique était sale (DefaultInput.ini et Character.cpp/.h, plus .claude/), ces changements sont exclus. Pas de fusion automatique. Les réserves actuelles ne constituent pas un plan définitif de village/chemins. Les arbres utilisent encore seulement deux familles de silhouettes. Le coût GPU doit être mesuré avec la caméra joueur retenue.

## Reproduire

Ouvrir le projet de CE worktree (association moteur inchangée). La map par défaut est `Lvl_AnastasisSlice`:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' 'C:\Users\alex_\.codex\worktrees\macro-forest-001\ANASTASIS_UNREAL\Anastasis_UnrealV2.uproject'
```

Tests ciblés: `tools\unreal\report-tests.ps1 -Filter 'Anastasis.Ecology+Anastasis.Terrain+Anastasis.Presentation'`. Le run de cette mission utilise les mêmes filtres et le même classificateur avec `-nullrhi`. Les processus de validation avaient `UE_SKIP_UBT_SDK_SETUP=1`, variable locale aux processus, après compilation réussie; elle évite la file des contrôles SDK concurrents, sans changement du projet ni du rendu.
Captures: définir `ANASTASIS_FOREST_OUT`, lancer l'éditeur avec `-ExecCmds="py <worktree>/tools/unreal/capture-macro-forest.py"`. Le script ne sauve pas d'asset; il mesure l'ouverture par défaut, capture A/B à caméras identiques et compare deux reconstructions.

NEXT: arbitrer les lisières depuis la caméra de jeu et réserver les accès du futur village; diversifier les couronnes seulement après validation de ces masses. Aucun sous-bois détaillé dans cette passe.
