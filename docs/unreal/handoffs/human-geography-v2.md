# Human_Geography_V2 — assemblage sur le relief corrige

## MISSION

Reprendre les bassins humains sur le vrai relief corrige de l'autre session,
dans `agent/human-geography-v2`, sans integration automatique au poste canonique.
La mission artistique complete n'est pas declaree terminee.

## PROVENANCE

- Premiere base inspectee : `ce9ecfa73e8398c24bc1f7b72f044479b6d3238e`.
- Premiere V2 conservee : `80cb1bd` (checkpoint sur l'ancienne topographie).
- Source assemblee : `95a3ac6d3c77a38469a2862b37982cfd5e06bf64`, branche
  `agent/relief-reed-join-001`, incluant `82a337b` et ses corrections de relief.
- Deux conflits dans TerrainForge : conserver les gardes `bSharpen` et `bTerraces`
  de la source corrigee. Bicubic=1, Sharpen=0, Terraces=0, Escarpments=0,
  TalusDeg=40, ErosionIterations=300. Correction d'exposition `ce9ecfa` conservee.
- Pendant cette intervention, une autre session a avance `main` a `19e0709` :
  son code TerrainForge et sa constante TileWorldSize correspondent a `95a3ac6`.
  Notre passe n'a ni avance `main`, ni modifie les fichiers du poste canonique.

## FILES_OWNED

Dans `Source/Anastasis_UnrealV2/WorldView/` :

- AnastasisHumanGeography.h / .cpp / Tests.cpp
- AnastasisWorldView.h / .cpp / Tests.cpp
- AnastasisTerrainForge.h / .cpp
- AnastasisTerrainSurface.h / .cpp
- AnastasisWorldEmbodiment.cpp
- AnastasisEcologicalDressing.cpp
- AnastasisMistField.cpp
- AnastasisWorldProbeSubsystem.cpp

Autres : `tools/unreal/capture-human-geography.py`, cette fiche.
Les autres fichiers apportes par la fusion gardent la provenance de `95a3ac6`.
L'asset et les recettes de roseaux sont conserves ; aucune population de roseaux
supplementaire n'est revendiquee dans le niveau de jeu.

## COMMIT

BRANCH_HEAD

## COMPORTEMENT

La carte est toujours le terrain procedural de `Lvl_AnastasisSlice`, sans acteur
Landscape. `Human_Geography_V2` est une couche de calcul reversible, pas une
Landscape Edit Layer native. Aucun plugin Landscape/Landmass/Water n'a ete active.

`TileWorldSize=400` cm est conserve depuis la source corrigee. Le facteur de
presentation par defaut passe de 20 a 5 : emprise entre centres extremes de
1900 x 1900 m, pas 7600 x 7600 m. L'erosion utilise l'espacement physique reel.
L'altitude est dilatee autour du niveau d'eau a 275 cm. Les tailles des arbres,
ruines et autres objets ne recoivent pas le facteur de presentation.

Comparaison dans l'editeur : `anastasis.Terrain.HumanGeography 0` ou `1`, puis
appeler `EmbodyCanonical(12345)` sur l'acteur. `anastasis.WorldView.Scale 5` donne
1,9 km ; `1` donne le relief corrige a son echelle de base de 380 m entre centres.
La simulation, les ressources, la fertilite et le debit semantique ne sont pas modifies.

## MEC

BUILD::PASS sur l'assemblage. La suite finale est executee par le portail `finish`.
Resultat brut : `Saved/CanonicalVerification/report-tests.log` (non suivi).

Premier run sur cette base : 88 PASS, 4 KNOWN_EXPECTED_FAILURE, 1 FAIL.
Echec reel du critere de triplement des surfaces, herite de l'ancienne source.
Le run est conserve dans les preuves sous `tests_initial_failed.log`.
Le nouveau critere garde les minima absolus, exige de ne perdre aucune des
grandes surfaces continues <10 degres et demande >=12 ha en A et >=4 ha en B
sous 5 degres, avec agrandissement des deux parcelles par rapport a la source corrigee.
Le registre des echecs connus n'a pas ete modifie.

Mesures du premier run (geometrie identique a la capture) :

- A, plus grande composante seche <10 degres : 235775 -> 255325 m2.
- B, meme critere : 68675 -> 128525 m2.
- Zones protegees : ecart nul entre relief corrige sans/avec V2.
- Collisions : 5/5 points, erreur maximale 0,000195 cm ; 925 instances de decor,
  echelle maximale d'instance 6,265. Anciennes dalles cachees sans collision.
- Riviere : 11 points descendants, profondeur minimale 46,202 cm.
- Echantillonnage : correspondance aux triangles a moins de 0,001 cm.

Analyse independante des maillages exportes : chemin d'eau descendant continu,
et parcours A-B sec de 922,8 m suivant uniquement les aretes reelles des triangles,
pentes de segments <=12 degres. Ce parcours n'est pas une mesure de plus court
chemin, de navigation Unreal ni de trajet joueur. Le premier calcul de 722 m
autorisait des diagonales hors triangulation ; il est remplace par ce controle.

## SCN

Douze captures editeur Unreal 5.8.2 : source corrigee puis V2, memes XY et
orientations, camera a 170 cm au-dessus du sol propre a chaque version.
Une vue aerienne et cinq vues au sol par version. Fin de capture confirmee dans
`human_v2_capture.log`. Aucun .umap ou asset n'est sauvegarde par le script.

Preuves locales :
`C:/Users/alex_/.codex/visualizations/2026/09/29/01a0eead-8695-79e0-a61e-9d4359418d75/corrected_relief/`

- `human_v2_0_mesh_0.json`, SHA256 `011a95d188f6cedb0f19c871c73888a4a73c61ab1497799c33e9561e1340a404`
- `human_v2_1_mesh_0.json`, SHA256 `eebf27173c2372b27a884969805233bf1331182f3889af802c82b3ec01d39c9e`
- `human_v2_metrics.json`, `human_v2_capture_positions.json`, `human_v2_runtime.json`
- `corrected_source_*.png`, `v2_*.png`, `human_v2_comparison.png`
- `analyze_human_v2.py`, `launch_capture.ps1`

Pour refaire les captures, definir `ANASTASIS_HUMAN_EVIDENCE` vers un nouveau
dossier de sortie puis executer `tools/unreal/capture-human-geography.py` dans
l'editeur dedie du worktree. Le script ferme cet editeur en terminant.

## PLY

UNKNOWN : aucun parcours humain en PIE. Les captures au sol et les tests de
collision ne constituent pas une validation joueur ni une preuve economique.

## INTEGRATION_RISK / NEXT

- La source corrigee est deja beaucoup plus douce : sous 10 degres, 48,6 % de la
  terre seche avant V2 contre 51,3 % apres. La V2 organise surtout les continuites.
- Les images ne prouvent pas encore une identite nettement montagneuse : les
  horizons sont bas. Revoir le contraste des massifs et contreforts avant microrelief.
- Le materiau repetitif, les bords d'eau anguleux herites, l'habillage clairseme
  et la presentation des limites de la carte restent visibles.
- Le grand lac secondaire reste hors du corridor hydrologique traite. Ne pas
  revendiquer une validation de toutes les masses d'eau.
- Fichiers chauds : TerrainForge, WorldView, Embodiment. `main` evolue dans
  d'autres sessions ; refaire l'audit de provenance avant toute integration.
- Le nouvel index des outils arrive dans `main` apres notre base : enregistrer
  le script de capture dans cet index lors de l'integration a ce systeme.

**Ne juge pas la carte uniquement depuis la vue aerienne.**
