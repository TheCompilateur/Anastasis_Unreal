# HANDOFF: lifestyle-decision-001

## MISSION

Faire peser dans la décision le mode de vie et la nature des habitants, comme la référence. Au tick 32 du
harnais `endurance`, la ligne `build` de npc-2 (noctambule, « loyal », « gourmand ») valait 288,7 en C++
contre 223,8 dans la référence (registre de provenance de la référence, `enableCognitiveProvenance`).

1. **Nature** (`src/life/nature.js`) : module `Life/AnastasisNature` — `natureGoalBias`, `natureWorkFactor`,
   `natureStickBonus`, sur la nature telle qu'`ensureNature` la rend (attributs bornés, catalogue filtré).
2. **Rythme** : `phaseWorkFactor(sim, npc)` complet (`AnastasisRhythm::PhaseWorkFactor`, phase personnelle,
   garde, aubergiste, bourreau de travail, noctambule, matinal).
3. **Décision** (`ChooseGoal`) :
   - facteur de travail = `effectiveWork × phaseWorkFactor × 1 × natureWorkFactor` ;
   - terme `score.rhythm_status_lifestyle` = `phaseBias + statusBias + lifestyleBias` sur toutes les lignes ;
   - `score.nature` sur chaque ligne (après la météo) ;
   - collant : `natureStickBonus` ; `goalExplain` : la cause « caractère ».
4. **Lecteur** : `npc.nature` et `npc.gold` (lus, jamais réécrits).

## FILES_OWNED

- `Source/AnastasisSim/Public/Life/AnastasisNature.h`, `Private/Life/AnastasisNature.cpp` (nouveaux)
- `Source/AnastasisSim/Public/Life/AnastasisVillageRhythm.h`, `Private/Life/AnastasisVillageRhythm.cpp` : `PhaseWorkFactor`
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h`, `Private/Village/AnastasisVillage.cpp` : `FNpc::Nature` / `Gold`, `RhythmStatusLifestyle`, `StatusBias`, `NatureGoalBiasOf`, facteur de travail, collant, explication
- `Source/AnastasisSim/Private/Harness/AnastasisJsSave.cpp` : lecture de `nature` et `gold`
- `Source/AnastasisSim/Private/Tests/AnastasisNatureTests.cpp`, `AnastasisNatureVectors.inl` (nouveaux), `AnastasisRhythmTests.cpp`, `AnastasisRhythmVectors.inl` (régénéré)
- `tools/migration/parity/nature.mjs` (nouveau), `tools/migration/parity/village-rhythm.mjs` (cas `PhaseWorkFactor`)
- `Source/AnastasisSim/ECARTS.md` : n° 8 et n° 10 modifiés
- `docs/unreal/handoffs/lifestyle-decision-001.md`

## COMMIT

Voir `git log agent/lifestyle-decision-001`. Posée sur `agent/premiere-pensee-001` (03bfd41).

## MEC

- BUILD : `BUILD::PASS`.
- Vecteurs : `node tools/migration/gen-parity.mjs -ref <clone anastasis-ref-p3> nature.mjs` → 3 cas, 5 200 vecteurs ;
  `village-rhythm.mjs` → 3 cas, 1 272 vecteurs (dont `PhaseWorkFactor`, 18 fractions × 5 métiers × 7 modes de vie).
- TESTS (`report-tests.ps1 -Filter Anastasis.Sim`, avant `finish -Prove`) : PASS 128, KNOWN_EXPECTED_FAILURE 2
  (`Parite.Fbm`, `Parite.SemantiqueJs`), FAIL 0, 130/130 annoncés. `Parite.Nature` (nouveau) et `Parite.Rythme`
  passent ; tous les `Village.*` restent verts (leurs habitants n'ont ni nature ni mode de vie : rien ne change).
- HARNAIS (`endurance`, 16 200 ticks, forage au tick 32) :
  - ligne `build` de npc-2 : **288,7 → 211,8** ; la référence vaut 223,8, soit 211,8 + 12 de l'intention du jour
    (`score.day_intent`, non portée, n° 24) ; collant **19,4** (= référence, « loyal » : 18 × 1,08) ;
  - premier tick divergent toujours 32 (`actors`) : les deuxième et troisième lignes de la référence
    (`deliver` 164,1 par micro-plan, `gatherStone` 155,9 par ambition) ne sont pas portées (n° 1 / n° 24).

## PROOFS

PROOFS: (aucune)

## SCN

`endurance`, inchangé.

## PLY

Sans objet.

## ECARTS

- modifié : n° 8 — `lifestyleBias` dans la table et `phaseWorkFactor` complet sont portés ; restent `lifestyleTravelFactor` (nav-service-001), `lifestyleIndoorDuration`, `lifestyleTarget`, et le tirage d'un mode de vie absent.
- modifié : n° 10 — la nature lue pèse dans la décision (penchant, facteur de travail, collant) ; restent moyens les liens (`natureSocialMods`), l'apprentissage (`natureLearnFactor`), l'héritage, et le tirage d'une nature absente.
- hérité : n° 32, n° 33 (planner-module-001, resource-targets-001), n° 34, n° 35, n° 36 (premiere-pensee-001), non encore versés ; inchangés ici.

## INTEGRATION_RISK

- Dépend de premiere-pensee-001 (et donc de resource-targets-001, planner-wiring-001) : verser après elles.
- `ChooseGoal` : le terme de rythme de chaque ligne passe par `RhythmStatusLifestyle` (5 appels). nav-service-001
  (renfort) ne touche pas `ChooseGoal`.
- Comportement du jeu : un village créé par le C++ n'a ni nature ni mode de vie ni or : sa décision ne change pas.
  `statusBias` lit un or absent comme celui d'un habitant neuf (10 à 29) : ni misère ni aisance.

## STOP

- Ne revendique pas la concordance de la table au tick 32 : l'intention du jour, le micro-plan et l'ambition (n° 24)
  et les lignes non portées (n° 1) restent.
