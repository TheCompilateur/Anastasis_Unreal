# HANDOFF: sim-report-004

## MISSION

Rapport 4 JS / Unreal sur `main` 4fef72d, après les besoins, le mode de vie, le flux partagé et la
reconsidération. Il est archivé dans `docs/migration/phase3/P3_PREMIER_RAPPORT.md`, section « Rapport 4 ».

## FILES_OWNED

- `docs/migration/phase3/P3_PREMIER_RAPPORT.md` (section ajoutée en fin de fichier)
- `docs/unreal/handoffs/sim-report-004.md`

## COMMIT

Voir `git log agent/sim-report-004`.

## MEC

- BUILD : `BUILD::PASS` sur 4fef72d.
- TESTS : `report-tests.ps1 -Filter Anastasis.Sim`, avec `ANASTASIS_HARNESS_TICKS=16200` et `ANASTASIS_HARNESS_DRILL=1` :
  PASS 120, KNOWN_EXPECTED_FAILURE 2, FAIL 0, 122/122.
  Forage au tick 32 (`TICKS=40`, `DRILL=32`, `-Filter Anastasis.Sim.Harnais`) : PASS 3, KEF 0, FAIL 0.
- RAPPORT : premier tick divergent 1 (`actors`, 15 champs) ; `buildings` au tick 32, `rng` au 125, `mealReservations` au 133, `tileDiff` au 257.
- Forage du tick 32 : la seule différence hors `actors` est un point d'accès du puits, retiré par la référence et gardé par le C++. Le site qui le retire n'est pas encore nommé.

## ECARTS

AUCUN — documentation seule, aucun fichier de `Source/AnastasisSim/`.

## PROOFS

PROOFS: (aucune)

## SCN

`endurance`, inchangé.

## PLY

Sans objet.

## INTEGRATION_RISK

- Ajout en fin de `P3_PREMIER_RAPPORT.md`. En cas de conflit, garder les deux sections.

## STOP

- La cause du point d'accès retiré au tick 32 n'est pas trouvée : c'est un ordre de travail, pas une conclusion.
- Les tirages de `reconsider-001` ne sont pas jugés : le flux décale avant eux, au tick 125.
