# ASTRAL — ENV-001 REPORT

## VERDICT

PARTIAL. Première grammaire forestière fonctionnelle, déterministe et observée dans
Unreal. La lisière et les ouvertures progressent, mais une seule silhouette et
l'absence de sous-bois/sol forestier laissent un paysage schématique. Aucun PASS
global de crédibilité écologique ni de performance GPU.

## BASELINE

- Canonique initial : C:/dev/ANASTASIS_UNREAL, main,
  e96175f95afb753fa746ecce05f3d927baf7895a.
- État initial : Config/DefaultEditor.ini modifié ; .claude/ non suivi. Préservés.
- Travail : agent/astral-env-001, C:/dev/ANASTASIS_WORKTREES/astral-env-001.
- Base canonique reprise après inspection : 2cf1328c5de4b5b0fdd880b3bf26f279753582a4.
- Code final compilé/testé/capturé : fe1b665 ; le commit final de rapport et de
  diagnostic Python ne change pas le C++ ni les assets.
- Évolutions de main inspectées pendant la mission :
  7ddae7b (ruine), 4531373 (construction éditeur/unicité), 2cf1328 (arbre branché).
  Deux conflits résolus dans le seul worktree : conserver Ecology + Surface=2,
  et ForestDressing + OnConstruction. Aucun écrasement ni écriture sur main.
  Les mouvements suivants 5321a97 et 293efad concernent l'outillage, pas ce rendu.
- Worktrees concurrents observés : asset-agent-002, atmosphere-light-001,
  atmosphere-mist-002, multi-agent-control-001, puis trunk-integration/-002.
  Intersection WorldEmbodiment avec multi-agent-control inspectée avant combinaison.

## DISCOVERY

Pipeline réel :
AnastasisSim::GenerateWorld(seed,96,96) -> WorldView snapshot/crop ->
AAnastasisWorldEmbodiment -> ProceduralMeshComponent (terre/eau) +
PresentationRegistry/Resolver -> HISM par archétype/variante.
Aucun consommateur PCG, Landscape, Foliage ou MPC trouvé dans ce pipeline.

Données : Type, Alt, Wetness, Shore, Flow, Fertility, Resource/Amount.
ForestMargin est invalidé par le cap forestier du worldgen : non consommé.
La pente provient du triangle rendu. Pas de donnée d'influence humaine inventée.

| Catégorie | Emplacement / utilisation réelle | Qualité / rôle |
|---|---|---|
| Arbre | /Game/Anastasis/Vegetation/SM_Tree_Generic_01 | Chargé dans Unreal ; un LOD, Z -50..50, HISM ; silhouette tronc+cône, trois tailles de composition |
| Présentation | /Game/Anastasis/Presentation/DA_AnastasisPresentation | Seule autorité des références ; audit live final confirme Tree et Ruin dédiés |
| Ruine | /Game/Anastasis/Architecture/SM_Ruin_Generic_01 | Asset intégré par l'autre chantier ; placement ASTRAL inchangé |
| Sol/berge/eau | /Game/Anastasis/Materials/M_AnastasisSlice | Matériau existant, couleurs de sommets, Shore et eau plane ; inchangé |
| Arbustes/herbes/fougères/bois mort/roseaux/rochers écologiques | Aucun mesh dédié trouvé dans le canonique inspecté | Non créés, non simulés par des objets impropres |
| Références | docs/visual/reference/ | Direction artistique, pas assets runtime ni instructions autonomes |
| Prototypage | Content/LevelPrototyping/ | Primitives présentes, non détournées en assets écologiques |

L'audit a distingué présence et branchement : sur e96175f, Tree_Generic était
chargeable mais le registre rendait encore Cone. Les preuves finales utilisent
les branchements effectivement intégrés dans 2cf1328.

## ARCHITECTURE IMPLEMENTED

- FAnastasisForestDressingSettings : paramètres regroupés sur WorldEmbodiment,
  éditables dans Details, aucun nouveau Data Asset ou Blueprint.
- AnastasisEcologicalDressing::Build : plan pur sur le snapshot canonique complet ;
  pas de RNG simulation, pas d'asset path, pas d'Actor, rejet avec chemin explicite
  des entrées invalides.
- WorldEmbodiment garde le snapshot complet pour le contexte, puis émet uniquement
  dans l'emprise réellement rendue. Le crop et la caméra ne reconfigurent pas la forêt.
- Plan forestier -> présentation Forest existante -> scale de strate -> ancrage sur
  SampleHeight et minimum Z du mesh -> HISM existants.
- Les matériaux sont préparés une fois par composant et reconstruction, au lieu
  d'allouer un MID par tuile. Pas de Tick ajouté.
- Mode historique conservé : anastasis.Dressing.Ecology 0, puis réincarner.
  Mode candidat : anastasis.Dressing.Ecology 1 (défaut), sur terrain continu.
  ForestDressing.bEnabled permet également le retour local au comportement ancien.
- Aucun changement sémantique dans Source/AnastasisSim, worldgen ou ressources.
  Les petites silhouettes sont des classes de présentation, pas des âges simulés.

## VISUAL RESULT

A/B final : même binaire, scène, seed 12345, emprise96x96, caméra et éclairage.
Soleil75000 lux, EV10014, fog retiré de la scène temporaire, bloom/vignette/motion
blur à zéro. Aucun asset/map sauvegardé par le script.

Observé : disparition d'une partie du mur uniforme, grandes silhouettes regroupées,
petits sujets vers les zones ouvertes, dégagement des berges/pentes.
Limites : cœur encore sombre et compact, silhouette unique répétée, sol nu et
couleurs sémantiques visibles, pas de véritables arbustes/litière/bois mort.
Le cadrage overview est trop distant pour juger les détails ; la vue edge sert
de preuve perceptuelle. Aucun changement de lumière destiné à masquer ces limites.

Dossier de preuve absolu :
C:/Users/alex_/.codex/visualizations/2026/09/14/01a09d80-2bbb-75f0-8f3f-b5b2b6ca6ef9/

Captures finales : astral-env-001-current/A_edge.png, B_edge.png,
A_overview.png, B_overview.png.
Métadonnées : astral-env-001-current/observation.json.
Logs : astral-observe-current.log, astral-tests-final.log, astral-build-final.log.
Le dossier astral-env-001 initial documente une ancienne passe aux primitives :
NE PAS le présenter comme le résultat final.
Le premier diagnostic court astral-pie-audit compare accidentellement OFF à ON
pour son champ de hash ; il est impropre comme preuve DET. Utiliser observation.json
de la capture complète ou le diagnostic corrigé astral-pie-final.

## ECOLOGICAL RULES

1. Habitat admissible : Forest/Grass/Scrub ; exclusion Water/Stone/Field/Ruin.
2. Support forestier pondéré par la distance aux centres Forest voisins.
   Les jeunes sujets peuvent donc déborder la classe Forest sans créer de ressource.
3. Champ de variation spatiale interpolé, indépendant de la RNG simulation :
   ouvertures locales et densités cohérentes, pas seulement tirages indépendants.
4. Probabilité = Density * sqrt(support) * patch * (1 - WetnessPenalty * Wetness).
5. Gradient de maturité visuelle à partir du support ; trois enveloppes d'échelle.
6. Rejet de la pente exacte du triangle, du sol sous le niveau d'eau + marge,
   et des points sans support sous le centre et quatre points racinaires.
7. Espacement minimal par index spatial ; ordre canonique déterministe.

| Paramètre | Défaut |
|---|---|
| CandidatesPerTile | 4 maximum |
| EdgeRadius | 2,5 tuiles |
| Density | 0,72 |
| ClusterSpan / ClearingThreshold | 4 tuiles / 0,28 |
| MaxSlopeDegrees | 35 degrés |
| WetnessPenalty | 0,55 |
| WaterClearanceUU | 12 cm |
| MinimumSpacing / RootRadius | 0,55 / 0,12 tuile |
| YoungScale | 0,25–0,45 fois l'enveloppe du registre |
| SecondaryScale | 0,50–0,78 |
| CanopyScale | 0,95–1,20 |

## PERFORMANCE

- Éditeur A/B : 6 Actors de scène dans les deux cas, dont un embodiment.
- Dressing : 1162 -> 886 instances = 438 arbres + 448 ruines.
- Forêt : 180 jeunes, 129 secondaires, 129 dominants.
- Deux HISM vivants en éditeur et en PIE (438 Tree, 448 Ruin), aucun Actor par arbre.
- Le compteur interne DressingMeshes.Num() logge quatre slots en PIE ; l'inspection
  get_components_by_class n'en trouve que deux vivants, total886. Dette de compteur/
  références lors de duplication ; pas de double peuplement observé.
- Génération CPU observée, session finale : OFF18,365–19,958ms ;
  ON14,013–31,966ms. Peu d'échantillons, machine concurrente, coûts par reconstruction,
  pas temps de frame ni preuve de gain.
- Draw calls réels, frame GPU, VRAM, longues sessions et densités extrêmes : UNKNOWN.
- Un seul LOD sur l'arbre ; aucune affirmation de gain Nanite ou de tenue à grande densité.

## VALIDATION

| Gate | Résultat et portée |
|---|---|
| BUILD | PASS : compilation Unity propre, puis cache de build contrôlé au portail finish |
| LAUNCH | PASS : éditeur dédié, captures, PIE actif, sortie0 |
| [SCN] | PASS borné : un embodiment PIE, deux HISM vivants,886 instances ; ancrage et exclusion testés |
| [VIS] | FAIL pour la cible complète de paysage écologiquement crédible ; progrès A/B réel, encore schématique |
| [PERF] | UNKNOWN pour le rendu GPU ; inventaire et coûts CPU ci-dessus seulement |
| [DET] | PASS : plan répété et hash des transforms numériques identique |

Portail finish sur fe1b665 : HANDOFF_READY::YES.
Suite : 51 PASS / 4 KNOWN_EXPECTED_FAILURE / 0 FAIL, total55.
Les quatre divergences du registre restent séparées :
Fbm, SemantiqueJs et les deux expectedFailure Python AnastasisInspect.

Tests significatifs :
- Anastasis.Ecology.DeterminismAndAnchoring :438 instances ; même ordre, positions,
  scale/strate/seed, ancrage, espacement, snapshot inchangé.
- Anastasis.Ecology.EdgeAndConditioning : frange40 / intérieur de même largeur181 ;
  sec2242 / humide1289 ; immergé0 / pente impossible0.
- Anastasis.Ecology.RejectInvalidInput :NaN refusé à Source.Tiles[17],
  configuration invalide et faux contexte crop refusés.
- Anastasis.Terrain.DressingOnGround : contrat historique conservé explicitement,
  erreur d'ancrage0 ; les tests terrain/parité restent verts ou attendus au registre.
- Anastasis.Visual.SingleEmbodiment et Anastasis.Level.HoldsNoWorldTruth : PASS.
- Hash final :4d22d7d9879c140cb3f922dc0b4ffe52f421547a0fad096ccc9a190d54452810.

## FILES CHANGED

Diff ASTRAL exact contre 2cf1328, racine C:/dev/ANASTASIS_WORKTREES/astral-env-001 :

- Source/Anastasis_UnrealV2/WorldView/AnastasisEcologicalDressing.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisEcologicalDressing.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisEcologicalDressingTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainSurfaceTests.cpp
- tools/unreal/astral-observe.py
- docs/unreal/ASTRAL_ENV_001.md

Aucun asset généré ou binaire commité par ASTRAL. Les assets et changements de
cycle de vie repris du canonique gardent leur provenance dans le commit de merge.

## COMMITS

- bb883e6 : grammaire forestière, raccord HISM, tests et observation.
- 73ebc92 : combinaison isolée avec le canonique inspecté2cf1328.
- fe1b665 : correction du type de plan des tests pour compilation Unity.
- Commit de clôture : rapport et extension du diagnostic PIE uniquement.
Aucun push, aucune intégration dans main.

## REGRESSIONS

Aucune régression détectée par la suite. Le défaut de double embodiment de
l'ancienne base a été éliminé en reprenant le correctif canonique, confirmé en PIE.
Les risques non tranchés ne sont pas des PASS : performances GPU, silhouettes
répétées, lisibilité du sol et stabilité des compteurs sur de longues reconstructions.

## FOLLOW-UP OPPORTUNITIES

| Travail non réalisé | Bénéfice | Coût estimé | Dépendances | Priorité |
|---|---|---|---|---|
| Transition du sol forêt/lisière | Relier visuellement la végétation au terrain nu | 1 passe bornée | Propriété du matériau/terrain, même A/B | Haute |
| Nettoyer/distinguer slots et HISM vivants | Métrologie exacte après duplication PIE | Petit correctif | Propriétaire cycle de construction | Moyenne |
| Variantes botaniques/sous-bois | Réduire la répétition et matérialiser les strates basses | Moyen, disponibilité inconnue | Audit/import d'assets existants avant toute création | Moyenne |
| Profil GPU à densité accrue | Décider culling/LOD sur mesure réelle | 1 campagne bornée | Scène figée, machine disponible | Moyenne |
| Berges humides et bois mort | Élargir la grammaire écologique | Moyen | Assets réellement disponibles | Ultérieure |

## NEXT RECOMMENDED MISSION

ASTRAL-ENV-002 : une transition de sol forêt -> lisière sur la même zone, en
réutilisant le matériau existant. Paramètres et caméra figés, aucun nouvel éclairage,
aucune extension à l'hydrologie complète. Trancher par A/B si les strates végétales
et leur sol commencent réellement à former un même milieu.
