# Captures — SHORELINE_FORGE_001

Preuve vivante de `docs/unreal/SHORELINE_FORGE_001.md`.

**Prises sur le relief forgé** (`main` à `ce537ff`, TERRAIN_FORGE actif par
défaut, 381×381 = 145 161 sommets). Les captures faites avant ce rebasage
montraient un monde qui n'existe plus ; elles ont toutes été refaites, aucune
n'a été recopiée.

**Comparaison contrôlée.** Tout est identique entre les deux images d'une paire :
même niveau (`/Game/Anastasis/Maps/Lvl_AnastasisSlice`), seed `12345`, monde
canonique `96×96`, soleil 75 000 lux à −38/−55, exposition figée EV100 = 14,
`viewmode lit`, `ShowFlag.MotionBlur 0`, **même caméra au uu près**. Seule change
`anastasis.Terrain.Shoreline`.

| CVar | Ce que le log imprime |
|---|---|
| `0` | `ANASTASIS_SHORELINE enabled=0 water_material=M_AnastasisSlice channels=0 water_vertices=145161 forged=1` |
| `1` | `ANASTASIS_SHORELINE enabled=1 water_material=M_AnastasisShoreWater channels=145161 water_vertices=145161 forged=1` |

`channels == water_vertices` est la ligne qui compte : elle prouve que chaque
sommet de la nappe **réellement rendue** porte ses canaux. C'est exactement ce
qui manquait juste après le rebasage, et ce que le test interdit désormais de
reperdre.

| Fichier | Octets | SHA-256 (16) |
|---|---:|---|
| `A_close_off.png` | 974 466 | `E592BBB5176B1FC9` |
| `A_close_on.png` | 980 864 | `787A31A04B6E9A7F` |
| `B_mid_off.png` | 1 101 509 | `EA2E9A833B435F35` |
| `B_mid_on.png` | 1 100 320 | `BADD72B2E66CCA72` |
| `C_aerial_off.png` | 678 514 | `92ED5EB6983FA5A7` |
| `C_aerial_on.png` | 679 755 | `98D9F1DE2BA1EEFD` |

## Caméras

| Vue | Caméra | Visé |
|---|---|---|
| `A_close` | `(6005,6155,717)` pitch `-27` yaw `45` | site TYPE_A mesuré sur le maillage forgé : `(6200,6350)`, `flatness=1.000` |
| `B_mid` | `(5564,5714,1035)` pitch `-27` yaw `45` | même visée, 700 uu plus loin sur le même rayon |
| `C_aerial` | `(-5400,-5400,10500)` pitch `-32.8` yaw `45` | la carte entière — **exactement** la caméra de `docs/visual/terrain-extent/` |

Le point visé sort du marqueur `TERRAIN_SHORELINE_FORGED_SITE`, mesuré sur le
maillage que l'on photographie. La **hauteur** de caméra est référencée au niveau
de la mer, pas à l'altitude de la tuile : la forge exagère le relief, et une
caméra posée sur l'altitude tuilée se retrouvait enterrée sous le sol forgé.

## Ce que chaque image prouve

**`A_close`** — la pièce maîtresse. Dans `off`, le plan d'eau du ravin est un
aplat à **deux tons** : une zone cyan claire et une zone bleu nuit séparées par
une **frontière franche et dentelée**, plus une arête nette contre la berge. Deux
polygones colorés. Dans `on`, la même eau est un dégradé continu : plateau pâle
contre la berge gauche, approfondissement progressif vers le centre, et la
frontière interne a disparu au profit d'une gradation qui suit le fond immergé.
Le contact avec les deux berges est adouci.

**`B_mid`** — le même bassin dans son contexte. `off` : une cuvette bleu nuit
uniforme, détourée. `on` : un liseré clair en périphérie, un cœur sombre, le
relief du fond lisible sous l'eau. Gain réel, plus discret qu'en gros plan.

**`C_aerial`** — la lisibilité à l'échelle carte. La baie du sud passe d'une tache
cyan plate à un bassin qui porte son propre dégradé. Aucun élément n'a été
ajouté : la carte n'est pas devenue plus bruyante.

## Ce qui n'est PAS dans ce dossier, et pourquoi

**Les variantes de rive (GATE 7) ne sont toujours pas photographiées.** Les trois
familles sont mesurées sur le maillage forgé et le test échoue si l'une
disparaît. Ce qui manquait était le **cadrage**, et trois des quatre inconnues
ont été résolues — en les mesurant, pas en les réglant :

| Inconnue | Avant | Maintenant | Ce que la mesure a révélé |
|---|---|---|---|
| **direction** de l'eau | yaw figé à 45° | `water_yaw` mesuré par site | TYPE_A est à 41.2° — la diagonale marchait **par accident** ; les autres sites étaient à 50° près |
| **hauteur** de dégagement | 760 uu fixes | `clear_z` mesuré par site | il faut 891 uu ici, 1527 là : une hauteur fixe enterre la caméra |
| **bordure** du monde | ignorée | sites à < 2500 uu du bord écartés | la caméra recule autant qu'elle monte et **sortait de la carte** (y = −560) |
| **échelle** du sujet | recul unique | *non résolu* | les deux familles non-A sont des **chenaux étroits en ravin** : au recul MID ils ne font qu'un filet |

Le yaw se déduit du barycentre des sommets immergés moins celui des émergés ; la
hauteur du point le plus haut du relief dans un rayon de 900 uu. Les deux sortent
du marqueur `TERRAIN_SHORELINE_FORGED_SITE` à chaque exécution.

Reste donc une seule inconnue, et elle est nommée : **le recul doit suivre la
taille du sujet.** Un bassin large se cadre à 900 uu, un chenal de ravin demande
d'être beaucoup plus près — et la vue rapprochée, elle, bute encore sur la
capture. C'est la prochaine mesure à ajouter : la largeur locale du plan d'eau.

**Aucune image de variante n'est versée.** Les frames obtenues montrent du relief
avec un filet d'eau ; les étiqueter « variantes de rive » serait une preuve
fausse, ce qui reste pire qu'une preuve absente.

## Reproduire

```powershell
tools\unreal\shore-water.ps1 -Rebuild
tools\unreal\shore-capture.ps1 -Out A_close_off.png -View TYPE_A_CLOSE -Mode 0
tools\unreal\shore-capture.ps1 -Out A_close_on.png  -View TYPE_A_CLOSE -Mode 1
tools\unreal\shore-capture.ps1 -Out B_mid_off.png   -View TYPE_A_MID   -Mode 0
tools\unreal\shore-capture.ps1 -Out B_mid_on.png    -View TYPE_A_MID   -Mode 1
tools\unreal\shore-capture.ps1 -Out C_aerial_off.png -View AERIAL      -Mode 0
tools\unreal\shore-capture.ps1 -Out C_aerial_on.png  -View AERIAL      -Mode 1
```

Deux défauts de capture ont été corrigés en cours de route, tous deux trouvés en
regardant les images :

1. **`HighResShot 1920x1080` sur un viewport 1280×720 force le rendu en tuiles**,
   et sur le maillage forgé (288 800 triangles) il n'écrit **aucun fichier et
   aucune erreur** — huit relances, puis `SHORE_SHOT_MISSING`. C'est trait pour
   trait le `KNOWN_DEBT` n°1 de `TERRAIN_SURFACE_EXTENT.md`, resté sans cause
   depuis. La capture se demande désormais à la taille du viewport.
2. **Le SkyLight en capture temps réel** n'avait pas fini au premier cliché : une
   paire est sortie avec les arbres gris d'un côté, verts de l'autre. Le délai
   est passé de 9 s à 18 s.
