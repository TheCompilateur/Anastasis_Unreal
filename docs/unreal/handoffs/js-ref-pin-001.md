# HANDOFF: js-ref-pin-001

## MISSION

Phase 3, jalon A, première mission (`docs/migration/phase3/P3_PLAN.md`) : épingler la référence JS
du portage (`fee66ae`) sur GitHub par un tag, l'écrire dans `docs/migration/phase3/REFERENCE_JS.md`,
et régénérer `P2_INVENTAIRE_JS.md` contre elle avec une catégorie « partiellement porté ».
Le plan (commit `f9610a0` de `origin/agent/sim-phase3-plan`) entre dans `main` avec cette mission
(cherry-pick). Aucun C++.

## FILES_OWNED

- `docs/migration/phase3/P3_PLAN.md` (cherry-pick de `f9610a0`, inchangé)
- `docs/migration/phase3/REFERENCE_JS.md` (nouveau)
- `docs/migration/phase2/P2_INVENTAIRE_JS.md` (régénéré)
- `tools/migration/inventory-js-sim.mjs` (catégorie partiel, refus d'une référence modifiée, contrôle du scanner)
- `tools/migration/ported-functions.mjs` (nouveau : ce que PORTAGE.md déclare porté, fonction par fonction)
- `tools/migration/js-functions.mjs` (nouveau : étendue des fonctions d'un module JS)
- `Source/AnastasisSim/PORTAGE.md` (§ « Ce qu'il reste — l'inventaire » seulement : commande, chiffres)
- `docs/unreal/handoffs/js-ref-pin-001.md`

Hors dépôt Unreal, autorisé par Alexandre : **un** tag poussé sur `TheCompilateur/Jeux-IV-Kingdoms`.

## COMMIT

BRANCH_HEAD (`agent/js-ref-pin-001`, rebasée sur `main` `6371405`)

## MEC

- RÉFÉRENCE, dépôt JS en lecture (`C:\dev\Jeux IV Kingdoms`) :
  - `git cat-file -t fee66ae` → `commit` ; `git merge-base --is-ancestor fee66ae HEAD` → oui ; `HEAD` = `fee66ae`
    sur `codex/p0-temporal-hud` ; `git status` → **36 modifications sous `src/`** (voir REFERENCE_JS.md).
  - `git ls-remote origin` avant : `main` = `f461eda` ; `codex/p0-temporal-hud` = `fee66ae` (déjà joignable,
    mais par une branche mobile) ; aucun tag `anastasis*`.
  - `git tag -a anastasis-ref-p3 fee66ae -m "Reference du portage Unreal, phase 3"` puis
    `git push origin anastasis-ref-p3` → `* [new tag] anastasis-ref-p3 -> anastasis-ref-p3`. Aucune branche
    poussée, pas de `--force`. Copie de travail JS non touchée (HEAD et 36 modifications identiques après).
  - `git ls-remote origin 'refs/tags/anastasis-ref-p3*'` → `c1145ba9…` (tag) et `fee66ae8…` (`^{}`).
  - Clone frais : `git clone --depth 1 --branch anastasis-ref-p3 <url> <scratchpad>/jsref-clone` →
    `HEAD = fee66ae8b571f6f7bcbe6a61f9749d0a812b84e2`, 0 fichier modifié.
- INVENTAIRE : `node tools/migration/inventory-js-sim.mjs -ref <clone> -out docs/migration/phase2/P2_INVENTAIRE_JS.md`
  → `233 modules ; 9 portes, 31 partiels, 166 a porter` ; 58 721 lignes de code à porter (41 342 + 17 379 restant
  dans les partiels) ; contrôle du scanner : 0 définition de colonne 0 avalée sur 233 modules.
  - Contre la copie de travail : `REFUS: … porte 36 modification(s) non commitee(s) sous src/.` (sortie 3).
  - L'ancien inventaire (234 modules, 63 492 lignes) comptait `sim/observability.js`, fichier non suivi de la
    copie de travail, absent de `fee66ae`.
- VECTEURS (lecture seule des `.inl`) : les 12 déclarations `gen-parity.mjs` (11 947 vecteurs) et
  `gen-nav-vectors.mjs` (32 chemins, 24 cases) régénérées depuis le tag **dans le scratchpad**, comparées aux `.inl`
  commités (en-têtes de provenance et fins de ligne exclus) : **13 sur 13 identiques**. Aucun `.inl` modifié.
- BUILD : `tools\unreal\anastasis-unreal.ps1 build` → `BUILD::PASS` (152 s, avant rebase ; mission sans C++).
- TESTS (avant rebase, `tools\unreal\report-tests.ps1`) : `TESTS::PASS` —
  **221 PASS / 4 KNOWN_EXPECTED_FAILURE / 0 FAIL**, 225 annoncés. Les 4 marqués : `Anastasis.Sim.Parite.Fbm`,
  `Anastasis.Sim.Parite.SemantiqueJs`, et les deux `AI.Toolsets.AnastasisInspect…` du registre.
- FINISH : refait après rebase sur `main` `6371405` (voir le compte rendu de passation).
- COMMANDS :
  - `git -C "C:/dev/Jeux IV Kingdoms" tag -a anastasis-ref-p3 fee66ae -m "Reference du portage Unreal, phase 3"`
  - `git -C "C:/dev/Jeux IV Kingdoms" push origin anastasis-ref-p3`
  - `git clone --depth 1 --branch anastasis-ref-p3 https://github.com/TheCompilateur/Jeux-IV-Kingdoms.git <tmp>`
  - `node tools/migration/inventory-js-sim.mjs -ref <tmp> -out docs/migration/phase2/P2_INVENTAIRE_JS.md`
  - `tools\unreal\anastasis-unreal.ps1 build` ; `tools\unreal\report-tests.ps1`

## SCN

Sans objet : aucune scène, aucun asset.

## PLY

Sans objet : rien de jouable.

## INTEGRATION_RISK

- `tools/migration/inventory-js-sim.mjs` **refuse** désormais une référence dont `src/` est modifié. Sans `-ref`,
  il pointe `C:/dev/Jeux IV Kingdoms`, qui est modifié : la commande nue échoue (sortie 3), c'est voulu.
- Les autres outils de `tools/migration/` prennent toujours la copie de travail par défaut et ne refusent rien ;
  `REFERENCE_JS.md` dit de leur passer `-ref`. `sim-scenario-001` ajoute la provenance (commit, état propre) à
  l'en-tête des traces.
- `ported-functions.mjs` doit suivre `PORTAGE.md` : une mission qui porte une fonction l'ajoute dans le même commit,
  sinon l'inventaire la compte encore « à porter ».
- `sim-scenario-001` (branche `agent/sim-scenario-001`) est empilée sur cette mission : la verser d'abord.

## STOP

- Ne revendique pas que les vecteurs de **couches 0-1** (`AnastasisParityVectors.inl`, générés par
  `tools/unreal/gen-parity-vectors.mjs` du dépôt JS) sont fidèles au tag : non régénérés ici (le générateur écrit
  depuis le dépôt JS).
- Ne revendique pas qu'une fonction « portée » de l'inventaire est prouvée bit à bit : la colonne recopie
  `PORTAGE.md` (et, pour les modules cités en entier, ce que le C++ nomme) ; la preuve reste dans
  `Anastasis.Sim.Parite.*`.
- Le reste d'un module partiel est un ordre de grandeur : recherche de noms indulgente pour les modules cités en
  entier, stricte pour les modules à liste ; code hors fonction réparti au prorata.
- N'a pas touché aux `.inl`, au registre `known-expected-failures.txt`, ni au dépôt JS (hors le tag).
- `P3_PLAN.md` §1 et §8 gardent leurs chiffres et leur constat du 2026-10-01 (63 492 lignes ; « GitHub s'arrête à
  `f461eda` ») : c'est le plan tel que décidé ; la correction est dans `REFERENCE_JS.md` et `PORTAGE.md`.
