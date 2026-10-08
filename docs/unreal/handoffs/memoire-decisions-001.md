# HANDOFF: memoire-decisions-001

## MISSION

Mission 3 du jalon « Valmire pousse toute seule » (mandat d'Alexandre, 2026-10-08, `docs/unreal/MEMOIRE_DECISIONS_001.md`) :
une mémoire de ce que les habitants vivent et se racontent (portée de `src/ai/episodes.js`), un carnet du
joueur qui se remplit de ce qu'il entend, version par version, et des décisions : une famille sans maison
décide de bâtir, va demander de l'aide, et chacun accepte ou refuse en se souvenant.

RELAIS: familles-feu-001

## FILES_OWNED

- `Source/AnastasisSim/Public/Life/AnastasisEpisodes.h`, `Private/Life/AnastasisEpisodes.cpp` (nouveaux)
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h`, `Private/Village/AnastasisVillage.cpp` (mémoire, deuil, récit, oubli, ressenti, biais de but, maison de famille, demande d'aide, accès au chantier), `AnastasisVillageGeo.cpp` (arrivées), `AnastasisVillageWood.cpp`, `AnastasisVillageStateDigest.cpp`, `Private/Sim/AnastasisSimulation.cpp` (travaux de minuit)
- `Source/AnastasisSim/Private/Tests/AnastasisEpisodesTests.cpp` (nouveau), `ECARTS.md` (n°47, n°48)
- `Source/Anastasis_UnrealV2/Sim/AnastasisNotebook.*`, `AnastasisMemoryTests.cpp` (nouveaux) ; `AnastasisDialogueLines.*` (texte d'un souvenir), `AnastasisValmireFounders.*` (souvenirs du feu), `AnastasisVillageChronicle.*` (histoires, légendes, décisions, aide), `AnastasisSimulationSubsystem.*` (carnet, `Anastasis.Carnet.Write`, lecteurs Python)
- `Content/Anastasis/Dialogue/repliques-valmire.json` (souvenirs du feu et de l'aide, raisons d'accepter et de refuser)
- `tools/unreal/memory-pie.py` (nouveau), `tools/unreal/proofs.txt`, `AGENTS.md` (une ligne d'index)
- `docs/unreal/MEMOIRE_DECISIONS_001.md`, cette fiche

## COMMIT

Le dernier commit de la branche `agent/memoire-decisions-001` (marqué par `finish`).

## MEC

- BUILD: `tools\unreal\anastasis-unreal.ps1 build` -> `BUILD::PASS` (2026-10-08) ; la suite complète sans rendu est jouée par `finish`.
- TESTS (avant les trois derniers réglages : toit, maison d'ouverture, dette vécue) : `report-tests.ps1 -Filter "Anastasis.Sim+Anastasis.Memoire+Anastasis.Familles+Anastasis.Chronique"` -> PASS 184, KNOWN_EXPECTED_FAILURE 2 (`Anastasis.Sim.Parite.Fbm`, `Anastasis.Sim.Parite.SemantiqueJs`), FAIL 0, total 186.
- PROOF: `editor-batch.ps1 -Proofs memory-pie` -> `PROOF::PASS memory-pie (67.6s)`, `MEMORY_PIE PASS rumors=54 legends=0 houses=4 help=6/8 notes=24` ; carnet : 24 notes, 15 histoires, 10 conteurs, 11 adressées au joueur.
- ECARTS: `node tools/migration/check-ecarts.mjs -base main -handoff docs/unreal/handoffs/memoire-decisions-001.md` -> `ECARTS::PASS`
- STATE_FIELDS: `node tools/migration/check-state-fields.mjs -base main` -> `STATE_FIELDS::PASS structures=37`

## PROOFS

PROOFS: chronicle-pie, villager-pie, memory-pie

## SCN

Partie de trente jours avec le joueur (`memory-pie`), échantillon dans `docs/unreal/memoire-decisions-001/` :

- une maison levée grâce à une aide : « La troisième maison est achevée, grâce à Sabas. Elle revient à la maison du Scribe. »
- un refus motivé par un souvenir : Georgios, qui avait dit non à Konstantinos au jour 3, lui demande de l'aide au jour 7 :
  « Konstantinos refuse : « Quand j'avais besoin de toi, tu étais occupé. Moi aussi, maintenant. » »
- une histoire qui change de bouche en bouche : l'icône de Konstantinos, racontée par lui, puis par Sabas (« racontée autrement »).
- le carnet : 24 choses entendues, une page par habitant, puis les histoires version par version.

Défaut vu, hors mission : le joueur (Nikolaos) meurt de faim au jour 8 ; il n'exécute plus « boire » / « manger » à partir
du jour 3 (tâche séparée proposée).

## PLY

NOT_JUDGED — Alexandre lit la chronique et le carnet.

## ECARTS

- ouvert : n° 47 — Mémoire épisodique : portée sans croyances sur les personnes, sans culture, sans famine, et étendue au feu et à l'aide (REDUIT, A_TRANCHER)
- ouvert : n° 48 — Maison de famille et demande d'aide (EXTENSION, A_TRANCHER)
- modifié : n° 16 — `spreadRumorExchange` est porté pour les histoires (`shareEpisodes`), pas pour les croyances sur les personnes
- relayé : n° 44 — Valmire fondée par l'hôte (familles-feu-001, ASSUME par Alexandre le 2026-10-08)
- modifié : n° 18 — une maison de famille s'ouvre par sa famille (écart n°48) ; la ligne `build` ne vaut que pour un chantier qui admet l'habitant

## INTEGRATION_RISK

- Relais : porte `familles-feu-001` (rebasée sur `main`, où `chronique-village-001` est versée). La verser dans le même lot, en la nommant d'abord, ou avant. Rebasée le 2026-10-08 : `save-state-001` a changé le hacheur en parcours d'archive (`VisitState`) ; souvenirs, maison de famille et demandes d'aide y sont réécrits, donc aussi sauvés.
- Tirages : `shareEpisodes` tire dans `sim.rng` (le départ, puis l'envie de répéter, puis la déformation) dès qu'un habitant a un souvenir. Le harnais (aucun souvenir sans foyer ni chantier achevé par un habitant) devrait rester au bit près ; un scénario qui achève un chantier crée un souvenir `raised` et déplace le flux, comme la référence (qui tire aussi `sharePeopleBeliefs`, non porté).
- `FNpc`, `FBuilding` et `FVillage` gagnent des champs, hachés par `StateDigest`.
- Numéros d'écart 46 et 47 d'origine, renumérotés 47 et 48 par relay-memoire-001 (le 46 de main est save-history-001).

## STOP

- Le joueur meurt de faim vers le jour 8 dans `memory-pie` (il cesse d'exécuter un but posé) : ce n'est pas corrigé ici, la preuve le journalise (`MEMORY_PIE_PLAYER`) sans en faire un critère.
- Aucune légende n'apparaît en trente jours (il faut trois bouches et des souvenirs encore lourds).
- Le tour du village ne se marche pas : la demande se fait le soir. Le joueur ne peut pas encore demander lui-même.
- Ni croyances sur les personnes, ni culture, ni famine, ni vol (écart n°47). Pas d'interface de carnet, pas de bulles.
