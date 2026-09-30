# macro-forest-001 — forêt de la vallée actuelle

MISSION: Première passe de macro végétation, sans modifier la topographie ni le simulateur PNJ.

FILES_OWNED:
- AGENTS.md (une ligne dans l'index des outils)
- Source/Anastasis_UnrealV2/WorldView/AnastasisEcologicalDressing.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisEcologicalDressing.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisEcologicalDressingTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisHumanGeographyTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp
- tools/unreal/capture-macro-forest.py
- docs/unreal/handoffs/macro-forest-001.md

COMMIT: BRANCH_HEAD

## Base et inspection

Base validée: `50a66d82b1f67b7a2f005e59c7572525967ebcf6`, avec Human_Geography_V2 et l'étendue de 1,9 km. Branche: `agent/macro-forest-001`.
Worktree: `C:\Users\alex_\.codex\worktrees\macro-forest-001\ANASTASIS_UNREAL`.
Ce chemin provient du gestionnaire Codex; l'opérateur historique refuse ce préfixe. Le build utilise directement le même Build.bat UE 5.8.2. Aucun opérateur modifié, aucun seal canonique revendiqué.

Map inspectée en éditeur: `/Game/Anastasis/Maps/Lvl_AnastasisSlice`, un `AnastasisWorldEmbodiment`, surface procédurale TerrainForge issue du snapshot 96×96. Aucun acteur Landscape/PCG/foliage peint dans cette scène. Le placement écologique et les HISM existent déjà, ainsi que huit variantes conifère/feuillu × quatre statures et leurs matériaux. Référence Three.js consultée en lecture seule: `forestGenerator3d.js` et `landForest3d.js`, pour les masses et clairières; aucun port du renderer.

## Implémentation

Extension du placement existant avec échantillonnage du relief et de l'eau réellement rendus. Les épaules rocheuses deviennent plantables, avec pente maximale de 48°. Les falaises, eau, champs et ruines sont exclus. Les poids EXISTANTS HumanGeography réservent les vallées et la rivière; le bassin conserve une ouverture de 8 tuiles et une transition de 4 tuiles. Les prairies basses et plates restent ouvertes.

Masses corrélées sur 14 tuiles; espacement minimal des troncs de 500 cm indépendamment de SpatialScale; hauteur ×3,5 appliquée aux enveloppes existantes; majorité de canopées et émergents. Troncs verticaux, azimut variable, pieds ancrés par les bornes réelles des meshes. Maximum de 128 candidats par tuile, placement déterministe, HISM existants, aucun acteur par arbre. Ce peuplement est de la présentation: aucune ressource ou sémantique de simulation ajoutée.

Activation par défaut à l'ouverture de la map et en BeginPlay. Pour l'A/B, reconstruire après `anastasis.Dressing.MacroForest 0` (grammaire antérieure) ou `1` (macro forêt). Réglages dans `ForestDressing`, catégorie Macro.

## Preuve mécanique

MEC: BUILD::PASS, empreinte Source/Config inchangée pendant la compilation. Suite ciblée complète: 36 PASS, 0 KNOWN_EXPECTED_FAILURE, 0 FAIL, 0 manquant, sortie 0. Filtres: `Anastasis.Ecology+Anastasis.Terrain+Anastasis.Presentation`. Classification par `tools/unreal/automation-log.ps1` et le registre officiel, sans modification des marqueurs attendus. Run `-nullrhi`: ce résultat ne prouve pas le rendu.

Nouveaux tests:
- `Anastasis.Ecology.MacroForestRenderedHabitat`: relief rendu, eau élevée, falaises, bassin, champs, ruines, espacement et déterminisme.
- `Anastasis.Ecology.MacroForestCanonicalRelief`: trois graines à SpatialScale=5, racines, eau et pentes réelles, réserves HumanGeography, terrain inchangé, répétabilité.

| Graine | Arbres | Canopées, émergents compris | Sur tuiles Stone |
|---|---:|---:|---:|
| 12345 | 16123 | 13428 | 5159 |
| 42 | 24534 | 20092 | 9557 |
| 98765 | 13377 | 10789 | 1359 |

`Anastasis.Terrain.HumanGeography.CollisionAndDressing` distingue maintenant la stature physique des arbres du scale des autres objets: l'ancien proxy mesh-scale <10 interdisait les grands arbres voulus. Nouveau garde-fou: arbres sous 30 m, autres objets scale <10, collisions toujours contrôlées. Résultat: 5/5 sites, erreur maximale 0,000195 cm; scale autres 1,099; arbre maximal 2707,8 cm. Cela détecte encore une multiplication accidentelle par SpatialScale=5.

Preuves brutes finales, hors Git:
`C:\Users\alex_\.codex\visualizations\2026\09\29\01a0ef4d-f7a0-72c0-8f4d-cc4c8234f6c4\`
- `build-delivery.log`
- `automation-delivery.log`
- `tests-delivery.json`
- `delivery/forest-scene.json`, `delivery/capture.log`, quatre images A/B.

Les runs antérieurs et les captures sur l'ancienne étendue ne sont pas la preuve de cette livraison.

SCN: OBSERVED — quatre captures A/B terminées, vues aérienne et depuis la vallée à 170 cm. Masses boisées visibles sur les reliefs et pourtours, deux grandes vallées ouvertes. Les silhouettes restent répétitives et la forêt est encore une première passe artistique.

Ouverture par défaut: 16120 arbres effectivement instanciés (le plan en contient 16123, avant les filtres finaux existants), dans six variantes HISM de sous-canopée/canopée/émergents. Hauteurs min/médiane/max: 7,70 / 14,17 / 27,08 m. `completed`, `default_matches_macro`, `repeat_matches` et `terrain_unchanged` sont tous vrais. Empreinte des arbres identique à l'ouverture et après reconstruction: `761467322d424df6fe4e1c5267de826d914961f69852e3216efd2792a00824ae`. Le SHA256 des sommets et triangles de terrain reste identique entre les modes A/B. Le jugement visuel porte sur ces deux caméras d'éditeur, pas sur une partie jouée.

PLY: UNKNOWN. Performance GPU et validation humaine non établies.

## Intégration et ouverture

INTEGRATION_RISK: Pas de fusion automatique dans le canonique. `AnastasisWorldEmbodiment.cpp` est partagé avec d'autres chantiers: examiner le diff depuis la base indiquée. Aucun changement de Content, Config, AnastasisSim, TerrainForge ou HumanGeography runtime. Les modifications concurrentes du canonique sont exclues.

Ouvrir le projet du worktree avec le moteur existant; la map par défaut contient le peuplement:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' 'C:\Users\alex_\.codex\worktrees\macro-forest-001\ANASTASIS_UNREAL\Anastasis_UnrealV2.uproject' -ModelContextProtocolPort=18731
```

Captures reproductibles: définir `ANASTASIS_FOREST_OUT`, puis lancer cet éditeur avec `-ExecCmds="py <worktree>/tools/unreal/capture-macro-forest.py"`. Le script ne sauvegarde aucun asset. Il mesure l'ouverture par défaut, compare les modes à caméras identiques, reconstruit une seconde fois et compare le terrain. Les processus de validation utilisent `UE_SKIP_UBT_SDK_SETUP=1` localement, après le build réussi, et des ports MCP dédiés 18731/18732 pour ne pas commander les autres éditeurs.

NEXT: arbitrer les lisières et accès depuis la caméra de jeu; diversifier les couronnes existantes si la répétition reste trop visible; mesurer le coût GPU avant une nouvelle densification. Les réserves actuelles ne sont pas encore un plan définitif de village ou de chemins. Aucun sous-bois détaillé dans cette passe.
