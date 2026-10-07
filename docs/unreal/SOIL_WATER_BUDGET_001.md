# SOIL_WATER_BUDGET_001 — réserve indicielle des champs

## Hypothèse bornée

Une pluie plus forte laisse davantage d'eau stockée dans un champ et peut
augmenter sa repousse au prochain passage de minuit. La météo du jeu fournit
une intensité normalisée de pluie, pas des précipitations mesurées. La réserve,
ses flux et ses coefficients sont donc **sans unité physique**. Ce modèle ne
reconstitue ni un sol réel, ni une nappe, ni le bassin versant entier.

## Règle

Chaque champ commence à son `tile.wetness` généré. Par jour :

`réserve avant + pluie = réserve après + évaporation + drainage + débordement`.

La pluie lue est celle du jour précédent (`weatherAt(seed, day-1)`), ou la météo
forcée d'un test. L'évaporation dépend seulement de la saison. Le drainage
agit au-dessus de 0,70 de réserve. La réserve reste dans [0,1]. Un facteur
de croissance [0,70 ; 1,10], maximal près de 0,55, multiplie la fertilité
**seulement pour la repousse** ; la fertilité générée n'est pas écrasée.
Un champ ouvert comme source finie `food-supply` reste exclu de la repousse.

`anastasis.Village.SoilWaterBudget=0` par défaut : le chemin JS de référence
reste inchangé. Le passage à 1 réinitialise les réserves et agit aux minuits
suivants. Pour comparer A/B, réinitialiser le monde entre bras : couper le
commutateur ne retire pas de nourriture déjà produite. L'état de réserve se
lit par `GetSoilWaterAt` après le premier passage de minuit.

## KEEP / REJECT

- KEEP local : mêmes bras désactivés ; bilan eau fermé et borné ; pluie forte
  augmente la réserve et la repousse d'un champ éligible ; répétition exacte.
- REJECT local : conservation brisée, non-déterminisme, changement du bras
  désactivé ou absence de contraste sur le champ témoin.

## Frontière

La preuve locale n'est ni une calibration pédologique ni une preuve que les
habitants mangent davantage. Le rendu de la carte ne lit pas encore la réserve.
L'état expérimental n'est pas encore sérialisé dans la sauvegarde JS ; ne pas
reprendre une partie sauvegardée avec ce commutateur pour revendiquer la même
trajectoire. L'effet sur la faim reste inconnu, surtout après le résultat
contre-intuitif du test fertilité → repas (+6 repas et +618 personnes-ticks de
faim critique dans une fixture de 12 jours).

## Résultat local

`Anastasis.Sim.SoilWater` : 2 PASS, 0 FAIL dans le worktree. Sur 20 pas
contrôlés, réserve sèche 0, réserve arrosée 1, chaque pas au bilan fermé.
Sur un champ éligible au printemps (jour 5), bras désactivés 5/5 unités ;
bras activés : sec 4 (réserve 0,158), pluie forte 5 (réserve 0,458).
La répétition donne le même digest. Ces valeurs prouvent ce champ témoin,
pas une distribution de rendements sur la carte ni un effet alimentaire.
