# HANDOFF: arrivants-001

## MISSION

Mission 4 du jalon « Valmire pousse toute seule » (mandat d'Alexandre, 2026-10-08, `docs/unreal/ARRIVANTS_001.md`) :
le monde extérieur s'ouvre avec Valmire ; des groupes arrivent par la route en fuyant une cause réelle du
scénario ; le conseil du soir (les chefs de famille) les accueille ou les renvoie, chacun avec sa raison ; les
accueillis bâtissent et racontent ; les refusés repartent, et le remords pèse au conseil suivant.

RELAIS: relay-memoire-001

## FILES_OWNED

- `Source/AnastasisSim/Private/Village/AnastasisVillageArrivals.cpp` (nouveau) ; `Public/Village/AnastasisVillage.h` (familles en attente, groupes, conseil) ; `Private/Village/AnastasisVillage.cpp` (familles en attente ou reparties : ni maison, ni sollicitées) ; `Private/Village/AnastasisVillageStateDigest.cpp` ; `Private/Life/AnastasisEpisodes.cpp` (`fled`) ; `Private/Sim/AnastasisSimulation.cpp` (cause et origine des arrivants, conseil dans `lifeDaily`) ; `Public/Sim/AnastasisSimulation.h` (`SaveFormatVersion` 5) ; `Public/Geo/AnastasisGeo.h`, `Private/Geo/AnastasisGeo.cpp` (`recit`)
- `Source/AnastasisSim/Private/Tests/AnastasisArrivalsTests.cpp` (nouveau), `Source/AnastasisSim/ECARTS.md` (n°49, n°38 modifié)
- `Source/Anastasis_UnrealV2/Sim/AnastasisArrivals.{h,cpp}`, `AnastasisArrivalsHostTests.cpp` (nouveaux) ; `AnastasisSimulationGeo.cpp` (`LoadGeoScenario`, `OpenValmireToTheWorld`, `anastasis.Geo.AutoLoad`) ; `AnastasisSimulationSubsystem.{h,cpp}` (`get_arrivals_status`) ; `AnastasisVillageChronicle.{h,cpp}` (arrivée, conseil, verdict) ; `AnastasisNotebook.{h,cpp}` (le joueur entend le conseil)
- `Content/Anastasis/Scenario/valmire-arrivants.json` (nouveau), `geo-pontos-1204.json` (trois chocs, `recit`), `Content/Anastasis/Dialogue/repliques-valmire.json` (paroles du conseil, souvenirs `fled`, `hosting`, `hostingRefusal`)
- `tools/unreal/arrivants-pie.py` (nouveau), `tools/unreal/proofs.txt`, `AGENTS.md` (deux lignes d'index)
- `docs/unreal/ARRIVANTS_001.md`, `docs/historicity/briefs/arrivants-001.json`, cette fiche, `docs/unreal/arrivants-001/`

## COMMIT

Le dernier commit de la branche `agent/arrivants-001` (marqué par `finish`).

## MEC

- BUILD: `tools\unreal\anastasis-unreal.ps1 build` -> `BUILD::PASS`.
- TESTS: `report-tests.ps1 -Filter "Anastasis.Sim+Anastasis.Memoire+Anastasis.Familles+Anastasis.Chronique+Anastasis.Arrivants"` -> PASS 193, KNOWN_EXPECTED_FAILURE 2 (`Anastasis.Sim.Parite.Fbm`, `.SemantiqueJs`), FAIL 0 ; dont les quatre de la mission (`Anastasis.Sim.Arrivants.SansFoyer`, `.Conseil`, `Anastasis.Arrivants.Groupes`, `.SoixanteJours`). La suite complète sans rendu : `finish`.
- PROOF: `geo-remote-crisis-pie` et `world-theatre-threat-pie` (même scénario extérieur) : PASS ; `editor-batch.ps1 -Proofs arrivants-pie` -> `PROOF::PASS arrivants-pie`, `ARRIVANTS_PIE PASS groups=4 councils=4 welcomed=3 refused=1 houses=3 legends=1 rumors=164 people=21` sur la base `relay-memoire-001`, en lot avec `chronicle-pie`, `villager-pie`, `memory-pie`, `geo-remote-crisis-pie` : 5/5 PASS. Le PIE n'est pas au bit près d'un run à l'autre (121 à 164 histoires, 1 ou 2 légendes) ; les verdicts du conseil, eux, sont restés les mêmes.
- ECARTS: `node tools/migration/check-ecarts.mjs -base main -handoff docs/unreal/handoffs/arrivants-001.md` -> `ECARTS::PASS`.
- HISTORICITE: `python tools/historicity/check-brief.py docs/historicity/briefs/arrivants-001.json` -> `BRIEF::PASS traceability only`.

## PROOFS

PROOFS: chronicle-pie, villager-pie, memory-pie, arrivants-pie

## SCN

Soixante jours dans le jeu (`arrivants-pie`), échantillon dans `docs/unreal/arrivants-001/` :

- jour 11 : la famille de Theophilos, qui fuit le pillage des hameaux hauts de Paipert, est gardée à 4 contre 0 (« Qui a fui sa maison ne ferme pas la sienne. ») ;
- jour 15 : les bergers de Lazaros, gardés à 4 contre 1 ; Theophilos, arrivé quatre jours plus tôt, dit non (« Nous n'avons pas fini de nous loger nous-mêmes. ») ;
- jour 26 : Xene, seule, fuyant la prise de la Ville, gardée à 6 contre 0 ;
- jour 45 : la maison d'Hovhannes, refusée à 6 contre 1 (« Personne ici ne sait d'où ils viennent vraiment. ») ; seule Xene dit oui ;
- trois maisons levées par des arrivants ; une légende, née le soir où les bergers sont accueillis : « On raconte qu'à Valmire, les maisons se lèvent à plusieurs » (selon le run : « Les anciens disent qu'ici, personne ne lève un toit seul »).

## PLY

NOT_JUDGED — Alexandre lit la chronique. Le joueur ne vote pas ; il entend le conseil (carnet).

## ECARTS

- ouvert : n° 49 — Les arrivants et le conseil du soir (EXTENSION, A_TRANCHER)
- modifié : n° 38 — le monde extérieur se charge avec les fondateurs de Valmire (`anastasis.Geo.AutoLoad 1`)
- relayé : n° 47, n° 48 — mémoire épisodique, maison de famille et demande d'aide (relay-memoire-001)

## INTEGRATION_RISK

- Relais : porte `relay-memoire-001` (le relais de `memoire-decisions-001` sur `main`, écarts 47 et 48). La verser avant, ou dans le même lot en la nommant d'abord. Rebasée le 2026-10-08 sur `agent/relay-memoire-001` (23b3b354) : écart 48 → 49, `SaveFormatVersion` 5 ; `familles-feu-001` est déjà sur `main`.
- `anastasis.Geo.AutoLoad 1` : toute partie fondée par les fondateurs de Valmire charge maintenant le monde extérieur ; les chroniques de `chronicle-pie` et `memory-pie` changent (des groupes y arrivent). La preuve `geo-remote-crisis-pie` charge son scénario elle-même : un monde déjà chargé y est rechargé par `Anastasis.Geo.Load`.
- `SaveFormatVersion` 5 (4 par `relay-memoire-001`) : une sauvegarde d'avant est refusée, comme prévu.
- Numéro d'écart 49 : à renuméroter si une autre branche le prend avant.

## STOP

- Le joueur ne vote pas et ne demande pas l'asile ; un groupe ne s'adresse pas à un foyer précis (`tryHostGuest` non porté).
- Les refusés disparaissent au matin ; on ne les revoit pas.
- Les arrivants n'apportent ni bêtes ni grain ; seul le canal `Migration` (et l'insécurité, dans le vote) du monde extérieur agit sur le village.
- Le défaut du joueur qui meurt de faim (tâche séparée) n'est pas corrigé ici ; `arrivants-pie` n'incarne pas de joueur.
