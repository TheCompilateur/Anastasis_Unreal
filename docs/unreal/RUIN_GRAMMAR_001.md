# RUIN_GRAMMAR_001 — les vestiges d'ANÁSTASIS

Grammaire de vestiges + regroupement en sites. Suite de `ROCK_FORGE_001`, même méthode,
domaine opposé.

---

## LE CONSTAT

Le monde portait **448 exemplaires d'un seul mesh**, `SM_Ruin_Generic_01` — cinq boîtes
dont deux pans verticaux, 65 à 118 uu de haut, chacun au **yaw aléatoire**.

Mesuré, pas estimé (`tools/unreal/probe_world_inventory.py`) :

| objet | instances | hauteur en monde |
|---|---|---|
| `SM_Ruin_Generic_01` | **448** | 65 – 118 uu |
| arbres canopée | 103 | 357 – 501 uu |
| arbres émergents | 26 | 487 – 627 uu |

Un pan de mur qui arrive au genou d'un arbre, répété 448 fois sans axe commun. Ça se
lisait comme un semis de pierres tombales, et c'était exact : **trop petit pour du bâti,
trop vertical pour du bloc, et sans plan.**

---

## CE QU'EST UNE RUINE ICI

`docs/visual/reference/pontique-grammaire-architecturale-batiment.png` fait autorité, et
elle ne décrit pas l'antiquité. Le monde est post-1204, des réfugiés rhômaioi qui
reconstruisent : « *Les mêmes mains reconstruisent, avec moins, mais toujours avec sens* ».

La planche décompose une maison modeste :

- **SOUBASSEMENT** — « Pierre locale, assise sèche. Drainage, contact terrain. »
- **MURS** — « Ossature bois, clayonnage, torchis, maçonnerie mixte. »

Le bois, le torchis et la toiture disparaissent ; **la pierre reste**. Ce qui subsiste
d'une maison n'est donc pas un pan dressé, c'est son **soubassement** — une empreinte
basse, assisée, orthogonale. Et la planche liste « réemplois » parmi ses principes.

La simulation dit exactement la même chose : une tuile Ruin porte
`Resource = Stone, Amount = 8`. **Un soubassement de pierre et un tas de pierre
réutilisable.** Le mesh précédent racontait l'inverse de la fiction et de la mécanique.

---

## GÉNOME — L'INVERSE DU GÉNOME ROCHEUX

| | roche | ruine |
|---|---|---|
| symétrie | asymétrie contrôlée | **orthogonalité, angles droits** |
| arêtes | fracture (plane cut) | **assises horizontales empilées** |
| plan | aucun | **une empreinte de bâtiment** |
| orientation | yaw aléatoire | **un site = un axe** |
| jitter | 0,34 | **0,10** |
| inclinaison | 9° | **0°** — une assise a été posée de niveau |

```
plan_w / plan_d  empreinte du bâtiment disparu (100 uu = 1 m)
wall_t           épaisseur du mur — une assise sèche est épaisse
course_h         hauteur d'une assise
courses          assises encore debout
breach           part du périmètre effondrée
decay            irrégularité du haut des assises
rubble           tas de pierre de réemploi au pied
settle           tassement sous le plan de contact — « drainage, contact terrain »
lean             dévers des assises hautes. Faible : c'est du bâti
```

Le pivot suit la convention des rochers : plan de contact à `Z = -50`, `settle` passe
dessous. `ResolveInstanceTransform` remonte d'une constante `50 × Scale`, donc aucun C++
de placement n'a été touché pour obtenir le contact au sol.

### Six archétypes × 3 variantes = 18 assets

| archétype | intention | empreinte | haut |
|---|---|---|---|
| `SOUBASSEMENT` | l'empreinte du noyau d'habitation — LE vestige canonique | 4,3 × 5,6 m | ~1 m |
| `ANGLE` | deux murs qui se rencontrent ; les angles tiennent mieux que les pans | 3,9 × 4,3 m | ~1,5 m |
| `MUR` | un pan isolé, cassé aux deux bouts | 5,2 m | ~1 m |
| `FOYER` | « foyer en pierre, cœur de la maison » — l'élément intérieur le plus durable | 2 × 1,8 m | ~0,7 m |
| `ENCLOS` | « clôture, enclos » : long, très bas ; marque un territoire, pas un abri | 7,6 m | ~0,4 m |
| `REEMPLOI` | la pierre triée, en attente d'être reprise. Le seul sans plan | 2,5 × 2,1 m | ~0,15 m |

48 à 360 triangles. Nanite désactivé, même raison que la roche.

> **Corrigé d'une passe.** La première version donnait 52 cm sur une empreinte de
> 4,3 × 5,6 m : la planche montrait une trace au sol, pas une structure. Trois assises et
> murs plus épais — 0,8 à 1 m, ce qui reste exact (un soubassement de pierre sèche fait
> couramment 0,6 à 1,2 m) et c'est la hauteur à partir de laquelle la masse porte une
> ombre, donc existe.

---

## DISTRIBUTION — `AnastasisRuinDressing`

Le problème n'était pas seulement la forme. **Un bâtiment a un axe ; un yaw par pièce le
détruit.**

Le passage regroupe les tuiles Ruin en **sites** (flood-fill 8-connexe : deux tuiles qui
se touchent par un coin appartiennent au même bâtiment), puis donne à chaque site **un
seul axe** et à chaque tuile un **rôle** :

```
rang 0 (cœur)     SOUBASSEMENT si le site fait ≥ 4 tuiles, sinon ANGLE
rang 1            FOYER, si ≥ 5 tuiles — il est dedans, jamais au bord
rangs suivants    MUR, ou ENCLOS sur les grands sites
pourtour (38 %)   REEMPLOI
site d'1 tuile    REEMPLOI seul — un tas de pierre n'est pas une maison
```

**L'axe n'est pas tiré au sort** : c'est l'axe principal du nuage de tuiles. Le bâtiment
se couche donc le long de son propre vestige au lieu de le traverser. Repli sur un axe
haché seulement si la tache est trop ronde ou trop petite pour avoir une direction.

`PRESENCE = VÉRITÉ DE SIMULATION` : au plus une instance par tuile Ruin, jamais ailleurs.
Ce passage ne décide que *quelle* pièce et *dans quel sens*.

### Mesuré

```
ANASTASIS_RUIN_BIND variants=18 pools=[3 3 3 3 3 3]
ANASTASIS_RUIN sites=82 isolated=24 largest_site=31 planned=462 ungrounded=18
               soubassement=38 angle=17 mur=149 foyer=32 enclos=55 reemploi=153
```

462 tuiles éparpillées → **82 sites orientés**, dont 24 réduits à un tas.

---

## LE BUG QUI A COÛTÉ LE PLUS

Le plan produisait 462 placements et le monde n'en dessinait **aucun** — sans une seule
erreur, sans un avertissement.

```
ANASTASIS_RUIN_BIND variants=1 first_mesh=.../SM_Ruin_Generic_01
```

`create_ruin_grammar.py` fait `delete_asset` puis recrée. **Supprimer un mesh que
`DA_AnastasisPresentation` référence fait tomber la référence** : l'entrée RUIN était
revenue à son unique mesh d'origine.

> **L'ordre est contraint :** `create_ruin_grammar.py` **PUIS** `set_ruin_presentation.py`.
> Jamais l'inverse. Le même piège attend `create_rock_assets.py`.

Deux garde-fous ajoutés, parce qu'un échec silencieux est pire qu'un échec bruyant :

1. `ANASTASIS_RUIN_BIND` rapporte le nombre de variantes et la taille de chaque réserve.
   Un plan de 462 poses qui ne dessine rien ressemblait exactement à une fonctionnalité
   éteinte ; maintenant il se lit.
2. Si aucun nom ne s'associe, on retombe sur **toutes** les variantes plutôt que sur
   rien. Une ruine dessinée avec la mauvaise pièce est un bug cosmétique que quelqu'un
   signale ; 462 poses invisibles, personne ne le voit. Ce repli a effectivement sauvé le
   diagnostic.

L'association pièce→mesh se fait sur le **nom** (`PieceName`), pas sur le rang dans le
tableau : un tri ou un ajout dans l'éditeur ne peut pas faire dessiner un foyer là où un
mur va.

---

## FICHIERS

```
tools/unreal/create_ruin_grammar.py       la grammaire
tools/unreal/set_ruin_presentation.py     le câblage (idempotent, additif)
tools/unreal/capture-ruin-lineup.py       la planche de présentation
tools/unreal/probe_world_inventory.py     inventaire : un mesh, un compte
Source/.../AnastasisRuinDressing.{h,cpp}  le regroupement en sites
Content/Anastasis/Architecture/SM_Ruin_*  18 assets
```

Modifiés : `AnastasisWorldEmbodiment.cpp` (branchement, ~90 lignes dans `PlaceDressing`),
`AnastasisPresentationRegistry.cpp` (repli code), `DA_AnastasisPresentation.uasset`.

La CVar `anastasis.Ruin.SiteDressing` est déclarée près de `PlaceDressing` et non dans le
bloc en tête de fichier : ce bloc était édité sur une branche concurrente, et rien ne
justifiait d'élargir la surface de conflit par souci de rangement.

---

## LIMITATIONS

1. **Pas de capture du monde entier avec les nouveaux vestiges.** Quatre tentatives mortes
   en `editor hung` : la machine (16 Go, jusqu'à quatre éditeurs concurrents) ne peut pas
   incarner 96×96. Le code et les données sont prouvés par les compteurs et la planche ;
   la preuve en situation reste à faire quand la machine se libère.
2. **Les sites peuvent être longs.** `largest_site=31` tuiles produit un ensemble de murs
   alignés qui, en vue carte, se lit comme un long complexe rectiligne. Plausible pour un
   hameau, à surveiller si ça se met à ressembler à une muraille.
3. **La forme du site ne décide pas encore de la forme du bâtiment.** Un site de 31 tuiles
   devrait peut-être porter plusieurs bâtiments distincts plutôt qu'un seul plan étiré.
4. **Teinte non validée en situation** (0.168, 0.155, 0.140). La planche de présentation
   utilise le matériau par défaut, elle ne juge donc pas la teinte.

## NEXT

1. La capture monde, dès que la machine respire.
2. Découper les grands sites en plusieurs bâtiments plutôt qu'un plan étiré.
3. Faire dépendre l'archétype du site de la pente et de la proximité de l'eau — un enclos
   en pente douce, un foyer à l'abri.
