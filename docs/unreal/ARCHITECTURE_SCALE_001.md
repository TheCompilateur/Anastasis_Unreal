# ARCHITECTURE_SCALE_001 — l'humain mesure le village

Mission `architecture-crusade-001` (2026-10-07), mandat d'Alexandre : « reconstruire le langage
architectural du jeu ». Ce document est la **convention d'échelle** ; toute construction du village
s'y mesure. Le générateur (`tools/unreal/create-village-architecture.py`) refuse un bâtiment qui
la viole (`validate()`), et le catalogue fonctionnel C++ (`Village/AnastasisArchitecture.*`) en
porte les mêmes nombres, contrôlés par `Anastasis.Village.Architecture.*`.

## 1. Pourquoi les maisons paraissaient petites (cause systémique)

| Fait | Source |
|---|---|
| Les meshes du village étaient modelés « à l'échelle de la tuile, 1 tuile = 400 cm » | `create-village-buildings.py:1`, `handoffs/village-buildings-001.md` |
| Une tuile de simulation fait **2000 uu = 20 m** à l'écran (`TileWorldSize` 400 × `anastasis.WorldView.Scale` 5) | `AnastasisWorldView.h:30`, `AnastasisWorldEmbodiment.cpp:34` |
| Les habitants sont à taille réelle : Manny/Quinn 180 cm × 0,93 / 0,875 → ≈ 168 / 158 cm | `AnastasisVillagerLooks.cpp:163` |
| Les bâtiments sont posés **sans échelle** | `AnastasisVillagePresentation.cpp:200` |
| L'accès d'un bâtiment est la case voisine : ~20 m du centre | `AnastasisVillage.cpp:366` |

Une maison de 3,6 × 3,1 m flottait donc au centre d'une parcelle de 20 × 20 m dont elle était censée
remplir la case. Sa porte laissait 146 cm libres au-dessus du plancher : **moins que la taille des
habitants**. La charpente faisait 45° (toit de chalet alpin), les pignons étaient ouverts en bas,
la collision était une coque convexe (porte infranchissable). Un `Scale = 5` aurait donné une porte
de 7 m : la cause n'est pas un facteur, c'est que **le bâtiment n'avait jamais été pensé à l'échelle
d'un corps**.

## 2. Unités et repères

- 1 uu = 1 cm. Le **pivot** d'un bâtiment est le centre de sa parcelle, `z = 0` = niveau de la cour.
- `+Y` local = façade d'accès (portail, porte principale). La présentation tourne l'acteur pour que
  `+Y` regarde le premier point d'accès de la simulation (la case voisine, côté village).
- Le **corps** (`SM_Arch_*`) monte de `z = 0`. L'**assise** (`SM_Arch_*_Footing`) est un mesh séparé :
  plateforme de cour en terre battue et soutènement en pierre sèche qui descend jusqu'à `z = -480`,
  pour qu'une maison sur une pente soit **terrassée**, jamais posée en l'air.

## 3. Le corps humain de référence

| Mesure | Valeur | Ce qu'elle impose |
|---|---|---|
| adulte | **170 cm** (habitants 155–172) | règle de toutes les hauteurs |
| épaules | 45 cm | passage ≥ 85 cm |
| pas | 70 cm | marche d'escalier 28–32 cm de giron, 17–22 cm de hauteur |
| charge portée (sac, jarre, fagot) | +25 cm de côté | porte d'habitation ≥ 90 cm |
| âne bâté / vache | 110–140 cm de large, 150 cm au garrot | porte d'étable ≥ 130 × 195 |
| charrette | 170 cm de voie | portail de cour ≥ 240 cm |

## 4. Règles chiffrées

| Règle | Seuil | Contrôlée par |
|---|---|---|
| **ARCH-01** porte d'habitation, hauteur libre au-dessus du seuil | **≥ 185 cm** (porte basse byzantine, mais l'adulte passe sans se courber) | `validate()`, test C++ |
| **ARCH-02** porte, largeur libre | ≥ 85 cm (90–110 habitation, ≥ 130 étable, ≥ 240 grenier / portail) | `validate()` |
| **ARCH-03** hauteur sous plafond (sol → solive ou entrait) | ≥ 225 cm | `validate()` |
| **ARCH-04** mur de pierre | 50–65 cm d'épaisseur ; ossature bois 18–24 cm | `validate()` |
| **ARCH-05** pente de toit | 22–36° (tuile canal ou planches pontiques), jamais 45° | `validate()` |
| **ARCH-06** débord de toit | 50–90 cm (pluie pontique : le mur et sa base restent secs) | `validate()` |
| **ARCH-07** fenêtre | petite : 40–80 cm de large, allège ≥ 90 cm, volets | `validate()` |
| **ARCH-08** pièce de vie | ≥ 16 m² au sol, foyer contre un mur, évacuation de fumée | `validate()` |
| **ARCH-09** emprise | tout le corps et l'assise dans la parcelle, à ≥ 100 cm de son bord (ruelle ≥ 2 m entre deux maisonnées voisines) | `validate()`, test C++ |
| **ARCH-10** contact au sol | aucun bâtiment posé sans assise ; terrassé à la médiane du terrain sous l'emprise | `AnastasisVillagePresentation`, test C++ |

## 5. Ordres de grandeur retenus

| Bâtiment | Emprise du corps | Hauteur | Repère humain |
|---|---|---|---|
| maison pauvre (un seul espace) | 6,4 × 5,2 m | faîtage 4,3 m | 2,5 humains au faîtage |
| maison moyenne (référence, deux niveaux) | 9,6 × 6,8 m + galerie | faîtage ≈ 8 m | 4,7 humains |
| ferme (maison + aile de grange + cour close) | 17 × 15 m | faîtage ≈ 8 m | |
| grenier communautaire | 11 × 8 m | faîtage ≈ 7,6 m | |
| puits de village | margelle 90 cm, aire pavée 5 m | treuil 2,4 m | |

## 6. Ce qui est encore faux et le reste

- La simulation ne fait évoluer `HousePhase` qu'à 1 (le port de la référence n'a pas encore les
  agrandissements). La présentation choisit donc la typologie d'une maison par phase **et** par une
  graine stable de son identifiant (`AnastasisArchitecture::ChooseVariant`). Le jour où la phase
  bouge, la maison s'agrandit d'elle-même ; la graine ne sert qu'à éviter les clones.
- Une tuile de 20 m reste la granularité de la simulation : deux maisonnées voisines partagent une
  ruelle de 2 m au moins. Un vrai tissu de village (maisons mitoyennes) demanderait des parcelles
  plus fines : décision d'Alexandre, pas de cette mission.
