# HANDOFF: save-history-001

## MISSION

Suite du chantier 4 d'IRON_CRUSADE_001 (« Oui » d'Alexandre, 2026-10-08) : une partie rechargée rend le même
village **à l'écran**. La biographie des bâtiments (SETTLEMENT_MORPHOGENESIS_001 : fondateur, métier, foyer,
changements de mains, nuits pleines) fixe la forme des maisons. Elle était écrite par la présentation
Unreal, image par image, hors simulation : un rechargement la perdait et une maison pouvait changer d'aspect.
Elle est maintenant observée **par la simulation, à chaque pas** : dans l'empreinte d'état, dans la
sauvegarde, et juste en temps accéléré.

## FILES_OWNED

- `Source/AnastasisSim/Private/Village/AnastasisVillageBiography.cpp` (nouveau) : `FVillage::ObserveBiographies`,
  `BiographyEventName`
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` : `EBiographyEvent`, `FBiographyEvent`,
  `FBuildingBiography`, `SetBiographyEnabled`, `GetBiographies`, `FindBiography`, champs `Biographies` /
  `bBiographyEnabled`
- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp` : appel en fin d'`UpdateActors`, remise à zéro dans `Bind`
- `Source/AnastasisSim/Private/Village/AnastasisVillageStateDigest.cpp` : `VisitState` des deux structures,
  `biographyEnabled` / `biographies` dans `ArchiveState`
- `Source/AnastasisSim/Public/Sim/AnastasisSimulation.h` : `SaveFormatVersion` 2
- `Source/AnastasisSim/Private/Tests/AnastasisSaveStateTests.cpp` : `Anastasis.Sim.Sauvegarde.Biographie`
- `Source/AnastasisSim/ECARTS.md` : fiche n° 46
- `Source/Anastasis_UnrealV2/Village/AnastasisSettlementLedger.{h,cpp}` : `FLedger` devient une vue : il recopie
  les faits de la simulation, en déduit la forme (`ProgramFor`, inchangé) et journalise les événements nouveaux
- `Source/Anastasis_UnrealV2/Village/AnastasisSettlementTests.cpp` : la simulation observe avant la vue
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp` : `SetBiographyEnabled(true)` dans `ResetCanonical`
- `Source/Anastasis_UnrealV2/Sim/AnastasisSaveGame.h`, `AnastasisSimulationSave.cpp` : commentaire et ligne de log
- `tools/unreal/save-load-pie.py` : trois jours vécus avant la sauvegarde ; forme de chaque bâtiment comparée
  avant / après chargement, au moins une maison fondée exigée
- `tools/migration/state-fields.json` : `FBuildingBiography`, `FBiographyEvent`
- `docs/unreal/SAVE_STATE_001.md`

## COMMIT

Voir `git log main..agent/save-history-001`, base `main` = `ad18bb99` (`save-state-001` versée).

## MEC

- BUILD: PASS (worktree, base `main` = `ad18bb99`)
- TESTS: `tools\unreal\report-tests.ps1 -Filter 'Anastasis.Sim.Sauvegarde+Anastasis.Village.Settlement+Anastasis.Sim.Empreinte+Anastasis.Village.Architecture'`
  → PASS 14 / KNOWN_EXPECTED_FAILURE 0 / FAIL 0, 14 annoncés, run complet (sans rendu) :
  - `Anastasis.Sim.Sauvegarde.Biographie` : maison fondée par `npc-1`, sauvée, rechargée. Même fondateur, mêmes
    événements, biographie toujours active, même `StateDigest`. Ensuite, dans les deux parties : le
    fondateur s'en va, `npc-2` reprend les murs. `SAVE_HISTORY house=building-2 founder=npc-1 heir=npc-2
    events=5 owner_changes=1`, identique des deux côtés, même `StateDigest`.
  - `Anastasis.Village.Settlement.Biographie` (test d'origine de SETTLEMENT_MORPHOGENESIS_001, la simulation
    observe avant la vue) : PASS, mêmes formes, mêmes compteurs.
  - `Anastasis.Sim.Sauvegarde.{AllerRetour,MondeExterieur,Refus}`, `Anastasis.Sim.Empreinte.*`,
    `Anastasis.Village.Architecture.*` : PASS (refus de version : `format 3, ce jeu lit le format 2`).
  - Un premier run a fini FAIL sur `Biographie` : le test donnait la maison à un héritier tant que son
    fondateur la tenait, et `AssignHome` refuse. Le test a été corrigé (le fondateur part d'abord, chaque
    `AssignHome` vérifié) ; le code n'a pas changé.
- PROOFS: `tools\unreal\editor-batch.ps1 -Proofs save-load-pie,settlement-morphogenesis-pie` → `EDITOR_BATCH::PASS 2/2`
  (`Saved/EditorBatch/20261008-194209/`) :
  - `SAVE_LOAD_PIE PASS saved=8791231caac10f38 future=b7de5adea2905378 replay=b7de5adea2905378 npcs=12
    buildings=4 bytes=3427252 day=4 founded=building-1 shapes_kept=4` : trois jours vécus, la maison
    d'ouverture fondée ; après rechargement, les 4 bâtiments gardent programme, variante affichée et
    fondateur, puis le même futur ;
  - `settlement-morphogenesis-pie` PASS (36 jours, formes de la biographie).
- `check-ecarts` : `ECARTS::PASS fiches=42` ; `check-state-fields` : `STATE_FIELDS::PASS structures=46 lacunes=2`.

## PROOFS

PROOFS: save-load-pie, settlement-morphogenesis-pie

## SCN

Ce qui change pour le joueur :
- **Recharger** une partie rend chaque maison avec sa forme : celle de son fondateur, même si le fondateur
  est mort ou qu'un autre foyer a repris la maison.
- **En temps accéléré** (`Anastasis.Sim.Advance`, `Warp`), un changement de mains n'échappe plus à la
  biographie. Avant, l'hôte ne l'observait qu'une fois par image.

Une seule différence de journal : la ligne `ANASTASIS_SETTLEMENT event ... founded` donne
`npc (métier), foyer de N -> forme : cause`. Avant, c'était `npc (métier) -> forme : cause`.

## PLY

N/A

## ECARTS

- n° 46 — nouvelle, OUVERT, A_TRANCHER, EXTENSION : biographie des bâtiments observée par la simulation.
  Activée par l'hôte seulement, aucune règle ne la lit ; harnais : aucune section. Marques `ecart n°46` dans
  `AnastasisVillage.h`, `AnastasisVillage.cpp` et `AnastasisVillageBiography.cpp`.

## INTEGRATION_RISK

- **Conflit connu avec `opening-in-sim-001`** : même fichier `AnastasisVillageStateDigest.cpp` (champs
  ajoutés au même endroit de `ArchiveState`), même `SaveFormatVersion` (chacune passe de 1 à 2).
  - Résolution mécanique : garder les deux blocs de champs, et `SaveFormatVersion` = 3 pour la seconde
    versée.
  - Je fais ce rebase, quelle que soit celle qui passe d'abord.
- **`settlement-morphogenesis-pie`** lit la biographie (`get_settlement_status`) : elle est déclarée et rejouée.
- **Timing** : la biographie est observée à chaque pas de simulation, et non plus une fois par image. Le jour
  d'un événement est donc celui du pas où il arrive.
- **Mémoire anthropique** : toujours non sauvée (expérimentale, désactivée par défaut). Hors de ce chantier,
  dit dans `SAVE_STATE_001.md`.

## STOP

- Ne sauve pas la mémoire anthropique.
- Ne change aucune règle : la biographie n'est lue que par la présentation.
- Suite complète : jouée par `finish`.
