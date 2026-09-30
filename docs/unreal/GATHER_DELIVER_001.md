# GATHER_DELIVER_001 — cueillir puis livrer au grenier

Quatrième tranche verticale de la croisade « brancher le simulateur aux bâtiments ». Après le puits
(boire), la maison (dormir) et le grenier (manger par Noûs), la boucle qui **remplit** le grenier :
un habitant va aux champs, cueille, rentre, et le stock monte.

Référence : `C:\dev\Jeux IV Kingdoms`, commit **`fee66ae`** (voir « Référence » plus bas).

## Qui livre au grenier, dans la référence

La carte du JS a tranché avant le premier octet de C++ :

- un **sans-métier** (`settler`) qui cueille ne livre jamais au grenier. Son sac part à
  `deliver` → `sim.deliveryPos` (marché, entrepôt, camp) ou à `sell` (or, trésor) : rien de cela
  n'est porté ;
- le seul chemin « champ → grenier » est un **métier dont le poste est un dépôt de nourriture**.
  Le catalogue admet au grenier `steward`, `farmer`, `porter`. `depotResourceOf(granary) = "food"`,
  et `beginHaulToDepot` renvoie le sac au poste.

La tranche porte donc **le fermier au grenier** : `AssignWorkplace(npc, "farmer", granary)`.

## La boucle

```
perceive (7 tuiles, balayage si déplacé de 1,5)      mind.spots["x,y"] = { amount, day }
table adultScores : gatherFood calculée                  cible = recallResource (gisement connu)
A* + marche ; à l'arrivée progressCraftGather :
  resourceTileNear (3 x 3) ; ensureCraftSession : 1er coup à arrivée + 0,36 s
  fieldWorkTarget : un des 8 postes de la parcelle, jamais celui d'un autre
  chaque coup : yieldPerSwing (2, 3 dès la compétence 1,35) × saison (fieldSeasonGatherAmount)
               tile.amount -= pris ; inventory.food += pris ; gainSkill 0,008
               prochain coup : swingPeriodFor (0,52-0,72 s, fatigue de session)
  tuile vide : depleteTile -> champ en jachère
  sac > 9 : beginHaulToDepot -> but deliver, cible : un seuil du grenier
au seuil, DEHORS (dépôt de son poste, DEPOT_OUTSIDE) ; 1 s ; deliver() :
  min(place, sac, 12) -> building.stock.food.physical ; moral +1 ; gainSkill 0,002
```

La ligne `gatherFood` d'un fermier :
`(resourceScore(food) + faim × 0,15) × survivalWorkFactor + phaseBias + 22 (poste) + completionBias + traitGoalBias + skillGoalBias`,
avec `workFactor = moralPressure.effectiveWork × phase.work × 1 (adulte) × natureWorkFactor`.
Le test `Recolte.Selection` la recompose à la main depuis les fonctions prouvées : mêmes bits.

## Parité (`Anastasis.Sim.Parite.Recolte`, 2011 vecteurs, 0 écart)

Déclaration : `tools/migration/parity/gather.mjs`. L'atelier accepte désormais plusieurs modules
de la référence (`modules: { nom: chemin }`). Les habitants y sont créés par `createNpc` avec des
options **explicites** — métier, trait, nature moyenne, compétence : ce que le portage fournit au
lieu des tirages. Les huit besoins sont fixés, rien ne vient des valeurs de départ.

| Cas | Fonction JS | Vecteurs |
| --- | --- | --- |
| WorkScores | `believedStock(food)`, `beliefMarketWorkScores` (food, deliver) | 480 |
| Completion | `completionBias` | 192 |
| TraitBias | `traitGoalBias` | 54 |
| SkillTint | teinte des compétences par `createNpc` | 6 |
| WorkWill | `workWillFactor` | 960 |
| Moral | `moralPressure(...).effectiveWork` | 96 |
| Swing | `swingPeriodFor`, `yieldPerSwing` (farm) | 120 |
| Season | `fieldSeasonGatherAmount` | 22 |
| FieldPost | `fieldWorkTarget`, `preferredFieldPostIndex` | 45 |
| Learn | `gainDomainSkill`, `skillGoalBias` | 36 |

Les vecteurs ont été régénérés depuis une extraction propre de `fee66ae` (`git archive`) : identiques
octet pour octet à ceux de la copie de travail.

## Référence : le commit, pas la copie de travail

La copie de travail de `C:\dev\Jeux IV Kingdoms` porte des modifications **non commitées** d'un autre
agent, précisément dans cette boucle (`src/sim/npc.js`) :

- `shouldHaulGatherLoad` : `load > 9` → `load > 11` ;
- lot générique de `deliver()` : 3 → 8 (sans effet ici : le fermier livre à son dépôt, lot 12).

Ce portage suit le commit : **`load > 9`** (`AnastasisGather::HaulLoadAbove`). Si la modification est
commitée, changer la constante et régénérer — rien d'autre.

## Écarts déclarés (en-tête de `AnastasisVillage.h`, n° 10 à 14)

10. Métiers `settler` et `farmer` seulement, embauche par `AssignWorkplace` (pas de marché de l'emploi).
    `gatherFood` / `deliver` ne sont calculées que pour le fermier d'un grenier. Habitant sans
    ambition, plan ni technique, nature moyenne, trait « gardien » : options légales de `createNpc`.
11. Pas de raté de coup (`rollCraftMiss` tire `sim.rng`), pas d'exploration sans gisement connu
    (`exploreTarget` tire `sim.rng`), pas de rumeurs, pas de danger, pas de repousse.
    **Le monde généré reste immuable** : la référence écrit dans `sim.tiles`, le village tient
    l'état vivant des tuiles touchées (`LiveTileAt`) — mêmes lectures, mêmes valeurs.
12. Livraison à son dépôt seulement ; le stock de marché (lu par la pression morale) est la somme
    des stocks physiques, recalculée à la lecture.
13. Référence `fee66ae` commitée (ci-dessus).
14. Cohabitation avec l'extension food-supply-001.

## Cohabitation avec food-supply-001 (Codex)

`main` a reçu pendant cette mission `ec5322f` : un circuit « source → sac → grenier → repas »
**fait à la main**, qui se déclare lui-même « PAS un portage de trajectoire JS » (un coup de 2 en 3 s,
scores fixes 85 et 100 + 5 × sac, pas de métier ni de session). Il ne s'active que si une source est
ouverte par `ActivateFoodSource` (commande `Anastasis.Village.FoodSupply`).

Les deux circuits vivent côte à côte, sans se marcher dessus :

- le fermier d'un grenier suit la référence ; tout autre habitant garde l'extension ;
- une tuile ouverte par l'extension n'a qu'une vérité, son registre fini : `LiveTileAt` le lit, et
  une récolte du fermier le débite ;
- les compteurs sont partagés (`GatheredFood`, `DeliveredFood`).

**Décision à prendre par Alexandre** : garder l'extension comme banc de conservation, ou la retirer
maintenant qu'un chemin fidèle existe. Deux boucles pour le même geste, c'est une de trop à terme.

## Ce que la référence fait, et que les tests montrent

- Arrivé à 0,75 tuile de son gisement, le fermier prend la **première** tuile de nourriture du
  3 × 3 autour de lui (`resourceTileNear`, lignes du haut d'abord) : il ne récolte pas
  forcément la tuile qu'il visait.
- Un champ vidé sous ses yeux reste en mémoire jusqu'au prochain balayage, qui exige d'avoir
  bougé de 1,5 tuile. Sans rien à cueillir à côté, trois échecs le renvoient à la table ;
  sac chargé, `deliver` l'emporte.
- Grenier plein : le lot est borné par la place, le reste **reste au sac**, et `deliver` échoue ensuite.
- Faim : il mange ce qu'il porte (Noûs, source `inventory`) avant de penser au grenier.

## Preuves

- Tests : `Anastasis.Sim.Parite.Recolte` ; `Anastasis.Sim.Village.Recolte.Perception`, `.Selection`,
  `.Cueillette`, `.Livraison`, `.Epuisement`, `.MultiAgents`, `.Destruction`, `.Plein`, `.Hote`.
  Conservation vérifiée **à chaque tick** : champs (état vivant) + sacs + stocks + repas = constante.
- PIE : `tools/unreal/gather-deliver-pie.ps1` — voir la fiche de passation.
