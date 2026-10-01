# BONDS_RUMORS_001 — les liens et les rumeurs

Septième tranche de la croisade. social-relax-001 avait porté `socialize()` dans sa seule branche
**sans compagnon** (écart n° 15) : on allait au puits, on y gagnait un peu de social, seul. Cette
tranche porte l'autre branche : deux habitants qui se trouvent, se parlent, se souviennent l'un de
l'autre, et **se racontent où sont les gisements**.

Référence : `fee66ae`, extraction propre (`git archive`).

## La boucle

```
socialize gagne (solitude, ami nouveau, personne mémorisée à portée)
  -> cible : le puits, recouvert par l'ami disponible (rendez-vous), puis par la personne mémorisée
  -> sur place : le compagnon le mieux placé dans la grille du tick (affinité - distance x 3,2)
  -> porte de conversation : cooldown de paire 18 s, fatigue < 2 propos / 60 s
       fermée : branche ambiante, social +14 (sans le moral +1)
  -> échange : relation +6 / +5 (+8 / +7 entre amis), social +38 (+6) et +12 (+4) pour l'autre,
     gisements racontés dans les deux sens (2 au plus, jamais un on-dit), fiches et théorie de l'esprit
  -> porte de parole (shouldSpeakNow) : un propos qui vaut quelque chose, sinon ~12 % de chance
       passée : session — les DEUX sont figés, 2 à 4 tours de 2,2 s, refus de répondre possible
  -> minuit, 8e tick : le travail `memory` oublie les gisements de plus de 14 jours, les fiches de plus de 26
```

## Ce qui est porté

**Fonctions pures** (`Life/AnastasisBonds`, prouvées bit à bit) : `hashTalk`, `chance`,
`isTalkUrgent`, `isTalkWorkBusy`, `bondKindBetween`, `dominantNeed`, `speakWorth`, `shouldSpeakNow`,
`talkHoldDuration`, `talkMaxTurns`, `refusesReply`, `bondTalkGain`, `companionAffinity`,
`bondStageRank`, `notePersonDirect`, `forgetStalePeople`, `observeMind`, `estimatedGoalOf`,
`socialMemoryBias`, `pickRememberedSeek`, les moodlets (`newFriend` : poser, tick, biais).

**Assemblage** (`Village/AnastasisVillage`) :

- la grille spatiale des habitants, reconstruite une fois par tick (`rebuildActorSpatialIndex`) ;
- `socialize()` branche compagnon : `pickSocialCompanion`, `canStartTalk`, `bumpRelation` +
  `noteBondStageCross`, besoins des deux, `createInformResourceSpotActs` /
  `commitHearsayResourceSpot`, `recordTalk`, `noteMeeting` ;
- la session : `beginTalkSession` (budget de gels simultanés), `holdTalkAct` (figés, le starter
  avance les tours), `advanceTalkTurn`, coupure sur urgence, distance ou refus ;
- la table : ligne `socialize` + `moodletGoalBias` + `socialMemoryBias` + `socialSeekBias`, dans
  l'ordre d'`adultScores` ;
- la cible : couches `bondSocialTarget` (ami disponible → milieu du segment ; solitaire → un proche
  qui ne travaille pas) et `rememberedSocialTarget` (la personne mémorisée, `socialSeekId`) ;
- les moodlets au tick des besoins.

**Hôte** (`Sim/AnastasisSimulation`) : la file de minuit a désormais ses **17 travaux**, dans l'ordre
de la référence, 2 par tick. Portés : `landRegen` (n° 0) et `memory` (n° 14 : `forgetStale`,
`forgetStalePeople`). Les 15 autres tiennent leur rang sans rien faire — l'oubli tombe donc au
**8e tick** après minuit, comme dans la référence.

## Écart déclaré n° 16

Dans l'en-tête d'`AnastasisVillage.h`. En bref, non porté :

- le **texte** des répliques : le refus (le seul effet de jeu) est évalué au premier tirage ; la
  boucle de re-tirage contre les redites dépend du texte ;
- `shareRumors` hors gisements (marché, puits, lits, dangers, accès bloqués, savoir négatif, fiches
  colportées, épisodes) : le portage n'a pas ces croyances ;
- visites de voisinage, conseils d'aîné, rencontres quotidiennes (`runSocialEncounters`), frictions ;
- les tirages `sim.rng` des rumeurs viennent d'un flux propre au village (graine du monde) : la
  trajectoire JS n'est pas promise au tirage près ;
- l'**ancre du regard** (`npc.target` = partenaire pendant la session) est relâchée à la fin : la
  référence repense sa cible à chaque pensée, ce portage seulement sans cible (écart n° 2).

Ce dernier point a été trouvé par l'endurance : sans lui, un habitant en `observer` dont la cible
devenait l'ancre ne repensait plus jamais — faim et soif à 100, santé 0 au jour 11.

## Parité

`Anastasis.Sim.Parite.Liens` — 14 cas, **2 834 vecteurs**, générés depuis l'extraction `fee66ae` :

```bash
node tools/migration/gen-parity.mjs tools/migration/parity/bonds.mjs -ref <extraction git archive fee66ae>
```

## Assemblage

| Test | Ce qu'il prouve |
| --- | --- |
| `Village.Liens.Conversation` | 3 échanges filtrés par la porte de parole, puis une session : les deux figés 265 ticks (4,42 s, 2 tours), activité `socialise`, relations 6 + 5 par échange, confiance 2,5 + 2,125, une rencontre par échange, cooldown de 18 s, ancre relâchée |
| `Village.Liens.Amitie` | 44 → 50 / 49 : palier « ami » franchi, moodlet `newFriend` (72 s) aux deux |
| `Village.Liens.Rumeur` | B, qui n'a jamais vu le champ, en apprend 2 gisements de A : on-dit, source A, une bouche, âge du souvenir d'origine ; un on-dit ne se recolporte pas |
| `Village.Liens.Memoire` | A, qui se souvient d'un allié à 16 tuiles, va vers lui plutôt qu'au puits (`socialSeekId`, couche `memory`) |
| `Village.Liens.Oubli` | hôte réel : au 8e tick après minuit, gisement de 15 jours et on-dit vieilli effacés, 14 jours gardé ; fiche de 27 jours effacée, 26 gardée |
| `Sim.Tick.DayAdvance` | 15 travaux restent après le tick de minuit, file vide en 9 ticks |

## Endurance

`Anastasis.Sim.Village.Endurance`, 12 jours : livraison chaque jour, grenier au plafond dès le jour 4,
faim max 57, santé min 95, social min 49 ; **149 conversations avec compagnon**, **466 gisements
appris par on-dit** (réappris après rognage ou oubli).

## Suite

- **Se soulager** (`relieve`) : l'hygiène reste le dernier besoin de base sans remède.
- Les **croyances de stock** colportées (`stockBeliefs` dans `shareRumors`) : le premier échange
  non vide hors gisements — ce qu'on se dit du grenier.
