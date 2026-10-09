# HANDOFF: soif-dabord-001

## MISSION

Demande d'Alexandre (2026-10-09) : « quand on meurt de soif, on boit d'abord ; un effet psychologique positif de boire, qui
les incite à boire quand ils ont soif ; un système de récompense court ».

Trouvé par l'étude d'une année (annee-valmire-001, monde 1204) et confirmé par un relevé pas à pas : le puits marche (un
habitant logé y boit trois fois le premier jour), mais les autres meurent de soif en six jours à côté de lui. La faim gagne
toujours la table (la préséance vitale ne lève `drink` qu'au-dessus du repos et du loisir), les portes Noûs imposent `eat`
sans nourriture nulle part, et une décision de fatigue périmée (`eat->rest (urgency_fatigue)`) cloue les sans-abri au repos
à énergie 100. Au monde 12345, la famille du Scribe meurt de faim (la cause est seulement nommée « de soif »).

Écart n° 58 (réservé par `agent-worktree.ps1 ecart`), deux règles :

1. **la soif d'abord** : à soif mortelle (88), si boire est dans la table, le but devient `drink`, quoi qu'aient dit la table,
   les portes ou le verrou de quart (`CommitGate` : `…->drink (soif mortelle)`) ; le joueur n'est pas touché ;
2. **le soulagement** : boire avec une soif d'au moins 40 pose l'humeur `drinkRelief` : +3 de moral tout de suite, +0,02/s
   pendant environ une journée, et +10 à l'envie de boire si la soif revient pendant ce temps.

Interrupteur `anastasis.Village.ThirstFirst` (défaut 1), posé à la remise à zéro et à chaque image ; éteint dans `FVillage`
(harnais et parité au bit près).

## FILES_OWNED

- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp` : `ChooseGoal` (la soif mortelle ; ligne `drink`), `Perform` (le
  soulagement)
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` : `SetThirstFirstEnabled`, `bThirstFirstEnabled`
- `Source/AnastasisSim/Private/Village/AnastasisVillageStateDigest.cpp` : `thirstFirstEnabled`
- `Source/AnastasisSim/Public/Sim/AnastasisSimulation.h` : `SaveFormatVersion` 7
- `Source/AnastasisSim/Public/Life/AnastasisBonds.h`, `Private/Life/AnastasisBonds.cpp` : `DrinkRelief*`, `StampDrinkRelief`,
  `DrinkReliefBias`, `TickMoodlets`
- `Source/AnastasisSim/Private/Tests/AnastasisThirstFirstTests.cpp` (nouveau) : `Anastasis.Sim.Village.Soif.Recompense`,
  `…Soif.Dabord`
- `Source/AnastasisSim/ECARTS.md` : fiche n° 58
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp` : CVar `anastasis.Village.ThirstFirst`
- `Source/Anastasis_UnrealV2/Sim/AnastasisThirstFirstHostTests.cpp` (nouveau) : `Anastasis.Village.Soif.Monde1204`

## COMMIT

Voir `git log main..agent/soif-dabord-001`.

## MEC

- BUILD: PASS (worktree)
- TESTS: `tools\unreal\report-tests.ps1 -Filter 'Anastasis.Sim.Village.Soif+Anastasis.Village.Soif'` → PASS 3 /
  KNOWN_EXPECTED_FAILURE 0 / FAIL 0 :
  - `Anastasis.Sim.Village.Soif.Recompense` : l'humeur monte le moral (+3), tire vers le puits (+10), passe après 112,5 s, ne
    se double pas ; allumée, boire assoiffé la pose (moral 35,3 contre 23,0 éteinte) ; éteinte, jamais ;
  - `…Soif.Dabord` : mourant de soif et affamé, grenier vide, il boit (soif 92 → 7) ; soif ordinaire : même but qu'éteint ;
  - `Anastasis.Village.Soif.Monde1204` (le vrai village, 8 jours) : **règle allumée 0 mort de soif** (12 de faim, 2 en
    vie) ; **référence 12 morts de soif** (1 de faim, 1 en vie). Ce monde meurt encore **de faim** : son grenier est à 20
    cases du village et reste vide — autre mission (site d'ouverture, sans-métier qui ne vont pas aux champs).
- Suite complète : jouée par `finish`.
- `check-ecarts` : `ECARTS::PASS fiches=51` ; `check-state-fields` : `STATE_FIELDS::PASS structures=46 lacunes=2`

## PROOFS

PROOFS: save-load-pie, npc-life-pie, chronicle-pie

## SCN

Dans le jeu : un habitant qui meurt de soif va boire, même affamé, même au repos. Boire quand on a soif rend un peu de
moral, et l'habitant retourne plus volontiers au puits la fois suivante. La chronique ne devrait plus dire « a soif, alors
que le puits est à sa portée » suivi d'une mort.

## PLY

NOT_JUDGED.

## ECARTS

- n° 58 — nouvelle, OUVERT, A_TRANCHER, EXTENSION : la soif d'abord, et le soulagement de boire. Marques `ecart n°58` dans
  `AnastasisVillage.{h,cpp}`, `AnastasisVillageStateDigest.cpp`, `AnastasisBonds.{h,cpp}`, hôte `AnastasisSimulationSubsystem.cpp`.

## INTEGRATION_RISK

- **`SaveFormatVersion` 7** (`main` = 6) : une autre mission qui le monte aussi prend la valeur suivante au rebase.
- `MoodletMaxSlots` vaut 2 : le soulagement peut pousser une humeur « amitié neuve » plus ancienne (même règle que la référence
  pour deux humeurs).
- Ne règle pas la famine elle-même (3 cultivateurs pour 14, sans-métier qui ne vont pas aux champs) : c'est la mission
  suivante de l'étude d'une année.

## STOP

- Ne touche ni la faim, ni le repos, ni le joueur (son remède passe déjà, écart n° 21 ; ses mains sont mains-joueur-001).
