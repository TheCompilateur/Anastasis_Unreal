# HANDOFF: voix-conseil-001

## MISSION

« Une voix au conseil » (mandat d'Alexandre, 2026-10-09, `docs/unreal/VOIX_CONSEIL_001.md`) : le joueur entre dans les
décisions de Valmire avec les règles des autres — il vote au conseil, on lui demande de l'aide et il répond (le silence
est un refus), il bâtit sa maison et demande de l'aide à son tour — en étranger à éprouver (demi-voix, `nouveau`), et le
village le juge sur ses actes, en face (la raison d'un refus) et dans son dos (ses actes deviennent des histoires).
C'est aussi le mandat de la parole du joueur, `PLAYER` minimal jusque-là.

RELAIS: arrivants-001

## FILES_OWNED

- `Source/AnastasisSim/Private/Village/AnastasisVillageVoice.cpp` (nouveau) ; `Private/Village/AnastasisVillageArrivals.cpp` (la voix du joueur au conseil, voix pesées) ; `Private/Village/AnastasisVillage.cpp` (`OpenFamilySiteNear` extrait, demandes faites au joueur, silence, `on_dit`, `porte_fermee`, `nouveau`) ; `Public/Village/AnastasisVillage.h` ; `Private/Village/AnastasisVillageStateDigest.cpp` ; `Private/Life/AnastasisEpisodes.cpp` (`votedYes`, `votedNo`) ; `Public/Sim/AnastasisSimulation.h` (`SaveFormatVersion` 6)
- `Source/AnastasisSim/Private/Tests/AnastasisVoiceTests.cpp` (nouveau) ; `Source/AnastasisSim/ECARTS.md` (n°54)
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationPlayer.cpp` (`Anastasis.Player.Vote`, `.Help`, `.Build`, `.Ask` ; le joueur s'appelle Nikolaos dans un village de foyers) ; `AnastasisVillageChronicle.{h,cpp}` (demandes au joueur, ses réponses, sa voix, les reproches avec qui / quel groupe) ; `AnastasisNotebook.{h,cpp}` (« ce que j'ai fait », « ce qu'on dit de moi ») ; `AnastasisArrivals.cpp` (état du joueur dans `get_arrivals_status`)
- `Content/Anastasis/Dialogue/repliques-valmire.json` (`aide.refuse.on_dit`, `.porte_fermee`, `.nouveau`, souvenirs `votedYes` / `votedNo`)
- `tools/unreal/voix-pie.py` (nouveau), `tools/unreal/proofs.txt`, `AGENTS.md` (une ligne d'index)
- `docs/unreal/VOIX_CONSEIL_001.md`, `docs/unreal/voix-conseil-001/`, cette fiche
- porte aussi, copié, le commit de `player-goal-stall-001` (le joueur dort quand son corps le demande) : voir INTEGRATION_RISK

## COMMIT

Le dernier commit de la branche `agent/voix-conseil-001` (marqué par `finish`).

## MEC

- BUILD : `anastasis-unreal.ps1 build` -> `BUILD::PASS`.
- TESTS : `report-tests.ps1 -Filter "Anastasis.Sim.Voix+Anastasis.Sim.Arrivants+Anastasis.Arrivants+Anastasis.Memoire+Anastasis.Sim.Episodes"` -> PASS 15, FAIL 0 (`Anastasis.Sim.Voix.DemiVoix`, `.Silence`, `.OnDit` compris). La suite complète sans rendu : `finish`.
- PROOF : `editor-batch.ps1 -Proofs voix-pie` -> `PROOF::PASS voix-pie` ; `VOIX_PIE PASS votes=4 asks_to_me=3 my_asks={"non:porte_fermee": 2, "non:nouveau": 2, "non:inconnu": 1, "oui:amitie": 1} about_me=21 deeds=13 notes=237`, toutes les vérifications (voix, refus, silence, refus pour ses actes, dans son dos, vivant, fichiers).
- ECARTS : `node tools/migration/check-ecarts.mjs -base main -handoff docs/unreal/handoffs/voix-conseil-001.md` -> `ECARTS::PASS`.
- STATE_FIELDS : `node tools/migration/check-state-fields.mjs -base main` -> `STATE_FIELDS::PASS structures=46`.

## PROOFS

PROOFS: chronicle-pie, villager-pie, memory-pie, arrivants-pie, geo-remote-crisis-pie, voix-pie

## SCN

Soixante jours avec un joueur difficile (`voix-pie`), échantillon dans `docs/unreal/voix-conseil-001/` :

- jour 11, au conseil : « Nikolaos, le nouveau : non. Une demi-voix : on ne le connaît pas encore. » (la famille de Theophilos est gardée quand même) ;
- dans son dos : « Konstantinos à Sabas : “Nikolaos a dit non pour la famille de Theophilos, au conseil” » ;
- jour 18 : Theophilos vient lui demander de l'aide, Nikolaos refuse ; jour 23 : Lazaros, qu'il avait accueilli de sa voix, lui demande, et il ne répond pas ;
- jour 21 : il bâtit et demande ; en face : « Georgios refuse : “J'étais au feu quand tu as dit non pour la famille de Theophilos. Je m'en souviens.” », « Leon refuse : “Tu viens d'arriver. Lève d'abord un toit pour quelqu'un.” » ;
- son carnet : « CE QUE J'AI FAIT » (13 actes) et « CE QU'ON DIT DE MOI » (21 histoires).

## PLY

NOT_JUDGED — le joueur a des commandes, pas d'interface. Alexandre lit la chronique et le carnet.

## ECARTS

- ouvert : n° 54 — Une voix au conseil (EXTENSION, A_TRANCHER)
- relayé : n° 53 — Les arrivants et le conseil du soir (arrivants-001)
- relayé : n° 47, n° 48 — mémoire épisodique, maison de famille et demande d'aide (relay-memoire-001, par arrivants-001)

## INTEGRATION_RISK

- Relais : porte `arrivants-001` (lui-même posé sur `relay-memoire-001`, sur `main`). La verser après, ou dans le même lot en la nommant d'abord.
- Porte une copie du commit de `player-goal-stall-001` (cherry-pick, même contenu) : la verser avant ou dans le même lot, en la nommant d'abord ; la copie devient alors vide et disparaît à l'empilement.
- `EvaluateHelp` gagne `on_dit`, `porte_fermee` et `nouveau` pour tout demandeur : un groupe d'arrivants accueilli qui demande de l'aide hésite davantage (`nouveau`). `arrivants-pie` reste PASS à la mesure du lot.
- `SaveFormatVersion` 6 (5 par `arrivants-001`).
- Numéro d'écart 54 : à renuméroter si une autre branche le prend avant.

## STOP

- Pas d'interface : des commandes. Le joueur ne dit pas de phrase (ses réponses sont des actes).
- On ne le chasse pas ; une faute ne pèse pas plus lourd venant de lui (options non retenues par Alexandre).
- Le joueur qui dit oui à une demande puis ne vient pas travailler n'est pas encore jugé pour sa promesse non tenue.
