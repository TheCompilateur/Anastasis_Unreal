# HANDOFF: relay-valmire-001

## MISSION

Relais d'intégration (mandat de l'intégrateur, autorité d'Alexandre, 2026-10-09) : rejouer sur `main`
(4ff9c34a) trois missions prouvées mais en conflit avec `main` et entre elles : valmire-grows-001,
arrivants-001, voix-conseil-001. Branches d'origine `agent/<m>` intactes ; leurs commits propres sont
repris par `cherry-pick -x`, dans l'ordre valmire-grows, arrivants, voix-conseil (voix-conseil est bâtie
sur arrivants). Les commits de relay-memoire-001 portés par arrivants et voix-conseil, et celui de
player-goal-stall-001 porté par voix-conseil, sont déjà dans `main` par contenu (`git cherry` : `-`) :
non repris.

RELAIS: valmire-grows-001, arrivants-001, voix-conseil-001

Fiches d'origine reprises telles quelles (numéros d'écart corrigés) :
`docs/unreal/handoffs/valmire-grows-001.md`, `docs/unreal/handoffs/arrivants-001.md`,
`docs/unreal/handoffs/voix-conseil-001.md`.

## FILES_OWNED

Ceux des trois missions (voir leurs fiches). Résolutions du relais :

- `Source/AnastasisSim/ECARTS.md` : le n° 51 de `main` (eau du réseau de drainage, water-network-001) et la
  ligne de player-help-scene-001 sous le n° 48 sont gardés ; les trois fiches des missions sont ajoutées
  après, renumérotées n° 52, 53, 54. La modification par arrivants-001 de l'activation du n° 38 est gardée.
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp` (`SeedStartVillage`) : les deux appels
  gardés, dans cet ordre : `SetGrowthEnabled` (valmire-grows) puis `OpenValmireToTheWorld` (arrivants).
- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp` : `FVillage::AskHelp` de `main`
  (player-help-scene-001) gardé tel quel, suivi de `FVillage::OpenFamilySiteNear` extrait par voix-conseil,
  puis `UpdateFamilyHousesDaily` de voix-conseil.
- `Source/AnastasisSim/Public/Sim/AnastasisSimulation.h` : `SaveFormatVersion` 6 (voir INTEGRATION_RISK).
- Marques `ecart n°N` renumérotées dans les seules lignes apportées par les missions (attribution par
  `git blame`) ; aucune marque de `main` touchée.

## COMMIT

Le dernier commit de la branche `agent/relay-valmire-001` (marqué par `finish`).

- `728132be` <- ad5826bd (valmire-grows : des habitants ouvrent les chantiers)
- `7e01700c` <- d0a8eb98 (valmire-grows : le village décide de ses bâtiments communs)
- `2c910888` <- 23c14cc4 (arrivants)
- `138eb8b0` <- 4fdd5108 (voix-conseil)
- `01cabd66` renumérotation des écarts
- commit de cette fiche

## MEC

- ECARTS: `node tools/migration/check-ecarts.mjs` -> `ECARTS::PASS fiches=50 ouvertes=50 fail=0 warn=9` (avertissements préexistants de `main`)
- STATE_FIELDS: `node tools/migration/check-state-fields.mjs -base main` -> `STATE_FIELDS::PASS structures=46 lacunes=2`
- JSON apportés (`repliques-valmire.json`, `valmire-arrivants.json`, `geo-pontos-1204.json`, briefs, preuves) : relus, valides.
- BUILD / TESTS : `agent-worktree.ps1 finish -Mission relay-valmire-001` (build + suite sans rendu).
- Mesures des missions d'origine (sur leurs anciennes bases) : voir leurs fiches ; elles ne se recopient pas,
  le lot rejoue les preuves.

## PROOFS

PROOFS: chronicle-pie, villager-pie, npc-life-pie, material-courier-pie, save-load-pie, memory-pie, arrivants-pie, geo-remote-crisis-pie, voix-pie

## SCN

Voir les fiches d'origine (`arrivants-pie` : soixante jours sans joueur ; `voix-pie` : soixante jours avec un
joueur difficile ; croissance du village dans `chronicle-pie`).

## PLY

NOT_JUDGED — Alexandre lit la chronique et le carnet.

## ECARTS

Renumérotation (ancien -> nouveau), partout : `ECARTS.md`, marques du code, fiches d'origine,
`docs/unreal/ARRIVANTS_001.md`, `docs/unreal/VOIX_CONSEIL_001.md`, brief d'historicité, index d'`AGENTS.md`,
en-têtes des scripts de preuve, `valmire-arrivants.json`.

- ouvert : n° 52 — Le village décide de ses bâtiments communs : grenier et puits, par des règles propres (EXTENSION, A_TRANCHER) — numéro 50 dans valmire-grows-001
- ouvert : n° 53 — Les arrivants et le conseil du soir : accueillir ou renvoyer un groupe venu du monde extérieur (EXTENSION, A_TRANCHER) — numéro 49 dans arrivants-001
- ouvert : n° 54 — Une voix au conseil : le joueur vote, répond, bâtit et demande, et le village le juge sur ses actes (EXTENSION, A_TRANCHER) — numéro 50 dans voix-conseil-001
- modifié : n° 38 — monde extérieur : activation étendue au chargement avec les fondateurs de Valmire (arrivants-001)
- inchangé : n° 51 de `main` (water-network-001) ; n° 47, n° 48 (relay-memoire-001, déjà dans `main`)

## INTEGRATION_RISK

- `SaveFormatVersion` 6 (`main` = 5, monté par opening-in-sim-001). Les trois missions changent le parcours
  d'état (croissance, arrivants et conseil, voix et demandes au joueur) ; une seule montée suffit au-dessus de `main`.
  relay-player-001 (player-walk-001 + player-survie-001) ne change pas le parcours : pas de montée de son côté.
- Écarts n° 52-54 pris ici ; relay-player-001 prend les suivants (55, 56). Verser celui-ci d'abord, ou les deux
  dans le même lot : aucun fichier de code en commun hormis `ECARTS.md` (fin du fichier) et
  `AnastasisVillage.h` / `AnastasisSimulationPlayer.cpp` (zones distinctes à vérifier par le lot).
- Deux chemins pour que le joueur demande de l'aide : le panneau en jeu de `main` (player-help-scene-001,
  `AskHelp`, portée de voix et chemin exigés) et la commande `Anastasis.Player.Ask` de voix-conseil
  (`PlayerAskHelp`, sans contrôle de distance). Même jugement (`EvaluateHelp`), même `HelpLog` : compatibles,
  mais pas unifiés. À trancher plus tard (Alexandre), non bloquant.
- `anastasis.Geo.AutoLoad 1` (arrivants-001) : toute partie fondée par Valmire charge le monde extérieur ; les
  chroniques de `chronicle-pie` et `memory-pie` changent. Croissance (valmire-grows) + arrivants réunies pour la
  première fois : au-delà de 15 âmes le village décide d'un puits ; c'est le lot qui le voit.
- Le porteur (`SurvivalCritical`, valmire-grows) change `npc-life-pie` et `material-courier-pie` (déclarées).
- Fusion textuelle du C++ sur un `main` qui a reçu player-help-scene-001 et opening-in-sim-001 : jugée par le
  build et la suite. Un échec `Anastasis.Sim.Voix.*`, `Anastasis.Sim.Arrivants.*`, `Anastasis.Arrivants.*`,
  `Anastasis.*Growth*`, `Anastasis.Memoire.*`, `Anastasis.Sim.Episodes.*` ou `SaveState` désigne ce relais.

## STOP

- Ceux des trois missions d'origine (voir leurs fiches).
- Le relais ne revendique aucune mesure nouvelle : les preuves PIE attendent le lot.
