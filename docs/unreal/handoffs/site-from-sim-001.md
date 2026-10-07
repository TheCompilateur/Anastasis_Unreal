# HANDOFF: site-from-sim-001

## MISSION

Chantier 3 d'IRON_CRUSADE_001 (« Oui » d'Alexandre, 2026-10-07) : le site d'ouverture du village est
choisi par la simulation, et non plus par le maillage rendu. Les CVars de rendu ne déplacent plus le
village (GEO_MEASURE_001 : de 490 à 670 m avant). Changement de gameplay assumé : le village de départ
passe de (74 ; 36) à (39 ; 42).

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSurvey.{h,cpp}` : `ReadSimulation`,
  `MergeRenderObservation`, `SiteFromSimulation`, `SiteInputs` ; le sondage rendu note `RenderedSlope`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSite.{h,cpp}` : observations `RenderedSlope`,
  `SelectionSource` ; JSON `selection_source`, `rendered_slope_deg`, `water_access_rendered`
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp` : CVar `anastasis.Village.SiteSource`
  (1 = simulation par défaut, 0 = relief rendu) ; ouverture par `SiteInputs` ; plus de village absent
  quand le terrain tarde
- `Source/Anastasis_UnrealV2/Sim/AnastasisTerrainAccessProbe.cpp`, `AnastasisRiverUseProbe.cpp` : même
  choix que l'ouverture
- `Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSiteTests.cpp` : `Anastasis.SettlementSite.FromSimulation`
- `docs/unreal/SITE_FROM_SIM_001.md`
- Commit repris de `geo-measure-001` (`settlement-sensitivity-pie`, `GEO_MEASURE_001.md`) : voir INTEGRATION_RISK

## COMMIT

Commité le 2026-10-07 sur `main` = `3f945847`, avec le commit de `geo-measure-001` cherry-pické dessous.

## MEC

- BUILD: PASS (`tools\unreal\anastasis-unreal.ps1 build`, worktree)
- TESTS: `tools\unreal\report-tests.ps1 -Filter Anastasis.SettlementSite` → PASS 5 / KNOWN_EXPECTED_FAILURE 0 /
  FAIL 0, run complet. Ligne `SITE_FROM_SIM site=(39,42) score=96.00 eligible=1250 slope=1.50 water=40
  food=20 wood=0`. Le balayage du facteur de relief est dans `SITE_FROM_SIM_001.md`.
- COMMANDS:
  - `tools\unreal\editor-batch.ps1 -Proofs settlement-sensitivity-pie,settlement-site-pie,npc-life-pie,villager-pie,geography-concordance-pie,terrain-access-pie,river-use-pie,village-fabric-pie`
    → `Saved/EditorBatch/20261007-192033/`, 7 PASS sur 8 :
    - `settlement-sensitivity-pie` PASS : (39 ; 42) dans les quatre états, `drainage0` et `humangeo0` STABLE ;
    - `settlement-site-pie` PASS, ses 12 contrôles ;
    - `npc-life-pie` PASS : 8 boissons, 22 pièces, propriétaire attribué, 32 matériaux ;
    - `villager-pie` PASS ;
    - `geography-concordance-pie` PASS, 833 cases en désaccord, inchangé ;
    - `terrain-access-pie` PASS : 0 segment en anomalie sur 5 344 mesurables (2 568 sur 5 944 avant) ;
    - `village-fabric-pie` PASS ;
    - `river-use-pie` FAIL `no_meaningful_journey` : connu et documenté, il échouait déjà avant ;
      l'habitant boit à la rive simulée, à une tuile.

## PROOFS

PROOFS: settlement-sensitivity-pie, settlement-site-pie, npc-life-pie, villager-pie, terrain-access-pie, village-fabric-pie

## SCN

Village d'ouverture déplacé de (74 ; 36) à (39 ; 42), choisi par la simulation. Pente rendue sous le site :
5,3°. L'eau visée par le village n'est pas visible (`water_access_rendered = 0`) : c'est l'une des 569 cases
de la simulation que le drainage rendu assèche. Décision 2 de GEO_MEASURE_001, en attente d'Alexandre.

## PLY

N/A

## ECARTS

Non concerné : aucun C++ de `Source/AnastasisSim/`.

## INTEGRATION_RISK

- **Dépend de `geo-measure-001`**, dont le commit est repris ici, à l'identique, sous le mien :
  - verser `geo-measure-001` dans le même lot, ou avant ;
  - si `geo-measure-001` passe seule d'abord, `git cherry` reconnaît le commit et ne le rejoue pas.
- **Changement de gameplay** : toute preuve ou capture qui suppose le site (74 ; 36) le verra bouger.
  - Les 6 preuves déclarées passent sur le nouveau site.
  - Les captures `anthropic-*` et `village-fabric-capture` n'ont pas été rejouées.
- `river-use-pie` n'est pas déclarée : son FAIL est le constat.
- `anastasis.Village.SiteSource 0` rétablit l'ancien choix sans recompiler.

## STOP

- Ne règle pas l'eau invisible du village : c'est la décision sur le drainage.
- Ne garantit pas une pente rendue douce sur une autre graine : dans le top 5, deux candidats ont 23 à 33°
  rendus pour environ 1,5° simulés.
- La politique de site reste dans l'hôte Unreal ; `SeedOpening*` et l'attribution de la maison aussi.
