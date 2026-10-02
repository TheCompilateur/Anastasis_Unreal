# HANDOFF: sim-report-003

## MISSION

Rapport 3 JS / Unreal sur `main` 5a607c9, après le lot 1 (`player-minimal-001`, `nav-service-001`,
`budget-cadence-001`, `realisme-ru-002`). Archivé dans `docs/migration/phase3/P3_PREMIER_RAPPORT.md`,
section « Rapport 3 ».

## FILES_OWNED

- `docs/migration/phase3/P3_PREMIER_RAPPORT.md` (section ajoutée en fin de fichier)
- `docs/unreal/handoffs/sim-report-003.md`

## COMMIT

Voir `git log agent/sim-report-003`.

## MEC

- BUILD: `anastasis-unreal.ps1 build` sur 5a607c9 → `BUILD::PASS`
- TESTS: `ANASTASIS_HARNESS_TICKS=16200 ANASTASIS_HARNESS_DRILL=1 report-tests.ps1 -Filter Anastasis.Sim.Harnais`
  → 3 PASS / 0 KNOWN_EXPECTED_FAILURE / 0 FAIL
- Rapport : premier tick divergent 1 (`actors`, A=`4cc03c8b776700c0`, B=`8b3a4d5cb53808f9`), puis
  `buildings` et `rng` au tick 32, `mealReservations` au tick 133, `tileDiff` au tick 257. Identique au rapport 2.
- Reproductibilité : les 601 premières lignes de la trace sont identiques à la trace du portail du lot 1
  (`_integration`).
- COMMANDS:
  - `node tools/migration/emit-state-digests.mjs -ref <clone anastasis-ref-p3> -scenario tools/migration/scenarios/endurance.json -days 3 -out js.jsonl`
  - `node tools/migration/compare-digests.mjs js.jsonl Saved/HarnessTraces/endurance-unreal.jsonl`

## PROOFS

PROOFS: (aucune)

## SCN

`endurance` (empreinte `52f66b01c0766137`, vue `settlement`).

## PLY

Non concerné. Le joueur est absent du harnais : `player-minimal-001` y est neutre (voir le rapport).

## INTEGRATION_RISK

- Documentation seule. Ajout en fin d'un fichier que d'autres missions de migration pourraient aussi
  compléter : en cas de conflit, garder les deux sections.

## STOP

- La neutralité de `player-minimal-001` dans le harnais est déduite du code (`INF`), pas mesurée par une
  trace C++ d'avant le lot : aucune trace longue antérieure n'a été conservée.
