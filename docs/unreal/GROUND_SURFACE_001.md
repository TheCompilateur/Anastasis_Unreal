# GROUND_SURFACE_001 — le sol d'ANÁSTASIS

Première version production du système de sols. Le périmètre est la **surface** :
matériaux, familles de surface, réponse morphologique, transitions. Ni la génération
du monde, ni sa géométrie, ni l'eau, ni la végétation, ni la lumière n'ont été touchées.

## GROUND_OWNER_MAP — qui possède quoi

Établi avant toute écriture, parce que le terrain d'ANÁSTASIS **n'est pas un Landscape
Unreal** et qu'aucun réflexe de Landscape ne s'y applique.

| Sujet | Propriétaire | Fichier |
|---|---|---|
| altitude, type de tuile, humidité, rive | simulation | `AnastasisSim/.../AnastasisWorld.cpp` |
| instantané sémantique, emprise, repère | adaptateur | `WorldView/AnastasisWorldView.h` |
| **géométrie de la surface, couleur et canaux de sommet** | **cette mission** | `WorldView/AnastasisTerrainSurface.cpp` |
| pose du maillage, choix du matériau | embodiment | `WorldView/AnastasisWorldEmbodiment.cpp` |
| **grammaire matérielle du sol** | **cette mission** | `Content/Anastasis/Materials/M_AnastasisGround` |
| **valeurs artistiques du sol** | **cette mission** | `Content/Anastasis/Materials/MI_AnastasisGround` |
| nappe d'eau | hors mission | `M_AnastasisSlice`, inchangé |
| arbres, ruines | autre mission | `AnastasisPresentationRegistry`, `AnastasisEcologicalDressing` |

Le sol est une `UProceduralMeshComponent`, section 0, **un sommet par tuile de 1 m**,
sans UV et sans tangentes avant cette mission. Deux conséquences qui commandent tout
le reste :

- **il n'existe aucune forme sous le mètre** — tout le relief micro doit venir du
  matériau, il ne peut pas venir de la géométrie ;
- **aucune tangente n'est produite**, donc une normale en espace tangent serait
  transformée par une base dégénérée. Le matériau travaille en **espace monde**
  (`tangent_space_normal = False`).

## Pourquoi le sol semblait un prototype — diagnostic, pas impression

Trois causes, mesurées et distinctes.

**1. Les albédos étaient physiquement impossibles.** `SurfaceTypeColor` servait de Base
Color à un matériau PBR sous 75 000 lux et EV100 = 14. À cette exposition une surface
d'albédo 0.5 sort environ deux diaphragmes au-dessus du gris moyen : elle est blanche.
Les deux teintes les plus hautes de la palette étaient `ShoreSand` (0.694) et
`HighlandRock` (0.518) — et ce sont aussi les deux mélangées le plus largement, par la
rive et par l'altitude. D'où `docs/visual/terrain-extent/C_world_surface.png` : un monde
de plâtre. Les valeurs sont redescendues dans la plage des sols réels (herbe humide
0.10–0.18, litière 0.05–0.10, roche mouillée 0.12–0.20), teintes conservées.

**2. Le matériau ne lisait qu'une seule chose.** `M_AnastasisSlice` fait
`BaseColor = VertexColor.rgb`, plus deux interpolations de rugosité pilotées par
l'alpha. Pas de normale, pas de variation, rugosité constante à 0.93 sur toute la terre.
La couleur de sommet étant interpolée sur des triangles d'un mètre, le sol **ne peut
pas** porter d'information en dessous du mètre : il est lisse par construction.

**3. Rien ne répondait à la morphologie.** La pente n'était lue nulle part, alors
qu'elle est déjà dans la normale de sommet. `Wetness` était calculée par le simulateur
pour **toutes** les tuiles et n'était lue par personne.

## Ce qui a été construit

### Canaux morphologiques (C++)

`AnastasisTerrainSurface::Build` exporte désormais deux canaux UV. La couleur de sommet
ne pouvait pas les porter : elle est déjà une couleur, et un test scellé exige qu'elle en
reste une (bleue sur l'eau, jamais bleue sur la terre).

```
UV0 = (Rock, Litter)      poids de famille
UV1 = (Worked, Wetness)   poids de famille + humidité [0,1]
```

**Quatre familles, pas sept.** Les sept `ETileType` ne portent pas sept matières : le
maquis est de l'herbe sous une litière partielle, une ruine est de la pierre remaniée.
Les poids forment une **partition de l'unité** (l'herbe est le reste, `1-R-L-W`), ce qui
est le point technique : un index de type est catégoriel et n'a aucun sens une fois
interpolé — entre `Grass`(0) et `Forest`(4), le milieu vaut `Stone`. Une partition, elle,
reste une partition après interpolation linéaire. C'est ce qui donne les **transitions**
entre tuiles au lieu de frontières.

Mesuré sur le monde canonique par `Anastasis.Terrain.MorphologyChannels` :

```
vertices=9216 rock=1858 litter=716 worked=1470 grass=5172 wet_gt_half=3791 max_wetness=1.000
```

Les quatre familles existent réellement et aucune n'est résiduelle. La revendication de
quatre familles est donc **mesurée**, pas supposée.

**Ce qui n'est délibérément PAS exporté**, et pourquoi : la **pente** est déjà la normale
de sommet ; l'**altitude** et les **coordonnées de tuile** sont déjà la position monde
(un sommet est au centre de sa tuile) ; **`Shore`** est le même champ de distance à l'eau
que `Wetness` à un rayon plus court, et il est déjà peint dans la couleur de sommet.
Les exporter aurait dupliqué une vérité que le matériau tient déjà.

**`Fertility` et `ForestMargin` ont été examinés et rejetés** : le simulateur ne les
renseigne que sur les tuiles `Field` et `Forest` respectivement, et `Fertility` est un
rendement agricole, pas une apparence. Leur donner un sens visuel aurait été inventer
de l'écologie, ce que la mission interdit.

### Grammaire matérielle

`M_AnastasisGround` (maître) + `MI_AnastasisGround` (instance), tous deux produits par
`tools/unreal/ground-material.py`, qui en est la **source d'autorité** — même contrat que
`observe-slice.py` pour `M_AnastasisSlice`.

Partage des rôles :

- **le maître porte la structure** — quelles données sont lues, dans quel ordre elles se
  mélangent, à quelles fréquences la variation opère ;
- **l'instance porte le look** — 22 paramètres : teintes, amplitudes, seuils, rugosités.
  Changer la couleur d'une roche ne demande ni recompilation C++ ni ce script.

L'instance est volontairement **sans override** : tout vient du maître, il n'existe donc
qu'une valeur par défaut par paramètre, visible au même endroit que la structure qui la
consomme.

Chaîne, dans l'ordre :

```
albédo   = teinte de tuile → litière → terre travaillée → roche
           × masses macro (~60 m) × valeur meso (~11 m) × grain (~70 cm)
           → assombrissement de la bande humide
rugosité = sol → roche → humide, plus le grain
normale  = normale de sommet inclinée par le gradient borné du bruit de détail
```

Trois échelles nettement séparées, jamais superposées : une fréquence unique donne le
« papier peint procédural » que la direction artistique interdit nommément.

### Réponse morphologique réellement branchée

| Entrée | Origine | Effet |
|---|---|---|
| famille Rock | `ETileType` Stone/Ruin | couleur et rugosité de roche |
| famille Litter | Forest, partiellement Scrub | sol plus sombre et plus chaud sous couvert |
| famille Worked | Field, marginalement Ruin | terre travaillée plus claire |
| **pente** | normale de sommet | exposition rocheuse, brisée par le bruit meso |
| **humidité** | `Wetness` du simulateur | bande détrempée, plus sombre et plus lisse |
| profondeur | `PixelDepth` | atténuation du détail |

Le bruit meso casse la bande de pente : sans lui, la roche dessinerait une courbe de
niveau, ce qui se lit immédiatement comme une fonction mathématique.

## Le signet `FOREST`

Ajouté à `EnsureDefaultBookmarks` parce que la réponse « sol forestier » était le seul
morceau du système qu'aucune image ne montrait.

Il ne vise pas une tuile `Forest` : le dressing écologique pose les troncs avec un jitter
à l'intérieur de leur tuile, donc une caméra posée sur une tuile forestière se retrouve
volontiers **dans** un tronc — c'est exactement ce qui rend `GROUND` inexploitable, où un
cône vert occupe la moitié du cadre.

Il cherche donc le point le plus couvert du monde (densité de forêt sur un voisinage 5×5),
puis une tuile **non** forestière à 3-6 tuiles de là, et regarde le sol du cœur depuis
cette clairière. Plus près on est sous le couvert et un tronc masque le cadre ; plus loin
la litière n'occupe plus assez de pixels pour prouver quoi que ce soit.

Sur la graine canonique, imprimé au journal :

```
ANASTASIS_WORLD_BOOKMARK FOREST core=(74,44) density=25/25 stand=(71,47) ring=3 from_edge=1
```

Le repli est explicite : sans clairière à bonne distance, le signet se rabat sur le cœur
et le dit (`from_edge=0`). Mieux vaut un signet utilisable avec un tronc qu'un signet
absent — mais il ne ment pas sur ce qu'il a trouvé.

## L'atténuation de détail, et pourquoi elle a été nécessaire

Le point de conception le moins évident de la mission.

À `BumpStrength = 0.55` sans atténuation, la vue aérienne montrait un sol en vermicelles
clairs et sombres sur toute son étendue. Rabaisser l'amplitude à 0.14 réglait l'aérienne
et **vidait la vue au sol** : à deux mètres, le sol redevenait une surface peinte.

Le problème n'était pas l'amplitude, c'était la **distance**. Une structure de 70 cm fait
une dizaine de pixels vue de 105 m : ce qu'on appelle « détail » devient un **motif** à
cette distance, et aucune amplitude unique ne sert les deux bouts. Le détail est donc
fort de près et nul au-delà de 70 m, où la macro et la meso portent seules la lecture.

Coût : 12 instructions, une `PixelDepth`.

## Budget, mesuré

`MaterialEditingLibrary.get_statistics`, publié à chaque génération :

```
pixel_instructions=517 vertex_instructions=160 samplers=2 pixel_texture_samples=3 uv_scalars=4
```

Deux samplers pour trois bruits : `Fast Gradient - 3D Texture` coûte ~16 instructions et
1 lookup par niveau, le bruit le moins cher du moteur. Pas de virtual texture, pas de
tessellation, pas de displacement. Aucune texture de sol n'existe dans le projet — c'est
un fait d'inventaire, pas un choix : la variation est donc entièrement procédurale.

`get_statistics` lit la **shader map compilée**, pas le graphe. Juste après
`recompile_material` elle n'existe pas encore et l'appel rend `samplers=-1`. Le script
remet donc le sol en scène puis attend la map ; s'il ne l'obtient pas, il le dit et ne
publie **pas** un budget de zéro.

## Deux pièges qui ont coûté du temps, écrits pour le prochain

**`recompile_material` retourne les erreurs du compilateur.** Les ignorer produit un
asset qui s'enregistre parfaitement et que le moteur remplace silencieusement par le
Default Material au rendu. Le symptôme est un sol beige uniforme, sans eau ni sémantique
— qu'on peut prendre pour un mauvais réglage artistique au lieu d'un matériau mort. Le
script lève désormais et n'enregistre rien.

**`ComponentMask` sur l'alpha d'un `VertexColor` ne compile pas ici**, sous UE 5.8 :
`Not enough components in (DERIV_BASE_VALUE(...): float3) for component mask 0001`. Les
deux traducteurs déclarent pourtant `MCT_Float4` pour `VertexColor` ; la valeur redescend
en `float3` quelque part dans la génération analytique de dérivées. Constat, pas
explication. Conséquence retenue — et qui se trouve être le meilleur découpage de toute
façon : **le matériau de sol ne lit pas le drapeau d'eau**. La nappe est une autre section
de maillage et reçoit son propre matériau.

Les noms d'entrée exposés par Unreal ne sont pas ceux des champs C++ (`Position` s'expose
en `World Position`), et `set_editor_property` peut ne rien faire sans lever. Le script
résout les noms contre la liste réelle du nœud et **relit** chaque propriété écrite.

## Validation

| Porte | Résultat |
|---|---|
| `BUILD` | PASS — `Anastasis_UnrealV2Editor Win64 Development`, 0 warning |
| `MEC` | PASS — matériau compile, 517 instructions, 0 erreur, paramètres répondent |
| `TESTS` | PASS — 56 PASS / 4 KNOWN_EXPECTED_FAILURE / 0 FAIL |
| `SCN` | PASS — monde 96×96, `vertices=9216 triangles=18050 water_triangles=3544`, inchangé |
| `VISUEL` | PASS — trois A/B à CVar unique : aérien, rive, lisière (`docs/visual/ground-001`) |
| `PLY` | **NON ATTEINT** — voir ci-dessous |

Les 4 `KNOWN_EXPECTED_FAILURE` sont les quatre du registre, antérieures à cette mission.

`Anastasis.Terrain.MorphologyChannels` est nouveau. Il vérifie à la fois la **borne** et
la **non-vacuité** de chaque canal : un canal muet ne se voit pas comme une erreur, il se
voit comme un sol fade, ce qu'aucun test de couleur n'attrape.

**`PLY` n'est pas revendiqué.** Les captures au sol viennent d'une session PIE pilotée par
`probe-demo.ps1` à un signet fixe, pas d'un joueur qui traverse le terrain. `PLAYER` reste
`NOT_IMPLEMENTED`.

## Limites connues

1. **Aucune texture.** Le projet n'en contient aucune pour le sol ; la variation est
   procédurale. Elle tient à distance moyenne et près du sol, mais elle ne donnera jamais
   la densité d'information d'un albédo photographié. C'est la limite structurelle du
   résultat, pas un réglage.
2. **Un sommet par mètre.** Tout ce qui est sous le mètre est une normale, pas une forme :
   la silhouette du sol reste facettée en vue rasante.
3. **`HighlandRock` reste indexé sur l'altitude normalisée par l'emprise** (`Crop.MaxAlt`),
   donc un petit crop se minéralise différemment du monde entier. Défaut préexistant,
   conservé pour ne pas élargir le diff ; la roche de **pente**, elle, est indépendante
   de l'emprise.
4. **Le signet `GROUND` cadre un tronc d'arbre** et ne donne pas de vue de sol
   exploitable. Les preuves au sol utilisent `SHORE` et `FOREST`.
5. **Le grain reste visible en bande moyenne** (15–70 m) sur les faces raides, où la
   normale de détail assombrit par plaques. Atténué, pas supprimé.
