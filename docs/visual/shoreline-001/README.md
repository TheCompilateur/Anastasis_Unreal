# Captures — SHORELINE_FORGE_001

Preuve vivante de `docs/unreal/SHORELINE_FORGE_001.md`.

**Comparaison contrôlée.** Tout est identique entre les deux images d'une paire :
même niveau (`/Game/Anastasis/Maps/Lvl_AnastasisSlice`), seed `12345`, monde
canonique `96×96` (`anastasis.Terrain.Surface 2`), soleil 75 000 lux à −38/−55,
exposition figée EV100 = 14, `viewmode lit`, `ShowFlag.MotionBlur 0`, **même
caméra au uu près**. Seule change `anastasis.Terrain.Shoreline`.

| CVar | Ce que le log imprime |
|---|---|
| `0` | `ANASTASIS_SHORELINE enabled=0 water_material=M_AnastasisSlice channels=0` |
| `1` | `ANASTASIS_SHORELINE enabled=1 water_material=M_AnastasisShoreWater channels=9216` |

La géométrie est identique dans les deux cas — `ANASTASIS_TERRAIN` imprime
`vertices=9216 triangles=18050 water_triangles=3544` des deux côtés. **Aucun
sommet ne bouge entre A et B** : ce qui change est ce que la nappe sait d'elle.

| Fichier | Octets | SHA-256 (16) |
|---|---:|---|
| `A_close_off.png` | 1 986 563 | `B56EB7667EAF3593` |
| `A_close_on.png` | 1 975 499 | `5A66483FA9901F0F` |
| `B_mid_off.png` | 2 332 091 | `277B604A34F39A40` |
| `B_mid_on.png` | 2 327 664 | `CA642DC089D945FA` |
| `C_gameplay_off.png` | 2 153 787 | `D3D1CBFD88C4F317` |
| `C_gameplay_on.png` | 2 164 996 | `84EC549A86534598` |
| `D_aerial_off.png` | 1 384 220 | `D76EF331F077B2BF` |
| `D_aerial_on.png` | 1 395 730 | `4E2DF2BA52D2899D` |
| `E_flowing_off.png` | 2 282 431 | `2EF6F70BF54B52B4` |
| `E_flowing_on.png` | 2 281 218 | `8F65BE6A6E0A230A` |
| `F_steep_off.png` | 2 095 720 | `4BB6A61607E801EB` |
| `F_steep_on.png` | 2 110 577 | `8E858DE2A49C2081` |

## Caméras

Aucune n'est cadrée à l'œil. Les points visés sortent du marqueur
`TERRAIN_SHORELINE_SITE` du test `Anastasis.Terrain.Shoreline`, qui parcourt le
trait de côte du monde canonique et désigne **par mesure** une rive plate, une
rive de chenal et une berge abrupte.

| Vue | Caméra | Site visé |
|---|---|---|
| `A_close` | `(4304,3804,434)` pitch `-27` yaw `45` | TYPE_A — tuile `(45,40)`, `flatness=1.000 flow=0.000` |
| `B_mid` | `(3914,3414,715)` pitch `-27` yaw `45` | idem, 620 uu plus loin sur le même rayon |
| `C_gameplay` | `(4153,3653,445)` pitch `-9` yaw `45` | idem, ramené à 170 uu au-dessus du niveau de la mer |
| `D_aerial` | `(-5400,-5400,10500)` pitch `-32.8` yaw `45` | la carte entière — **exactement** la caméra de `docs/visual/terrain-extent/` |
| `E_flowing` | `(6948,248,1421)` pitch `-40` yaw `45` | TYPE_B — tuile `(81,14)`, `flow=1.000` |
| `F_steep` | `(7348,48,1467)` pitch `-40` yaw `45` | TYPE_C — tuile `(85,12)`, `flatness=0.481` |

`A_close`, `B_mid` et `C_gameplay` sont **la même visée à trois distances** : la
pose MID est posée sur le site, les deux autres avancent sur son propre rayon.
Elles ne peuvent donc pas cadrer deux sujets différents.

## Ce que chaque image prouve, et ce qu'elle ne prouve pas

**`A_close` — VIEW_A, la question « le bord est-il encore coupé ? ».** Dans
`off`, la rive proche du lac est une **droite nette** : sable, puis d'un pixel à
l'autre le bleu plein. Dans `on`, un plateau turquoise pâle s'étale depuis le
trait de côte et s'élargit vers la gauche là où la berge est douce, puis se fond
dans l'eau plus sombre. La ligne droite a disparu. **C'est la capture la plus
concluante de la série.**

**`B_mid` — VIEW_B, la bande dans son contexte.** Les deux plans d'eau du cadre
passent d'un aplat bleu détouré à un bassin qui porte un liseré clair et un cœur
plus sombre. Le gain est réel mais **modéré** à cette distance : la marge occupe
les 6 à 19 premiers uu de profondeur, ce qui fait peu de pixels sur une berge
raide.

**`C_gameplay` — VIEW_C, à hauteur d'œil.** Gain **le plus faible de la série**,
et il faut le dire : à 170 uu au-dessus de l'eau on voit la nappe en incidence
rasante, la marge se comprime, et l'essentiel de ce qu'on gagne est l'absence de
l'arête franche. Une rive ne se juge pas depuis cette vue seule.

**`D_aerial` — VIEW_D, la lisibilité écologique.** Le gain le plus net après
`A_close`. Dans `off`, chaque plan d'eau est un **autocollant cyan uniforme** :
lac et chenal ont exactement la même matière. Dans `on`, chaque bassin porte son
propre dégradé — bord pâle, cœur profond — et les chenaux se lisent comme des
fils continus. La carte n'est pas devenue plus bruyante : aucun élément n'a été
ajouté, seule la nappe a gagné une lecture.

**`E_flowing` vs `F_steep` — GATE 7, variation contrôlée.** Le chenal de
`E_flowing` (FlowAmt = 1.0) porte un ourlet **marqué** des deux côtés et un cœur
franchement sombre. La berge abrupte de `F_steep` (normale Z = 0.481) porte un
liseré **beaucoup plus étroit** et un contact plus net. Ni l'une ni l'autre n'est
écrite à la main : les deux sortent du même matériau lisant `Flatness` et `Flow`.

## Ce qui manque, et ce qui a dû être refait

**La bande de limon ne se lit pas comme du limon.** `SiltWet` (albédo 0.090) est
écrit dans le matériau et mesurable, mais à l'écran la marge rend **pâle**, pas
brune. Deux causes cumulées, toutes deux hors du périmètre de cette mission : le
relief immergé est peint en bleu par `TileColor` (section 0, GROUND_SURFACE_001),
et le sol voisin est encore surexposé à EV100 = 14. Ce qu'on voit est donc un
**plateau d'eau peu profonde**, lecture juste mais pas celle qui était visée.

**Trois séries ont été jetées avant celle-ci, et il faut savoir pourquoi :**

1. Une table de vues vide rendait `SHORE_VIEW_UNKNOWN` **non fatal** : le
   viewport de l'éditeur garde sa caméra d'une session à l'autre, donc huit
   captures sont sorties correctement cadrées — par la session précédente — et
   l'A/B semblait tenir. Corrigé : une vue inconnue interrompt la capture.
2. Une paire `TYPE_B` avait les arbres **gris d'un côté, verts de l'autre** : le
   SkyLight en capture temps réel n'avait pas fini au moment du premier cliché.
   La différence mesurée aurait été celle du ciel. Le délai d'attente est passé
   de 9 s à 18 s.
3. Les vues `CLOSE` et `GAMEPLAY` étaient d'abord posées chacune par un recul et
   une hauteur propres ; la rapprochée tombait derrière une crête et
   photographiait un talus. Elles avancent désormais sur le rayon de MID.

Ces trois défauts ont été trouvés **en regardant les images**, comme
`docs/unreal/ATMOSPHERE_002.md` l'exige. Aucune des images de ce dossier n'a été
versée sans avoir été regardée.

## Reproduire

```powershell
tools\unreal\shore-water.ps1 -Rebuild
tools\unreal\shore-capture.ps1 -Out A_close_off.png -View TYPE_A_CLOSE   -Mode 0
tools\unreal\shore-capture.ps1 -Out A_close_on.png  -View TYPE_A_CLOSE   -Mode 1
tools\unreal\shore-capture.ps1 -Out D_aerial_off.png -View AERIAL        -Mode 0
tools\unreal\shore-capture.ps1 -Out D_aerial_on.png  -View AERIAL        -Mode 1
tools\unreal\shore-capture.ps1 -Out E_flowing_on.png -View TYPE_B_W_MID  -Mode 1
tools\unreal\shore-capture.ps1 -Out F_steep_on.png   -View TYPE_C_W_MID  -Mode 1
```
