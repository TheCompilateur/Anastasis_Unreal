# HANDOFF: nav-service-001

## MISSION

Porter `src/sim/navService.js` (reference `anastasis-ref-p3` = `fee66ae`) en C++, **module seul** : file de
requetes de chemin budgetee, cache partage exact + zone, budget A* par vitesse, priorites. Plus les morceaux de
`navGrid.js` dont il depend et que la couche terrain n'avait pas pris : metriques de navigation, anneau de trace,
`navigationTargetKey`. Mission confiee par la session « Simulateur IV Kingdoms migration phase 3 », qui fera le
branchement dans le village apres `budget-cadence-001`. Aucun appel depuis `FVillage` dans cette mission.

## FILES_OWNED

- `Source/AnastasisSim/Public/World/AnastasisNavService.h` (nouveau)
- `Source/AnastasisSim/Private/World/AnastasisNavService.cpp` (nouveau)
- `Source/AnastasisSim/Private/Tests/AnastasisNavServiceTests.cpp` (nouveau)
- `Source/AnastasisSim/Private/Tests/AnastasisNavServiceVectors.inl` (nouveau, genere)
- `tools/migration/gen-nav-service-vectors.mjs` (nouveau)
- `tools/migration/ported-functions.mjs` : entree `navService.js`, entree `navGrid.js` completee
- `Source/AnastasisSim/PORTAGE.md` : couche 2
- `docs/migration/phase2/P2_INVENTAIRE_JS.md` : regenere
- `docs/unreal/handoffs/nav-service-001.md` : cette fiche

Non touches, comme demande : `Village/AnastasisVillage.*`, `Harness/*`, les `.inl` existants,
`tools/migration/scenarios/*` et les outils du harnais, `known-expected-failures.txt`.

## COMMIT

Le commit qui porte cette fiche sur `agent/nav-service-001`, base `main` `b8c6d06`.

## MEC

- BUILD : `tools\unreal\anastasis-unreal.ps1 build` → `BUILD::PASS` (UE 5.8.2 / CL 56702186).
- TESTS cibles : `tools\unreal\report-tests.ps1 -Filter Anastasis.Sim.Parite.NavService`
  → PASS 2, KNOWN_EXPECTED_FAILURE 0, FAIL 0, 2 annonces / 2 terminees.
  - `Anastasis.Sim.Parite.NavServiceFonctions` : 202 vitesses (stepMult, TTL, sweep, maxCalcs, motif binaire),
    36 cles de cache, 5 cles de cible, 140 priorites.
  - `Anastasis.Sim.Parite.NavService` : 5 scenarios, **5 473 operations**, SHA-1 du texte canonique de l'etat
    complet apres chacune (file, index des attentes, cache dans l'ordre d'insertion, champs de chemin des 8 PNJ,
    metriques, anneau de trace).
- Vecteurs : `node tools/migration/gen-nav-service-vectors.mjs -ref <clone anastasis-ref-p3>` ;
  `-dump <fichier>` ecrit les textes canoniques JS pour comparer a l'oeil avec le journal du test.
- Couverture des scenarios (compteurs JS en fin de scenario) : file jusqu'a 6 jobs, cache jusqu'a **480**
  (le garde-fou retire les 80 premieres cles), resolutions de file servies par le cache (`CACHE_HIT`),
  chemins vides, eau refusee puis toleree, TTL, version perimee, sweep, vitesses 0/1/2/5/10, budgetMul
  NaN/0/3/-1/0.15, priorite explicite, faveur, `maxJobs` 0 et 1, acteur retire, cible changee.
- MUTATIONS (le test doit mordre), chacune construite puis testee avec le filtre ci-dessus, puis retiree :
  - garde-fou efface les 80 dernieres cles : **detectee** (`garde-fou #1355`, cache 401/401, contenu different).
  - seuil de zone 8 au lieu de 9 : **non detectee — mutation equivalente** (premier noeud a 8 cases au plus du
    demandeur dans la meme zone 8x8), voir PORTAGE.md.
  - seuil de zone 7 : **detectee** (`garde-fou #555`, cache 223/222).
  - tri non stable : non mesure (files <= 6 jobs, le tri d'Unreal y est stable aussi).
- Suite complete : par `agent-worktree.ps1 finish` (build unity apres commit + `report-tests`), resultat
  rapporte a l'integrateur avec le nom de la branche.

## SCN

Aucune scene, aucun asset. Module headless.

## PLY

Sans objet : rien n'est branche dans le jeu.

## INTEGRATION_RISK

- Fichiers nouveaux uniquement cote C++ ; collision possible seulement sur `PORTAGE.md`,
  `ported-functions.mjs` et `P2_INVENTAIRE_JS.md` si une autre mission les touche (fusion texte simple).
- Pour le branchement (mission suivante) : `FNavAgent` porte les champs de chemin que le service ecrit. `FNpc`
  en a une partie sous les memes noms (`Path`, `PathStep`, `bHasPathGoal`, `PathGoal`, `bPathFailed`,
  `PathCooldown`, `NavTargetKey`, `NavVersion`) ; il lui manque `PathFailStreak` et `NavRequestedAt`. Choix
  laisse a l'integrateur : faire porter un `FNavAgent` par `FNpc`, ou un adaptateur `INavServiceHost`.
- `simulation.js` appelle `beginNavTick(this, { budgetMul: budgetMul.pathBudgetMul })`, puis `processNavQueue`
  avant ET apres la boucle des PNJ ; `requestPath` est appele depuis la marche (`simulation.js` ~8364).
- La sauvegarde JS emporte `navCache` (`save.js`) : `FNavService::RestoreCache` est pret, non branche au lecteur
  `Harness/AnastasisJsSave` (hors perimetre).

## STOP

- Ne revendique pas le branchement dans le village, ni la parite du harnais differentiel sur `navCache`.
- `crowdNav.js` et `logicalLod.js` : non portes.
- Compteurs `navMetrics` des PNJ (`stuck*`, `pathDoorWaits`...) et points d'acces des batiments : non portes.
- Ecart theorique assume : `PathBudgetForSpeed` utilise `FMath::Pow`, qui peut differer d'un ulp de V8 ;
  l'arrondi entier l'absorbe, verifie sur 202 vitesses, pas prouve pour toute vitesse.
