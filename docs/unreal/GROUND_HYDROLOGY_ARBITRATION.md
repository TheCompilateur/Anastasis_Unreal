# Arbitrage GROUND_SURFACE_001 × hydrology-surface

Deux missions ont travaillé le même fichier en même temps, sans se voir :
`AnastasisTerrainSurface.cpp`. Ce document dit ce qui a été gardé de chaque côté, ce
qui a été changé, et pourquoi. Il est écrit depuis la voie du sol — l'autre voie n'a
pas été consultée, faute de pouvoir l'être.

## Ce que chaque mission faisait

| | GROUND_SURFACE_001 | hydrology-surface (`92e5f5f`) |
|---|---|---|
| Support | canaux `UV0`/`UV1` + matériau par pixel | couleur de sommet |
| Humidité | bande étroite, seuillée, bruitée | teinte large `WetMud` |
| Rive | teinte `ShoreSand` retonée | teinte `SaturatedBank` retonée |
| Pente | masque de roche, depuis la normale rendue | éclairement ±18 %, depuis `Shade` sim |
| Écoulement | — | `FlowAmt` → `ChannelWater` sur l'eau |
| Familles de sol | roche / litière / terre travaillée / herbe | — |

La lecture qui compte : ce ne sont **pas** deux fois le même travail. Trois zones se
recouvrent vraiment, deux sont propres à une seule mission.

## Ce qui a été gardé, et pourquoi

**L'eau et l'écoulement, entièrement à hydrology-surface.** `ChannelWater`,
`WaterDepthFromShade`, le gradient `FlowAmt` : GROUND_SURFACE_001 avait explicitement
sorti l'eau de son périmètre et lui avait même donné son propre matériau. Rien à
arbitrer, tout est additif.

**Les familles de sol, entièrement à GROUND_SURFACE_001.** hydrology-surface n'a pas
d'équivalent.

**La rive : même valeur trouvée deux fois.** `ShoreSand` à (0.251, 0.216, 0.159) contre
`SaturatedBank` à (0.247, 0.220, 0.165) — trois millièmes d'écart, deux diagnostics
indépendants du même défaut (l'original à 0.694 sortait blanc à l'exposition du rig).
C'est le nom d'hydrology qui reste : son code le référence, et il décrit mieux la
matière. La corroboration vaut mieux que l'arbitrage — deux missions qui ne se
parlaient pas ont mesuré la même chose.

**L'altitude : la valeur de GROUND_SURFACE_001.** hydrology-surface gardait
`HighlandRock` à 0.518, l'autre moitié du halo blanc. Corriger la rive sans corriger
l'altitude laissait les sommets en neige. Descendu à 0.171.

## Ce qui a dû changer, et ce que ça a appris

**`WetMud` : de 0.204 à 0.082.** C'est le point intéressant de tout l'arbitrage, et
c'est *leur* test qui l'a trouvé.

`Anastasis.Terrain.HydrologyGradient` affirme « la crue assombrit le sol ». Il passait
chez eux et **échouait** une fois les deux missions réunies. Raison, arithmétique :
leur `WetMud` (luminance 0.185) avait été calibrée contre l'herbe **d'origine**
(luminance 0.359), qu'elle assombrissait bien. GROUND_SURFACE_001 a ramené l'herbe à
0.153 pour la sortir du blanc — et à cette luminance, la même vase devient plus
**claire** que le sol sec. La crue éclaircissait le terrain.

Ni l'une ni l'autre des deux missions ne pouvait voir ça seule. La vase saturée est
l'une des surfaces naturelles les plus sombres (albédo réel 0.05–0.10) ; à 0.073 de
luminance elle repasse sous l'herbe, et leur test redevient vert **pour la bonne
raison**, pas parce qu'on l'a affaibli.

**`DampDarken` : de 0.52 à 0.88.** Les deux missions lisaient la même `Wetness` et
assombrissaient chacune de leur côté ; au bord de l'eau le sol sortait deux fois plus
sombre qu'aucune ne le voulait. Le partage suit ce que chaque support sait faire :

- la **couleur de sommet** porte la chromie de la zone humide — large, basse fréquence,
  et elle fonctionne même à `anastasis.Terrain.GroundMaterial 0` ;
- le **matériau** garde le lustre — rugosité et spéculaire au trait de côte, que la
  couleur de sommet ne peut pas exprimer du tout.

Le matériau ne creuse plus qu'un dernier cran au contact de l'eau.

## Ce qui n'a pas été tranché, et pourquoi

**`SlopeLit` — l'éclairement ±18 % depuis `Shade`.** Gardé tel quel, avec une objection
écrite.

`Shade` est, côté simulation, un terme d'éclairage **2D** calculé pour une direction de
lumière propre au simulateur JS. Il est ici cuit dans l'albédo d'une scène éclairée en
3D par un soleil à −38°/−55°, sur un relief que `TERRAIN_FORGE` a par ailleurs
tessellé. Les deux éclairages n'ont aucune raison de coïncider : un versant réellement
au soleil peut être assombri par le `Shade` du simulateur, et inversement.

Je n'ai pas retiré ce terme. Son amplitude est faible, c'est une décision délibérée de
l'autre voie, et **je ne peux pas prouver le dommage** sans une capture A/B dédiée que
je n'ai pas faite. Retirer le travail d'un autre agent sur un jugement non prouvé est
précisément ce que le protocole multi-agent interdit.

Ce qui trancherait : deux captures au même signet, `SlopeLit` actif puis neutralisé,
sur une pente dont on sait de quel côté vient le soleil. Si le versant éclairé par
Unreal est celui que `Shade` assombrit, le terme est à retirer.

## État

```
BUILD::PASS
TESTS::PASS   69 PASS / 4 KNOWN_EXPECTED_FAILURE / 0 FAIL   (73 au total)
```

Les suites des deux missions passent ensemble, y compris
`Anastasis.Terrain.HydrologyGradient` et `Anastasis.Terrain.SlopeShade`, qui
verrouillent le comportement d'hydrology-surface, et
`Anastasis.Terrain.MorphologyChannels` / `Anastasis.Terrain.Forge.CarriesMorphology`,
qui verrouillent celui du sol.

Preuve visuelle : `docs/visual/ground-001`, couple `K`/`L`.

## Réserve

`agent/hydrology-surface` n'était pas intégrée quand ceci a été écrit. Son commit
`92e5f5f` est **repris tel quel** dans l'historique de cette branche, avec sa paternité.
Si cette voie continue de travailler, ses commits suivants se rebasent normalement
par-dessus. Si l'un des arbitrages ci-dessus lui déplaît, chacun est une constante
nommée ou un paramètre d'instance : ils se défont sans rien démonter.
