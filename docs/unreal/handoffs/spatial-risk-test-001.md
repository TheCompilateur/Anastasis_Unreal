# HANDOFF: spatial-risk-test-001

## MISSION

Commandée par « Simulateur IV Kingdoms migration phase 3 » (2026-10-02), en complément de resource-targets-001.
`Parite.RisqueSpatial` prouve au bit la partie pure du risque spatial et de la prévision de survie. Mais aucun
test de la suite ne vérifiait l'ASSEMBLAGE dans la décision du village ; seul le harnais, lancé à part, le
montrait. Mission : un test village et deux mutations. Et déclarer le jour même une limite connue de la
projection des gisements d'on-dit (écart n° 16).

`Anastasis.Sim.Village.RisqueSpatial` :
1. la carte est calculée à la décision : un gisement lointain dont l'habitant se souvient, à minuit (budget de
   nuit nul) ;
2. elle s'ajoute à chaque ligne après la météo, `(ligne + prévision) + risque`, au bit près. La trace de
   décision garde chaque ligne avant et après (`RowsBeforeRisk` / `RowsAfterRisk`) ;
3. les replis de la préparation filtrent paresseusement les seuils : une maison posée sur le premier seuil du
   puits après lui, le seuil reste enregistré jusqu'à la décision, qui le retire. Le but choisi ne vise pas
   le puits. C'est le mécanisme du tick 32 d'endurance.

## FILES_OWNED

- `Source/AnastasisSim/Private/Tests/AnastasisVillageSpatialRiskTests.cpp` (nouveau)
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` : `FDecisionTrace::RowsBeforeRisk` / `RowsAfterRisk`
- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp` : le bloc d'ajout balisé `--- resource-targets-001`
  (deux lignes de trace)
- `Source/AnastasisSim/ECARTS.md` (n° 16)

## COMMIT

Les commits de `agent/spatial-risk-test-001`, posés sur `agent/resource-targets-001` (a6b827c).

## MEC

- BUILD : `tools\unreal\anastasis-unreal.ps1 build` → `BUILD::PASS`.
- `Anastasis.Sim.Village.RisqueSpatial` : PASS — 25 lignes tracées, 17 déplacées par les deux cartes, risque
  `gatherFood` −68,4 (pénalité bornée 1,8 × 38), gagnant `rest`, 3 seuils sur 4 au puits après la décision.
- MUTATIONS (posée, build, `Village.RisqueSpatial`, retirée), toutes deux détectées :
  - M1, biais non ajouté aux lignes : `ligne rest : 133.785, attendu 153.94899999999998`,
    `ligne gatherWood : -16.92, attendu -90.53` ;
  - M2, préparation absente (cartes vides) : `risque calcule pour gatherFood` nul ; `le seuil recouvert est retire
    par la preparation de la decision` faux ; `les autres seuils restent` : 4 au lieu de 3.
- Suite : voir `finish -Prove`.

## PROOFS

PROOFS: (aucune)

## SCN

Aucune scène, aucun asset.

## PLY

Sans objet.

## ECARTS

- modifié : n° 16 — la projection d'un gisement d'on-dit créé par le C++ (`CommitHearsaySpot`) n'a ni
  `speechActId`, ni `confidence`, ni `viaPlayerId` : elle diffère de la référence dans `actors` dès qu'une rumeur
  de gisement passe ; `recallResource` ne lit aucun des trois. (Le coordinateur avait cité le n° 14 ; il porte en
  fait la cohabitation food-supply et ne parle pas des rumeurs.)
- hérités de la base, non modifiés ici : n° 32, n° 33.

## INTEGRATION_RISK

- Dépend de `agent/resource-targets-001`, elle-même posée sur `agent/planner-wiring-001` : à verser après les deux.
- Ne change aucun comportement : deux champs de trace et un test.

## STOP

- Le test juge l'assemblage sur un village construit, pas la parité avec la référence (le harnais et
  `Parite.RisqueSpatial` le font).
