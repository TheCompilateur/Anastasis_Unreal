# HANDOFF: labo-ecarts-001

## MISSION

Laboratoire de jugement des écarts de portage : mesurer, dans la référence JS (`anastasis-ref-p3`), l'effet
théorique (`INF`, cône causal) puis réel (`EVD`, expérience contrefactuelle appariée) d'un écart C++, avec un
verdict au bit et un verdict statistique. Premier dossier : n° 5 (cadence) et n° 16 (flux propre des
rumeurs). Branche partie de `agent/ecarts-protocole-001` (PR #2, non versée dans `main` au départ).

## FILES_OWNED

- `tools/migration/labo_ecarts/` (nouveau) : `labo.py`, `cone.py`, `observables.py`, `stats.py`,
  `bras.mjs`, `injections.mjs`, `tolerances.json`, `experiences/n05.json`, `experiences/n16.json`, `LISEZMOI.md`
- `docs/migration/ecarts/` (nouveau) : `METHODE.md`, `n05.md`, `n16.md`, `cones/n05.md`, `cones/n16.md`,
  `donnees/n05.json`, `donnees/n16.json`, `donnees/n05-resultats.md`, `donnees/n16-resultats.md`
- `Source/AnastasisSim/ECARTS.md` : champ `jugement` (table du format, fiches n° 5 et n° 16) ; fiche n° 5
  remise à jour après `budget-cadence-001` (voir ECARTS). Aucun destin ni statut changé.
- `docs/migration/PROTOCOLE_ECARTS.md` : un paragraphe de renvoi au laboratoire.
- `tools/unreal/agent-worktree.ps1`, `.claude/skills/anastasis-mission/SKILL.md` (résolutions de conflit,
  voir INTEGRATION_RISK) et `tools/unreal/test-agent-worktree.ps1` (le dépôt jetable reçoit ce que lit
  `check-ecarts.mjs`).

## COMMIT

BRANCH_HEAD sur `agent/labo-ecarts-001`, rebasée sur `main` (`22db051`, après `editor-queue-001`) le
2026-10-01, au signal de l'intégrateur. Le lot contient les deux commits de `ecarts-protocole-001` (PR #2 :
registre, contrôleur, protocole), puis ceux de cette mission : le laboratoire et les **prédictions seules**
(`882a216` ; `baa47e5` avant rebase, commité avant toute expérience à N répliques), les résultats, le
correctif du banc, les fiches.

## MEC

- BUILD / TESTS : `finish` sur la pointe rebasée → voir le message à l'intégrateur (`HANDOFF_READY::YES (queued)`
  attendu : build seul, la suite au lot). Historique : sur `1342e8a` (base `b720008`), `finish` complet avait
  rendu `ECARTS::PASS`, `BUILD::PASS`, suite **PASS 231 / KNOWN_EXPECTED_FAILURE 4 / FAIL 0** ; `integrate`
  avait été refusé (main avait bougé), puis tout arrêté sur la consigne d'Alexandre (arrêt des tests,
  intégrateur unique). Ce résultat porte sur une base périmée : ce n'est pas la preuve de cette pointe.
  Note : la consigne annonçait une session cloud Linux ; la mission a tourné sur le poste Windows
  d'Alexandre (worktree `C:\dev\ANASTASIS_WORKTREES\labo-ecarts-001`).
- Référence : `git clone --depth 1 --branch anastasis-ref-p3` → `HEAD = fee66ae8b571f6f7bcbe6a61f9749d0a812b84e2`,
  0 fichier modifié (vérifié).
- Coût mesuré : un jour simulé JS = 2 à 4 s pour 5 habitants (Node 24.16) ; un passage de 3 jours ≈ 9 s.
  n05 : 280 passages en 707 s (5 travailleurs) ; n16 : 210 en 558 s ; calibration : 80 + 80.
- Déterminisme : le même bras lancé par deux expériences rend des relevés identiques octet pour octet (3 vérifiés).
- Calibration A/A (`endurance`, 6 paires de témoins) : règle v1 → 6/6 `DERIVE` ; règle v2 (Holm) → 1/6
  `DERIVE`, 5/6 `INDETERMINE`, 0/6 `NEUTRE`. Contrôle de bout en bout A = B (même bras) : `IDENTIQUE`, toutes
  différences nulles.
- `node tools/migration/check-ecarts.mjs -base main -handoff docs/unreal/handoffs/labo-ecarts-001.md` sur
  `22db051` → `ECARTS::PASS fiches=19 ouvertes=19 fail=0 warn=4` (« à attribuer » n° 5, 9, 17 ; marques
  manquantes 6, 7, 14, 15, 19 — le n° 5 a désormais sa marque, posée par `budget-cadence-001`).
- `-bilan` → `A_FERMER=15 A_TRANCHER=4` (inchangé).
- COMMANDS (depuis la racine, `<jsref>` = clone du tag, `<runs>` = dossier hors dépôt) :
  - `python tools/migration/labo_ecarts/labo.py cone --ref <jsref> --experience tools/migration/labo_ecarts/experiences/n05.json`
  - `python tools/migration/labo_ecarts/labo.py lancer --ref <jsref> --experience tools/migration/labo_ecarts/experiences/n05.json --out <runs>`
  - `python tools/migration/labo_ecarts/labo.py analyser --experience tools/migration/labo_ecarts/experiences/n05.json --out <runs> --json docs/migration/ecarts/donnees/n05.json --md docs/migration/ecarts/donnees/n05-resultats.md`
  - idem avec `n16.json`.

## SCN

Résultats (`EVD`), verdicts en règle v2 :

| | Au bit | Statistique | Prédiction |
|---|---|---|---|
| n° 5, contre la référence du harnais (vue 0,0) | `DIVERGE` tick 1 (70/70 paires) | `RUPTURE` (ampleur) : épisodes `helpFarm` 46 → 11 s, temps dedans 0,38 → 0,55, social +12,7, énergie +9 ; faim et survie équivalentes | « dérivant faible » **réfutée** |
| n° 5, contre la référence vue du village (contrôle) | `DIVERGE` tick ~1 375 / ~766 | `INDETERMINE`, 0 grandeur différente : profil A/A | « artefact de la vue » soutenu, pas démontré formellement |
| n° 16 | `DIVERGE` au premier tirage détourné (tick ~300 / ~3 138) | `INDETERMINE`, 0 grandeur différente : profil A/A | chaotique : aucun attendu contredit sauf « peut-être dormant » (réfuté : 12 à 44 tirages / passage) |

Mécanisme du n° 5 (`OBS` code, `INF` effet) : à 1 Hz, la chance de reconsidération est toujours tirée sur
`thinkDt = 0,12 s` → taux de reconsidération ÷ 8 dans la référence en bande `far`.

## PLY

NOT_APPLICABLE — aucun changement de jeu, aucun asset, aucun niveau.

## PROOFS

PROOFS: (aucune)

## ECARTS

- **n° 1 à 19 : fiches créées par `ecarts-protocole-001`** (PR #2), versées dans `main` avec ce lot ; leur
  détail et leur banc sont dans `docs/unreal/handoffs/ecarts-protocole-001.md`. Cette mission n'en crée,
  n'en ferme et n'en reclasse aucune.
- Cette mission ajoute le champ `jugement` (table du format, fiches de la cadence et du flux des rumeurs),
  sans changer classe, destin ni statut, et ne modifie aucun code C++ de `Source/AnastasisSim/`. Le seul
  `.h` du lot est celui de la PR #2 (deux lignes de commentaire dans `AnastasisVillage.h`).
- **n° 5 modifié (texte seulement)** : `budget-cadence-001` (`cb44bdf`, déjà dans `main`) a branché la
  cadence dès qu'une vue est posée, sans toucher `ECARTS.md` (absent de `main` à ce moment). La fiche
  disait « non branché » : titre, `cpp`, `fermeture`, `entree` et texte remis à jour d'après son commit et
  l'en-tête `AnastasisVillage.h` ; classe `REDUIT`, destin `A_FERMER`, statut `OUVERT` inchangés (le reste :
  sans vue posée). Le `jugement` précise qu'il a été mesuré avant ce branchement.

## INTEGRATION_RISK

- **Le versement apporte aussi la PR #2** (`ecarts-protocole-001`) : `ECARTS.md`, `check-ecarts.mjs`, le
  protocole, et l'appel du contrôleur dans `agent-worktree.ps1 finish`, qui devient actif pour toutes les
  missions suivantes. La PR #2 sur GitHub reste à fermer ou à rebaser après le push.
- **Conflit résolu au rebase sur `ce939f8`** : `integration-queue-001` a remplacé, dans `finish`, le build et
  la suite systématiques par un portail Unreal conditionnel (`Invoke-UnrealGate`) ; la PR #2 y insérait le
  contrôle des écarts. Résolution : contrôle des écarts d'abord, puis le portail conditionnel de `main`.
  `test-agent-worktree.ps1` l'a pris en défaut : son dépôt jetable n'avait pas `check-ecarts.mjs`, et S8 à
  S11 tombaient (`MODULE_NOT_FOUND`). Le banc copie désormais le contrôleur, `masks.mjs` et `ECARTS.md` :
  **23/23 PASS**.
- **Conflits résolus au rebase sur `22db051`** (`editor-queue-001`) : dans `finish`, le contrôle des preuves
  déclarées (`PROOFS:`) d'abord, puis le contrôle des écarts, puis `Invoke-UnrealGate` (build seul, la suite
  au lot) ; dans `SKILL.md`, la fiche demande à la fois `ECARTS` et `PROOFS:`. `test-agent-worktree.ps1`
  sur la pointe rebasée : **36/36 PASS, 0 FAIL**.
- `tolerances.json` v1 et les règles v1 / v2 sont des **propositions** de l'agent, non validées par
  Alexandre ; la v2 a été introduite **après** la première mesure, à cause de la calibration A/A — dit dans
  `METHODE.md` et `n05.md`, les deux règles sont rapportées.

## STOP

- Aucun écart ne passe à `ASSUME` ; aucun destin n'est changé. La question « parité au bit ou équivalence
  statistique ? » reste à Alexandre ; les dossiers rapportent les deux verdicts.
- Le niveau 3 (résidu dans Unreal) n'existe pas : seul le format `labo-releve` v1 est spécifié
  (`METHODE.md`) ; il faut un émetteur C++ du relevé.
- `NEUTRE` n'est démontré pour aucun écart : à N = 40, D = 3 l'instrument ne sait pas le conclure.
- Le mécanisme de reconsidération du n° 5 est déduit, pas isolé par une expérience.
- Les relevés bruts ne sont pas commités (≈ 600 Mo) : ils se refont avec `labo.py lancer`.
- Les écarts autres que n° 5 et n° 16, les interactions entre écarts, les horizons > 3 jours : non mesurés.
