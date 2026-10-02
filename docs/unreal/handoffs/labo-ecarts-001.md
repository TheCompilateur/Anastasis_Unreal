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
- `Source/AnastasisSim/ECARTS.md` : champ `jugement` (table du format, fiches n° 5 et n° 16). Aucun destin
  ni statut changé.
- `docs/migration/PROTOCOLE_ECARTS.md` : un paragraphe de renvoi au laboratoire.
- `tools/unreal/agent-worktree.ps1` (résolution de conflit, voir INTEGRATION_RISK) et
  `tools/unreal/test-agent-worktree.ps1` (le dépôt jetable reçoit ce que lit `check-ecarts.mjs`).

## COMMIT

BRANCH_HEAD sur `agent/labo-ecarts-001`, rebasée sur `main` (`ce939f8`) le 2026-10-01 pour versement.
Le lot versé contient **cinq** commits : les deux de `ecarts-protocole-001` (PR #2 : registre, contrôleur,
protocole), puis les trois de cette mission — le laboratoire et les **prédictions seules** (`442656e`,
`baa47e5` avant rebase, commité avant toute expérience à N répliques), les résultats, cette fiche.

## MEC

- BUILD / TESTS : voir la sortie de `finish` (versement du 2026-10-01). La mission ne touche aucun `.cpp`
  ni `.h`. Un premier `finish`, avant rebase, s'était arrêté au contrôle des écarts : `ECARTS.md` n'existait
  pas dans `main`, et la section `ECARTS` de cette fiche ne nommait pas les fiches que le lot apporte avec
  la PR #2 (`ECARTS::FAIL fail=17`). Corrigé ci-dessous, section `ECARTS`.
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
- `node tools/migration/check-ecarts.mjs -base main -handoff docs/unreal/handoffs/labo-ecarts-001.md` → voir
  `finish`. Avant rebase, contre `origin/agent/ecarts-protocole-001` : `ECARTS::PASS fiches=19 fail=0 warn=3`
  (avertissements préexistants : « à attribuer » n° 9 et 17, marques manquantes).
- `node tools/migration/check-ecarts.mjs` → `ECARTS::PASS fiches=19 ouvertes=19 fail=0 warn=3` (inchangé) ;
  `-bilan` → `A_FERMER=15 A_TRANCHER=4` (inchangé).
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

## ECARTS

- **n° 1 à 19 : fiches créées par `ecarts-protocole-001`** (PR #2), versées dans `main` avec ce lot ; leur
  détail et leur banc sont dans `docs/unreal/handoffs/ecarts-protocole-001.md`. Cette mission n'en crée,
  n'en ferme et n'en reclasse aucune.
- Cette mission ajoute seulement le champ `jugement` (table du format, fiches de la cadence et du flux des
  rumeurs), sans changer classe, destin ni statut, et ne modifie aucun code C++ de `Source/AnastasisSim/`
  (aucun `.cpp`, aucun `.h`).

## INTEGRATION_RISK

- **Le versement apporte aussi la PR #2** (`ecarts-protocole-001`) : `ECARTS.md`, `check-ecarts.mjs`, le
  protocole, et l'appel du contrôleur dans `agent-worktree.ps1 finish`, qui devient actif pour toutes les
  missions suivantes. La PR #2 sur GitHub reste à fermer ou à rebaser après le push.
- `ECARTS.md` : la fiche n° 5 sera touchée par `budget-cadence-001` (autre session, qui branche la cadence) ;
  conflit textuel possible sur la fiche, à fusionner à la main (garder les deux champs).
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
