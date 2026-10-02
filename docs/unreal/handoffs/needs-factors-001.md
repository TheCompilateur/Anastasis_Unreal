# HANDOFF: needs-factors-001

## MISSION

Porter les cinq facteurs de besoins PAR HABITANT de `src/life/needs.js` (reference `anastasis-ref-p3` =
`fee66ae`) et ce qu'ils lisent : le genome et son phenotype (`src/life/genome.js`), le conditionnement
(`src/life/conditioning.js`). Fonctions pures, plus une extension des branches de `tickNeeds` par un
argument `FNeedFactors` dont le defaut (1 partout) laisse le comportement actuel identique au bit pres.
Mission confiee par « Simulateur IV Kingdoms migration phase 3 » : le rapport 2 place la premiere
divergence au tick 1 sur les besoins. Aucun branchement dans `FNpc`, le lecteur ni `UpdateNpc`.

## FILES_OWNED

- `Source/AnastasisSim/Public/Life/AnastasisGenome.h`, `Private/Life/AnastasisGenome.cpp` (nouveaux)
- `Source/AnastasisSim/Public/Life/AnastasisConditioning.h`, `Private/Life/AnastasisConditioning.cpp` (nouveaux)
- `Source/AnastasisSim/Public/Life/AnastasisNeeds.h`, `Private/Life/AnastasisNeeds.cpp` (ajouts)
- `Source/AnastasisSim/Private/Tests/AnastasisGenomeTests.cpp`, `AnastasisNeedsFactorsTests.cpp` (nouveaux)
- `Source/AnastasisSim/Private/Tests/AnastasisGenomeVectors.inl`, `AnastasisConditioningVectors.inl`,
  `AnastasisNeedsFactorsVectors.inl` (nouveaux, generes)
- `tools/migration/parity/genome.mjs`, `conditioning.mjs`, `needs-factors.mjs` (nouveaux)
- `tools/migration/ported-functions.mjs`, `Source/AnastasisSim/PORTAGE.md`,
  `docs/migration/phase2/P2_INVENTAIRE_JS.md` (regenere)
- `docs/unreal/handoffs/needs-factors-001.md`

Non touches : `Village/AnastasisVillage.*`, `Harness/*`, `tools/migration/scenarios/*`, les `.inl`
existants (`AnastasisNeedsVectors.inl` regenere pour controle : seule la ligne de provenance changeait,
fichier remis tel quel), `known-expected-failures.txt`.

## COMMIT

Le commit qui porte cette fiche sur `agent/needs-factors-001`.

## MEC

- BUILD : `tools\unreal\anastasis-unreal.ps1 build` → `BUILD::PASS`.
- TESTS cibles : `tools\unreal\report-tests.ps1 -Filter Anastasis.Sim.Parite` → PASS 27,
  KNOWN_EXPECTED_FAILURE 2 (`Parite.Fbm`, `Parite.SemantiqueJs`, au registre), FAIL 0, 29/29 annonces.
  `Parite.Besoins` (existant) reste vert sans changement de vecteur. Valeurs comparees :
  BesoinsFacteurs 20 808, Genome 2 996, Conditionnement 791.
- NaN : la reference rend `clamp01(NaN)` en NaN NEGATIF (`fff8...`), le C++ en NaN positif. Le signe
  d'un NaN n'est pas observable en JS hors `DataView` ; les tests comparent donc deux NaN comme egaux
  (meme regle que `Parite.Clamp01`). Premier run : `Conditionnement` FAIL sur ce seul point, corrige
  dans le test, pas dans le portage.
- MUTATIONS (posees ensemble, chacune visee par un test different, puis retirees) :
  - `TickNeedsRestInside` sans `RecoveryConditioningMul` : **detectee** (`BesoinsFacteurs`,
    `TickNeedsFactors[63]` situation 4, energie).
  - recombinaison : mutation maternelle tiree AVANT le choix paternel : **detectee** (`Genome`,
    `GenomeRecombine[0]` des le premier locus).
- Vecteurs : `node tools/migration/gen-parity.mjs -ref <clone anastasis-ref-p3> genome.mjs conditioning.mjs needs-factors.mjs`
  - `Parite.Genome` : 232 vecteurs — hachage, graine, 42 fondateurs et 40 enfants recombines compares
    ALLELE PAR ALLELE (24 par genome, mutations et parent absent compris), 42 phenotypes + absent,
    13 valeurs des six derivees (bornes de clamp01, NaN), 42 empreintes.
  - `Parite.Conditionnement` : 266 — `tickConditioning` sur 4 etats x 4 pas de temps x les 16
    combinaisons de drapeaux, multiplicateurs, valeur absente.
  - `Parite.BesoinsFacteurs` : 1 908 — `tickNeeds` de la reference, tel quel, sur 9 habitants x 14
    situations (toutes les branches, dont `relieve`, travail dedans, repos dehors) x 5 jeux de facteurs
    (dont phenotype et conditionnement absents) x 3 pas de temps ; huit metres ET trois meres du
    conditionnement relus apres le tick ; plus 18 cas de `needsCritical` de part et d'autre des seuils.
- Suite complete : par `agent-worktree.ps1 finish`, rapportee a l'integrateur avec la branche.

## CHAMPS DE L'HABITANT JS QUE CES FACTEURS LISENT (liste de lecture du lecteur)

`packActor` (`save.js`) recopie l'habitant entier (`{ ...actor }`) : genome, phenotype et conditionnement
sont dans la sauvegarde.

- `npc.phenotype.hydrationLossMultiplier` → `FNeedFactors::Hydration`
- `npc.phenotype.metabolicDemandMultiplier` → `FNeedFactors::Metabolic`
- `npc.phenotype.fatigueRecoveryMultiplier` → `FNeedFactors::FatigueRecovery`
- `npc.conditioning.fatigueAdaptation` → `FNeedFactors::FatigueAdaptation` (par
  `FatigueAdaptationEnergyFallMultiplier`) ; avance a chaque tick
- `npc.conditioning.recoveryConditioning` → `FNeedFactors::RecoveryConditioning` (par
  `RecoveryConditioningEnergyGainMultiplier`) ; avance a chaque tick
- `npc.conditioning.workConditioning`, `npc.conditioning.version` : avancent aussi, aucun consommateur
  dans les besoins aujourd'hui ; a lire pour que l'etat reste complet
- Phenotype absent mais genome present : `ensureGenome` le derive de `npc.genome.loci.<locus>[0|1]`
  (`DerivePhenotype`) ; seuls `metabolicEfficiency`, `hydrationRetention`, `fatigueRecovery` comptent
  pour les facteurs. Genome absent : `CreateGenome(sim.seed, npc.id)` (save.js le fait au chargement).
- Champs absents : facteur 1 (`?? 1`, conditionnement neutre 0,5) — `NeedFactorsFor(nullptr, nullptr)`.

Drapeaux de `tickConditioning` (fin de `tickNeeds`), a calculer comme `TickNeedsConditioning` :
`working = WORK_GOALS.has(npc.goal)` (dedans compris), `resting = npc.goal === "rest"`,
`overworked = needsCritical(npc)` et `fatigued = 100 - energy >= 32`, tous deux sur les metres APRES la
branche, `tickVitality` et `tickMoodlets`.

## SCN

Aucune scene, aucun asset.

## PLY

Sans objet : rien n'est branche dans le jeu.

## INTEGRATION_RISK

- `AnastasisNeeds.h/.cpp` : les signatures des branches gagnent un dernier argument par defaut ; les
  appels existants du village compilent sans changement et gardent leurs bits.
- Nouveau nom `AnastasisNeeds::AreNeedsCritical` : `AnastasisVillage::NeedsCritical` (meme formule)
  existe deja ; les avoir sous le meme nom rendait les appels ambigus (ADL). A unifier au branchement.
- `PORTAGE.md`, `ported-functions.mjs`, `P2_INVENTAIRE_JS.md` : fusion texte si une autre mission les touche.

## STOP

- Ne revendique pas le branchement (FNpc, lecteur, UpdateNpc), ni le recul de la divergence au tick 1 :
  c'est la mesure du rapport suivant, apres branchement.
- `tickMoodlets` non porte (sans moodlet actif, il ne fait rien) ; `ensureNeeds` non porte ici (tirages
  sur `sim.rng`).
- `serializeGenome`, `deserializeGenome`, `phenotypeSummary` non portes.
