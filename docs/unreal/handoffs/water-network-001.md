# HANDOFF: water-network-001

## MISSION

Décision d'Alexandre, 2026-10-08 : « l'eau doit respecter son réseau ». La simulation prend l'eau du
réseau de drainage, celle que le joueur voit, calculée à partir de la seule graine avec une recette fixe,
sans CVar de rendu. Le site de départ lit le relief drainé de cette même géographie. Détail et preuves :
`docs/unreal/WATER_NETWORK_001.md`.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisCanonicalGeography.{h,cpp}` (nouveau) et
  `AnastasisCanonicalGeographyTests.cpp` (nouveau, `Anastasis.WaterNetwork.Canonical`)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisProjectedMesh.h` (nouveau) : échantillonneur partagé
- `Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainForge.{h,cpp}` : `FSettings`, `Apply(..., Settings)`.
  L'ancien `Apply` délègue avec `FromConsole()`, sans changement de comportement.
- `Source/Anastasis_UnrealV2/WorldView/AnastasisDrainage.{h,cpp}` : `FParams::ForgeExaggeration`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSurvey.{h,cpp}` : échantillonneur partagé ;
  `ReadSimulation(..., Canonical)` ; `SiteInputs` lit le relief canonique
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp` : CVar `anastasis.Sim.WaterNetwork`
  (1 par défaut), application au reset, ligne `ANASTASIS_WATER_NETWORK`
- `Source/AnastasisSim/Public|Private/World/AnastasisWorld.*` : `RestampWater`
- `Source/AnastasisSim/Public|Private/Sim/AnastasisSimulation.*` : `ApplyWaterMask`
- `Source/AnastasisSim/Private/Tests/AnastasisWaterRestampTests.cpp` (nouveau) :
  `Anastasis.Sim.Monde.Eau.Reseau.Restamp`, `.Simulation`
- `Source/AnastasisSim/ECARTS.md` : n° 44
- `docs/unreal/WATER_NETWORK_001.md`

## COMMIT

Commité le 2026-10-08. La branche porte, sous ce commit, les commits de `site-from-sim-001` (relais).

## RELAIS

RELAIS: site-from-sim-001

## MEC

- BUILD: PASS (`tools\unreal\anastasis-unreal.ps1 build`, worktree)
- TESTS: `report-tests.ps1 -Filter 'Anastasis.WaterNetwork+Anastasis.Sim.Monde.Eau+Anastasis.SettlementSite+Anastasis.Sim.Tick+Anastasis.Terrain'`
  → PASS 58 / KNOWN_EXPECTED_FAILURE 0 / FAIL 0, run complet.
  - `WATER_NETWORK seed=12345 network_water=893 js_water=1198 both=629 changed=833 rivers=19 lakes=8`
  - `WATER_NETWORK_SITE site=(36,20) score=96.00 eligible=840 slope=0.61 water_m=40`
  - La suite complète n'a pas été jouée : elle attend le lot.
- COMMANDS:
  - `editor-batch.ps1 -Proofs geography-concordance-pie,settlement-sensitivity-pie,settlement-site-pie,npc-life-pie,material-courier-pie,river-use-pie,terrain-access-pie,villager-pie,village-fabric-pie`
    → `Saved/EditorBatch/20261008-130148/`, 8 PASS sur 9 :
    - **concordance : 0 désaccord sur 9 216** (833 avant) ;
    - sensibilité du site : (36 ; 20) STABLE sous `Drainage 0` et `HumanGeography 0` ;
    - `river-use-pie` FAIL `no_meaningful_journey`, le constat attendu (règle `ShoreReach` de la référence).
  - A/B de `material-courier-pie` en quatre états (eau JS ou réseau, site par rendu, tuiles ou relief
    drainé) : voir `WATER_NETWORK_001.md`.
  - `node tools/migration/check-ecarts.mjs` → `ECARTS::PASS`

## PROOFS

PROOFS: geography-concordance-pie, settlement-sensitivity-pie, settlement-site-pie, npc-life-pie, material-courier-pie, terrain-access-pie, villager-pie, village-fabric-pie

## SCN

- Le village de départ passe en (36 ; 20) : pente rendue 0,61°, eau visible à 40 m.
- La simulation et le rendu ont la même eau à tous les centres de tuiles.
- `anastasis.Sim.WaterNetwork 0` rend l'eau JS, pour une A/B ou un retour arrière.

## PLY

N/A

## ECARTS

n° 44 OUVERT (EXTENSION, A_TRANCHER) : l'eau du monde suit le réseau de drainage canonique ; appelé par
l'hôte seulement. `GenerateWorld` et `Reset` ne changent pas, la parité est intacte.

## INTEGRATION_RISK

- **Relais de `site-from-sim-001`**, non versé (le lot de 11 h l'a laissé dehors).
  - `opening-in-sim-001` porte aussi ces commits, en relais.
  - Les deux missions modifient `AnastasisSimulationSubsystem.cpp` dans des fonctions différentes
    (`ResetCanonical` ici, `SeedOpening*` là-bas). Le conflit, s'il y en a un, est textuel.
- **Changement de gameplay** : l'eau change sur 833 tuiles, le site de départ bouge. Les preuves
  déclarées passent ; les captures n'ont pas été rejouées.
- **Coût** : 2 à 3 s de calcul au premier reset d'une graine dans un processus (forge et drainage refaits).
- **Numérotation** : n° 40 est pris par `opening-in-sim-001` (non versé), n° 41 par `canopy-rain-shelter-001`
  (versé pendant la mission), n° 42 par `relay-settlement-001` (versé ensuite), n° 43 par `labor-social-001` (versé le 2026-10-08), n° 44 par celle-ci (renumérotée à trois rebases).
- **Fiabilité de `npc-life-pie`** : `opening-in-sim-001` l'a vue échouer sur `main` = `70237070`, sur une
  machine chargée. Elle passe ici : base `4d57ad49`, machine moins chargée. Je soupçonne son délai de
  220 s en temps réel, mais ce n'est pas établi.

## STOP

- Ne change pas la règle de boisson de la référence (`ShoreReach`) : `river-use-pie` reste en échec.
- **Rivières étroites** : elles sont lues au centre des tuiles de 20 m. Une rivière de 6 m peut tomber
  entre deux centres et devenir un gué.
- Les types tirés de l'humidité de fond (champs, forêt) ne sont pas recalculés pour la nouvelle eau.
- Le calcul canonique n'est pas partagé avec l'incarnation : le travail est fait deux fois.
