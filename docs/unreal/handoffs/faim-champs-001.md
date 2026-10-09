# HANDOFF: faim-champs-001

## MISSION

« Oui go » d'Alexandre (2026-10-09), à la proposition : que les sans-métier aillent aux champs quand le grenier se vide.

Trouvé par l'étude d'une année (annee-valmire-001) : trois cultivateurs nourrissent quatorze bouches, six à dix adultes
restent sans métier pour toujours, le grenier se vide dès le premier hiver (monde 12345 : 35 soirs vides en deux ans, la
famille du Scribe morte de faim au jour 55) alors que des dizaines de milliers de portions restent sur pied.

Écart n° 59 (réservé par `agent-worktree.ps1 ecart`) : chaque soir (travail `lifeDaily`), s'il reste dans les greniers moins
de deux jours de nourriture (un repas par personne et par jour), l'adulte sans métier le plus affamé (16 ans et plus, ni
joueur, ni porteur) qui atteint le grenier en devient cultivateur. Un par soir, jamais plus de la moitié du village aux
champs. La chronique le raconte déjà (« Basileios devient cultivateur »). Interrupteur `anastasis.Village.FieldHands`
(défaut 1), éteint dans `FVillage` (harnais et parité au bit près).

## FILES_OWNED

- `Source/AnastasisSim/Private/Village/AnastasisVillageFieldHands.cpp` (nouveau) : `UpdateFieldHandsDaily`
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` : `SetFieldHandsEnabled`, `FieldHandsStockDays`, `bFieldHandsEnabled`,
  `FieldHandsHired`, `LastFieldHandsDecision`
- `Source/AnastasisSim/Private/Village/AnastasisVillageStateDigest.cpp` : `fieldHandsEnabled`, `fieldHandsHired`
- `Source/AnastasisSim/Private/Sim/AnastasisSimulation.cpp` : l'appel du soir ; `Public/Sim/AnastasisSimulation.h` :
  `SaveFormatVersion` 7
- `Source/AnastasisSim/Private/Tests/AnastasisFieldHandsTests.cpp` (nouveau) : `Anastasis.Sim.Village.Faim.BrasAuxChamps`
- `Source/AnastasisSim/ECARTS.md` : fiche n° 59 ; `tools/migration/state-fields.json` : `LastFieldHandsDecision` (observation)
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp` : CVar `anastasis.Village.FieldHands`
- `Source/Anastasis_UnrealV2/Sim/AnastasisFieldHandsHostTests.cpp` (nouveau) : `Anastasis.Village.Faim.SoixanteJours`

## COMMIT

Voir `git log main..agent/faim-champs-001`.

## MEC

- BUILD: PASS (worktree)
- TESTS: `tools\unreal\report-tests.ps1 -Filter 'Anastasis.Sim.Village.Faim+Anastasis.Village.Faim'` → PASS 2 /
  KNOWN_EXPECTED_FAILURE 0 / FAIL 0 :
  - `Anastasis.Sim.Village.Faim.BrasAuxChamps` : grenier vide, le plus affamé des adultes part, puis le suivant ; jamais
    l'enfant ; plafond à la moitié (4 sur 8) ; assez de réserves : personne ; éteint : personne ;
  - `Anastasis.Village.Faim.SoixanteJours` (le vrai village, 60 jours) : **monde 12345 : 0 soir à grenier vide avec la règle
    contre 45 sans**, 7 cultivateurs (4 embauches) contre 3, 0 mort des deux côtés, 15 habitants contre 14 ; **monde 1204 :
    inchangé** (23 morts, dont 22 de soif — corrigé par `soif-dabord-001`, pas ici).
- `check-ecarts` : `ECARTS::PASS` ; `check-state-fields` : `STATE_FIELDS::PASS structures=46 lacunes=2`

## PROOFS

PROOFS: npc-life-pie, save-load-pie, chronicle-pie

## SCN

Dans le jeu : quand le grenier baisse sous deux jours de réserve, un habitant sans métier part aux champs, chaque soir s'il
le faut, et la chronique le dit. Le grenier ne reste plus vide des semaines.

## PLY

NOT_JUDGED.

## ECARTS

- n° 59 — nouvelle, OUVERT, A_TRANCHER, EXTENSION : quand le grenier se vide, des bras vont aux champs. Marques `ecart n°59`
  dans `AnastasisVillageFieldHands.cpp`, `AnastasisVillage.h`, `AnastasisVillageStateDigest.cpp`, `AnastasisSimulation.cpp`,
  hôte `AnastasisSimulationSubsystem.cpp`.

## INTEGRATION_RISK

- **`SaveFormatVersion` 7** (`main` = 6) : `soif-dabord-001` le monte aussi à 7 ; la mission versée en second prend 8.
- Les deux missions touchent les mêmes lignes de `AnastasisSimulationSubsystem.cpp` (un `Set…Enabled` après
  `SetSoilWaterEnabled`) et `AnastasisVillage.h` / `AnastasisVillageStateDigest.cpp` (un drapeau après `bSoilWaterEnabled`) :
  conflit textuel simple, garder les deux.
- Personne n'est rendu à son ancien état quand le grenier déborde : à trancher.

## STOP

- Ne touche ni la soif (`soif-dabord-001`), ni le site du village, ni la récolte elle-même.
