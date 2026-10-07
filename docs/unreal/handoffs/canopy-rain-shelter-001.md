# HANDOFF: canopy-rain-shelter-001

## MISSION

Relier l'interception partielle de l'averse aux couronnes réellement instanciées dans `Lvl_AnastasisSlice`. Un habitant dehors sous une couronne perd un peu moins d'énergie et de santé durant l'orage ; dehors à découvert et à l'intérieur, les règles existantes demeurent. Aucun nouvel état climatique ni asset.

## FILES_OWNED

- AGENTS.md (index du nouveau script)
- Source/AnastasisSim/ECARTS.md
- Source/AnastasisSim/Private/Life/AnastasisWeatherBehavior.cpp
- Source/AnastasisSim/Private/Tests/AnastasisWeatherBehaviorTests.cpp
- Source/AnastasisSim/Private/Village/AnastasisVillage.cpp
- Source/AnastasisSim/Public/Life/AnastasisWeatherBehavior.h
- Source/AnastasisSim/Public/Village/AnastasisVillage.h
- Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp
- Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.h
- tools/unreal/canopy-rain-pie.py
- tools/unreal/proofs.txt
- docs/unreal/handoffs/canopy-rain-shelter-001.md

## COMMIT

PENDING. Base 4579198 sur `agent/canopy-rain-shelter-001`.

## MEC

- Premier build du worktree avant modification : PASS, 213,40 s.
- Build modifié : PENDING.
- Test ciblé : PENDING.
- `python -m py_compile tools/unreal/canopy-rain-pie.py` : PASS (syntaxe seulement).
- `node tools/migration/check-ecarts.mjs -base main -handoff docs/unreal/handoffs/canopy-rain-shelter-001.md` : ECARTS::PASS, avertissements hérités des n° 2, 5, 9, 17, 33 et marques anciennes 6/14/15.
- `tools/unreal/editor-batch.ps1 -Proofs canopy-rain-pie -DryRun` : DRYRUN 1 preuve, script et registre résolus.

## PROOFS

PROOFS: canopy-rain-pie

## SCN

Capture de référence du niveau vivant : `tools/unreal/capture-slice.ps1 -Out canopy_rain_baseline.png -Cam world -TimeoutSec 300` → `CAPTURE::PASS`, fichier local `Saved/SliceEvidence/canopy_rain_baseline.png` (1 101 844 octets), inspecté. Éditeur de **ce** worktree, niveau `Lvl_AnastasisSlice` chargé ; journal : carte source 96×96, cadrage de capture 32×32, 47 arbres retenus dans la tranche. Vue aérienne : un bosquet de couronnes et une zone ouverte discernables ; elle ne démontre pas encore l'effet de pluie ni la visibilité à hauteur humaine. La preuve PIE sur la carte complète reste PENDING. Son script vérifie A/B/A du commutateur hôte à l'échantillon d'une vraie couronne et l'absence de couverture sur une case ouverte. Il ne fait aucune assertion sur l'apparence des gouttes.

## PLY

UNKNOWN. La couverture est reçue par le village et module le coût d'exposition ; aucun parcours joueur, aucune observation d'un habitant placé sous l'arbre n'est encore prouvé.

## ECARTS

n° 41 ouvert (renuméroté par relay-lot13-001 : le numéro trente-huit était déjà pris sur main) : extension hôte, interception partielle, A_TRANCHER. Village portable sans callback : couverture 0 et trajet historique. La règle ne tire aucun aléa.

## INTEGRATION_RISK

- `AnastasisSimulationSubsystem.cpp`, `AnastasisWorldEmbodiment.cpp` et `AnastasisVillage.cpp` sont des fichiers chauds. Rejouer `finish` après rebase si `main` avance ; ne pas incorporer implicitement d'autres modifications.
- La couverture dépend de l'incarnation effective des arbres et du couple seed/dimensions du monde. Les missions qui déplacent les arbres ou changent l'échelle spatiale doivent rejouer `canopy-rain-pie`.
- Le facteur maximum 20 % est un réglage de jeu. La littérature établit l'interception et le ruissellement à travers la couronne, mais pas une valeur universelle pour les essences et les orages de cette carte : https://research.fs.usda.gov/treesearch/42816 ; https://research.fs.usda.gov/treesearch/24791.

## STOP

Pas de claim de protection médicale ou de prédiction météorologique. Pas d'intégration ni de push par cette session. `queued` ne sera pas rapporté comme une preuve PIE passée.
