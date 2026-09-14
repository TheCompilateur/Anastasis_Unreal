# GROUND_001 — preuve visuelle A/B du sol morphologique

Comparaison contrôlée. **Tout est identique entre A et B**, et entre C et D : même
binaire, même graine `12345`, même monde canonique 96×96, même soleil 75 000 lux,
même exposition figée EV100 = 14, même caméra. Seule change la CVar
`anastasis.Terrain.GroundMaterial`.

C'est ce que cette CVar existe pour permettre : les deux chemins bâtissent
exactement la même géométrie et les mêmes canaux de sommet, si bien qu'une capture
A/B ne mesure que la fonction qui les lit.

| Image | CVar | Matériau posé | Vue | Octets | SHA-256 (16) |
|---|---|---|---|---:|---|
| `A_world_ground_off.png` | `0` | `M_AnastasisSlice` | aérienne `(-5400,-5400,10500)` | 1 394 362 | `AC01C528C4E37127` |
| `B_world_ground_on.png` | `1` | `MI_AnastasisGround` | idem | 1 044 500 | `D07383C2CB76B571` |
| `C_shore_ground_off.png` | `0` | `M_AnastasisSlice` | rive, PIE, signet `SHORE` | 864 651 | `0516BF307455A754` |
| `D_shore_ground_on.png` | `1` | `MI_AnastasisGround` | idem | 1 046 790 | `BE0368CEDAB0F278` |
| `F_forest_ground_off.png` | `0` | `M_AnastasisSlice` | lisière, PIE, signet `FOREST` | 883 444 | `3021A2C7CE5E3AB2` |
| `G_forest_ground_on.png` | `1` | `MI_AnastasisGround` | idem | 978 775 | `9D171E595AD68C1E` |

Les caméras aériennes portent l'angle scellé de `docs/visual/terrain-extent` :
pitch `-32.8`, yaw `45`.

## Ce que chaque image montre, dit tel quel

**`A` / `B` — la lecture à distance.** En `A`, le sol est une nappe pastel continue :
les vallées, les crêtes et les plateaux portent la même matière, et rien ne distingue
une pente d'un fond plat. En `B`, trois lectures apparaissent qui n'existaient pas :
des masses chromatiques larges (vallées vertes, hauteurs sèches ocre), de la roche
grise sur les ruptures de pente et sur la bordure du monde, et une bande sombre au
contact de l'eau. Le réseau hydrographique se lit par ses berges, plus seulement par
sa couleur.

Ce qui **ne** change **pas** entre `A` et `B` : la géométrie
(`vertices=9216 triangles=18050 water_triangles=3544` dans les deux cas), l'eau, les
arbres, la lumière. Le seul delta est la fonction de surface.

**`C` / `D` — la lecture au sol.** C'est la comparaison qui compte le plus, parce
qu'un matériau de sol se juge à hauteur d'homme. En `C`, la rive est une surface
absolument lisse, d'une seule teinte crème ; on voit les facettes de la triangulation
et rien d'autre — le symptôme « maquette » que la mission visait. En `D`, la même rive
porte un grain, une bande humide plus sombre et plus lisse près de l'eau, une prairie
mouchetée de zones sèches, et une falaise dont la matière se distingue de l'herbe qui
la surmonte.

**`F` / `G` — la lisière.** La comparaison qui manquait au premier commit. En `F`, la
forêt et la prairie partagent **exactement le même sol** : une nappe olive uniforme, sur
laquelle les arbres sont simplement posés. En `G`, le peuplement a un sol à lui — plus
sombre, plus brun, moucheté — et la clairière au premier plan reste plus claire et plus
sèche. La transition entre les deux est continue, pas une frontière de tuile : c'est ce
que la partition de l'unité produit à l'interpolation.

C'est la démonstration du critère « les forêts émergent d'un sol compatible avec elles ».
Elle était annoncée comme mécaniquement satisfaite mais visuellement non démontrée ; le
signet `FOREST` la rend observable.

Les arbres eux-mêmes restent les cônes de remplacement — ils appartiennent à la mission
`anastasis-tree-visuals`, pas à celle-ci, et ils sont identiques dans les deux images.

**Limite honnête de `C`/`D` :** la palette C++ a été retonée dans le même commit, et
elle s'applique aux deux images. `C` n'est donc **pas** l'état d'avant la mission —
c'est l'état d'avant le *matériau*, palette déjà corrigée. L'état réellement initial
est `docs/visual/terrain-extent/C_world_surface.png`, où le monde est un plâtre blanc.

## `E` — une régression, gardée exprès

`E_regression_leopard_bump.png` (1 095 728 octets, `085C5106E55F681E`) n'est pas une
preuve de réussite : c'est un état intermédiaire, conservé parce qu'il documente la
seule erreur de conception réelle de cette mission.

Le relief micro est dérivé du **gradient** d'un bruit de Perlin. Ce gradient n'est pas
normé : sa longueur dépasse couramment 1. À `BumpStrength = 0.45` la normale basculait
donc de plus de 40°, et sous un soleil rasant (−38°) cela ne produit pas du relief mais
des taches **entièrement à l'ombre** — le sol au bord de l'eau se lit comme une peau de
léopard.

Le correctif n'est pas « baisser le nombre » : c'est **borner le gradient**
(`G / (1 + |G|)`), après quoi `BumpStrength` redevient une grandeur lisible — la
tangente de l'inclinaison maximale. Voir `tools/unreal/ground-material.py`.

## Reproduire

```powershell
tools\unreal\ground-material.ps1
tools\unreal\capture-slice.ps1 -Mode 2 -Out A_world_ground_off.png -PreCmds 'anastasis.Terrain.GroundMaterial 0'
tools\unreal\capture-slice.ps1 -Mode 2 -Out B_world_ground_on.png  -PreCmds 'anastasis.Terrain.GroundMaterial 1'
tools\unreal\probe-demo.ps1 -Mission shore-ground-off -Bookmark SHORE -PreCmds 'anastasis.Terrain.GroundMaterial 0'
tools\unreal\probe-demo.ps1 -Mission shore-ground-on  -Bookmark SHORE -PreCmds 'anastasis.Terrain.GroundMaterial 1'
tools\unreal\probe-demo.ps1 -Mission forest-off -Bookmark FOREST -PreCmds 'anastasis.Terrain.GroundMaterial 0'
tools\unreal\probe-demo.ps1 -Mission forest-on  -Bookmark FOREST -PreCmds 'anastasis.Terrain.GroundMaterial 1'
```

Le signet `FOREST` se calcule, il n'est pas codé en dur : il cherche le point le plus
couvert du monde sur un voisinage 5×5, puis une tuile non forestière à 3-6 tuiles de là.
Sur la graine `12345` cela donne, et le journal l'imprime :

```
ANASTASIS_WORLD_BOOKMARK FOREST core=(74,44) density=25/25 stand=(71,47) ring=3 from_edge=1
```

`density=25/25` : les vingt-cinq cellules autour du cœur sont forestières. C'est une
masse, pas un arbre isolé.

Les captures brutes et leurs journaux restent dans `Saved/SliceEvidence/` et
`Saved/Anastasis/Captures/` (non versionnés). Ces sept copies-ci sont versionnées
parce qu'elles étayent le rapport.

## L'outil de capture a encore photographié le mauvais viewport

Rappel de `docs/visual/atmosphere-002` : l'outil de capture peut photographier le
viewport éditeur au lieu du PIE. **Toute image de ce projet doit être regardée avant
d'être versée en preuve.** Ces sept-là l'ont été — et la règle a resservi.

Les deux premières tentatives sur `FOREST` ont rendu le viewport éditeur de
`Lvl_FirstPerson` : géométrie de prototypage grise, plots jaunes, sprites d'éditeur,
aucun terrain. Deux fois de suite, le même mauvais viewport — les fichiers ne sont pas
identiques au bit près (1 256 683 et 1 256 981 octets ; les sprites d'éditeur bougent),
mais ils montrent la même scène. Les journaux des deux runs sont pourtant corrects et
identiques à ceux qui réussissent :

```
PROBE_DEMO_PIE_ACTIVE
ANASTASIS_WORLD_STATUS map=UEDPIE_0_Lvl_FirstPerson actors=153 terrain_present=1 tiles=9216 camera_active=1
ANASTASIS_WORLD_GOTO bookmark=FOREST loc=(7150.0,4750.0,615.0) rot=(-22.6,-45.0,0.0) fov=85.0
CAPTURE::PASS
```

PIE tournait, le terrain était là, la caméra était au bon endroit, et `CAPTURE::PASS`
a été rapporté. Seule l'image est fausse. Les deux tentatives suivantes, sans aucun
changement, ont rendu le bon cadre.

C'est le **défaut 1** diagnostiqué dans `docs/unreal/ATMOSPHERE_002.md` : la requête de
capture est globale et `bInRestrictToGameViewport` est du code mort sur ce chemin
(`bShowUI = false`).

Une signature utile en attendant : une capture d'éditeur pèse ici ~1,25 Mo, une capture
de jeu ~0,9 Mo. `forest-off2`, écartée, faisait 1 257 559 octets.

**Corrigé depuis** — voir `docs/unreal/CAPTURE_VIEWPORT_001.md`. Les images A–G
ci-dessus ont été prises avant le correctif, et chacune a été regardée.
