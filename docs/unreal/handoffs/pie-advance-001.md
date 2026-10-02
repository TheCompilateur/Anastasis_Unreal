# HANDOFF: pie-advance-001

## MISSION

Consigne d'Alexandre (2026-10-01) : les agents avancent le temps simulé dans leurs preuves au lieu de l'attendre.
Les preuves PIE qui attendaient le plus longtemps passent au temps accéléré (TIME_WARP_001), reçoivent un
verdict PASS/FAIL explicite et entrent au registre `tools/unreal/proofs.txt`, pour être rejouées par le lot
(EDITOR_QUEUE_001). La règle devient explicite dans `AGENTS.md`, pour Cursor comme pour Claude.

## FILES_OWNED

- `tools/unreal/village-weather-pie.py` : `anastasis.Sim.Warp 10` (le pas de `Speed 10`), verdict
  `VILLAGE_WEATHER PASS/FAIL` (entré à l'abri pour `shelterRain` PUIS ressorti), `Warp 1` rendu en partant.
  Délais inchangés, en temps simulé.
- `tools/unreal/sky-clock-pie.py` : un jour et quart SIMULÉ à `Warp 4`, environ 30 s au lieu de 110 s réelles fixes ;
  échantillon toutes les 5 s simulées ; verdict `SKY_PIE PASS/FAIL` (durée + six phases du village vues) ;
  `ANASTASIS_SKY_PIE_SIM_SECONDS` / `_WARP` ; `_SECONDS` devient un plafond réel.
- `tools/unreal/editor-batch.py` : le rythme (`TimeScale`, `Speed`, `Warp`, `WarpBudgetMs`) reprend sa valeur de
  départ avant chaque preuve du lot ; sinon un `Warp 10` déborderait sur la preuve suivante.
- `tools/unreal/proofs.txt` : `village-weather-pie` (nouveaux motifs), `sky-clock-pie`, `house-rest-pie`,
  `granary-eat-pie`.
- `AGENTS.md` : règle « une preuve n'attend pas le temps simulé, elle l'avance », index de `village-weather-pie`,
  `sky-clock-pie`, `editor-batch`.

## COMMIT

PENDING

## PROOFS

PROOFS: village-weather-pie, sky-clock-pie, house-rest-pie, granary-eat-pie

## MEC

- BUILD: sans objet (aucun fichier Unreal).
- TESTS: aucun exécuté (Alexandre a suspendu les tests le 2026-10-01). Vérifié hors éditeur :
  `python -m py_compile` sur les trois `.py` : code 0 ; `editor-batch.ps1 -List` : les six preuves lues, scripts
  présents sauf `player-pie.py` (arrive avec player-minimal-001).
- Les quatre preuves déclarées SONT la preuve de cette mission : le lot les rejoue dans un éditeur.

## SCN

Sans objet.

## PLY

Sans objet.

## INTEGRATION_RISK

- **Empilée sur `editor-queue-001`** (branche partie de `agent/editor-queue-001`, `d6aed30`) : à verser APRÈS elle,
  donc après player-minimal-001. Rebase sur `main` une fois editor-queue-001 versée.
- **Comportement des preuves converties NON vérifié en éditeur.** `village-weather-pie` passe du pas fin de 1/60 s
  (Speed 1) au pas ×10 de la référence. C'est le pas de `Speed 10`, déjà utilisé par house-rest, granary-eat et
  first-building, mais cette preuve-là n'a jamais tourné ainsi. Si le lot la sort `FAIL`, revenir à `Warp 1`
  (une ligne) et le dire.
- `sky-clock-pie` à `Warp 4` : un jour = 22 s réelles. L'adaptation d'exposition du ciel est lissée sur le temps
  réel, mais la preuve ne lit que l'horloge et les phases.

## STOP

- Pas converties : `gather-deliver-pie`, `build-site-pie`, `food-supply-pie`, `villager-pie`, `villager-body-pie`
  (captures à des étapes précises, gel par `TimeScale 0`). Les accélérer sans les rejouer risquait de rater une
  étape. `first-building-pie` : calendrier en secondes réelles, sans verdict, pas inscrite.
- `house-rest-pie` et `granary-eat-pie` sont inscrites telles quelles : déjà à `Speed 10`, elles finissent en
  quelques secondes.
