# TREE_DESIGN_REPORT — ANASTASIS_UNREAL_TREE_FORM_001

Mission : transformer les arbres de la scène — de cônes de proxy en silhouettes
conçues, avec hauteur, stature, masse et tronc. Périmètre : présentation uniquement.
Ni terrain, ni hydrologie, ni simulation, ni redistribution écologique.

Worktree `anastasis-tree-visuals-136553`, branche `claude/anastasis-tree-visuals-136553`,
base `main` à `1cbeef6`. `main` n'a pas bougé pendant la mission (vérifié au début, entre les
deux passes, et à la fin).

Deux passes :

- **Passe 1 — stature.** Six silhouettes construites, quatre classes d'âge. Commit `8060b2a`.
- **Passe 2 — espèce.** Le mélange conifère / feuillu cesse d'être un tirage aveugle et
  devient une lecture du site (`Shade`, `Wetness`). Huit silhouettes, quatre statures × deux
  familles. C'était le NEXT TARGET de la passe 1.
- **Passe 2.5 — ombrage.** La géométrie faisait son travail, le rendu non. Feuillage deux
  faces + normales fractionnées. Aucun sommet déplacé, aucune ligne de C++ touchée.
- **Passe 2.6 — deux matériaux.** Le bois cesse d'être déclaré comme du feuillage. Correction
  de structure, **pas** d'apparence : mesurée, elle ne change presque rien à l'image, et le
  diagnostic qui l'avait motivée était faux.

---

## 1. État initial découvert

Le pipeline réel, lu avant toute mutation :

```
AnastasisSim::GenerateWorld(seed, 96, 96)            vérité de simulation
  -> AnastasisWorldView::CaptureCanonicalWorld        adaptateur, aucune donnée nouvelle
  -> AnastasisEcologicalDressing::Build               OÙ pousse un arbre + sa strate
  -> AnastasisPresentationResolver                    À QUOI il ressemble + sa transform
  -> HISM par (archétype, variante)                   instanciation
  -> SM_Tree_Generic_01                               un seul mesh
```

Les arbres n'étaient pas des `StaticMeshActor` : ce sont des instances
`UHierarchicalInstancedStaticMeshComponent`, un composant par variante résolue. Le
placement était déjà déterministe, déjà écologique (lisière, clairières, pente,
humidité), et déjà stratifié en trois couches `Young / Secondary / Canopy`.

**Le défaut n'était donc pas le placement. C'était l'incarnation.** Les trois strates
étaient le MÊME mesh à trois échelles uniformes — exactement ce que la loi de mission
interdit. Et ce mesh unique, produit par `tools/unreal/create_tree_asset.py`, était un
cylindre de tronc (rayon 9) surmonté d'un cône (rayon de base 34) : vue de dessus à 32°,
la jupe du cône mangeait intégralement le tronc. La lecture finale était `GREEN_CONE`.

### Mesure, pas impression

Sonde de lecture seule sur l'incarnation réelle (seed 12345, monde 96×96, mode surface) :

| | avant |
|---|---|
| instances d'arbres | 438 |
| instances de ruines | 448 |
| hauteur d'arbre min / médiane / max | **49 / 104 / 237 uu** |
| hauteur de ruine min / médiane / max | 65 / 91 / 118 uu |
| silhouettes distinctes | **1** |

Une tuile de simulation vaut 100 uu. **L'arbre médian mesurait donc 1,04 m** — la hauteur
d'un mètre de terrain, et à peine plus que le moignon de ruine posé à côté. Ce n'était
pas une forêt : c'était un champ de marqueurs.

## 2. Fichiers / assets dont je prends la propriété

| Fichier | Rôle | Action |
|---|---|---|
| `tools/unreal/create_tree_asset.py` | autorité des meshes d'arbres | **réécrit** — produit désormais 8 silhouettes + 1 alias + le matériau |
| `tools/unreal/set_tree_grammar.py` | autorité du câblage FOREST | **créé** |
| `tools/unreal/capture-tree-lineup.{py,ps1}` | planche de stature (test anti-arnaque) | **créés** |
| `Content/Anastasis/Vegetation/SM_Tree_*.uasset` | 8 meshes + alias | **créés / régénéré** |
| `Content/Anastasis/Materials/M_AnastasisVegetation.uasset` | lecture des couleurs de sommet | **créé** |
| `Source/.../AnastasisPresentationRegistry.{h,cpp}` | données de présentation | axes de stature et de famille + biais + inclinaison |
| `Source/.../AnastasisPresentationResolver.{h,cpp}` | choix de variante et transform | sélection par stature **et famille**, fail-open en deux temps, gradient d'espèce |
| `Source/.../AnastasisWorldEmbodiment.cpp` | pose des instances | strate → stature, site → famille, journaux de profil et d'espèce |
| `Source/.../AnastasisPresentationResolverTests.cpp` | tests | 5 tests ajoutés |

**Fichiers partagés touchés, et pourquoi** (diff concurrent inspecté avant) :

- `tools/unreal/set_presentation_meshes.py` — la ligne `FOREST` en est **retirée**. Cet
  outil ne sait écrire que `variants[0].mesh` ; le relancer sur Forest écraserait
  silencieusement la variante *understory* par le mesh générique et casserait la
  grammaire. Ruin, qui appartient à un autre chantier, est laissé intact.
- `tools/unreal/presentation-registry.py` — la graine `FOREST` pointait encore sur
  `/Engine/BasicShapes/Cone.Cone`. Un registre recréé de zéro repartait donc sur le cône
  même que cette mission retire. Corrigé pour Forest seulement.
- `tools/unreal/observe-slice.py` + `capture-slice.ps1` — **ajout additif** de deux
  variables d'environnement de caméra (`ANASTASIS_SLICE_CAM_LOC` / `_ROT`). Absentes,
  rien ne change et les captures scellées de WORLD_SLICE_006 restent reproductibles
  telles quelles. Elles existent parce qu'une preuve de silhouette se juge de près : à
  la distance de la caméra monde, un arbre fait dix pixels.

**Aucun fichier de `Source/AnastasisSim/` n'est touché. `AnastasisEcologicalDressing`
n'est pas touché** — la distribution écologique est le travail d'un autre agent
(`bb883e6`) et reste exactement ce qu'elle était.

## 3. Références utilisées

Autorité principale : `docs/visual/reference/pontique-etat-zero-3-stratification-forestiere.png`.
Sa coupe écologique nomme les strates, et c'est d'elle que vient le vocabulaire du code :

```
STRATE ÉMERGENTE   grands arbres, arbres anciens
CANOPÉE            feuillus matures / conifères dominants (sapins, épicéas, pins)
SOUS-CANOPÉE       jeunes arbres, arbres intermédiaires (érables, charmes)
STRATE ARBUSTIVE   jeunes semis
```

Sa checklist PCG demande explicitement « variation d'âge des arbres (jeunes, matures,
anciens, sénescents) » et « hiérarchie de densité et de strates ».

Complément : `docs/visual/P1_6_PONTIC_BYZANTINE_ART_DIRECTION.md` — « la forêt n'est pas
un tampon d'arbre répété », « lire l'échelle par la hauteur des arbres », palette retenue,
pas de compensation par bloom ou fog.

## 4. Échelle du monde constatée

```
1 tuile              = 100 uu  (gelé P1.5, "Lot 4.5 ne choisit pas d'échelle joueur")
monde canonique      = 96 x 96 tuiles = 9600 uu ≈ 96 m
niveau de la mer     = 275 uu
ruine (mesurée)      = 65–118 uu, médiane 91
adulte (repère)      = 180 uu
arbre AVANT (mesuré) = 49–237 uu, médiane 104
```

### La tension, énoncée plutôt que masquée

Si l'on prend `1 tuile = 1 m` au pied de la lettre, un conifère pontique mature mesure
10 à 25 m, soit **10 à 25 tuiles**. Sa couronne couvrirait alors 4 à 6 tuiles, alors que
l'espacement minimal du dressing écologique est de 0,55 tuile. Atteindre la hauteur
littérale exigerait de diviser la densité forestière par un ordre de grandeur — c'est-à-dire
**redessiner la distribution écologique, ce que cette mission s'interdit explicitement**.

Choix retenu, et c'en est un : porter la stature au maximum que la distribution scellée
peut accueillir sans perdre la lisibilité macro, et **traiter les RAPPORTS entre classes
comme la vraie livraison**. La hauteur absolue littérale est documentée en NEXT TARGET,
pas bricolée ici.

## 5. Familles créées

Huit silhouettes construites, une par case de la grille (strate × conifère/feuillu) que la
planche de référence autorise. Chaque mesh est normalisé sur Z = [−50, +50] : **seules les
proportions INTERNES changent d'une case à l'autre**, jamais une simple mise à l'échelle.

| Mesh | Strate | Famille | Largeur/hauteur | Signature |
|---|---|---|---|---|
| `SM_Tree_Conifer_Understory_01` | arbustive | conifère | **0,27** | flèche fine, feuillue jusqu'en bas |
| `SM_Tree_Broadleaf_Understory_01` | arbustive | feuillu | **0,54** | cépée de noisetier, **aucun fût** — quatre tiges depuis le sol |
| `SM_Tree_Conifer_Subcanopy_01` | sous-canopée | conifère | **0,35** | 3 étages, tronc lisible |
| `SM_Tree_Broadleaf_Subcanopy_01` | sous-canopée | feuillu | **0,47** | fourche + petit dôme asymétrique |
| `SM_Tree_Conifer_Canopy_01` | canopée | conifère | **0,39** | épicéa d'Orient, 4 étages à décrochement |
| `SM_Tree_Broadleaf_Canopy_01` | canopée | feuillu | **0,57** | hêtre, 3 branches maîtresses, large dôme |
| `SM_Tree_Conifer_Emergent_01` | émergente | conifère | **0,44** | fût massif, cime **émoussée** (sénescence) |
| `SM_Tree_Broadleaf_Emergent_01` | émergente | feuillu | **0,60** | vieux hêtre, ramure redressée, repère du couvert |

L'arbuste feuillu n'a délibérément **pas de tronc** : un noisetier part en cépée. Lui coller
un fût unique en aurait fait un petit arbre, c'est-à-dire la confusion de strate exacte que
la planche de référence sépare (strate arbustive vs sous-canopée).

`SM_Tree_Generic_01` survit comme alias géométrique de la silhouette de canopée : c'est le
chemin que code en dur le repli de `AnastasisPresentationRegistry.cpp`, et il devait rester
valide sans devenir un septième dessin.

Deux règles de dessin portent tout le reste :

- **Décrochement d'étage.** Chaque tronçon de couronne *repart plus large* que ne finit
  celui du dessous. C'est ce décrochement, pas le chevauchement, qui fait une jupe visible
  à moyenne distance. La première itération chevauchait sans décrocher : les étages se
  fondaient et la silhouette redevenait un cône. Corrigé et re-capturé.
- **Six à sept lobes pour un feuillu.** Trois gros lobes se lisent comme trois boules
  empilées — un brocoli, pas un hêtre. Au-delà de cinq, les masses fusionnent et seul le
  bord reste irrégulier, ce qui est exactement l'effet cherché.

Couleur : chaque sous-partie porte une couleur de sommet (écorce / aiguilles / feuillage)
et `M_AnastasisVegetation` la lit — même langage que `M_AnastasisSlice` pour le sol, la
couleur EST la sémantique. Sans ce matériau, la teinte unique de l'archétype peindrait
aussi le tronc en vert et le tronc cesserait d'exister. L'écorce a été assombrie après
lecture de la planche anti-arnaque : sous le soleil neutre du banc, un linéaire de 0,13
remonte à ~0,40 en sRGB, c'est-à-dire du beige.

## 6. Hauteurs retenues — mesurées dans le monde

`hauteur = 100 uu × enveloppe d'entrée × facteur de strate × biais de variante`

Enveloppe portée de `1.6–2.4` à `3.6–5.0`. Les facteurs de strate appartiennent à
`FAnastasisForestDressingSettings` et **ne sont pas touchés**.

| | avant | après |
|---|---|---|
| hauteur min | 49 uu (0,5 m) | **97 uu (0,97 m)** |
| hauteur max | 237 uu (2,4 m) | **627 uu (6,3 m)** |
| amplitude | ×4,8 | **×6,5** |
| silhouettes | 1 | **8** |
| classes de stature | 3 (échelles du même mesh) | **4 (meshes distincts)** |
| familles d'espèce | 0 | **2, choisies par le site** |
| instances | 886 | **886 — inchangé** |

Profil journalisé à l'incarnation, mesuré et non affirmé :

```
ANASTASIS_TREE_STATURE understory=180 subcanopy=129 canopy=103 emergent=26 height_uu=[97,627]
ANASTASIS_TREE_SPECIES conifer=225 broadleaf=213 shade=[-0.74 -0.04 0.79]
                       wetness=[0.00 0.08 0.85] p_conifer=[0.06 0.57 0.86]
ANASTASIS_ECOLOGY      young=180 secondary=129 canopy=129 full_plan=438   (identique à avant)
```

La forme de la distribution est celle qu'une forêt réelle a : beaucoup de semis, une
majorité d'arbres moyens, un peuplement dominant, et **26 émergents seulement** qui donnent
sa ligne de ciel au couvert.

## 7. Méthode de variation

Trois axes, tous déterministes, tous pilotables par la donnée :

1. **Stature → mesh.** La strate écologique décidée par `AnastasisEcologicalDressing`
   devient une demande de stature. Le résolveur ne sert que les variantes construites pour
   elle. La quatrième strate — les émergents — n'est pas une décision écologique nouvelle :
   c'est la queue la plus âgée de la canopée, séparée **en présentation** par le même hash
   déterministe, à hauteur de 18 %. Aucun arbre ne se déplace.
2. **Biais d'échelle par variante.** Un émergent est plus haut qu'un arbre de canopée de
   la même espèce (×1,35) ; un feuillu est plus bas qu'une flèche de conifère (×0,85–0,92).
   Un seul archétype couvre ainsi plus de hauteur qu'une paire Min/Max ne sait exprimer.
3. **Inclinaison.** Enveloppe de 5° par entrée, tirée puis **élevée au carré** : la plupart
   des instances restent d'aplomb et quelques-unes penchent franchement. Un tirage uniforme
   donnerait à tout un peuplement la même ivresse moyenne — du bruit, pas des individus.
   `MaxLeanDegrees = 0` (la valeur par défaut, celle de Ruin) reproduit la transform
   historique **exactement** : un test le vérifie à 1e-6.
4. **Espèce → mesh (passe 2).** Voir §7bis. Orthogonale à la stature : un peuplement peut
   changer d'espèce sans changer de structure d'âge, et l'inverse.

Le repli est en deux temps, et l'ordre est un choix : **la famille tombe avant la stature**.
Perdre l'espèce est un moindre mensonge que perdre l'âge — une hêtraie dessinée en épicéas
reste une forêt de la bonne forme, tandis qu'un semis dessiné en dominant casse tout le
profil vertical du peuplement. Un test le verrouille.

## 7bis. L'axe d'espèce — lire le site, pas tirer au sort

La planche de référence demande un « mélange d'espèces selon l'humidité, l'altitude et
l'exposition ». Ces trois mots tiennent dans **deux champs que la simulation produit déjà** :

```
Shade    [-1,1]  eclairement du relief : pente face a la lumiere, melangee a l'altitude.
                 C'est DEJA la composition de "altitude et exposition" -- lire Alt comme
                 troisieme terme compterait l'altitude deux fois.
Wetness  [0,1]   humidite.
```

```
Coniferousness = Base + 0,37 · Shade − 0,34 · (Wetness − 0,08)
```

Les conifères (épicéa d'Orient, sapin) prennent le haut, l'exposé, le sec ; les feuillus
(hêtre, charme, aulne) tiennent le bas, l'humide, l'abrité. Le résultat est une
**probabilité** résolue contre un hash par site, jamais un seuil : une coupure franche
dessinerait une ligne de niveau visible en travers de la carte — un artefact de rendu, pas
un écotone.

### Les constantes viennent de la mesure, et la première tentative était fausse

Mesuré sur les 438 sites forestiers réellement habillés :

```
Shade    min -0,74   médiane -0,04   max 0,79
Wetness  min  0,00   médiane  0,08   max 0,85
```

**La médiane d'humidité vaut 0,08, pas 0,5.** Un champ dans [0,1] invite à supposer un
pivot au milieu : un premier jet pivoté à 0,55 ajoutait donc un bonus quasi constant à
presque tous les sites et rendait **366 conifères contre 72 feuillus**, avec `p_conifer`
saturé **aux deux bouts** — exactement le « seuil déguisé en gradient » que le commentaire
du code interdisait deux lignes plus haut. Pivot ramené à la médiane mesurée.

Le diagnostic lui-même était faux une seconde fois : il rapportait
`Coniferousness(pire ombre, pire humidité)`, une combinaison qui peut n'exister **nulle
part** sur la carte, et affichait donc un clamp qu'aucun arbre ne rencontrait. Il rapporte
maintenant la distribution des sites réels. Un diagnostic qui lève une fausse alerte est
pire que pas de diagnostic.

Résultat final, sur les sites réels : `p_conifer = [0,06 ; 0,57 ; 0,86]` — **aucun site ne
touche un mur**, et le mélange sort à 225 conifères / 213 feuillus. Une forêt colchique est
effectivement mixte ; ce qui compte est que la proportion **bascule avec le terrain**, et
elle le fait de 6 % à 86 %.

Tout est reproductible : même seed, même monde ⇒ même arbre au même endroit, à chaque
lancement.

## 7ter. Passe 2.5 — l'ombrage, sans toucher un sommet

Constat avant : **les couronnes étaient des solides opaques à ombrage lissé**, éclairés par
une seule constante de rugosité. Une masse de feuillage qui ne laisse pas passer la lumière
lit comme du plastique, quelle que soit sa silhouette — et c'était l'écart le plus net au
canon pontique (« lumière filtrée », « jeux d'ombres, volumétrie »).

### .5a — Modèle d'ombrage : `MSM_TWO_SIDED_FOLIAGE`

`M_AnastasisVegetation` passe du Default Lit au feuillage deux faces. La lumière traverse
désormais la couronne : les étages bas reçoivent ce que les étages hauts laissent passer.

La transmission n'est pas une seconde couleur peinte à la main — elle est **dérivée** de la
couleur de base :

```
SubsurfaceColor = VertexColor.rgb x TRANSMISSION_WARMTH x VertexColor.a
```

Un conifère sombre transmet sombre, un hêtre clair transmet clair, sans avoir à tenir deux
palettes cohérentes entre elles. Une feuille à contre-jour perd le bleu, d'où la chaleur.

**L'alpha des sommets devient un masque de feuillage** : 0 sur le bois, 1 sur les masses
foliaires. Sans lui, le modèle deux faces rendrait aussi les troncs translucides — et un fût
qui laisse passer le jour cesse de peser exactement autant que le tronc invisible que la
passe 1 avait corrigé. Le canal était libre : la palette l'écrivait à 1 partout.

**Calibré, pas choisi.** Un premier jet à `TRANSMISSION_WARMTH = (2.6, 2.1, 0.8)` faisait
virer les feuillus au citron et effaçait la masse sombre des conifères : la transmission
n'y révélait plus le volume, elle éclaircissait tout. C'est l'oversaturation que
`P1_6_PONTIC_BYZANTINE_ART_DIRECTION.md` refuse nommément, et une passe qui gagne en
spectacle ce qu'elle perd en matière n'est pas un gain. Ramené à `(1.5, 1.25, 0.55)` :
visible là où la couronne est fine et à contre-jour, invisible ailleurs.

### .5b — Normales : dures où la forme décroche, douces où elle tourne

C'est une **correction d'un défaut introduit en passe 1**, pas une amélioration. Les options
de build portaient `enable_recompute_normals = True`, qui moyenne tout : les décrochements
de jupe que la grammaire construit exprès étaient ensuite lissés au rendu, et le fût prenait
un aspect caoutchouteux.

```
compute_split_normals(opening_angle_deg = 45)
enable_recompute_normals = False      <-- sans ça, le build jette le travail ci-dessus
```

45° sépare exactement ce qu'il faut : les facettes radiales d'un tronçon (360/11 = 33°) et
celles d'un lobe (~36°) se lissent, donc une couronne reste ronde ; les décrochements
d'étage et les jonctions bois/feuille (~90°) restent francs.

Le faux ami mérite d'être nommé : à `True`, l'option de build aurait **jeté en silence** les
normales authorées — aucune erreur, aucun avertissement, et .5b n'aurait servi à rien. C'est
la sonde d'API qui l'a montré, pas la lecture du code.

### Preuve — refaite, pas recopiée

`main` a bougé pendant cette passe : `ea87acc`
(*le relief se lit — tessellation et morphologie*) a landé un terrain entièrement neuf sous
mes arbres. La preuve prise avant ce commit ne décrivait donc plus ce qui est sur le disque.

Branche rebasée sur `ea87acc`, puis **les deux captures refaites sur le terrain courant** :

- `C_close_after.png` — assets de la passe 2, terrain post-forge
- `D_shading_after.png` — assets .5a/.5b, **même terrain, même caméra, même seed**

Les assets « avant » ont été restitués depuis git (`git checkout ea87acc -- …`) puis rendus,
et l'état .5 restauré depuis git également — jamais par régénération, pour qu'aucune dérive
d'octets ne puisse s'glisser entre la capture et ce qui est committé.

Les captures `A_*` et `C_wide_after.png` datent d'avant le forge de terrain : elles
documentent les passes 1 et 2, déjà scellées dans `main`, et ne sont **pas** comparables à
`D`. Elles sont conservées comme historique, pas comme référence.

Sur `B_stature_board.png` (banc neutre, sol plat, insensible au forge), .5b est sans
ambiguïté : chaque étage est une bande nette à arête franche, l'intérieur des couronnes reste
lisse.

Après rebase : **62 PASS / 4 KNOWN_EXPECTED_FAILURE / 0 FAIL** (2 tests de plus, apportés par
le forge), `TREE_PIVOT variants_checked=8`, et surtout `ground_error=0.000000000` — les
arbres reposent exactement sur le sol tessellé neuf, sans que rien n'ait eu à changer côté
végétation.

### Un défaut cherché et non trouvé

Des entailles sombres apparaissaient dans la surface au premier plan de `D_shading_after.png`.
Vérification à courte distance (caméra `3500,4700,880`, pitch −28) : ce sont des **ombres dans
des plis concaves**, pas des trous — la surface est continue. Noté ici parce qu'un défaut
supposé et non vérifié vaut moins que rien, et parce que le prochain à regarder cette capture
se posera la même question.

Aucun C++ modifié : `BUILD` inchangé, et la suite reste à 60 PASS / 4 KNOWN_EXPECTED_FAILURE
/ 0 FAIL, `TREE_PIVOT variants_checked=8`, `ground_error=0.000000000`.

## 7quater. Passe 2.6 — deux slots de matériau, et un diagnostic démenti

La limite n°9 de la passe 2.5 disait : *« `MSM_TWO_SIDED_FOLIAGE` change la réponse diffuse
de tous les pixels du matériau, écorce comprise ; sur la planche neutre les fûts remontent en
valeur. »* Le correctif annoncé était de séparer les matériaux. Il a été fait — et **la mesure
a démenti le diagnostic**.

### Ce qui a été fait

Chaque primitive porte désormais un `material_id` à la construction : **0 = feuillage,
1 = bois**. Les identifiants survivent à `append_mesh` (vérifié sur ce build : deux
`material_id` donnent `static_materials = 2`), donc les 9 meshes ont deux slots.

- `M_AnastasisVegetation` (slot 0) — `MSM_TWO_SIDED_FOLIAGE`, deux faces, rugosité 0,82.
- `M_AnastasisBark` (slot 1) — **`MSM_DEFAULT_LIT`, une seule face**, rugosité 0,93,
  spéculaire 0,10 : une écorce humide n'accroche pas la lumière comme une feuille cireuse.

Côté moteur, `FAnastasisPresentationVariant` gagne `AdditionalMaterialOverrides` (slots 1..N).
Le slot 0 garde son champ et son sens, donc la donnée écrite avant cette passe résout
exactement comme avant. `PlaceDressing` pose maintenant les slots supplémentaires — il
n'écrivait que le slot 0, et un slot laissé vide rend en damier gris, ce qui est pire qu'un
tronc mal ombré.

### La mesure, qui dit non

Même planche, mêmes fenêtres de pixels, avant et après la séparation :

| fenêtre | luma avant | luma après | delta |
|---|---|---|---|
| fût émergent conifère | 112,3 | 111,0 | **−1,3** |
| fût émergent feuillu | 122,9 | 121,4 | **−1,4** |
| couronne conifère | 140,2 | 140,2 | 0,0 |
| sol neutre (témoin) | 177,8 | 177,7 | −0,1 |

**−1,3 sur ~112, avec un témoin à −0,1 : c'est du bruit.** Le modèle d'ombrage n'était donc
pas ce qui rendait les fûts clairs. Ils sont clairs parce que `BARK_OLD` vaut 0,090 en
linéaire, soit ~0,33 en sRGB — un beige moyen. **C'est une couleur que j'ai choisie, pas un
artefact de rendu**, et le diagnostic de la limite n°9 était une hypothèse que je n'avais pas
vérifiée avant de l'écrire.

La couleur n'est pas retouchée pour autant : dans la scène réelle (`E_bark_slot_after.png`)
les fûts lisent sombres et solides contre l'herbe. Le beige n'apparaît que sur la planche, dont
le sol neutre clair est un banc d'essai, pas une cible. Régler une couleur pour flatter un banc
serait exactement l'inverse du test anti-arnaque.

### Ce que la passe achète réellement

Rien sur l'image, et c'est correct de le dire. Ce qu'elle achète est structurel :

- le bois n'a **plus du tout** de chemin de transmission — avant il en avait un, annulé par le
  masque alpha ; c'est la différence entre « à zéro » et « absent » ;
- l'écorce a sa propre rugosité et son propre spéculaire, impossibles à régler quand un seul
  matériau servait les deux matières ;
- le bois est **une seule face** : les fûts ne paient plus le rendu de leurs faces arrière ;
- et tout travail futur spécifique à l'écorce (texture, poids de vent différent du feuillage)
  devient possible — il ne l'était pas avec un slot unique.

`Anastasis.Presentation.TreeMaterialSlots` verrouille les deux moitiés : le mesh déclare deux
slots, la donnée en nomme autant, et le résolveur rend bien deux matériaux distincts et
chargés (`TREE_SLOTS variants_checked=8`).

## 8. Intégration Unreal

L'architecture n'a **pas** été réécrite. HISM reste le motif ; il y a maintenant un
composant par variante réellement utilisée, ce que le code faisait déjà par conception
(`Dressing_<Archetype>_v<N>`). Pas de nouvel Actor, pas de PCG, pas de Nanite, pas de
nouveau framework végétal.

`ANASTASIS_ECOLOGY_COST enabled=1 generation_ms=19–35 components=7 instances=886`

La contrainte structurante, tenue et verrouillée : **deux formules de lift coexistent** dans
`AnastasisWorldEmbodiment` — le chemin hérité utilise `0.5 × EngineBasicShapeSize × Scale`,
le chemin écologique `−MeshBounds.Min.Z × Scale`. Elles ne coïncident que si chaque mesh
couvre Z = [−50, +50]. Un mesh reconstruit hors convention ferait flotter ou enfoncer les
arbres, **et dans un seul des deux chemins**. Le générateur vérifie donc la normalisation au
lieu de la supposer (il échoue au-delà de 0,01 uu), et un nouveau test la verrouille côté
moteur.

La donnée fait autorité, pas le code : `ANASTASIS_PRESENTATION_REGISTRY source=asset entries=2`.
Le repli code dit la même chose, pour qu'un checkout sans data asset rende la même grammaire.

## 9. Tests / build

```
BUILD::PASS                       Anastasis_UnrealV2Editor Win64 Development
PASS                   : 60
KNOWN_EXPECTED_FAILURE : 4        (les 4 du registre, aucune nouvelle)
FAIL                   : 0
TOTAL                  : 64
```

Cinq tests ajoutés :

- `Anastasis.Presentation.Stature` — une stature étiquetée atteint sa propre variante sur
  chaque tuile ; le choix est reproductible ; une demande `Any` couvre encore plusieurs
  looks ; **fail-open** : une donnée sans look pour la stature demandée dessine quand même
  l'archétype (la présence est vérité de simulation, la stature n'est qu'un habillage) ;
  une variante non étiquetée sert toutes les demandes, donc la donnée écrite avant cet axe
  rend à l'identique ; le lift suit le biais d'échelle.
- `Anastasis.Presentation.Lean` — `MaxLeanDegrees=0` reste d'aplomb à 1e-6
  (`worst_plumb=0.000000000`), une enveloppe incline réellement, et aucune instance ne
  dépasse l'enveloppe déclarée (`worst_lean=4.976`).
- `Anastasis.Presentation.TreePivotConvention` — les 8 variantes chargent et couvrent
  Z = [−50, +50] (`TREE_PIVOT variants_checked=8`).
- `Anastasis.Presentation.Species` — une demande (stature, famille) atterrit dans sa propre
  case sur chaque tuile ; le repli descend d'un cran **dans le bon ordre** (famille
  abandonnée, stature conservée) ; une variante non étiquetée sert encore toutes les
  demandes ; le tirage est reproductible.
- `Anastasis.Presentation.SpeciesGradient` — monotone dans chaque axe et dans le sens que
  l'écologie donne ; **ne sature nulle part sur la plage réelle des champs**
  (`SPECIES_GRADIENT low=0.064 high=0.919 span=0.855`) ; un champ non fini retombe sur la
  base au lieu d'introduire un biais silencieux. C'est ce test qui aurait attrapé le pivot
  à 0,55.

Et surtout, l'invariant scellé qui aurait pu casser en silence tient exactement :

```
DRESSING_ON_GROUND surface=1162 slice=126 slab=1178 ground_error=0.000000000 slab_error=0.000000000
```

## 10. Captures avant / après

Toutes au même banc (`Lvl_AnastasisSlice` : soleil 75000 lux, EV100 figé à 14), seed 12345,
monde 96×96, mode surface. Aucun fog, aucun coucher de soleil, aucun étalonnage.

| Fichier | Caméra | Contenu |
|---|---|---|
| `A_close_before.png` | `(3050,4250,1100)` pitch −15 yaw 45 | **AVANT** passe 1 — cônes identiques, aucun tronc (terrain pré-forge) |
| `C_close_after.png` | **identique** | passes 1+2, **terrain post-forge** — le « avant » de la passe .5 |
| `B_stature_board.png` | banc dédié, arc | les 8 silhouettes par paires strate×famille + ruine 0,9 m + repère 1,8 m |
| `D_shading_after.png` | **identique à C close** | après .5a/.5b, terrain post-forge — feuillage deux faces, normales fractionnées |
| `E_bark_slot_after.png` | **identique** | après 2.6 — bois et feuillage sur deux matériaux |
| `A_before_cone.png` | caméra monde scellée | avant, lecture macro |
| `C_wide_after.png` | **identique** | après, lecture macro |

La planche `B` place ses sujets sur un **arc centré sur la caméra**, pas en rang. En rang,
les sujets des extrémités sont plus loin que celui du milieu et la perspective les
rapetisse — à 1250 uu de recul pour un rang de 2100, le sujet du bord perd 23 % de sa
taille apparente. La planche aurait alors menti sur exactement ce qu'elle prétend montrer.

## 11. Fichiers modifiés

```
M  Content/Anastasis/Presentation/DA_AnastasisPresentation.uasset
M  Content/Anastasis/Vegetation/SM_Tree_Generic_01.uasset
A  Content/Anastasis/Materials/M_AnastasisVegetation.uasset
A  Content/Anastasis/Vegetation/SM_Tree_Broadleaf_Canopy_01.uasset
A  Content/Anastasis/Vegetation/SM_Tree_Broadleaf_Emergent_01.uasset
A  Content/Anastasis/Vegetation/SM_Tree_Broadleaf_Subcanopy_01.uasset
A  Content/Anastasis/Vegetation/SM_Tree_Broadleaf_Understory_01.uasset
A  Content/Anastasis/Vegetation/SM_Tree_Conifer_Canopy_01.uasset
A  Content/Anastasis/Vegetation/SM_Tree_Conifer_Emergent_01.uasset
A  Content/Anastasis/Vegetation/SM_Tree_Conifer_Subcanopy_01.uasset
A  Content/Anastasis/Vegetation/SM_Tree_Conifer_Understory_01.uasset
M  Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationRegistry.{h,cpp}
M  Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationResolver.{h,cpp}
M  Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationResolverTests.cpp
M  Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp
M  tools/unreal/create_tree_asset.py
A  tools/unreal/set_tree_grammar.py
A  tools/unreal/capture-tree-lineup.{py,ps1}
M  tools/unreal/capture-slice.ps1
M  tools/unreal/observe-slice.py
M  tools/unreal/presentation-registry.py
M  tools/unreal/set_presentation_meshes.py
A  docs/visual/tree-form-001/  (rapport + 5 captures)
```

## 12. Limites restantes

Constatées, pas corrigées — elles sortent du mandat ou méritent leur propre passe.

1. **La hauteur littérale n'est pas atteinte.** 6,3 m au maximum contre les 10–25 m d'un
   conifère pontique réel. Bloqué par la densité de la distribution scellée (§4).
2. **Pas de LOD.** `create_new_static_mesh_asset_from_mesh` ne produit que le LOD0. À 438
   instances c'est sans effet ; à la densité d'une vraie forêt, il en faudra.
3. **Collision NDOP10 sur l'arbre entier**, héritée de l'asset précédent. Sur un arbre haut,
   le volume est gros ; seul le fût devrait bloquer. Comportement inchangé, donc pas une
   régression — mais c'est désormais plus visible.
4. **Aucune micro-texture.** Voulu : la mission portait sur la silhouette. De près, l'écorce
   et le feuillage sont des aplats de couleur de sommet.
5. **Le gradient d'espèce n'a qu'un seul seed de preuve.** Les constantes sont réglées sur la
   distribution du monde canonique (seed 12345). Un autre seed produira d'autres plages et
   peut déplacer le mélange ; le diagnostic `ANASTASIS_TREE_SPECIES` le dira, mais rien ne
   le vérifie automatiquement sur plusieurs mondes.
6. **L'exposition n'entre que par `Shade`.** C'est volontaire — `Shade` compose déjà pente
   et altitude — mais cela signifie qu'une pente nord et une pente sud à altitude égale ne
   se distinguent que par le terme de pente, pas par une vraie orientation solaire.
7. **La graine `RUIN` de `presentation-registry.py` pointe encore sur
   `/Engine/BasicShapes/Cylinder.Cylinder`** alors que l'asset réel porte
   `SM_Ruin_Generic_01`. Dérive préexistante, chantier d'un autre agent : signalée, pas
   corrigée.
8. **Le sol reste pâle et lavé** sous le soleil du banc, ce qui affaiblit le contraste des
   troncs. `M_AnastasisSlice` appartient au chantier matériaux en cours.
9. ~~L'écorce est rendue par un modèle d'ombrage de feuillage.~~ **Corrigé en passe 2.6**
   (deux slots), mais le diagnostic associé était faux : la mesure montre que le modèle
   d'ombrage ne pesait que −1,3 de luma sur les fûts. Voir §7quater.
10. **La valeur de l'écorce reste une question ouverte.** Sur un fond neutre clair les fûts
    lisent beige moyen. C'est la couleur choisie (`BARK_OLD` ≈ 0,33 sRGB), pas un défaut de
    rendu, et dans la scène réelle elle fonctionne. À rouvrir seulement si un cadrage réel la
    met en défaut — pas pour flatter le banc d'essai.
11. **Aucune mesure de coût.** Toujours pas de compte de triangles, de draw calls ni de LOD.
    `create_new_static_mesh_asset_from_mesh` ne produit que le LOD0. À 438 instances ça ne
    mord pas ; c'est le point .5f, non fait.

## 13. NEXT TARGET recommandé — non exécuté

**`Stone` → rochers.** C'est désormais le candidat le plus mûr et le plus symétrique :
`ETileType::Stone` est une vraie tuile de simulation, elle ne reçoit aujourd'hui **aucune
instance** (couleur de sommet seulement), et `docs/unreal/ASSET_MAP_001.md` la désignait déjà
comme la suite naturelle après l'arbre et la ruine. Tout l'outillage bâti ici se réemploie
tel quel : le générateur paramétrique, la convention de pivot Z = [−50, +50], l'axe de
stature (un bloc erratique n'est pas un affleurement), le matériau à couleur de sommet, et
la planche anti-arnaque.

Candidat suivant : **le bois mort**. `ASSET_MAP_001` le note comme faisable sans toucher
`AnastasisSim` — une fraction des tuiles `Forest` résolue vers un archétype `DeadTree` par
le même hash déterministe. La planche de référence en fait une strate à part entière
(« bois mort, toutes les strates »), et la grammaire sait déjà exprimer une variante par
stature.

---

### Verdict

```
[x] les cônes ne sont plus la silhouette finale        8 silhouettes construites
[x] le tronc est perceptible                           fût dégagé de 22 % à 54 %, ou absent (cépée)
[x] plusieurs niveaux de stature                       4 classes x 2 familles, meshes distincts
[x] hauteur cohérente avec l'échelle du monde          97–627 uu contre 91 uu de ruine
[x] les arbres ne paraissent pas clonés                8 meshes x biais x inclinaison, déterministes
[x] l'espèce répond au terrain                         p_conifer 0,06 -> 0,86 selon Shade/Wetness
[x] la lumière traverse le feuillage                   MSM_TWO_SIDED_FOLIAGE, transmission masquée par l'alpha
[x] les décrochements d'étage survivent au rendu       normales fractionnées a 45 deg, build ne recalcule plus
[x] le bois n'est plus declare comme du feuillage      2 slots, M_AnastasisBark en Default Lit, une seule face
[x] lecture claire à distance                          C_wide_after.png, caméra scellée
[x] résultat stylisé                                   pas de photoréalisme, pas de Nanite
[x] aucune refonte terrain                             AnastasisTerrainSurface intact
[x] aucune refonte simulation                          Source/AnastasisSim/ intact
[x] aucun travail concurrent écrasé                    main immobile, dressing écologique intact
[x] le projet compile                                  BUILD::PASS
[x] le résultat existe dans Unreal                     886 instances, source=asset
[x] preuve visuelle finale                             A/B/C au même banc
```
