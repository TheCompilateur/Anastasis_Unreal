# Captures — SHORELINE_FORGE_001

Preuve vivante de `docs/unreal/SHORELINE_FORGE_001.md`.

**Prises sur le relief forgé** (`main` à `ea87acc`, TERRAIN_FORGE actif par
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
| `A_close_off.png` | 1 051 028 | `C75F36BCEEB592F6` |
| `A_close_on.png` | 1 032 722 | `CBB8AA98E8E5CFC6` |
| `B_mid_off.png` | 1 238 964 | `DFB7CD3A7FE98A6C` |
| `B_mid_on.png` | 1 245 023 | `887CC09119838E5E` |
| `C_aerial_off.png` | 679 931 | `6536824867A344B1` |
| `C_aerial_on.png` | 680 919 | `DA770FB66889D304` |

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

**Les variantes de rive (GATE 7) ne sont pas re-démontrées visuellement.** Les
trois familles sont **mesurées** sur le maillage forgé et le test les exige :

```
TERRAIN_SHORELINE_FORGED_FAMILIES soft=677 steep=1751 flowing=992
TERRAIN_SHORELINE_FORGED_SITE TYPE_A_soft_wet_bank  (6200,6350)  flatness=1.000 flow=0.000
TERRAIN_SHORELINE_FORGED_SITE TYPE_B_flowing_edge   (8200,1200)  flatness=0.738 flow=0.945
TERRAIN_SHORELINE_FORGED_SITE TYPE_C_steep_bank     (7350,1625)  flatness=0.083 flow=0.000
```

Mais la pose de cadrage `_W` (recul 1700, hauteur 1700, pitch −40) **dépasse le
site** et cadre une crête : les deux paires obtenues ne montrent pas d'eau. Elles
ne sont pas versées ici — une image de coteau étiquetée « variante de rive »
serait une preuve fausse, et c'est pire qu'une preuve absente.

Le correctif est de cadrage, pas de rive : la pose doit se dériver du site forgé
comme le font `A_close` / `B_mid`, au lieu d'un recul fixe. C'est la première
chose à refaire.

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
