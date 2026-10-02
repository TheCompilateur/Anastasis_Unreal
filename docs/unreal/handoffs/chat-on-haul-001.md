# HANDOFF: chat-on-haul-001

## MISSION

Commandée par « Simulateur IV Kingdoms migration phase 3 », périmètre approuvé le 2026-10-02 :

1. `maybeChatOnHaul` (npc.js l. 5503) dans `Deliver` : le tirage inconditionnel `sim.rng() > 0.42`, puis le
   corps — compagnon à 3,4 cases, `canStartTalk`, `recordTalk` (`kind: "coworker"` si même métier,
   `ambientChance: 0.1`), `shareRumors` (les gisements, dans les deux sens, l'un APRÈS l'autre), gain de lien ;
2. `rollCraftMiss` générique (craftMiss.js), tiré dans les deux boucles PORTÉES qui le sautaient : la
   cueillette du fermier (`farm`) et le chantier (`build`), avec `applyCraftMissRecovery`.

Hors mission, mesuré et transmis : au tick 257 d'endurance, le raté de la référence est `tend`, dans
`helpFarm` > `progressTendWork` (but non porté) — repris par help-farm-001. Les deux tirages de
`maybeChatOnHaul` d'endurance (ticks 125 et 141) valent 0.9787 et 0.7184 : la causette ne démarre jamais ce
jour-là ; pour le harnais, seul le tirage compte.

Branche partie d'`agent/act-gate-001` (a115574), puis rebasée sur `main` une fois act-gate-001 versée
(41ecbea).

## FILES_OWNED

- `Source/AnastasisSim/Public/Work/AnastasisCraftMiss.h`, `Private/Work/AnastasisCraftMiss.cpp` (nouveaux)
- `Source/AnastasisSim/Public/Work/AnastasisGather.h`, `Private/Work/AnastasisGather.cpp` : `CraftFatigueT`
  (le `t` de `craftFatigueOf`, partagé par la période et le raté ; calcul inchangé)
- `Source/AnastasisSim/Public/Life/AnastasisBonds.h`, `Private/Life/AnastasisBonds.cpp` : `ShouldSpeakNow`
  prend l'ambiance en paramètre (défaut inchangé)
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` : `FNpc::CraftMissAt`, `CraftMissKind`,
  `CraftMissCraftId`, `CraftMissStampAt` ; `FVillage::RollCraftMiss`, `ApplyCraftMissRecovery` (publics),
  `MaybeChatOnHaul`, `TellSpots`, options de `RecordTalk` ; note n° 12 de l'en-tête
- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp` : `ProgressCraftGather` (raté farm),
  `ProgressBuildWork` (raté build), `Deliver` (causette), `RecordTalk`, les nouvelles fonctions
- `Source/AnastasisSim/Private/Tests/AnastasisChatHaulTests.cpp`, `AnastasisChatHaulVectors.inl`,
  `AnastasisChatHaulDrawVectors.inl` (nouveaux) ; `AnastasisVillageGatherTests.cpp` (deux tests)
- `tools/migration/parity/chat-haul.mjs`, `tools/migration/trace-chat-haul.mjs` (nouveaux)
- `Source/AnastasisSim/ECARTS.md` (n° 8, 10, 11, 16), `PORTAGE.md`, `tools/migration/ported-functions.mjs`,
  `docs/migration/phase2/P2_INVENTAIRE_JS.md` (régénéré)

## COMMIT

Le commit qui porte cette fiche sur `agent/chat-on-haul-001`.

## INTERFACE — `rollCraftMiss` (pour help-farm-001 et build-materials-001)

```cpp
// FVillage, public. Tire au plus UNE fois dans le flux partagé ; rend vrai si le coup est raté (et l'estampille).
bool RollCraftMiss(FNpc& Npc, const FString& CraftId);
// Period = swingPeriodFor(npc, craftId) calculé AVANT d'incrémenter les coups de la session.
void ApplyCraftMissRecovery(FNpc& Npc, double Period);
```

Appel, comme la référence :

```cpp
if (RollCraftMiss(Npc, TEXT("tend")))
{
	ApplyCraftMissRecovery(Npc, /*swingPeriodFor(npc, "tend")*/ Period);
	return 1; // "working"
}
```

Fonctions pures : `AnastasisCraftMiss::MissKindFor`, `FatigueMissMul`, `MissChance(CraftId, Skill, Mastery,
SwingsDone, Energy)`, `CanRoll(CraftId, Now, CraftMissAt, LastMissAt)`. Maîtrise : 0 (aucune technique, n° 10).

## POUR LE LECTEUR — champs JS qui bougent

- `npc.craftMissAt` → `FNpc::CraftMissAt` (absent = 0).
- `npc.craftMiss = { kind, craftId, at }` → `CraftMissKind`, `CraftMissCraftId`, `CraftMissStampAt` (absent =
  vide, vide, 0).
- `npc.workSession.swingsDone`, `lastSwingAt`, `nextSwingAt` après un raté (déjà lus).
- Causette : `npc.relations[other]` des deux habitants, `npc.lastTalk` / `listener.lastTalk`, `talkFatigue`,
  `mind.spots` des deux (gisements entendus, `hearsay`) — déjà projetés pour `socialize`. Non portés, donc non
  écrits côté C++ : `mind.blocked…`, savoir négatif, `mind.market`, `mind.beliefs`, `mind.people` (n° 16).

## MEC

- BUILD : `tools\unreal\anastasis-unreal.ps1 build` → `BUILD::PASS`.
- Vecteurs purs : `node tools/migration/gen-parity.mjs -ref <clone anastasis-ref-p3> chat-haul.mjs` → 4 cas,
  4 536 vecteurs (`craftMissChance` × profils / compétences / coups / énergie / maîtrise, `bestCraftMastery`,
  `canRollCraftMiss`, `shouldSpeakNow` avec `ambientChance` 0,1).
- Tirages mesurés : `node tools/migration/trace-chat-haul.mjs -ref <clone>` → 43 tirages `rollCraftMiss`
  (tend, chop, quarry ; 1 raté), 2 tirages `maybeChatOnHaul`.
- TESTS (`report-tests.ps1 -Filter Anastasis.Sim`) : **PASS 124, KNOWN_EXPECTED_FAILURE 2** (`Parite.Fbm`,
  `Parite.SemantiqueJs`), **FAIL 0**, 126/126.
  - `Anastasis.Sim.Parite.CoupRate` : 4 640 valeurs, 0 écart (4 536 vecteurs purs, 43 tirages `rollCraftMiss`
    mesurés dont 1 raté, 2 tirages `maybeChatOnHaul` mesurés).
  - `Anastasis.Sim.Village.Recolte.CoupRate` : le raté (un tirage, pas de rendement, `whiff`, `craftMissAt`,
    reprise × 1,38 au bit), le refroidissement (aucun tirage, le coup rend), la porte rouverte (un tirage, le
    coup rend). Pendant l'amorce, le flux est tenu haut (aucun raté) et le sac vidé au grenier (conservé).
  - `Anastasis.Sim.Village.Recolte.CausetteDepot` : au-dessus de 0,42, un seul tirage et aucun lien ; dessous,
    compagnon à 1,30 case, tirage + un tirage par sens (gisements 9 / 8), liens +3 / +2.
- MUTATIONS (posée, suite `Anastasis.Sim` entière, retirée) :
  - C1, tirage de la causette retiré (`if (0.5 > 0.42) return;`) → `Recolte.CausetteDepot` : `pas de causette : un
    seul tirage` (état du flux), `lien du livreur +3` 3 / 0, `lien du compagnon +2` 2 / 0.
  - C2, reprise sans × 1,38 → `Recolte.CoupRate` : `rate : reprise allongee (memes bits)`.

## ECARTS

- modifié : n° 11 — `rollCraftMiss` porté et tiré pour `farm` et `build` ; restent les profils dont la boucle
  n'est pas portée (tend, chop, quarry) et `exploreTarget` dans la récolte.
- modifié : n° 16 — `maybeChatOnHaul` porté ; son `shareRumors` ne fait que les gisements, comme `socialize`.
- modifié : n° 8 — `maybeCounselPair` sans aîné.
- modifié : n° 10 — maîtrise des techniques à 0.

## PROOFS

PROOFS: gather-deliver-pie

## SCN

Aucune scène, aucun asset.

## PLY

Sans objet.

## INTEGRATION_RISK

- Le comportement change : un coup de cueillette ou de chantier sur ~12 rate (pas de rendement, reprise
  plus lente) ; une livraison sur ~2,4 tente une causette. Le flux partagé tire un coup de plus par
  livraison et par coup de métier.
- `AnastasisVillage.cpp` : `ProgressCraftGather`, `ProgressBuildWork`, `Deliver`, `RecordTalk` ; help-farm-001
  touche `Act` / `Perform` et la ligne `helpFarm` (pas de recouvrement de fonction).

## STOP

- Ne revendique pas le tick 257 : c'est `helpFarm` (help-farm-001).
- Les tests de village prouvent le raté et la causette sur un village construit, pas sur une trajectoire du
  harnais (la causette n'a jamais lieu sur endurance).
