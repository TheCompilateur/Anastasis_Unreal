# GROUND_TEXTURE_001 — le détail photo du sol

Suite de `GROUND_SURFACE_001`, qui finissait sur sa limite structurelle n°1 : **aucune
texture**, donc aucune information sous le mètre au-delà d'un bruit procédural. Cette
mission la lève. Elle ne touche ni au C++, ni aux canaux de sommet, ni aux albédos calés.

## Ce qui a été ajouté

Quatre textures **CC0** Poly Haven, 2K, une par famille de sol, choisies contre la direction
artistique P1.6 (écologie pontique humide, sécheresse méditerranéenne interdite) :

| Famille | Texture | Taille réelle |
|---|---|---|
| Herbe | `sparse_grass` | 2,0 m |
| Litière | `forest_leaves_02` | 3,0 m |
| Terre travaillée | `brown_mud_02` | 1,3 m |
| Roche | `mossy_rock` | 3,0 m |

Chaîne, en deux temps :

1. `tools/unreal/ground-textures.py` (Python système) télécharge et empaquette deux images
   par famille dans `Saved/GroundTextures/packed` (non versionné) :
   `AH` = albédo de détail + hauteur (sRGB), `NR` = normale DirectX XY + rugosité de détail
   + occlusion (linéaire). Provenance, URL et sha256 dans `Saved/GroundTextures/manifest.json`.
2. `tools/unreal/ground-material.ps1` importe les huit `T_Ground_*` (BC7) dans
   `Content/Anastasis/Materials/GroundTextures` et câble un nœud Custom HLSL dans
   `M_AnastasisGround`.

## Trois décisions, et pourquoi

**La photo module, elle ne colore pas.** `GROUND_SURFACE_001` a recalé les albédos sur des
valeurs physiques et la couleur de sommet porte la sémantique de la simulation (type, rive,
humidité). Une photo posée telle quelle aurait effacé les deux. L'albédo stocké est donc la
photo **divisée par sa moyenne locale** (valeur entière, chromie relative atténuée à 0,5) :
un facteur neutre en moyenne, appliqué après les teintes calées. Rugosité et occlusion
suivent la même règle. Au loin, les mips convergent vers 1 : le sol redevient exactement
l'ancien, sans fondu à régler.

**Passe-haut à 15 cm.** La première version gardait toute la photo. Vue oblique à 35 m,
la photo d'herbe de 2 m se lisait en **damier** — le « tile checkerboard » interdit par la
direction artistique (`E_regression_damier_v1_corrige_v2.png`, haut). Les taches à l'échelle
de la tuile ont été retirées à l'empaquetage (flou gaussien périodique par FFT, donc sans
cadre au bord de la tuile) ; la variation large reste portée par le bruit macro/méso du
matériau, qui ne se répète pas à l'échelle du monde. Contraste au-delà de 50 cm, mesuré :
herbe 0,012 → 0,001, litière 0,081 → 0,005, roche 0,059 → 0,002. Coût au rendu : nul.

**Un nœud Custom, pas des nœuds.** Projection triplanaire branchée (`[branch]` +
`SampleGrad` : le plat ne lit que l'axe Z, une famille absente n'est pas lue), normale posée
par « whiteout » axe par axe (aucune tangente nécessaire — le maillage n'en a pas), mélange
par hauteur (la terre remplit d'abord les creux entre les cailloux). En nœuds, ce serait
environ 300 expressions illisibles. Tous les réglages restent des paramètres de l'instance,
groupe `Ground|Texture`.

## Preuves

Build du worktree : `BUILD::PASS`. Génération : `GROUND_MATERIAL::PASS`, aucun
`Failed to compile Material` après `RECOMPILE_BEGIN` (nouveau garde-fou du `.ps1`).

```
STATS master pixel_instructions=924 vertex_instructions=160 samplers=10 pixel_texture_samples=3 uv_scalars=4
```

Avant : 517 instructions, 2 samplers. `pixel_texture_samples` ne compte pas les lectures du
nœud Custom ; le vrai nombre est 2 par famille présente et par axe actif (2 à 4 sur le plat,
jusqu'à 16 au pire).

A/B aux mêmes caméras, même session, `capture-ground-cover.ps1 -States
on,on_notex,bare,bare_notex` : les états `*_notex` rendent le sol par une instance
dynamique dont le fondu des textures est fermé, ce qui reproduit **exactement** l'ancien
matériau. GPU p50 (ms), sans → avec textures :

| Vue | herbe posée | sol nu |
|---|---|---|
| prairie_low | 11,0 → 10,9 | 9,4 → 9,4 |
| riviere_eye | 11,3 → 11,2 | 9,7 → 9,7 |
| lisiere_eye | 10,4 → 10,4 | 8,5 → 8,7 |
| oblique | 10,9 → 10,9 | 10,3 → 10,4 |
| vallee_b_eye | 11,2 → 11,0 | 8,1 → 8,2 |

Écarts dans le bruit de la machine partagée (run `tex-v1`). Images : `docs/visual/ground-texture-001/`.

## Limites connues

1. **Les « dunes » du relief procédural restent visibles** à mi-distance
   (`C_lisiere_sol_nu_off_on.png`). Elles sont présentes à l'identique sans textures : ce
   n'est pas une régression. Maintenant que la photo porte le détail fin, `BumpStrength` et
   `DetailContrast` pourraient baisser ; non fait, pour ne pas changer deux choses à la fois.
2. **La famille Roche ne distingue pas Stone et Ruin.** Le hameau, sur tuiles Ruin, se lit en
   dalle lichénisée plutôt qu'en gravats (`D_hameau_roche_off_on.png`). Il faudrait une
   cinquième famille, donc un canal de plus côté C++.
3. **Une texture par famille.** Pas de variante, pas de rotation par cellule : le passe-haut
   rend la répétition illisible, mais un œil collé au sol sur un grand aplat la retrouvera.
4. **Les textures pèsent 96 Mo d'assets LFS** (source PNG embarquée par Unreal).
