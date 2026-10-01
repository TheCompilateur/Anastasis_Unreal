# SOCIAL_RELAX_001 — socialiser et souffler

Sixième tranche de la croisade. Le test d'endurance de field-regrow-001 avait trouvé la limite de la
table réduite : sans `socialize`, le besoin social baisse sans remède, la solitude devient critique
au jour 3, et la référence coupe alors tout travail (`workWillFactor` = 0). Cette tranche porte les
deux remèdes : **socialiser** (la solitude) et **se détendre** (l'ennui).

Référence : `fee66ae`, extraction propre (`git archive`).

## Ce qui est porté

**Besoins** (`Life/AnastasisNeeds`) :

- `tickNeeds`, branche `socialize` : dedans, ou **dehors tant que le but est `socialize`** — le gain
  social court dès qu'on a décidé d'aller voir du monde, marche comprise (× 0,45 dehors). Ni hygiène
  ni fatigue de base dans cette branche.
- `tickNeeds`, branche `relax` : dedans ou dehors, même formule (loisir +4,2 /s, énergie +0,9 /s).
- `satisfySocial(npc, amount)`, `satisfyRelax(npc)` : dedans / dehors.

**Pression morale** : `moralPressure(...).socialMul` — moral bas → chercher compagnie (× 1,12,
× 1,08), famine et hiver → moins (× 0,92, × 0,94) ; ajouté aux lignes `socialize` et `relax`
sous la forme `(socialMul − 1) × 18`.

**Table** : `socialize` = `needs.socialize + jobPriority` ; `relax` = `needs.relax` ; puis rythme,
fin de tâche, trait, pression morale — pour **tous** les habitants.

**Cibles** :

- `socialize` → `socialPos` : le premier bâtiment achevé qui rassemble (`dailyMorale` > 0) — le
  **puits**. Le rythme (soir → taverne, midi → puits) mène au même endroit dans ce village. Les
  couches liens / mémoire / âge rendent la cible de base, faute de relations : c'est la référence.
- `relax` → but domestique : le foyer ou l'abri, sinon un abri ouvert, sinon (midi) le puits.
  Dedans 4,8 s (`relaxDuration`) ; sans toit, dehors.

**Action** : `socialize()` dans sa branche **sans compagnon** (`satisfySocial(14)`, moral +1) ;
`relax` complet.

## Écart déclaré n° 15 : pas de compagnon

La branche « avec un compagnon » de `socialize()` tire tout le module des liens : relations
(`bumpRelation`, affinité, amis, famille), délais et fatigue de conversation, paroles, **rumeurs et
gisements partagés**, rencontres, conseils, visites de voisinage. La porter à moitié serait inventer.
Ici, toute conversation est la branche ambiante — la même que la référence quand personne n'est à
5,2 tuiles. C'est **moins** de gain social que la référence quand deux habitants se croisent
(38 + 12 pour l'autre, au lieu de 14), donc une borne basse : la boucle tient quand même.

Sans puits, `socialPos` vise la place (routes) puis le site du marché : ni l'un ni l'autre n'est
porté, le repli est le point d'accès près de l'origine.

## Parité

- `Anastasis.Sim.Parite.Besoins` : + `TickNeedsSocial`, `TickNeedsRelax` (habitants × dedans/dehors ×
  pas de temps), `SatisfySocial`, `SatisfyRelax` — 142 vecteurs de plus ; les anciens sont identiques
  octet pour octet.
- `Anastasis.Sim.Parite.Recolte` : le cas `Moral` couvre désormais le moral (5 valeurs autour de 38
  et 22) et rend `effectiveWork` et `socialMul` — 2 395 vecteurs.

## Endurance, après

`Anastasis.Sim.Village.Endurance`, 12 jours (monde canonique, 2 fermiers, 3 sans-métier, grenier,
puits, maison) :

| jour | grenier | livré | repas |
| --- | --- | --- | --- |
| 1 | 68 | 68 | 0 |
| 2 | 138 | 81 | 16 |
| 3 | 225 | 95 | 11 |
| 4 | 288 | 70 | 7 |
| 5 → 12 | 296-300 (plafond) | 5 à 16 / jour | 9 à 18 / jour |

Les fermiers livrent **chaque jour** ; le grenier atteint son plafond au jour 5 et y reste (les
livraisons tombent alors au rythme des repas : il n'y a plus de place). Social minimal 48,
202 conversations, faim max 53, santé min 95.

Le jour 1 n'a pas de repas : il est entamé (on démarre le matin) et tout le monde part rassasié ;
avant cette tranche, il en comptait 12, uniquement parce qu'aucun autre besoin n'avait de remède.

## Suite

- **Les liens** : la branche compagnon de `socialize` — relations, paroles, et surtout les **rumeurs
  de gisements** : ce que les habitants se disent de ce qu'ils ont vu.
- **Se soulager** (`relieve`) : l'hygiène est le dernier besoin de base sans remède.
