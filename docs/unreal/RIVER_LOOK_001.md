# RIVER_LOOK_001 — la rivière qui existe, lue comme un courant

Suite de WATER_LOOK_001. Le réseau, les rubans et le matériau Single Layer Water
restent. Cette mission ne reconstruit pas la rivière et ne remplace pas le Water
System : il n'y en a pas. Elle fait lire le courant, la profondeur et le lit que
le drainage calcule déjà.

## Audit

### EXISTING SYSTEM

Pas de plugin Water, pas de `WaterBodyRiver`, pas de spline Unreal, pas de Water
Zone, pas de mesh d'eau du plugin, pas de flow map texture, pas de caustiques,
pas de passe underwater séparée. Choix du 2026-09-30, conservé.

L'eau rendue est un `ProceduralMesh` sur `AnastasisWorldEmbodiment` :

| Section | Géométrie | Matériau |
|---|---|---|
| 0 | relief | `M_AnastasisGround` |
| 1 | lacs, mer, mares | `M_AnastasisWater` |
| 2 | un ruban par rivière | le même |

`AnastasisDrainage` (HYDRO_NETWORK_001) donne à chaque point de rivière une
largeur, une profondeur, une vitesse de Manning et une pente. Largeur 4,5–42 m,
profondeur 0,18–2,4 m, vitesse 0,1–3,5 m/s (seed 12345). Le sens est la tangente
de la polyligne, de la source vers l'embouchure. `BuildRiverRibbons` le met dans
UV2. Le matériau d'avant advectait cinq vagues par ce vecteur, à la même vitesse
sur toute la largeur.

La profondeur optique est celle que voit Single Layer Water (absorption du fond
réel). Le lit est la couleur de sommet du relief : gravier au bord, vase au
large, plus le grain du matériau de sol. Le contact rive / eau est géométrique :
le ruban passe 3,5 m sous un talus qui traverse la ligne d'eau.

### MISSING FEATURES (avant cette mission)

Le matériau ignorait UV0 (travers du chenal), la courbure, la profondeur et la
pente. Les deux rives portaient le même vecteur. Pas de mousse. Rugosité fixe
0,035. Le lit était un dégradé lisse, sans taches de sable ou de pierre.

### SAFE TO MODIFY

`M_AnastasisWater` (autorité : `tools/unreal/water-look.py`), les canaux des
rubans, la teinte du fond immergé déjà écrite par WaterLook.

### CONFLICT RISK

Non touchés : PNJ, atmosphère, ciel, brouillard, micro-écologie, végétation,
berge végétale, matériau de sol, carte principale, hydrologie de simulation
(`AnastasisHydrology`, contrat de parité). La largeur ne reçoit pas d'étranglement
local : le test du réseau exige qu'un chenal ne rétrécisse pas vers l'aval.

## Ce qui change

1. **Rubans.** UV1.x = courbure signée [-1, 1] (positif = virage à gauche).
   UV3 = (profondeur au centre en mètres, pente). UV0 et UV2 sont inchangés.
2. **Courant.** Sur une rivière (longueur de UV2 > 0,012 ; les lacs sortent du
   shader et gardent les cinq vagues d'avant) :
   - le centre coule plus vite que les rives (profil 0,34 → 1) ;
   - l'extérieur d'un virage est un peu plus rapide, l'intérieur un peu plus lent
     et lu comme plus faible ;
   - trois vagues longues, advectées lentement ;
   - deux traînées plus fines, plus rapides, alignées sur le sens du courant.
   Ce n'est pas une texture qui glisse d'un bloc : le centre et les rives n'ont
   pas la même vitesse.
3. **Profondeur.** L'absorption d'avant (0,55, 0,16, 0,11 /m) reste celle du
   chenal. Là où le centre est peu profond, ou près de la rive, elle tombe
   (× `ShallowAbsorb` 0,38), la diffusion monte un peu, le fond s'éclaircit
   légèrement. La couleur de surface tire vers un olive sombre, pas vers un
   bleu de rive. Aucune teinte n'est indexée sur l'heure : le ciel reste celui
   de la scène.
4. **Lit.** Sous l'eau, la teinte de sommet mélange sable, gravier, vase et
   pierre par taches d'environ 16 m, toujours avec B ≤ R − 0,04. Le grain fin
   reste celui de `M_AnastasisGround`, non modifié.
5. **Mousse.** Seulement si un masque brisé (trainées de ~10 m) recouvre l'une
   de ces causes : eau rapide et peu profonde, pente forte et peu profonde,
   extérieur d'un virage, liseré de rive quand le courant est déjà vif. Elle
   relève la rugosité et casse le spéculaire. Ailleurs la rugosité reste 0,035 :
   le ciel, le soleil et la nuit se reflètent comme avant.

## Paramètres

Inchangés par défaut : `WaveSlope` 0,05, `Roughness` 0,035, `Specular` 0,5,
`SurfaceColor` (0,02, 0,045, 0,05), `Absorption` (0,55, 0,16, 0,11),
`Scattering` (0,015, 0,040, 0,048), `PhaseG` 0,1, `ColorScaleBehindWater` 1.

Nouveaux : `ShallowColor` (0,11, 0,125, 0,075), `ShallowBlend` 0,30,
`ShallowAbsorb` 0,38, `ShallowScatter` 1,65, `ShallowBehind` 1,12,
`FoamTint` (0,58, 0,60, 0,56), `FoamTintAmount` 0,28, `FoamRoughness` 0,26,
`SpecularOnFoam` 0,45.

Les traînées fines restent plus lentes à la rive qu'au centre, mais leur pente
est assez faible pour ne pas devenir un cannelage vu de près. La mousse est
relevée à la puissance 1,8 : seules les crêtes des zones rapides et peu
profondes, des pentes et de l'extérieur des virages restent.

## Performance

Aucun Niagara, aucun acteur, aucun tick. Deux sections de mesh comme avant.
Le surcoût est un shader un peu plus long sur les pixels de rivière ; les lacs
prennent la branche d'avant. Pas de Scene Capture.

## Fond à 18 cm — le sol se voit, l'eau non

> **Erratum, WATER_VOLUME_001 (2026-10-10).** Cette section concluait que Single Layer Water « ne montre
> pas le sol » vu d'aplomb. La cause était le pin Opacity, jamais branché donc à 1 : le moteur ne calcule
> alors pas le volume (`WaterVisibility = 1 - Opacity`, `BasePassPixelShader.usf:1141`), et les essais de
> coefficients ci-dessous se faisaient volume éteint. Avec Opacity à 0 hors écume et les coefficients en
> centimètres, le fond, les cailloux et le dégradé de profondeur apparaissent
> (`docs/unreal/handoffs/water-opacity-001.md`). Le reste de la section est conservé tel qu'il a été écrit.

Sur le monde intégré, à `34493, 65416`, le sol rendu est à 18 cm sous la nappe.
Vu d'aplomb, le ruban est un bandeau bleu-vert opaque (`contact-avant/I_fond_1.png`).

Le moteur multiplie l'extinction par la profondeur en centimètres
(`SingleLayerWaterShading.ush`). Les coefficients du matériau sont écrits par
mètre : 0,55 /m sur 18 cm devrait laisser voir le fond, 0,55 par centimètre le
ferme. Passer à 0,01 (`MetersToCentimeters`) ne change pas l'image : 1,87 % des
pixels, sous le bruit.

Sortir les rubans sur un composant à part (`RiverSurface`), pour que le sol
reste dans la profondeur opaque, ne l'ouvre pas non plus : 4,96 % en vue
d'aplomb, le bandeau reste bleu.

Une sonde ensuite : `SurfaceColor` rouge, absorption et diffusion à zéro.
`probe-red/I_fond_1.png` change de 35 % et le ruban est rouge plein. D'aplomb,
Single Layer Water peint la couleur de surface. Il ne montre pas le sol.

Le translucide, dessiné après le sol, le montre. Les pixels du ruban passent
de `(61, 85, 78)` à `(135, 115, 63)`, le tan du terrain autour
(`see-through/I_fond_1.png`, 7,8 % du cadre : le ruban seul). Ce qui apparaît
est le même sol sec, pas un lit. De la berge (`G`), le miroir des arbres
disparaît : 47 % du cadre, l'eau devient une nappe sombre.

Composer le pixel à la main (non éclairé : SceneColor teinté par l'épaisseur,
plus un SceneColor décalé par la normale au ras de l'eau) refait la même chose
d'aplomb (`composed/I_fond_1.png`, 8,3 %, mêmes pixels tan). De la berge le
bas du cadre tombe à `(1, 1, 1)` : le reflet d'écran est noir
(`composed/G_peu_profonde_1.png`, 52 %).

Ces essais sont retirés. `M_AnastasisWater` et `water-look.py` sont revenus
au Single Layer Water intégré, celui qui garde le miroir.

La couleur du ruban, d'aplomb, n'est donc pas la loi de Beer de ces 18 cm.
Ouvrir le ruban montre le sol de la berge, et ferme le reflet.

Quatre mètres en aval de cette cellule, le sol est déjà au-dessus de la nappe :
une vue zénithale mal calée photographie la berge. Un essai précédent (vagues
alignées, absorption des hauts-fonds plus faible) restait dans le bruit de l'eau
animée, 6 à 16 % entre deux prises du même état.

## Non exécuté

- Étranglements locaux de largeur : ils casseraient `NarrowingSteps == 0`.
- Interaction physique avec un rocher : pas de donnée d'obstacle sur le ruban,
  et un Niagara par pierre traverserait toute la carte.
- Caustiques, underwater, plugin Water.
- Heures du cycle codées dans le matériau. Les captures épinglent
  `anastasis.Sky.Hour 11` sans déplacer l'horloge de la simulation. Coucher
  et nuit non rephotographiés.
- Coût GPU en instructions : non mesuré.

## Validation

`report-tests.ps1 -Filter Anastasis.Terrain.Drainage.WaterLook` :
`TESTS::PASS` (1/1, 0 FAIL, 0 KNOWN_EXPECTED_FAILURE).

`compare.py` avant → retake2, même caméras, heure 11, brouillard remis.
Bruit de capture ~3,6 % de pixels > 16/255.

| Vue | Pixels > 16/255 |
|---|---|
| E étroite | 18,30 % |
| F large | 11,35 % |
| G peu profonde | 15,49 % |
| H profonde, vue haute | 0,73 % (sous le bruit) |

La vue étroite perd les cibles de réfraction circulaires et gagne des rides
alignées vers l'aval. Le coin noir au premier plan de cette vue, et les arêtes
dures des rubans vues d'en haut, sont la géométrie des rubans : déjà présents
avant ce matériau.
