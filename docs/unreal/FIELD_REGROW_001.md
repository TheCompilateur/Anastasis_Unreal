# FIELD_REGROW_001 — la repousse des champs, et le village sur plusieurs jours

Cinquième tranche de la croisade. Après le puits, la maison, le grenier et le fermier, deux choses :
la **repousse** qui empêche les champs de mourir, et un **test d'endurance** qui fait vivre les
quatre tranches ensemble plusieurs jours sur le monde canonique.

Référence : `C:\dev\Jeux IV Kingdoms`, commit **`fee66ae`**, lue dans une extraction propre
(`git archive`) : la copie de travail porte des modifications non commitées d'un autre agent.

## La repousse (`Simulation.regrowFieldsDaily`)

En tête des travaux différés de minuit (`landRegen`), pour chaque tuile `field` sous le plafond de 37 :

```
tirage = hash(x, y, jour)            (produit en DOUBLE, pas Math.imul — voir plus bas)
si tirage <= chance de la saison      (0,45 printemps, 0,48 été, 0,35 automne, 0,22 hiver)
  gain = max(1, round(3 × regen de saison × (fertilité || 1)))     (regen 1,6 / 1,85 / 1,1 / 0,55)
  stock = min(37, stock + gain)
  jachère (ou stock nul) -> culture tirée : hash(x, y, 91 + jour % 97) -> fruit / légumes / grain
```

`regrowForestDaily`, l'autre moitié de `landRegen`, est **sans effet** dans la référence :
`regrowWoodTile` rend toujours `false`. Rien à porter.

Le village l'applique à l'état vivant des tuiles (`LiveTileAt`), le monde généré reste immuable.
Une source ouverte par l'extension food-supply n'est jamais regarnie (elle se déclare « sans repousse »).
L'hôte l'exécute au tick du changement de jour, comme la file de la référence (2 jobs par tick,
`landRegen` en tête) ; hors différé, tout de suite.

### Le hash : double, pas imul

`fieldCrops.js` et `regrowFieldsDaily` font `h = (h * 1274126177) >>> 0` : un produit en **double**,
qui dépasse 2^53 et arrondit ses bits bas, puis ToUint32. Ce n'est pas `Math.imul`. Sur la carte
canonique, les deux variantes diffèrent sur 9 134 tuiles sur 9 216. Le portage reproduit le produit
en double (`AnastasisFields::RegrowRoll`, `FieldCropHash`). Le tirage de la repousse n'a d'ailleurs
**pas** le mélange final `h ^ (h >>> 16)` du `hash2d` de cultures : ce sont deux fonctions.

Le `PickFieldCropId` de la génération du monde (déjà porté) utilise `Imul` : il ne coïncide avec la
référence que parce que les seuils de culture sont rarement franchis par l'écart des bits bas. Signalé
à part (tâche « Corriger le hash de PickFieldCropId »), hors de cette mission.

## Parité (`Anastasis.Sim.Parite.Repousse`, 686 vecteurs, 0 écart)

`tools/migration/parity/regrow.mjs` appelle `Simulation.prototype.regrowFieldsDaily` de la référence
sur un `this` minimal (le jour, une tuile, `indexResource`) : 8 positions × 14 jours (chaque saison,
les frontières, le modulo 97) × 6 états (jachère vide, peu fertile, en culture, près du plafond, au
plafond, jachère avec stock) = 672, plus 14 vecteurs de quantité et de chance par jour.

## Tests d'assemblage

- `Repousse.Jachere` : une parcelle vidée par le fermier repart au premier jour où le tirage passe,
  avec la culture tirée ; le monde généré n'a pas bougé.
- `Repousse.Plafond` : 40 champs, 200 jours : jamais plus de 37, tous au plafond à la fin ; la source
  food-supply n'a jamais repoussé.
- `Repousse.Hote` : trois minuits, la file vidée au tick même, portions créées = gain des champs,
  deux hôtes de même graine identiques.

## Endurance (`Anastasis.Sim.Village.Endurance`)

Monde canonique (graine 12345), le champ généré le plus proche du centre, un grenier à 3-4 cases, un
puits, une maison ; 2 fermiers au grenier, 3 sans-métier ; 8 jours simulés (1,5 s de calcul).
À chaque tick : champs (état vivant) + sacs + stocks + repas − repousse = constante, personne sur une
case bloquée.

Ce que le village fait, jour par jour (run du 2026-09-30) :

| jour | grenier | repas | livré | note |
| --- | --- | --- | --- | --- |
| 1 | 40 | 12 | 48 | |
| 2 | 142 | 6 | 108 | |
| 3 | 212 | 10 | 80 | solitude critique chez les fermiers |
| 4-8 | 204 → 175 | 3 à 11 / jour | 0 | les fermiers ne travaillent plus |

Faim max 48, soif max 60, santé min 95 : **personne n'a faim** — le grenier rempli les trois premiers
jours nourrit le village jusqu'au bout.

### La limite que l'endurance a trouvée : la solitude

Le besoin social baisse sans remède (`socialize` n'est pas porté). À 35, la solitude devient
**critique** ; `workWillFactor` rend alors 0 — c'est la référence : un habitant en crise ne travaille
pas, aucun but de travail ne gagne par son score propre. Du jour 3 au jour 8, les fermiers dorment,
boivent, mangent, mais ne cueillent plus.

Le test l'exige explicitement : **tout jour sans livraison doit être un jour où un fermier avait un
besoin critique** (`UnexplainedIdleDays == 0`). L'arrêt est expliqué, jamais silencieux. Le jour où
`socialize` sera porté, les jours sans livraison doivent tomber à zéro, et ce test le montrera.

## Suite

`socialize` (et `relax`, qui soigne l'ennui de la même façon) est la tranche suivante qui débloque le
travail dans la durée. Sans elle, toute boucle de travail meurt au troisième jour.
