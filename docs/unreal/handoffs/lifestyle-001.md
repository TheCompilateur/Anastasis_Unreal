# HANDOFF: lifestyle-001

## MISSION

Porter `src/sim/lifestyle.js` (reference `anastasis-ref-p3` = `fee66ae`) en C++, module seul : six modes
de vie, leur tirage, leurs penchants sur les buts, la marche, la duree a l'interieur, la destination, la
memoire des lieux et le score de regularite quotidien. Structure `FLifestyle` au format de `npc.lifestyle`.
Mission confiee par « Simulateur IV Kingdoms migration phase 3 » (rapport 2 : `lifestyle.lastNotedDay`
bouge cote JS et pas cote C++ ; `ensureLifestyle` tire dans `sim.rng` avant la cadence). Aucun
branchement dans `FNpc`, `FVillage` ou `UpdateNpc`.

## FILES_OWNED

- `Source/AnastasisSim/Public/Life/AnastasisLifestyle.h`, `Private/Life/AnastasisLifestyle.cpp` (nouveaux)
- `Source/AnastasisSim/Private/Tests/AnastasisLifestyleTests.cpp` (nouveau)
- `Source/AnastasisSim/Private/Tests/AnastasisLifestyleVectors.inl` (nouveau, genere)
- `tools/migration/parity/lifestyle.mjs` (nouveau)
- `tools/migration/ported-functions.mjs` (entree `sim/lifestyle.js`, `lifestyleLabel` / `lifestyleColor` hors)
- `Source/AnastasisSim/PORTAGE.md`, `docs/migration/phase2/P2_INVENTAIRE_JS.md` (regenere)
- `docs/unreal/handoffs/lifestyle-001.md`

Non touches : `FNpc`, `FVillage`, `UpdateNpc`, `AnastasisVillage*`, `Harness/*`, les scenarios, `masks.mjs`,
`known-expected-failures.txt`, les `.inl` existants, le depot JS.

## COMMIT

Le commit qui porte cette fiche sur `agent/lifestyle-001`.

## MEC

- BUILD : `tools\unreal\anastasis-unreal.ps1 build` → `BUILD::PASS`.
- Vecteurs : `node tools/migration/gen-parity.mjs -ref <clone anastasis-ref-p3> lifestyle.mjs` — 11 cas,
  7 465 vecteurs : `dayPhase` (15), `lifestyleForId` (8), `assignLifestyle` a flux graine (786 : 3 graines x
  5 preferences x 5 stades x 10 metiers, plus traits et famille ; mode choisi ET etat du flux apres),
  `assignLifestyle` sur le flux de secours remis a graine (15), `ensureLifestyle` (21 : garde sans tirer,
  retire si absent ou inconnu), `lifestyleBias` (3 840 : 6 modes x 10 fractions x 16 buts x 4 profils),
  `lifestyleTravelFactor` (1 920), `lifestyleIndoorDuration` (288), `lifestyleTarget` (480 : 4 mondes de
  tavernes, maison, lieu favori), `lifestyleNotePlaceUse` (20 suites de 12 usages, ordre des cles compris),
  `lifestyleDailyUpdate` (72 suites de 60 pas sur plusieurs jours, memes jours compris).
- TESTS : TESTS_CIBLES
- MUTATION : MUTATION_RESULTAT
- Suite complete : par `agent-worktree.ps1 finish`.

## npc.lifestyle — CHAMPS ET ORDRE D'APPEL (liste de lecture du lecteur)

`npc.lifestyle` = `{ id, sinceDay, rhythmScore, lastNotedDay }` (`FLifestyle`, memes noms, `id` en chaine
JS). Sauve tel quel par `packActor` (`{ ...actor }`). Un champ absent d'une vieille sauvegarde vaut
`rhythmScore 0`, `lastNotedDay 0`, `sinceDay 1` (les `??=` d'`ensureLifestyle`, a appliquer au lecteur) ;
un `id` absent ou inconnu fait RETIRER le mode de vie (un tirage).

Champs de l'habitant que le module lit (`FLifestyleSubject`) : `lifeStage`, `apprenticing`, `jobId`,
`trait.explore` / `.build` / `.trade`, `familyId`, `partnerId`, `childIds.length`, `home` (son id),
`skill`, `energy`, `goal`, `target` (non nul), `placeMemory.favoriteBuildingId`.

Dans `updateNpc` (`src/sim/npc.js`, l. 811) :
1. `ensureLifestyle(npc, sim.rng)` — PREMIERE ligne, avant `ensureNeeds`, `ensureCulture` et la cadence,
   donc a chaque tick. Tire UN `sim.rng()` seulement si le mode de vie manque ou est inconnu.
2. `consumeNpcSimulationCadence` ; si elle ne rend pas `run`, sortie.
3. `lifestyleDailyUpdate(sim, npc)` — juste apres la cadence, AVANT `tickNeeds` ; lit `sim.day` et
   `sim.dayFrac()`, ecrit `rhythmScore` et `lastNotedDay` une fois par jour au plus. Ne tire pas (le mode
   de vie existe deja a ce point).

Ailleurs (aucun ne tire si le mode de vie existe) : `lifestyleBias` dans le score des buts (`npc.js`
l. 1150, x0,55 l. 2217, x0,8 l. 2266) ; `lifestyleIndoorDuration` (`npc.js` l. 3633) ;
`lifestyleTravelFactor` (`simulation.js` l. 990) ; `lifestyleNotePlaceUse` (`simulation.js` l. 4086, dans
`notePlaceUse`) ; `lifestyleTarget` (`destination.js` l. 60). `ensureLifestyle(npc, rng)` aussi a la
creation (`npc.js` l. 722), dans `normalizeNpcStory` (`simulation.js` l. 1462) et au chargement
(`save.js` l. 752, avec `sim.rng`).

## SCN

Aucune scene, aucun asset.

## PLY

Sans objet : rien n'est branche dans le jeu.

## INTEGRATION_RISK

- Fichiers C++ nouveaux uniquement. Fusion texte possible sur `PORTAGE.md`, `ported-functions.mjs`,
  `P2_INVENTAIRE_JS.md`.
- `lifestyleTarget` : le C++ rend le batiment, pas le point. Le point d'acces doit etre calcule par
  l'appelant exactement quand la reference appelle `sim.buildingAccessPoint` (une fois, seulement si un
  batiment est rendu).

## STOP

- Pas de branchement, pas de champ dans `FNpc`, pas de lecture dans `AnastasisJsSave`.
- `lifestyleLabel`, `lifestyleColor` non portes (presentation).
- Bizarrerie copiee, pas corrigee : `LIFESTYLES[id]` lit aussi le prototype d'`Object` en JS
  (`"toString"` serait un mode de vie « connu »). Le C++ ne reconnait que les six identifiants.
