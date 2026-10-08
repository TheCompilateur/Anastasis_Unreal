# HANDOFF: save-state-001

## MISSION

Chantier 4 d'IRON_CRUSADE_001 (« 4 Go » d'Alexandre, 2026-10-08) : le jeu sait sauver et recharger une
partie. Le rechargement est juste au sens de la brèche : l'**état complet** est égal (`StateDigest`, l'oracle
de STATE_ORACLE_001) **et le futur est égal**. Détail : `docs/unreal/SAVE_STATE_001.md`.

## FILES_OWNED

- `Source/AnastasisSim/Public/Core/AnastasisStateArchive.h`, `Private/Core/AnastasisStateArchive.cpp` (nouveaux) :
  `FStateArchive`, visiteur à trois modes (hacher, écrire, lire), `VisitArray`, `VisitOptional`, `VisitTile`
- `Source/AnastasisSim/Private/Village/AnastasisVillageStateDigest.cpp` : chaque `HashState(FStateWriter&, const T&)`
  devient `VisitState(FStateArchive&, T&)` ; `FVillage::ArchiveState`, `FVillage::AfterStateLoaded`
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` : déclarations
- `Source/AnastasisSim/Public/Sim/AnastasisSimulation.h`, `Private/Sim/AnastasisSimulation.cpp` : `ArchiveState`,
  `SaveFormatVersion`, `FSaveHeader`, `SaveState`, `ReadSaveHeader`, `LoadState` ; `StateDigest` passe par le parcours
- `Source/AnastasisSim/Private/Tests/AnastasisSaveStateTests.cpp` (nouveau) : `Anastasis.Sim.Sauvegarde.*`
- `Source/AnastasisSim/ECARTS.md` : fiche n° 45
- `Source/Anastasis_UnrealV2/Sim/AnastasisSaveGame.h` (nouveau) : `UAnastasisSaveGame`
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSave.cpp` (nouveau) : `SaveGameToSlot`, `LoadGameFromSlot`,
  `Anastasis.Sim.Save` / `Anastasis.Sim.Load`, CVar `anastasis.Sim.SaveSlot`, `get_save_status`
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.{h,cpp}`, `AnastasisSimulationGeo.cpp` : déclarations,
  chemin du scénario extérieur noté au `Geo.Load` (oublié au `Geo.Unload` et au reset)
- `tools/unreal/save-load-pie.py` (nouveau), `tools/unreal/proofs.txt` (`save-load-pie`)
- `tools/migration/state-fields.json`, `tools/migration/check-state-fields.mjs` : lecteurs renommés (`VisitState`, `ArchiveState`)
- `AGENTS.md` : index (`save-load-pie.py`) et règle de format (§ Deux empreintes)
- `docs/unreal/SAVE_STATE_001.md`

## COMMIT

Voir `git log main..agent/save-state-001`, base `main` = `ab6960c8`.

## MEC

- BUILD: PASS (`tools\unreal\anastasis-unreal.ps1 build`, worktree, base `main` = `ab6960c8`)
- TESTS: `tools\unreal\report-tests.ps1 -Filter 'Anastasis.Sim.Sauvegarde+Anastasis.Sim.Empreinte+Anastasis.Sim.Village.Chantier'`
  → PASS 13 / KNOWN_EXPECTED_FAILURE 0 / FAIL 0, 13 annoncés, run complet :
  - `Anastasis.Sim.Sauvegarde.AllerRetour` : `debut` (2 967 139 octets, 8 habitants) et `jour-3` (3 335 305 octets,
    7 habitants à J6). Même `StateDigest` après rechargement, fichier resauvé identique, même état à chaque
    demi-journée, sur 1 puis 3 jours.
  - `Anastasis.Sim.Sauvegarde.MondeExterieur` : scénario `geo-pontos-1204` chargé, même état et même futur sur
    3 jours. Sans scénario : refus qui nomme le scénario, et la simulation qui charge reste intacte.
  - `Anastasis.Sim.Sauvegarde.Refus` : 6 cas refusés avec leur raison, simulation intacte :
    - `signature ANSV absente` (×2) ;
    - `format 2, ce jeu lit le format 1` ;
    - `world.tiles.y : fin de fichier inattendue` ;
    - `2 octets en trop apres l'etat` ;
    - `cle « bootDeferred » attendue, « BootDeferred » lue`.
  - `Anastasis.Sim.Empreinte.*` (4) et `Anastasis.Sim.Village.Chantier.*` : PASS, sur le nouveau parcours.
- Identité du hachage : test temporaire `Anastasis.Sim.Sauvegarde.IdentiteDuHachage`, l'ancien hacheur (copie
  `LegacyStateDigestForTest`) contre le nouveau parcours, sur un village vivant, écritures invisibles à
  `Digest()` comprises. **13 points sur 13 égaux** (`SAVE_STATE_LEGACY checks=13 npcs=8 day=4`). La copie et le
  test ont été retirés avant commit, comme prévu.
- PROOFS: `tools\unreal\editor-batch.ps1 -Proofs save-load-pie` → `PROOF::PASS save-load-pie`, `EDITOR_BATCH::PASS 1/1` :
  - `SAVE_LOAD_PIE PASS saved=3b3dab2b96a69e00 future=87fdf0f7cff44e9c replay=87fdf0f7cff44e9c npcs=12 buildings=4 bytes=3241367 day=2` ;
  - `ANASTASIS_SAVE loaded ... match=1` ;
  - slot absent refusé.
  - Après cette preuve, une seule retouche, côté hôte : la sauvegarde retient le chemin du scénario extérieur
    tel que donné (relatif à `Content/`), et non plus résolu dans ce worktree. Avant, une partie sauvée dans un
    worktree ne se rechargeait pas depuis une autre copie du jeu. Build PASS après la retouche ; la preuve n'a
    pas été rejouée ici, le lot la rejoue (`PROOFS: save-load-pie`).
- `check-ecarts` : `ECARTS::PASS fiches=41` ; `check-state-fields` : `STATE_FIELDS::PASS structures=44 lacunes=2` (strict).

## PROOFS

PROOFS: save-load-pie

## SCN

Nouveau pour le joueur : `Anastasis.Sim.Save [slot]` et `Anastasis.Sim.Load [slot]` en console. Rien d'autre
ne change à l'écran : le hachage est le même, aucune règle de la simulation n'appelle la sauvegarde.

## PLY

N/A (pas d'entrée de menu ; les commandes console suffisent à cette étape).

## ECARTS

- n° 45 — nouvelle, OUVERT, A_TRANCHER, EXTENSION : format de sauvegarde propre (flux étiqueté, parcours de
  `StateDigest`), différent de `serialize(sim)` de la référence. Marques `ecart n°45` dans
  `Core/AnastasisStateArchive.h` et `Sim/AnastasisSimulation.cpp` (`SaveState`).

## INTEGRATION_RISK

- **Tout le fichier `AnastasisVillageStateDigest.cpp` change** (`HashState` → `VisitState`, `Out.` → `Ar.`).
  Toute mission qui y ajoute un champ (`opening-in-sim-001` y ajoute `OpeningSiteId` et `OpeningHome`) entrera
  en conflit textuel. La résolution est mécanique : écrire le champ en `Ar.Key(...).Number/String/Bool(...)`
  dans le nouveau parcours. Si `opening-in-sim-001` passe avant, je rebase ; si c'est celle-ci, c'est moi
  qui rebase `opening-in-sim-001`.
- **Règle nouvelle (AGENTS.md)** : changer le parcours change le format ; monter
  `FAnastasisSimulation::SaveFormatVersion` dans le même commit.
- **`check-state-fields`** lit désormais `VisitState` / `ArchiveState` (registre mis à jour). Une branche
  écrite avant, qui ajoute un `HashState` au registre, devra suivre le renommage.
- **Historiques de présentation non sauvés** : biographie des bâtiments (`Ledger`) et mémoire anthropique
  repartent de zéro au chargement (ligne `ANASTASIS_SAVE presentation history not restored`).
- **Écrit** `Saved/SaveGames/save-state-pie.sav` pendant la preuve (hors dépôt).

## STOP

- Pas de menu de sauvegarde, pas de sauvegarde automatique.
- Ne sauve pas la biographie des bâtiments ni la mémoire anthropique (présentation).
- Ne lit ni n'écrit le format JS.
- Suite complète non lancée ici : elle attend le lot (`finish` → `queued`).
