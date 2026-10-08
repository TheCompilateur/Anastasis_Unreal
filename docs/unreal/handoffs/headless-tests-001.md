# HANDOFF: headless-tests-001

## MISSION

Sortir la suite d'automation de l'editeur avec rendu. `report-tests.ps1` la lancait dans un `UnrealEditor-Cmd`
avec rendu, sur la carte de demarrage : shaders, monde incarne, 8 a 13 Go, et une place de la porte memoire,
pour des tests qui ne regardent jamais une image. Inventaire statique (2026-10-08, `640fa3e`) :

- `Source/AnastasisSim` : 160 tests, module sans `Engine` (`Core` + `CoreUObject`, aucun type UObject) ;
  ils ne peuvent pas dependre du rendu.
- `Source/Anastasis_UnrealV2` : 194 tests ; aucun ne fait de capture, de lecture de pixel, de readback ni
  n'utilise le RHI. Ceux qui ont besoin d'un monde le creent (`UWorld::CreateWorld`, `FTestWorld`) ; quelques-uns
  chargent des assets (`LoadObject`, `LoadSynchronous`), ce qui marche sans rendu.
- Python (`AI.Toolsets.AnastasisInspect`) : lit la session et le monde de l'editeur, n'importe lequel.

Changement : `report-tests.ps1 -Mode auto` (defaut) lance la suite en `-nullrhi -nosound` sur `/Engine/Maps/Entry`.

- Run incomplet (crash, marqueur de fin absent) : la suite entiere est rejouee avec rendu, ce run fait foi
  (`TEST_MODE::REPLI_GPU`). Un run incomplet n'est pas un verdict ; le rejouer ne cache aucun echec de test.
- Tests en echec sans rendu : eux seuls sont rejoues avec rendu (la suite entiere au-dela de 40). Un test qui y
  passe ne compte PASS que s'il est inscrit au nouveau registre `tools/unreal/rhi-tests.txt` ; sinon il reste FAIL
  (`HEADLESS_ECART::`) : une repetition qui passe ne distingue pas une dependance au rendu d'un test instable, et
  la porte ne doit pas devenir plus indulgente qu'avant.
- `-Mode headless` : un seul run sans rendu. `-Mode gpu` : exactement le run d'avant.
  `ANASTASIS_TESTS_MODE=gpu` : la trappe de l'integrateur si le mode sans rendu se revele faux.
- Chaque run ecrit `TEST_MODE::<HEADLESS|GPU> duree= pic_ws= pic_prive=` (processus principal) : la mesure qui
  dira si la porte memoire peut traiter un run sans rendu a part (pas change ici).

Sorties inchangees pour les appelants (`agent-worktree.ps1`, `scheduled-verify.ps1`) : memes lignes
PASS / KNOWN_EXPECTED_FAILURE / FAIL / TOTAL, meme `TESTS::PASS|FAIL`, meme code de sortie.

## FILES_OWNED

- `tools/unreal/report-tests.ps1`
- `tools/unreal/rhi-tests.txt` (nouveau, vide)
- `AGENTS.md` (index : `report-tests.ps1`, `rhi-tests.txt` ; section Tests)
- `docs/unreal/handoffs/headless-tests-001.md`

## COMMIT

Voir `git log` de la branche `agent/headless-tests-001`.

## MEC

- BUILD: sans objet (aucun fichier Unreal) ; `finish` doit sortir `(nounreal)`.
- TESTS: **aucun run Unreal**. Mission ecrite depuis une session cloud Linux, sans moteur. Verifie :
  - analyse syntaxique PowerShell 7.4 de `report-tests.ps1` : aucune erreur ;
  - banc a blanc (moteur simule : `Start-AnastasisEditor` remplace par un faux qui ecrit un log d'automation),
    11 scenarios, tous conformes : suite verte sans rendu (un run) ; echec sans rendu seulement, non inscrit
    (FAIL + `HEADLESS_ECART`) ; idem inscrit (PASS + `RHI_TEST`) ; inscrit mais passe sans rendu
    (`RHI_TEST::INUTILE`) ; run sans rendu incomplet puis suite avec rendu verte (PASS) ou rouge (FAIL) ;
    `-Mode gpu` (un run, sans `-nullrhi`) ; echec dans les deux modes (FAIL) ; echec connu casse (FAIL) ;
    `ANASTASIS_TESTS_MODE=headless` (pas de repli) ; filtre cible (`TOTAL` juste) ; valeur de mode invalide
    (erreur franche).
  - index `tools/unreal/` : `MISSING []`, `STALE []` (meme lecture que `tools-index.ps1`).
- COMMANDS:
  - `tools\unreal\report-tests.ps1`                 (auto)
  - `tools\unreal\report-tests.ps1 -Mode gpu`       (ancien run)

## PROOFS

PROOFS: (aucune)

## SCN

Sans objet.

## PLY

Sans objet.

## ECARTS

Sans objet : ne touche pas `Source/AnastasisSim`.

## INTEGRATION_RISK

- **Premiere mesure a faire, une fois, editeurs fermes, sur la racine d'integration :**
  `report-tests.ps1 -Mode gpu` puis `report-tests.ps1 -Mode headless`. Les deux doivent donner les memes
  `PASS`, `KNOWN_EXPECTED_FAILURE`, `FAIL` **et le meme `ANNONCES PAR LE LANCEUR`**. Ce dernier compte est le
  vrai risque : un plugin qui ne charge pas sous `-nullrhi` n'enregistre pas ses tests, et un test jamais
  enregistre ne peut pas echouer. Si le compte baisse : `ANASTASIS_TESTS_MODE=gpu` pour l'integrateur, et une
  mission pour comprendre. Les lignes `TEST_MODE::` des deux runs donnent le gain reel (duree, memoire).
- La branche ne touche aucun fichier Unreal : le lot ne lancera pas la suite pour elle. Le premier lot qui
  touche Unreal apres versement l'executera sans rendu.
- `project-health.ps1` lit `report-tests.log`, le run sans rendu. Apres un repli, le verdict qui fait foi est
  dans `report-tests-gpu.log` ; `health` peut alors montrer des echecs sans rendu que `report-tests` a juges.
- La porte memoire traite encore un run sans rendu comme un editeur complet. La relacher demande les mesures
  `TEST_MODE::` ; pas fait ici.

## STOP

- Ne revendique aucun run Unreal : ni que la suite passe sans rendu, ni le gain de temps ou de memoire. Ce
  sont des hypotheses tirees du code, a confirmer par la mesure ci-dessus.
- Pas de Low Level Tests (executable sans editeur) : demande une cible de test C++ jamais compilee contre le
  moteur installe de ce poste ; a faire seulement si le mode sans rendu laisse un cout qui le justifie.
- Ne touche ni a la porte memoire, ni a `agent-worktree.ps1`, ni aux preuves PIE (qui ont besoin du rendu).
