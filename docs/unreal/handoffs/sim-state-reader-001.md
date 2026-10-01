# HANDOFF: sim-state-reader-001

## MISSION

Phase 3, jalon A (`P3_PLAN.md` §3), vague 4 : lecteur C++ du format de sauvegarde JS, pour le harnais.
Charger un scénario (`tools/migration/scenarios/endurance.json`) en état C++ et le reprojeter sur les
sections de `serialize` ; fin : `Anastasis.Sim.Harnais.Lecture`, empreinte identique **au tick 0**.

## FILES_OWNED

- `Source/AnastasisSim/Public/Core/AnastasisJson.h`, `Private/Core/AnastasisJson.cpp` (nouveaux)
- `Source/AnastasisSim/Public/Harness/AnastasisJsSave.h`, `Private/Harness/AnastasisJsSave.cpp` (nouveaux)
- `Source/AnastasisSim/Private/Tests/AnastasisJsSaveTests.cpp`, `AnastasisScenarioVectors.inl` (nouveaux, `.inl` généré)
- `tools/migration/gen-scenario-vectors.mjs` (nouveau)
- `tools/migration/ported-functions.mjs` (`save.js`, `pristineWorld.js`), `docs/migration/phase2/P2_INVENTAIRE_JS.md` (régénéré)
- `Source/AnastasisSim/PORTAGE.md` (section du lecteur), `docs/migration/phase2/P2_MODELE_DONNEES.md` (ligne d'état)
- `docs/unreal/handoffs/sim-state-reader-001.md`

## COMMIT

BRANCH_HEAD

## MEC

- `node tools/migration/gen-scenario-vectors.mjs -ref <clone du tag anastasis-ref-p3>` → 35 sections (10 au périmètre),
  global `47a2a2ffc0e5d98a` — **identique** à la ligne `t=0` d'une trace `emit-state-digests.mjs -scenario endurance`
  (actors `f6c5317de127a90a`, buildings `3ef0d520c50b4b05`, tileDiff `ab93e285f337d4a9`, mealReservations `f38bc2b4d775df77`).
- Préalable mesuré en JS : l'empreinte de `serialize` en mémoire et celle du texte JSON du scénario sont identiques sur
  les 35 sections au tick 0 (aucun `-0`, `NaN`, `undefined` de tableau dans le périmètre) : un lecteur de texte peut
  donc rendre les bits du JS.
- `tools\unreal\anastasis-unreal.ps1 build` → `BUILD::PASS` (non-unity : les 3 nouveaux fichiers exclus de l'unity).
- `tools\unreal\report-tests.ps1 -Filter Anastasis.Sim.Harnais` → **2 PASS / 0 KNOWN_EXPECTED_FAILURE / 0 FAIL** :
  - `Anastasis.Sim.Harnais.Json` : 8 nombres relus au bit près (`0.1`, `5e-324`, `-0`, `9007199254740993`…, motifs
    calculés par Node, pas recopiés), échappements et paire de substitution, 7 entrées non strictes refusées, empreinte
    d'arbre = empreinte décrite à la main, clé répétée comme `JSON.parse`.
  - `Anastasis.Sim.Harnais.Lecture` : (1) les 35 sections du texte relu = empreintes JS du tick 0 ; (2) lecture typée :
    108 × 114, 3 bâtiments, 5 habitants ; (3) les 10 sections du périmètre **projetées depuis l'état C++** = empreintes
    JS ; `tileDiff` recalculé contre `GenerateWorld(12345, 108, 114)` = 590 cases, comme la référence ; (4) global du
    tick 0 = `47a2a2ffc0e5d98a` (périmètre projeté, 25 sections recopiées) ; (5) mutations côté C++ (un ulp de faim, un
    ulp de position, une portion au grenier, une tuile hors diff) changent leur section, et elle seule pour la faim ;
    (6) refus : `inside` non nul, `createdDay` 1,5. Info : « scenario endurance : 108 x 114, 590 cases de tileDiff,
    3 batiments, 5 habitants ; 10 sections projetees ».
- Inventaire régénéré contre le tag : 9 portés, 33 partiels (17 985 lignes restantes), 164 à porter ; 58 632 lignes.
- FINISH : voir le compte rendu de passation (build unity + suite complète).

## SCN

Sans objet.

## PLY

Sans objet.

## INTEGRATION_RISK

- Aucun fichier existant du C++ n'est modifié : lecteur, JSON et tests sont nouveaux. `FBuilding`, `FNpc`,
  `FMealReservation`, `FTile`, `GenerateWorld`, `FStateWriter` sont utilisés tels quels.
- Le test lit `tools/migration/scenarios/endurance.json` depuis `FPaths::ProjectDir()` : le fichier doit rester dans le
  dépôt. Reconstruire le scénario change son empreinte ; le test le dit et demande `gen-scenario-vectors.mjs`.
- `FCString::Atod` (UCRT `wcstod`) : le lecteur refuse de lire si la bibliothèque C ne lit pas le point décimal.

## STOP

- **Recopie, pas lecture**, pour tout champ non lu : un habitant JS a 105 champs de premier niveau, le lecteur en lit 31
  (plus `skills.{gather,trade,craft}` et `inventory.food` ; liste en tête de la section habitants de `AnastasisJsSave.cpp`) ; le reste est rendu tel quel par la projection. L'égalité au tick 0
  ne prouve rien sur ces champs-là, seulement qu'ils sont gardés.
- 25 sections hors périmètre (`colony`, `economy`, `life`, `logs`, `transport`…) : recopiées, non lues.
- L'état lu **ne tourne pas** : rien ne le branche encore sur `FVillage` / `FAnastasisSimulation` (prochain :
  `sim-digest-emitter-001`). La couronne du village (`stampVillageCrown`) que `deserialize` pose hors diff n'est pas
  portée ; les cases hors diff gardent la génération.
- Non lu : lignes de `tileDiff` au format ancien (< 9 colonnes), habitant à l'intérieur (`inside` non nul).
- Aucun vecteur `.inl` existant ni le registre `known-expected-failures.txt` touchés ; dépôt JS non écrit.
