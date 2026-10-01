# HANDOFF: sim-scenario-001

## MISSION

Phase 3, jalon A, deuxième mission (`P3_PLAN.md` §2-3) : scénarios et masques du harnais
différentiel, côté JS. Un scénario = une sauvegarde JS (`serialize`) construite avec l'API publique
de la référence ; les systèmes non portés sont masqués de l'extérieur ; l'émetteur charge le
scénario, le comparateur juge un périmètre déclaré. Premier scénario : `endurance`. Aucun C++.

## FILES_OWNED

- `tools/migration/build-scenario.mjs` (nouveau)
- `tools/migration/scenarios/endurance.mjs`, `endurance.json` (nouveaux)
- `tools/migration/scenarios/masks.mjs`, `scenario-format.mjs` (nouveaux)
- `tools/migration/emit-state-digests.mjs`, `compare-digests.mjs`, `selftest-harness.mjs` (étendus)
- `docs/migration/phase3/P3_SCENARIOS.md` (nouveau)
- `docs/migration/phase2/P2_HARNAIS_DIFFERENTIEL.md` (renvoi aux scénarios)
- `docs/unreal/handoffs/sim-scenario-001.md`

## COMMIT

BRANCH_HEAD (`agent/sim-scenario-001`, **empilée sur `agent/js-ref-pin-001`**)

## MEC

- Référence : clone frais du tag `anastasis-ref-p3` (`fee66ae`, 0 modification). Dépôt JS non écrit.
- `node tools/migration/build-scenario.mjs -ref <tag> -scenario endurance -out tools/migration/scenarios/endurance.json`
  → `empreinte a7c317b02da7be0f, 24 masques, point fixe en 2 aller(s)-retour(s), 116 Ko`. Monde 108 × 114 ;
  champ (50, 62) ; grenier (47, 59), puits (48, 57), maison (49, 57) ; 2 fermiers au grenier, 3 sans-métier.
- Audit `sim.rng`, 3 jours (16 200 ticks) : masques levés 5 706 tirages (`updateNpc` 5 593, `minuit.lifeDaily` 48,
  `onNewDay` 36, `minuit.memory` 16, `minuit.careers` 10, `minuit.immigration` 3) ; masques posés 5 946, tous dans
  `updateNpc`.
- `node tools/migration/selftest-harness.mjs -ref <tag> -scenario-days 3 -jobs 7` → **OK — 10 cas sur 10** (8 min 25 s) :
  cas 5 endurance masquée 3 jours, 16 201 ticks, 10 sections jugées, empreinte globale identique ; cas 6 ulp au tick 300
  → 300 ; cas 7 sans masque contre masquée → refus ; cas 8 `colony.treasury` perturbée au tick 300 (écart réel au
  tick 300) hors `-sections` → non annoncée ; cas 9 `-sections` hors périmètre → refus ; cas 10 tick recomposé contre
  tick de référence sans masque, 3 jours, empreinte globale identique. Cas 1 à 4 (sans scénario) inchangés, verts.
- Deux traces indépendantes de 3 jours : `IDENTIQUES sur les 16201 ticks compares`, 10 sections jugées, 25 ignorées.
- BUILD / TESTS : `agent-worktree.ps1 finish` (mission sans C++ ; valeurs dans le compte rendu de passation).

## SCN

Sans objet.

## PLY

Sans objet.

## INTEGRATION_RISK

- **Verser `js-ref-pin-001` d'abord** : cette branche contient ses deux commits (plan + référence).
- L'émetteur en mode scénario **refuse** une référence modifiée ou un autre commit que celui du scénario :
  passer `-ref` vers un checkout propre du tag (`REFERENCE_JS.md`). Sans `-scenario`, comportement inchangé
  (cas 1 à 4 de l'autotest).
- `masks.mjs` est épinglé sur la file de minuit de `fee66ae` (17 travaux dans cet ordre) : changer de référence
  peut le faire lever — voulu.
- Pour `sim-state-reader-001` : le monde du scénario est **108 × 114** (étendue imposée par `deserialize`), pas le
  96 × 96 du test C++ d'endurance ; le lecteur doit reconstruire graine + couronne + `tileDiff`. `deserialize`
  complète `trafficTimer: 0`, `inside: null`, `yardFood: 0` : le fichier porte déjà ces valeurs.

## STOP

- Ne revendique aucune comparaison JS / Unreal : ni lecteur ni émetteur C++ n'existent.
- Ne revendique pas que tous les systèmes non portés sont neutralisés : `navService` et ce qu'appelle `updateNpc`
  ne sont pas masquables (`P3_SCENARIOS.md` §3) ; `actors` divergera tôt côté Unreal.
- L'audit des tirages peut sous-estimer (une référence gardée à la fonction `rng` d'origine ne serait pas comptée) ;
  il ne change pas la trajectoire.
- N'a touché ni au C++, ni aux `.inl`, ni au registre `known-expected-failures.txt`, ni au dépôt JS.
