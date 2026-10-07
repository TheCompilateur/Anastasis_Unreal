# HANDOFF: field-truth-001

## MISSION

Remplacer l'illusion de parcelles sinusoïdales du fond de vallée par une lecture des vrais champs du snapshot sémantique initial. Conserver un témoin A/B sur le même binaire et décrire sans ambiguïté la limite statique de la couche.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisHumanGeography.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisHumanGeography.h`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisHumanGeographyTests.cpp`
- `docs/unreal/FIELD_TRUTH_001.md`
- `docs/unreal/handoffs/field-truth-001.md`

## COMMIT

PENDING — la fiche appartient au commit de passation ; `git rev-parse HEAD` donne le hash après commit.

## MEC

- BUILD: PASS sur état stable : `tools/unreal/anastasis-unreal.ps1 build`, 8 actions, `Result: Succeeded`, `BUILD::PASS`. Un premier essai avait été invalidé par un changement de source pendant sa course et ne compte pas.
- TESTS: QUEUED pour le lot ; `Anastasis.Terrain.HumanGeography.FieldTruth` est ajouté, pas encore exécuté.
- COMMANDS:
  - `tools/unreal/anastasis-unreal.ps1 build` dans ce worktree
  - `git diff --check` : 0 erreur

## PROOFS

PROOFS: (aucune)

## SCN

UNKNOWN — aucun éditeur ni capture ouvert dans cette mission, conformément à la file unique. A/B/A visuel prescrit dans `FIELD_TRUTH_001.md` ; aucune amélioration artistique revendiquée.

## PLY

UNKNOWN — aucun parcours joueur. Les positions des champs, les stocks vivants et la récolte ne sont pas modifiés.

## INTEGRATION_RISK

- `AnastasisHumanGeography.cpp` et son test sont des fichiers de carte potentiellement chauds, même si les branches eau, PNJ et bâtiments inspectées ne les modifient pas.
- `anastasis.Terrain.FieldTruth` vaut 1 par défaut. Si les champs générés ne sont pas lisibles depuis les caméras choisies ou si le sol devient trop uniforme, l'intégrateur peut remettre 0 avant d'intégrer ; la comparaison visuelle doit porter sur le même binaire.
- Pas de dépendance à une autre branche. La couche lit `Field` et `CropId` du snapshot existant.

## STOP

La mission ne prétend ni reconstruire un parcellaire byzantin documenté, ni refléter la récolte/repousse en runtime, ni prouver le gain visuel sans capture A/B regardée. Pas de nouveau `.uasset`.