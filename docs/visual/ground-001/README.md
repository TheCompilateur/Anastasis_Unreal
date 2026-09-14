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
```

Les captures brutes et leurs journaux restent dans `Saved/SliceEvidence/` et
`Saved/Anastasis/Captures/` (non versionnés). Ces cinq copies-ci sont versionnées
parce qu'elles étayent le rapport.

Rappel de `docs/visual/atmosphere-002` : l'outil de capture a déjà photographié le
viewport éditeur au lieu du PIE. **Toute image de ce projet doit être regardée avant
d'être versée en preuve.** Ces cinq-là l'ont été.
