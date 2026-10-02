# HANDOFF: mortality-001

## MISSION

MORTALITY_001. Premier pas de la mortalité : un habitant dont la santé est épuisée meurt. Décision
d'Alexandre (2026-10-02, conversation de la mission) : la mortalité ne tue qu'à santé 0. C'est la règle 1
de `causeOfDeath` (`src/life/mortality.js`), sans tirage ; ni âge, ni famine par tirage, ni vieillesse.

Raison : sans mort portée, une maison ne devient jamais vide en partie normale (`RemoveNpc` n'est qu'une
commande console), donc le ghost settlement d'`iceberg-001` et le vieillissement d'`abandon-001` n'ont
aucune voie de production.

## FILES_OWNED

- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` (`FDeath`, `CauseOfDeath`, `UpdateMortalityDaily`, `GetDeaths`, `MortalityFamineHunger`)
- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp`
- `Source/AnastasisSim/Public/Sim/AnastasisSimulation.h` (`DayJobLifeDaily = 10`)
- `Source/AnastasisSim/Private/Sim/AnastasisSimulation.cpp` (`RunDayJob`)
- `Source/AnastasisSim/Private/Tests/AnastasisMortalityTests.cpp`
- `Source/AnastasisSim/ECARTS.md` (fiche n° 28)
- cette fiche

Empilée sur `agent/abandon-001` (puis `agent/iceberg-001`) : **intégrer iceberg-001 puis abandon-001 d'abord**.

## COMMIT

`fd19ddf` — « feat(sim): la mortalite ne tue qu a sante epuisee, sans tirage (MORTALITY_001, ecart n°28) ».

## MEC

- BUILD: PASS — `tools\unreal\anastasis-unreal.ps1 build` dans ce worktree.
- TESTS: PASS 3/3, 0 échec connu, `report-tests.ps1 -Filter Anastasis.Sim.Mortality` :
  `Anastasis.Sim.Mortality.Cause` (santé 1 et 0,001 : vit ; 0 : soif si soif >= 88, sinon faim si faim >= 88,
  sinon épuisement si énergie <= 20, sinon faiblesse ; santé négative idem), `.Daily` (trois habitants, un à
  santé 0 et soif 95 : un mort, cause « de soif », les deux autres restent, sa maison est libre et datée,
  plus aucune relation avec lui chez les autres ; sans mort l'empreinte `Digest()` ne bouge pas ; le
  lendemain, 0 mort), `.DayJob` (rang 10 < 17).
- La suite complète est jouée par `finish -Prove` ; voir le verdict dans le message de passation.
- ECARTS: `node tools/migration/check-ecarts.mjs` → `ECARTS::PASS fiches=25 ouvertes=25 fail=0 warn=4`.
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\report-tests.ps1 -Filter Anastasis.Sim.Mortality`
  - `tools\unreal\agent-worktree.ps1 finish -Mission mortality-001 -Prove`

Les libellés attendus de `Cause` sont ceux de `mortality.js` lignes 70-76, **lus** et non exécutés :
`causeOfDeath` n'est pas exporté et `removeActor` tire dans une trentaine de modules.

## PROOFS

PROOFS: (aucune)

## SCN

UNKNOWN. Aucune preuve PIE : en jeu, un habitant n'atteint la santé 0 que par la soif ou la faim
prolongées, et le rythme de la partie (`Sim.TimeScale`) n'a pas été mesuré pour ce cas. Aucun scénario
intégré où une mort arrive d'elle-même n'a été observé.

## PLY

UNKNOWN. Aucune partie représentative.

## ECARTS

`n° 28` ouvert (REDUIT, A_TRANCHER) : mortalité réduite à la mort certaine par santé épuisée. Pas de mort par
âge ni de très grand âge, pas de famine personnelle ni de crise de grenier (`starvingDays` non porté, libellé
« de faim » sans `starvingDays > 0`), pas de vieillesse, pas de veuvage / orphelins / héritage / moral de
colonie / journal / mémorial / deuil. Appelé par le travail différé 10, hors de l'ordre de la référence
(`agePopulation` absent). Aucun tirage dans `VillageRng`.

## INTEGRATION_RISK

- Le travail 10 de la file de minuit n'était pas porté : il fait maintenant mourir des habitants. Tout test
  ou harnais qui laisse un habitant à santé <= 0 jusqu'à minuit voit désormais un mort là où il n'y en avait
  pas. Le harnais de parité vit sur la référence JS, qui tue aussi à santé 0 : la direction est celle de la
  référence, mais rien n'a été comparé.
- `AnastasisVillage.h` / `.cpp` : fichiers chauds, dépend des tampons de vacance d'`abandon-001`.
- `RemoveNpc` n'efface toujours pas les relations des autres pour la voie console ; seul
  `UpdateMortalityDaily` le fait (`forgetTheDead`).
- Rythme réel : jour ≈ 40 min de jeu (`Sim.TimeScale` 0,0375) ; les premières morts réelles viendront
  d'une soif ou d'une faim prolongées. Non mesuré.

## STOP

Ne revendique pas : une mortalité fidèle (une règle sur cinq, sans tirage), l'âge (le port n'a ni `age` ni
`lifeStage`), la famille, l'héritage, la parité du flux aléatoire avec la référence, ni qu'une partie
produise d'elle-même un ghost settlement. Le lien avec le rendu (maison noire puis usée) est établi
séparément par `iceberg-001` et `abandon-001`, sur `RemoveNpc`, pas sur cette mortalité.
