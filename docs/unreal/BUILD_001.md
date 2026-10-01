# BUILD_001 — le chantier

Huitième tranche de la croisade. Jusqu'ici, tout bâtiment était posé **achevé** par l'hôte. Le simulateur
JS, lui, monte ses bâtiments : un chantier s'ouvre, des bâtisseurs y frappent, chaque coup pose une pièce
et consomme sa part du devis. Cette tranche porte le **chantier** : le travail sur un site ouvert, jusqu'à
l'achèvement.

Référence : `fee66ae`, extraction propre (`git archive`).

## Ce que le JS fait, et ce qui est porté

```
tryOpenNewConstruction      choisir le type, l'emplacement, payer, apporter     NON (hôte : OpenSite)
buildScore                  besoin 85 dès qu'un chantier est ouvert             OUI
constructionAccessPoint     le seuil du premier chantier                        OUI
progressBuildWork           lier, élire le site, marcher, session, coups        OUI
workConstruction            part du devis, une pièce, moral, achèvement         OUI (sans annonces ni postes)
requestSiteDeliveries       porteurs qui livrent le site                        NON (build-002)
gatherWood / gatherStone    couper, tirer, déposer au camp                      NON (build-002)
```

**Le chantier** (`Work/AnastasisBuild`, pur, prouvé bit à bit) :

- `buildCost` : devis bois / pierre du catalogue × `costMultiplier` (gratuit jusqu'à 2 du type, puis
  +1,2 % par exemplaire, plafond × 2,2). Puits 10 + 18, maison 24 + 8, grenier 26 + 16. Pas de planches
  sans scierie.
- 22 pièces (`CONSTRUCTION_BLUEPRINT` : piquets, cordeau, fondation, poteaux, lisse, cinq assises de mur,
  chevrons, sous-toiture, trois rangs de toit) ; `progress` = posées / 22, 1 à la dernière.
- `consumeSiteMaterials` : pour chaque ressource encore due, une part `max(1, ceil(reste / pièces
  restantes))`, prise au stock du site ; la pièce n'est posable que si chaque part est là.
- `buildScore` avec un chantier ouvert : `85 × trait.build × métier.build + bâtisseur 18 + priorité`.
- Le rythme des coups (`build` : 0,62 s − compétence × 0,06, plancher 0,46, fatigue de session),
  l'ancrage de 0,4 s, le changement d'outil, la compétence `craft` teintée par le trait.

**L'assemblage** (`Village/AnastasisVillage`) :

- `OpenSite(type, x, y, livré)` : un bâtiment à `progress 0`, son devis, son stock de site (plafonds
  bois 80, pierre 60) ; `CreditSiteMaterials` pour livrer après coup.
- La ligne `build` d'`adultScores`, pour **tous** les adultes dès qu'un chantier est ouvert (dans le JS,
  tout le monde bâtit ; le bâtisseur un peu plus) ; sans chantier, elle reste au plancher.
- `progressBuildWork` : le site lié tant qu'il est posable, sinon le mieux noté (`pickBuildSite`) ; à plus
  de 0,75 tuile on marche ; session `build`, coups, une pièce par coup ; à sec, on bascule vers un
  chantier prêt, sinon on lâche.
- `workConstruction` : registre des bras, pièce, moral +1, et à l'achèvement moral +4, jour et auteur.
- Le bâtiment achevé sert aussitôt : la maison loge, le puits abreuve, le grenier embauche.
- Le métier `builder` (`SetJob`), sans poste comme dans le JS.

**Unreal** : `Anastasis.Village.FirstSite [type] [bâtisseurs] [livré] [x] [y]`,
`Anastasis.Village.DeliverSite <id> <bois> <pierre>` ; le corps du bâtiment monte avec ses pièces (échelle
verticale, à la place du rendu pièce par pièce du JS) ; l'étiquette de débogage montre
`chantier n/22, bois et pierre posés / devis (stock du site), bras`.

## Écart déclaré n° 18

Dans l'en-tête d'`AnastasisVillage.h`. En bref :

- l'**ouverture** par les habitants (choix collectif, emplacement, salaire, apport) : c'est l'hôte qui ouvre ;
- les **livraisons** au chantier : un chantier à sec attend que l'hôte livre ;
- mémoire d'échec et de danger, relais de pièces, épisodes, réputation, annonces, postes ouverts,
  restitution du reliquat ;
- le raté de coup (n° 11) et le bruit de but (n° 1), comme ailleurs.

Et une adaptation à l'écart n° 2 : un travail de chantier **fini** (achèvement, ou chantier à sec)
relâche la cible. La référence repense à chaque pensée ; ce portage seulement sans cible, et le bâtisseur
resterait planté devant un chantier à sec, la faim montant.

## Parité

`Anastasis.Sim.Parite.Chantier` — 8 cas, **432 vecteurs** : devis, métiers, traits et compétence,
rythme des coups, changement d'outil, pièces, matériaux, biais de fin de tâche.

```bash
node tools/migration/gen-parity.mjs tools/migration/parity/build.mjs -ref <extraction git archive fee66ae>
```

`buildScore` et `pickBuildSite` sont internes à `npc.js` : leurs tables (métiers, traits) sont prouvées
par vecteurs, leur assemblage par `Village.Chantier.Score`.

## Assemblage

| Test | Ce qu'il prouve |
| --- | --- |
| `Village.Chantier.Ouverture` | devis de la maison 24 / 8 livré, maison inachevée ni comptée ni habitable, plafond du site, types et métiers inconnus refusés |
| `Village.Chantier.Batisseur` | un bâtisseur seul : ancrage 0,4 s, 22 pièces, devis posé entier et conservé à chaque tick, achevé à son nom le jour 1 (13,9 s de chantier), compétence `craft` 1,0000 → 1,1042, la maison loge ; puis il quitte le chantier |
| `Village.Chantier.ASec` | puits livré 3 + 3 : trois pièces, puis rien ; livré du reste : achevé, il compte |
| `Village.Chantier.Plusieurs` | grenier : deux bâtisseurs et deux sans-métier, 22 pièces au registre des bras (7, 5, 5, 5), le grenier achevé embauche son fermier |
| `Village.Chantier.Score` | sans chantier, aucune ligne ; ouvert : `145,75 × wf + rythme + compétence`, la ligne gagne, cible `site` |

L'endurance (12 jours, sans chantier) rend exactement les mêmes chiffres qu'avant.

## Suite

- **build-002** : le camp (bâtiment de départ du JS, stock bois / pierre / nourriture), couper du bois,
  tirer de la pierre, livrer au camp ; puis les porteurs qui livrent le chantier ; puis l'ouverture par
  les habitants.
