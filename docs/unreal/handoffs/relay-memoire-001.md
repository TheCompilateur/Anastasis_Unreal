# HANDOFF: relay-memoire-001

## MISSION

Relais d'intégration (mandat de l'intégrateur, autorité d'Alexandre, 2026-10-08) : rejouer sur `main`
(da0c3094) la mission memoire-decisions-001, dont la branche était en conflit avec `main` sur
`Source/AnastasisSim/ECARTS.md`. Cause : save-history-001, versée entre-temps, a pris le n° 46 que
memoire-decisions-001 utilisait. Branche d'origine `agent/memoire-decisions-001` intacte ; ses deux
commits propres sont repris par `cherry-pick -x` (les cinq de familles-feu-001 qu'elle portait sont déjà
dans `main` par contenu, `git cherry` : `-`).

RELAIS: memoire-decisions-001

Fiche d'origine reprise telle quelle (numéros d'écart corrigés) : `docs/unreal/handoffs/memoire-decisions-001.md`.

## FILES_OWNED

Ceux de memoire-decisions-001 (voir sa fiche). Résolutions du relais :

- `Source/AnastasisSim/ECARTS.md` : le n° 46 de `main` (biographie des bâtiments, save-history-001) est gardé ;
  les deux fiches de la mission sont ajoutées après lui, renumérotées n° 47 et n° 48.
- `Source/AnastasisSim/Public/Sim/AnastasisSimulation.h` : `SaveFormatVersion` 4 (voir INTEGRATION_RISK).
- Marques `ecart n°N` renumérotées dans les seules lignes apportées par la mission ; les marques `ecart n°46`
  de save-history-001 (biographie) restent n° 46.

## COMMIT

Le dernier commit de la branche `agent/relay-memoire-001` (marqué par `finish`).

- `0d74fa36` <- d6a99756 (mémoire, carnet, décisions ; renumérotation)
- `6c91bf3f` <- e58ed60f (format de sauvegarde, 4 au lieu de 3)
- commit de cette fiche

## MEC

- ECARTS: `node tools/migration/check-ecarts.mjs -base main -handoff docs/unreal/handoffs/relay-memoire-001.md` -> voir finish
- STATE_FIELDS: `node tools/migration/check-state-fields.mjs -base main` -> `STATE_FIELDS::PASS structures=46 lacunes=2`
- BUILD / TESTS : `agent-worktree.ps1 finish -Mission relay-memoire-001` (build + suite sans rendu).
- Mesures de la mission d'origine (sur son ancienne base, sans la biographie de save-history-001) : voir sa fiche ;
  elles ne se recopient pas, le lot rejoue les preuves.

## PROOFS

PROOFS: chronicle-pie, villager-pie, memory-pie

## SCN

Voir `docs/unreal/handoffs/memoire-decisions-001.md` (partie de trente jours, `memory-pie`).

## PLY

NOT_JUDGED — Alexandre lit la chronique et le carnet.

## ECARTS

Renumérotation (ancien -> nouveau), partout : `ECARTS.md`, marques du code, fiche d'origine, `docs/unreal/MEMOIRE_DECISIONS_001.md`.

- ouvert : n° 47 — Mémoire épisodique : portée sans croyances sur les personnes, sans culture, sans famine, et étendue au feu et à l'aide (REDUIT, A_TRANCHER) — était n° 46 dans memoire-decisions-001
- ouvert : n° 48 — Maison de famille : décider de bâtir, demander de l'aide, le toit à plusieurs (EXTENSION, A_TRANCHER) — était n° 47 dans memoire-decisions-001
- inchangé : n° 46 de `main` (biographie des bâtiments, save-history-001)
- relayé : n° 44 — Valmire fondée par l'hôte (familles-feu-001, déjà dans `main`, ASSUME)
- n° 16, n° 18 : citées par la fiche d'origine comme touchées dans le code, fiches du registre inchangées

## INTEGRATION_RISK

- `SaveFormatVersion` : `main` est à 2, mais familles-feu-001 et save-history-001 y sont **toutes deux** sorties en 2
  (la fiche de save-history-001 prévoyait 3 pour la seconde ; ce n'est pas ce qui a été versé). La branche
  d'origine a écrit des sauvegardes en 3 sans la biographie. Le relais prend 4 : aucun parcours antérieur ne l'a porté.
  Défaut de `main` signalé, non corrigé ici.
- relay-opening-001 (pile site-from-sim -> water-network -> opening-in-sim) est rejouée elle aussi sur `main` et
  touche les mêmes lignes : fin d'`ECARTS.md`, `SaveFormatVersion` (elle prend 5), `AnastasisSimulationSubsystem.cpp`,
  `AnastasisVillage.h/.cpp`, `AnastasisVillageStateDigest.cpp`. Les deux relais ne s'empilent probablement pas
  dans le même lot : verser celui-ci d'abord, puis rebaser l'autre (conflits mécaniques attendus).
- Fusion textuelle propre du C++ sur un `main` qui a reçu la biographie (`ObserveBiographies` en fin
  d'`UpdateActors`, parcours d'état) : jugée par le build et la suite. Un échec `Anastasis.Memoire.*`,
  `Anastasis.Sim.Episodes.*`, `Anastasis.Familles.*` ou `SaveState` désigne ce relais.
- Tirages : `shareEpisodes` tire dans `sim.rng` dès qu'un habitant a un souvenir (fiche d'origine).

## STOP

- Ceux de memoire-decisions-001 (le joueur meurt de faim vers le jour 8 dans `memory-pie`, aucune légende en
  trente jours, la demande d'aide ne se marche pas).
- Le relais ne revendique aucune mesure nouvelle : les preuves PIE attendent le lot.
