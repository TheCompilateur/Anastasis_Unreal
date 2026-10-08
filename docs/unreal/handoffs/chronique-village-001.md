# HANDOFF: chronique-village-001

## MISSION

Mission 1 du jalon « Valmire pousse toute seule » (mandat d'Alexandre, 2026-10-08, mots validés dans
`docs/unreal/CHRONIQUE_VILLAGE_001.md`) : lancer trente jours et lire en français ce qui s'est passé au
village, qui a fait quoi, et pourquoi. Bible §36 : la chronique, lisible sans image.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/Sim/AnastasisVillageChronicle.h` (nouveau)
- `Source/Anastasis_UnrealV2/Sim/AnastasisVillageChronicle.cpp` (nouveau)
- `Source/Anastasis_UnrealV2/Sim/AnastasisVillageChronicleTests.cpp` (nouveau)
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.h` / `.cpp` : chronique tenue par l'hôte (frame et
  tranches d'`AdvanceBy`), remise à zéro dans `ResetCanonical`, CVar `anastasis.Chronicle.Enabled`, commandes
  `Anastasis.Chronicle.Write` / `Anastasis.Chronicle.Print`, `WriteChronicle`, lecteurs Python
  `get_chronicle_text` / `get_chronicle_status` / `write_chronicle` ; `SeedStartVillage` (public) sorti de
  `TryStartVillage` sans changer la séquence
- `tools/unreal/chronicle-pie.py` (nouveau), `tools/unreal/proofs.txt` (une ligne), `AGENTS.md` (une ligne d'index)
- `docs/unreal/CHRONIQUE_VILLAGE_001.md` (nouveau), `docs/unreal/chronique-village-001/*.txt` (deux chroniques d'exemple), cette fiche

## COMMIT

PENDING

## MEC

- BUILD: PASS (`anastasis-unreal.ps1 build` -> `BUILD::PASS`, 2026-10-08, après le dernier changement C++)
- TESTS: `report-tests.ps1 -Filter Anastasis.Chronique` -> PASS 3, KNOWN_EXPECTED_FAILURE 0, FAIL 0, TOTAL 3
  (`Anastasis.Chronique.Recit`, `Anastasis.Chronique.LectureSeule`, `Anastasis.Chronique.TrenteJours`).
  TrenteJours : 30 jours clos en 3,1 s, 12 habitants, 9 en vie, 3 morts (2 de soif, 1 de faim).
  Suite complète NON jouée ici : `finish` la met en file (`queued`), le lot la jouera.
- PROOF: `editor-batch.ps1 -Proofs chronicle-pie` -> `PROOF::PASS chronicle-pie (77.8s)`, `EDITOR_BATCH::PASS 1/1` ;
  `CHRONICLE_PIE PASS days_closed=30 entries=36 told=23 people=12 alive=10`
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\report-tests.ps1 -Filter Anastasis.Chronique`
  - `tools\unreal\editor-batch.ps1 -Proofs chronicle-pie`

## PROOFS

Preuves PIE que le lot rejoue pour cette mission, noms de `tools/unreal/proofs.txt` (EDITOR_QUEUE_001) :

PROOFS: chronicle-pie

## SCN

Le village du lancement, dans le vrai niveau (`Lvl_AnastasisSlice`, site choisi par l'arpentage, graine 12345),
trente jours sautés par `Anastasis.Sim.Advance 30d`. Chronique : `docs/unreal/chronique-village-001/chronique-pie-30-jours.txt`
(et le village sans rendu : `test-chronique-30-jours.txt`).

Ce qu'elle montre de la simulation, sans rien y changer :

- deux fondateurs (Gregorios, Stephanos) meurent de soif les jours 5 et 6 : aucun chemin ne mène au puits ni
  au grenier depuis la case où le village du lancement les a posés (îlot de navigation) ;
- la deuxième maison est achevée au jour 3 par les deux bâtisseuses, qui y trouvent un abri au jour 4 ;
  personne ne s'y voit attribuer la maison pendant un saut : `AssignCompletedOpeningHome` n'est appelé que
  par `Tick`, pas par `AdvanceBy` (constat, non corrigé ici) ;
- du jour 7 au jour 30, plus rien ne se passe : grenier stable (~290 portions), aucun nouveau chantier
  (l'ouverture d'un chantier par les habitants n'est pas portée, écart n°18), sept habitants sans métier.

Sans arpentage (test), le même village place plus d'habitants hors d'atteinte : trois morts, grenier vide
21 soirs sur 30, chantier arrêté 30 jours.

## PLY

NOT_JUDGED — le verdict est celui d'Alexandre en lisant la chronique (`Saved/Chronicle/`). Ni le test ni la
preuve ne disent si le récit est intéressant.

## ECARTS

AUCUN — rien dans `Source/AnastasisSim/` n'est modifié. La chronique vit dans `Anastasis_UnrealV2`, prend la
simulation en `const` et ne l'écrit pas (PROTOCOLE_ECARTS : ce qui lit sans écrire n'est pas un écart) ;
`Anastasis.Chronique.LectureSeule` compare `StateDigest()` de deux villages identiques après cinq jours, l'un
raconté, l'autre non.

## INTEGRATION_RISK

- `AnastasisSimulationSubsystem.cpp` est chaud : plusieurs missions y ajoutent des commandes. Les ajouts sont
  des blocs séparés (CVar, `SeedStartVillage`, `ObserveChronicle`, `WriteChronicle`, deux commandes, trois
  lecteurs) ; `TryStartVillage` appelle `SeedStartVillage` au lieu des quatre appels en ligne, même ordre.
- La chronique lit à chaque frame : une boucle sur les habitants, leurs relations et les bâtiments. Le
  pathfinding n'est lancé qu'au moment où un habitant franchit le seuil de la faim ou de la soif.
- `proofs.txt` : une ligne, écrite en une fois.

## STOP

- Pas de familles, pas de vrais noms (mission 2), pas de demande d'aide ni de dette (mission 3) : la
  chronique ne raconte que ce que la simulation porte aujourd'hui. Les noms sont provisoires (prénoms
  byzantins tirés d'une liste, accordés au portrait).
- Le village de départ du test sans rendu est posé au centre du monde, sans l'arpentage de site du jeu : ses
  habitants peuvent tomber sur des îlots de navigation. La preuve PIE lit le village du vrai jeu.
- Aucune règle de la simulation n'est changée, même là où la chronique montre qu'elle échoue.
