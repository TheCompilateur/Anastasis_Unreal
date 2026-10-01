# macro-forest-001 — raccord du sol forestier, 2026-09-30

MISSION: Relier les arbres au terrain par de grandes nappes de litiere, terre et mousse,
sans changer les arbres, le relief ni la simulation. Base: `9175a37`.

FILES_OWNED:
- `Source/Anastasis_UnrealV2/WorldView/AnastasisEcologicalDressing.{h,cpp}`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisEcologicalDressingTests.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.{h,cpp}`
- `tools/unreal/ground-material.py`
- `tools/unreal/capture-forest-walk.py`
- `Content/Anastasis/Materials/M_AnastasisGround.uasset`
- `Content/Anastasis/Materials/MI_AnastasisGround.uasset`
- cette fiche

COMMIT: PENDING

Reutilisation: le materiau morphologique existant et ses bruits meso/detail, son
humidite, ses masques de pente et sa rugosite. Le placement HISM reste identique.
Le WorldEmbodiment rasterise uniquement les arbres acceptes APRES resolution du mesh
et rejet par l'eau: masque transitoire RG 2048x2048 (16 MiB par copie CPU/GPU),
lit dans le materiau par coordonnees monde XY. R porte des nappes elliptiques qui
se recouvrent, G le contact des racines. Aucun asset de texture ni decal par arbre.
Les nappes varient avec la stature, et leurs lobes avec la graine visuelle.
Le masque est independant de l'ordre des arbres. Il ne modifie aucun buffer terrain.
La couche s'efface sur les faces raides et la bande detrempee; les nappes de mousse
utilisent l'humidite et le bruit existants. L'horizon garde le materiau sans ce masque.

Comparaison: `anastasis.Dressing.ForestGround 0/1`, puis reincarner. Valeur par defaut 1.
La force par defaut du materiau seul reste 0, pour les usages sans masque dynamique.
La recette `ground-material.py` est l'autorite des deux assets: livrer sources + assets
ensemble. Aucun fichier de carte, arbre, generateur de relief ou simulateur modifie.

MEC: PENDING (compilation en file derriere d'autres chantiers). Test ajoute:
`Anastasis.Ecology.GroundMaskAnchoringAndIsolation`.
SCN: PENDING, captures A/B fixes et empreintes terrain/arbres preparees.
PLY: UNKNOWN. Pas de controle joueur ni claim de performance GPU.
INTEGRATION_RISK: WorldEmbodiment et materiau de sol sont des fichiers partages.
Ne pas ecraser d'autres modifications lors de l'integration. Ce lot reste dans le
worktree gere `C:\Users\alex_\.codex\worktrees\macro-forest-001\ANASTASIS_UNREAL`.

Preuves de cette passe: `C:\Users\alex_\.codex\visualizations\2026\09\29\01a0ef4d-f7a0-72c0-8f4d-cc4c8234f6c4\forest-ground\`.

---

# Archive — passe de forme forestière du 2026-09-30

MISSION: Affiner les arbres existants dans une direction stylisée haut de gamme, de près et à distance. Worktree réutilisé après la livraison macro initiale; base de cette passe: `ee575f8aba1249edce9b90463c7f15c167720a89`.

FILES_OWNED:
- `tools/unreal/create_tree_asset.py`
- `tools/unreal/capture-forest-walk.py`
- `Content/Anastasis/Vegetation/SM_Tree_*_01.uasset` (les huit variantes existantes et Generic, neuf fichiers)
- `Content/Anastasis/Materials/M_AnastasisBark.uasset`
- `Content/Anastasis/Materials/M_AnastasisVegetation.uasset` (régénéré par la recette, graphe fonctionnel conservé)
- `AGENTS.md` (index de l'outil de capture)
- cette fiche

COMMIT: BRANCH_HEAD

Réutilisation: grammaire GeometryScript, palette, deux matériaux, registre et HISM du projet. Aucun nouveau système de placement. Les troncs sont affinés, avec contreforts de racines; les couronnes reçoivent des rameaux et feuilles pliées opaques. L'écorce obtient des fissures procédurales et une normale dérivée, sans texture externe. La graine de forme est fixée par nom de variante. Les neuf bornes Z restent [-50,+50], pour préserver le contrat d'ancrage. Aucun Source/, Config/, map, simulateur, terrain, bâtiment ou PNJ modifié.

Trois LOD par arbre: géométrie proche, réduction à 50% à la taille écran .22, enveloppe lointaine composée à .055. L'enveloppe lointaine évite la perte de couverture observée en supprimant simplement 93% des feuilles. Conifère canopée: 12534 / 6267 / 558 triangles; feuillu canopée: 9610 / 4804 / 450. Les tailles de meshes ne sont pas des triangles effectivement soumis au GPU.

MEC: génération complète `RESULT::PASS meshes=9`, sauvegardes et rechargement confirmés. Suite ciblée complète: 40 PASS, 0 KNOWN_EXPECTED_FAILURE, 0 FAIL, 0 incomplet, sortie 0. Filtres: `Anastasis.Ecology+Anastasis.Terrain+Anastasis.Presentation+Anastasis.Places`. Inclut `TreePivotConvention`, `TreeMaterialSlots`, `MacroForestCanonicalRelief` et `HumanGeography.CollisionAndDressing` (5/5 points, erreur maximale 0.000195 cm, arbre maximal 2707.8 cm). Index des outils: aucun Missing/Stale. Pas de modification C++ dans cette passe; Editor compilé sur la base indiquée au début du travail.

SCN: OBSERVED/PARTIAL. Trois vues fixes comparables en éditeur, 1600x900: vallée, rive, intérieur. 16120 arbres dans les six HISM existants. Captures finales terminées, mêmes caméras que le témoin. Les feuilles, rameaux, contreforts et fissures sont visibles; les masses lointaines restent présentes. Sol nu, petits buissons anciens, ombres très sombres et raccords de LOD en mouvement restent à travailler. Ce n'est pas une certification artistique AAA.

Preuves hors Git, sous `C:\Users\alex_\.codex\visualizations\2026\09\29\01a0ef4d-f7a0-72c0-8f4d-cc4c8234f6c4\forest-aaa\`:
- `generate-6.log`: génération complète finale, sortie 0, en éditeur vivant avec `-nullrhi` (pas un commandlet Python).
- `before/{valley,edge,interior}.png` et `final/{valley,edge,interior}.png`: comparaison retenue. La quatrième ancienne caméra crown visait trop haut et est exclue.
- `final/capture.log`, `final/report.json`, `comparison-cameras.json`: identité du worktree, nombre d'instances, LOD rechargés, caméras, fin du run.
- `verified/capture.log`, `verified/report.json`, `verified/exit-code.txt` et les trois PNG: seconde ouverture complète, sortie 0, mêmes 16120 arbres et mêmes LOD. C'est le run de livraison. Les anciens champs `instance_transform_sha256` de ces deux rapports utilisaient la représentation texte des structs Unreal et ne constituent pas une preuve de répétabilité interprocessus; l'outil est corrigé pour sérialiser les nombres triés. Pas de claim de répétabilité fondé sur ces anciens champs.
- `automation.log`, `tests.json`: résultats ciblés, classés par `automation-log.ps1` et le registre officiel.

La génération 5 a échoué par épuisement mémoire de la machine (0.03 GiB virtuel disponible, plusieurs éditeurs concurrents). La génération 6 a réécrit les neuf assets et réussi. Aucun autre éditeur fermé. Le run visuel `final` contient des ensures UE de détection d'accès concurrents au démarrage, avant le script; il a ensuite produit les trois images et marqué completed=true, mais son processus a rendu -1073741819 après fermeture du log. Les images sont observées, la fermeture de ce premier processus n'est pas PASS. Le contrôle `verified`, sans changement d'assets, a produit les trois captures puis fermé avec sortie 0. Aucune erreur de compilation du matériau observée. Pas de seal de santé globale moteur.

PLY: UNKNOWN. Aucune marche jouée ni validation humaine. PERF: UNKNOWN; les temps de callback de l'éditeur incluent la charge concurrente de la machine et ne permettent pas de conclure sur le coût GPU. L'ancienne clé `resident_lod0_triangle_upper_bound` des essais est mal nommée: il s'agissait d'une somme instance×triangles LOD0. L'outil livré la nomme `instance_weighted_lod0_triangle_upper_bound`; ce n'est ni la mémoire résidente, ni une mesure de soumission.

INTEGRATION_RISK: assets binaires partagés avec tout autre chantier d'arbres/matériaux. Intégrer la recette et les onze assets ensemble. Ne pas régénérer depuis une ancienne recette. Génération dans un éditeur dédié; elle refuse un commandlet avant toute écriture. Aucun seal canonique ni intégration revendiqué pour cette passe.

NEXT: composer le sol forestier avec les éléments existants, régler la lecture des ombres puis vérifier les transitions en mouvement et mesurer le GPU sur une machine disponible.

---

## Archive de la première passe macro, intégrée avant cette passe

Les preuves ci-dessous décrivent la livraison antérieure, pas les nouveaux assets.

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
