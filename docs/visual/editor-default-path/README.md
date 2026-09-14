# VISUAL_DEFAULT_PATH — voir la carte sans rien lancer

Le terrain était juste, l'emprise était bonne depuis `713be6b`, le dressing se
posait correctement depuis `ff423af`. Et pourtant, ouvrir le projet ne montrait
rien de tout ça. Trois obstacles, indépendants, empilés.

## Ce qui empêchait de voir

**L'éditeur n'ouvrait pas la carte.** `EditorStartupMap` et `GameDefaultMap`
pointaient sur le template `Lvl_FirstPerson`, qui ne contient aucun acteur
Anastasis. Jouer depuis là ne produisait aucun monde.

**La carte ouverte restait vide.** `Lvl_AnastasisSlice` ne porte aucune vérité de
monde — un `AAnastasisWorldEmbodiment` vide, de la lumière, une caméra. Le
terrain n'existait qu'à partir de `BeginPlay`, donc uniquement en PIE. Un
viewport éditeur sur cette carte était légitimement noir.

**Le mode par défaut n'était pas une carte.** `anastasis.Terrain.Surface` valait
`0` : des dalles DEBUG de 20 uu d'épaisseur posées à des altitudes espacées de
50 uu, donc percées de trous. Le mode `2`, qui donne une carte, existait mais
demandait de connaître son existence et de la taper.

## Ce qui change

| | Avant | Maintenant |
|---|---|---|
| Carte d'ouverture | `Lvl_FirstPerson` | `Lvl_AnastasisSlice` |
| `LoadLevelAtStartup` | non défini — valeur moteur | `ProjectDefault`, explicite |
| Viewport éditeur | vide hors PIE | construit au chargement (`OnConstruction`) |
| `anastasis.Terrain.Surface` | `0` | `2` |
| Sauvegarder le niveau | figeait 9216 instances dans le `.umap` | composants `RF_Transient`, rien n'est figé |
| Play sur la tranche | deux embodiments empilés à l'origine | un seul |

`OnConstruction` ne s'exécute qu'en monde éditeur : en jeu c'est `BeginPlay` qui
incarne, et faire les deux incarnerait deux fois à chaque PIE. Les deux passent
par `EmbodyFromConsoleVariables`, une seule lecture des CVars.

## Le double monde

`AAnastasis_UnrealV2GameMode::BeginPlay` spawnait un embodiment sans condition,
et `Lvl_AnastasisSlice` en place déjà un à l'origine. Jouer cette carte
incarnait donc **deux fois le même monde au même endroit** : Z-fighting sur tout
le relief, chaque compteur doublé, chaque instance payée deux fois. Le bug ne
s'était jamais manifesté parce que la carte par défaut était le template, qui ne
place pas d'embodiment — le rendre carte par défaut l'aurait rendu permanent.

La décision est sortie dans `ShouldSpawnEmbodiment`, publique et statique, pour
être vérifiable sans monter une session PIE.

## Preuve

`default_open.png` — 1 300 511 octets, SHA-256 `CE214FB290C0C820`.

Éditeur lancé **sans argument de carte**, piloté par un script d'observation à
qui il est interdit d'appeler `load_level` ou `EmbodyCanonical`. Tout ce qui est
visible vient de `EditorStartupMap` puis de `OnConstruction`.

```
ANASTASIS_TERRAIN source=96x96 crop=(0,0) 96x96 tiles=9216 vertices=9216
                  triangles=18050 water_triangles=3544 material=slice
                  boundary=tile_centers legacy_visible=0
ANASTASIS_PRESENTATION dressing_instances=1162 tree_tiles=716 ruin_tiles=462
                       source=asset components=2
PROOF_MAP=Lvl_AnastasisSlice
PROOF_EMBODIMENT_ACTORS=1
PROOF_SURFACE sections=2 visible=True
```

`PROOF_EMBODIMENT_ACTORS=1` est l'assertion qui compte autant que l'image.

## Ce qui est vérifié en continu

Deux invariants de scène, là où les tests existants vérifiaient des données :

- `Anastasis.Visual.SingleEmbodiment` — un niveau qui place un embodiment
  supprime le spawn du GameMode.
- `Anastasis.Level.HoldsNoWorldTruth` — sol et surface transients sur le CDO
  (donc posés par le constructeur, pas hérités d'un flag de spawn), et le
  dressing créé à l'incarnation l'est aussi.

Suite complète : 48 PASS, 0 FAIL, 4 KNOWN_EXPECTED_FAILURE inchangés.
