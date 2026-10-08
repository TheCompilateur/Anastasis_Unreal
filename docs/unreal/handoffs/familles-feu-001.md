# HANDOFF: familles-feu-001

## MISSION

Mission 2 du jalon « Valmire pousse toute seule » (mandat d'Alexandre, 2026-10-08, `docs/unreal/FAMILLES_FEU_001.md`) :
quatre familles fondatrices et un moine, chacune avec un passé, un objet et une dette morale ; le premier soir au
feu raconté dans la chronique ; une base de répliques en français (reprise du JS et complétée).

RELAIS: chronique-village-001

## FILES_OWNED

- `Content/Anastasis/Scenario/valmire-fondateurs.json`, `Content/Anastasis/Dialogue/repliques-reference.json` (généré), `Content/Anastasis/Dialogue/repliques-valmire.json`
- `tools/migration/gen-talk-lines.mjs`
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h`, `Private/Village/AnastasisVillage.cpp`, `Private/Village/AnastasisVillageStateDigest.cpp` (identité et foyers, écart n°44), `Private/Tests/AnastasisFamilyTests.cpp`, `ECARTS.md` (n°44)
- `Source/Anastasis_UnrealV2/Sim/AnastasisDialogueLines.*`, `AnastasisValmireFounders.*`, `AnastasisFoundersTests.cpp` (nouveaux)
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.*` (`anastasis.Village.Founders`, `SeedStartVillage`, `TellFounding`), `AnastasisVillageChronicle.*`, `AnastasisVillageChronicleTests.cpp`
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagerLooks.*`, `AnastasisVillagePresentation.cpp` (portrait selon sexe et âge)
- `tools/unreal/villager-pie.py` (14 au lancement, portrait d'enfant), `tools/unreal/chronicle-pie.py` (familles et feu)
- `docs/unreal/FAMILLES_FEU_001.md`, `docs/historicity/briefs/familles-feu-001.json`, cette fiche

## COMMIT

PENDING

## MEC

- BUILD: PASS (`anastasis-unreal.ps1 build` -> `BUILD::PASS`, après le dernier changement C++)
- TESTS: `report-tests.ps1 -Filter "Anastasis.Familles+Anastasis.Chronique+Anastasis.Sim.Famille+Anastasis.Sim.Mortality+Anastasis.Sim.Empreinte"`
  -> PASS 16, KNOWN_EXPECTED_FAILURE 0, FAIL 0, TOTAL 16. Valeurs : `REPLIQUES pools=312 lines=1459` ;
  `FONDATEURS recits differents sur 20 graines : 20` ; `FONDATION scene=34` (répliques du feu) ; les 14 fondateurs atteignent le puits.
  Suite complète NON jouée ici : `finish` la met en file, le lot la jouera.
- PROOF: `editor-batch.ps1 -Proofs chronicle-pie,villager-pie` -> `PROOF::PASS chronicle-pie (97.8s)`, `PROOF::PASS villager-pie (115.0s)`,
  `EDITOR_BATCH::PASS 2/2` ; `CHRONICLE_PIE PASS days_closed=30 entries=70 told=51 people=14 alive=14 checks=...,families=1,fire_scene=1`
- BRIEF: `python tools/historicity/check-brief.py docs/historicity/briefs/familles-feu-001.json` -> `BRIEF::PASS traceability only`
- ECARTS: `node tools/migration/check-ecarts.mjs` -> `ECARTS::PASS fiches=40 ouvertes=40 fail=0`

## PROOFS

PROOFS: chronicle-pie, villager-pie

## SCN

Le village du lancement dans le vrai niveau (graine 12345), trente jours sautés :
`docs/unreal/familles-feu-001/chronique-pie-30-jours.txt` (et sans rendu : `test-chronique-30-jours.txt`).

- Les quatre familles et le moine, présentés en deux phrases chacune, puis le premier soir au feu : 34 répliques,
  le moine interroge chaque famille, chacune répond (variantes tirées par la graine), quelqu'un réagit.
- **Personne ne meurt en trente jours** (deux morts de soif dans la chronique de la mission 1). Cause corrigée :
  les fondateurs sont posés sur des cases d'où le puits est atteignable, et la maison d'ouverture n'est plus
  acceptée si elle enferme son résident (`SeedOpeningHousehold`, constat : Georgios enfermé au premier essai,
  `work=none`). Ce second correctif vaut aussi pour le village anonyme (`Founders 0`).
- La deuxième maison est bâtie au jour 2 (Niketas, Leon, Theodora). Ensuite le village est calme : rien
  n'ouvre de chantier (écart n°18) et treize personnes dorment sans maison à soi ou sous un abri.
- Portraits : chacun porte un visage de son sexe et de son âge, aucun en double (`villager-pie`). L'ordre de pose
  tient compte du stock peint : deux hommes cultivateurs, trois femmes et cinq hommes sans métier.

## PLY

NOT_JUDGED — Alexandre lit la chronique du premier soir. Le joueur n'entre pas dans la scène.

## ECARTS

- ouvert : n° 44 — Foyers fondateurs posés par l'hôte : identité et parenté comme données, sans vie familiale (EXTENSION, A_TRANCHER)

## INTEGRATION_RISK

- Relais : cette branche porte `chronique-village-001` (commits 3f8a861b, 2a316330). La verser après elle, ou dans le même lot en la nommant d'abord.
- Écart n°44 : le numéro 43 est déjà pris par `labor-social-001`, non versé. Si une autre branche prend 44 avant, renuméroter.
- Le village du lancement change : 14 fondateurs nommés au lieu de 12 anonymes (`anastasis.Village.Founders 0` rétablit l'ancien). `villager-pie` est adapté ; les preuves qui fixent `StartVillagers 12` (settlement-site, settlement-sensitivity, geography-concordance) mesurent le site, pas le nombre, mais leur village change : à surveiller au lot.
- `FNpc` gagne six champs et `FVillage` deux, hachés par `StateDigest` : toute mission qui ajoute aussi des champs à `FNpc` touchera les mêmes lignes.
- `AnastasisSimulationSubsystem.cpp` et la chronique sont chauds (mission 1 dans le même lot).

## STOP

- Pas de vie familiale (couples, naissances, enfants qui ne travaillent pas, parenté dans les liens) ; pas de passé dans la simulation ; pas de dette.
- Les répliques ne sont pas dites en jeu (pas de bulles) ; celles de la référence restent sans accents.
- Les biographies sont une hypothèse de conception, pas un fait historique.
