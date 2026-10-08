# HANDOFF: settlement-morphogenesis-001

> **Relayé par `relay-settlement-001` (2026-10-08).** Écart renuméroté (38 sur la branche d'origine, pris sur main par le monde extérieur) ; le compteur de passage est celui de nav-wiring-001 (un seul `RecordPassage` dans `AnastasisVillageNav.cpp`, un seul `FNpc::TrafficTimer`, `Traffic` passé en `float`). Voir `docs/unreal/handoffs/relay-settlement-001.md`.

## MISSION

ANASTASIS_SETTLEMENT_MORPHOGENESIS_001 (mandat d'Alexandre, 2026-10-07) : que la forme du village soit la
conséquence de la simulation (foyers, travail, passage, temps), pas d'un tirage. Porte 0 (carte des
propriétaires), puis tranche verticale sur le village d'ouverture. Rapport complet :
`docs/unreal/SETTLEMENT_MORPHOGENESIS_001.md`.

Établi :
- **Passage → sentier** (simulation, port de la référence JS) : `sim.traffic` (+1 par 0,85 s de marche, f32,
  plafond 180), décroissance de minuit, sentier de désir (≥ 14 passages, 18 nuits d'effort) → case `Road`,
  coût de marche du profil (sentier 0,86, ruelle 0,74 si le passage de naissance ≥ 46), version de nav.
- **Biographie des bâtiments** (présentation, par observation) : fondateur, métier, foyer, achèvement,
  changements de mains, nuits pleines, vides. La forme d'une maison se fixe quand un foyer la prend et la garde.
- **Projection** : archétype = programme de la biographie ; patine (`Weathering`) = âge depuis l'achèvement ;
  sentiers rendus en bande de terre battue jusqu'aux portes des parcelles, herbe écartée dessous.
- **Retiré** : le choix de typologie par graine d'identifiant (architecture-crusade-001).

## FILES_OWNED

- `Source/AnastasisSim/Public/World/AnastasisTraffic.h`, `Source/AnastasisSim/Private/World/AnastasisTraffic.cpp`
- `Source/AnastasisSim/Private/Tests/AnastasisTrafficTests.cpp`
- `Source/AnastasisSim/ECARTS.md` (fiche n°42)
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h`, `Private/Village/AnastasisVillage.cpp` (trafic, `RecordPassage`, `DecayTrafficDaily`, `UpdateRoadEvolutionDaily`, `FNpc::TrafficTimer`), `Private/Village/AnastasisVillagePlayer.cpp` (passage du joueur)
- `Source/AnastasisSim/Public/Sim/AnastasisSimulation.h`, `Private/Sim/AnastasisSimulation.cpp` (minuit : décroissance ; travail 8 `roadEvolution`)
- `Source/Anastasis_UnrealV2/Village/AnastasisSettlementLedger.{h,cpp}`, `AnastasisSettlementPaths.{h,cpp}`, `AnastasisSettlementTests.cpp`
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagePresentation.{h,cpp}`, `AnastasisVillageBuilding.{h,cpp}` (patine, MID permanent), `AnastasisArchitecture.{h,cpp}` + `AnastasisArchitectureTests.cpp` (graine retirée)
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.{h,cpp}` (`anastasis.Village.RoadEvolution`, `Anastasis.Village.SettlementReport`, `GetSettlementStatus`, débogage, métiers du hameau)
- `tools/unreal/create-village-architecture.py` (paramètre `Weathering`), `Content/Anastasis/VillageArchitecture/**` (régénéré)
- `tools/unreal/settlement-morphogenesis-pie.{py,ps1}`, `tools/unreal/proofs.txt` (une ligne), `AGENTS.md` (index)
- `docs/unreal/SETTLEMENT_MORPHOGENESIS_001.md`

## COMMIT

PENDING

## MEC

- BUILD: PASS (`anastasis-unreal.ps1 build`, worktree, 2026-10-07)
- TESTS: `Automation RunTests Anastasis.Sim.Village.Sentiers+Anastasis.Village` (worktree, 2026-10-07) :
  23 PASS, 0 KNOWN_EXPECTED_FAILURE, 0 FAIL. Valeurs : `Sentiers.Naissance` 24 sentiers, premier au jour 21,
  0 hors route, coût 0,860 ; `Sentiers.Renforcement` trajet qui suit l'usage 23 cases de route, coût 230,0 ->
  197,8, trajet parallèle 0 case, 230,0 -> 230,0 ; `Settlement.Biographie` refuge 3 dormeurs, 2 nuits pleines,
  maison possédée 1 dormeur. Suite complète : au lot. (Le scénario `Hamlet` modifié après ce run n'a pas de test.)
- ECARTS: `node tools/migration/check-ecarts.mjs` -> `ECARTS::PASS fiches=29 ouvertes=29 fail=0`
- PIE (`settlement-morphogenesis-pie.ps1 -Keep`, 36 jours, village d'ouverture sans scénario) :

| Mesure | Valeur |
|---|---|
| passages comptés | 3 495 (jour 7 : 1 172 ; jour 19 : 2 079 ; jour 37 : 3 495) |
| sentiers nés | 6 (jours 12, 19, 22, 28, 31, 34), passage à la naissance 15 à 62 ; 5 sentiers à 0,86, 1 ruelle à 0,74 (né à 62) |
| bande rendue | 5 segments, 4 portes, 71 touffes d'herbe écartées |
| maisons fondées | 2 / 2 : `building-1` cultivateur → ferme ; `building-3` (chantier des bâtisseurs) pris au jour 7 par un bâtisseur → réformée `house_poor -> house_medium` |
| verdict du run | `MORPH_PIE FAIL` dû à MON contrôle (exigeait 0,86 et refusait la ruelle de la référence) ; contrôle corrigé dans ce commit, run NON rejoué (le lot le rejouera) |

## PROOFS

PROOFS: settlement-morphogenesis-pie, architecture-pie

## SCN

Village d'ouverture inchangé. `Anastasis.Village.Hamlet` : les foyers reçoivent des métiers (cultivateur au
grenier, deux bâtisseurs) pour que la forme de leurs maisons ait une cause. `Anastasis.Village.SettlementReport`.

## PLY

Le joueur foule aussi (`DrivePlayer`, seulement s'il bouge, comme `drivePlayerActor`).

## ECARTS

- n°42 OUVERT (REDUIT, A_TRANCHER) : sentiers de désir posés sans paiement de bois (marché non porté), sans
  raser une case porteuse d'une ressource, sans classes formelles (montée en ruelle/axe, pavage) ; activation
  `anastasis.Village.RoadEvolution` (hôte, défaut 1), village C++ nu à 0. Marques `ecart n°42` dans
  `AnastasisTraffic.{h,cpp}` et `AnastasisVillage.cpp`.
- Porté fidèlement (sans écart) : `recordPassage`, `trafficTimer` (y compris joueur), `decayFootTraffic`,
  effort de défrichage, `roadClassForTraffic`, coût du profil. Le trafic n'est pas dans le digest.

## INTEGRATION_RISK

- **Empilée sur `architecture-crusade-001`** (copie rebasée de 651017a dans cette branche) : verser
  architecture-crusade-001 d'abord, ou cette branche seule (elle la contient). `village-fabric-001` après les deux.
- `AnastasisVillage.{h,cpp}` sont chauds : `planner-wiring-001` (+335 lignes, `FBuilding::VacantSinceDay`) et
  `abandon-001` y écrivent. Cette mission n'ajoute AUCUN champ à `FBuilding` ; elle ajoute `FNpc::TrafficTimer`
  et l'état `Traffic/Roads/RoadEfforts` de `FVillage`. Conflits textuels probables, sémantiques non.
- `AnastasisSimulation.cpp` `RunDayJob` : travail 8 désormais actif (`roadEvolution`), à son rang de la référence.
- Deux systèmes de chemins coexistent désormais : `village-fabric-001` (lanes tracées en présentation, sans
  cause) et ces sentiers (nés du passage). À réconcilier : les lanes de fabric devraient se greffer sur `GetRoads()`.
- Le défrichement des sentiers met des instances `GroundCover_*` à l'échelle zéro ; la mémoire anthropique
  (`anastasis.Anthropic.Memory`, défaut 0) restaure SES originaux : si les deux sont actifs sur la même touffe,
  le dernier écrit gagne.
- `abandon-pie` lit un MID en slot 0 avec `Neglect` : tenu (MID permanent des corps d'archétype).

## STOP

- **Foyer → agrandissement : BLOQUÉ.** La référence exige un foyer plein et de l'or + un marché : rien de
  cela n'est porté ; une maison possédée n'abrite que son propriétaire (`findOpenShelter` refuse l'intrusion).
  Le registre compte les nuits pleines (refuges) et écrit la pression ; le levier attend `goals-family-001`.
- **Un sentier n'attire pas les trajets voisins** (réfuté, `Sentiers.Renforcement`) : l'heuristique de l'A*
  de la référence surestime sous un sentier.
- Les prises du run ne prouvent pas la lisibilité visuelle des sentiers : la prise « jour 1 » a été faite
  avant que la caméra de preuve ne prenne la main (pas un A/B valide), la prise à 1,7 m est mal cadrée.
- Pas de strates par composant, pas de réemploi de matériaux, pas de persistance (aucune sauvegarde Unreal).
