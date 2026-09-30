# HYDRO_NETWORK_001 — réseau de drainage

Faire lire la carte comme un bassin versant : hautes terres → ruisseaux de tête →
affluents → rivière principale → lac / exutoire. Structure hydrologique et intégration
au relief ; ni shader d'eau, ni écume, ni cascades, ni Niagara.

## Audit : ce qui existe réellement

La consigne supposait le Water System natif (WaterBodyRiver, Landscape Edit Layers,
Landscape Patch). **Aucun des trois n'existe dans ce projet.**

| Élément | Constat |
|---|---|
| Landscape | aucun. Le sol est un `ProceduralMeshComponent` (`ExperimentalTerrain`, section 0 sol, section 1 nappe) rebâti à chaque `EmbodyCanonical` |
| WaterBody / WaterZone / splines de rivière | aucun ; `Water` et `LandscapePatch` sont installés avec le moteur, **non activés** dans le `.uproject` |
| PCG | non utilisé |
| Origine des rivières | `AnastasisSim` (`AnastasisHydrology.cpp`), portage de `hydrology.js`, verrouillé par `Anastasis.Sim.Parite.Monde` |
| Rendu de l'eau | une nappe **plate** à `WaterPlaneZ` ; seules les trois rivières écrites de Human_Geography_V2 descendent |
| Travail antérieur | `agent/hydra-forge-001` (14/09, jamais commité) : rubans décoratifs posés sur la nappe plate, sans toucher la structure |

Le carving des WaterBody n'écrit que dans un Landscape : l'adopter imposait de remplacer
la génération du terrain, ce que la consigne interdit. Choix d'Alexandre (session du
2026-09-30) : **couche de présentation WorldView, sans plugin.**

### Pourquoi le réseau paraissait artificiel

`CarveChannels` abaisse chaque cellule de chenal sous `SeaLevel − Dig`, quelle que soit son
altitude naturelle ; `StampLakeBasins` fait de même pour les lacs. Mesure sur le monde de
référence (seed 12345, 96×96, référence JS) :

- 944 des 1 192 tuiles d'eau étaient de la **terre** avant l'hydrologie ;
- entaille médiane 0.185 (p90 0.249, max 0.362) : tout le relief médian au-dessus de la mer ;
- 27 plans d'eau disjoints, la plupart sans exutoire ;
- chenaux rectilignes : le priority-flood remplit le bassin intérieur (cuvette cernée d'un
  rempart) en un plat, et le routage y trace des droites ;
- trois largeurs possibles (1, 3, 5 tuiles).

Sur le maillage rendu s'ajoute un halo : TERRAIN_FORGE écrase le relief près de l'eau de
simulation (`LocalExag × lerp(1, 0.42, ShoreBlend)`), d'où des cuvettes rectangulaires
autour de chaque tranchée.

## Ce que fait `AnastasisDrainage`

`Source/Anastasis_UnrealV2/WorldView/AnastasisDrainage.{h,cpp}`, appelé par
`AAnastasisWorldEmbodiment` juste après TERRAIN_FORGE (mode 2 seulement : la tranche
scellée du mode 1 reste bit à bit celle de WORLD_SLICE_006). Travaille sur la grille fine
réellement rendue (381×381 sommets, pas de 5 m à l'échelle 5).

1. **Classement de l'eau existante.** Mers de bord de monde gardées ; lac écrit de HG gardé
   (niveau et fond d'origine) ; lac intérieur retenu si rayon inscrit ≥ 40 m et aire ≥
   1.5 ha, réduit à son cœur (ouverture morphologique : sans les bras de tranchée).
2. **Réparation.** Le halo d'écrasement est défait analytiquement autour de l'eau non gardée
   (pondéré par la vallée écrite de HG), puis toute eau non gardée, élargie de 25 m, est
   remplie harmoniquement depuis ses berges. Couture lissée sur trois sommets.
3. **Routage.** Priority-flood (Barnes) + D8 sur le relief réparé ; lacs à niveau ; rivières
   écrites de HG « brûlées » (−3 m) pour que le réseau s'y raccorde. Accumulation d'aire.
4. **Réseau clairsemé.** Seuil d'initiation 4 ha, relevé jusqu'à 16 têtes ; les têtes ne
   naissent qu'à 100 m du bord (le versant extérieur du rempart ne draine rien) ; affluents
   de moins de 220 m élagués ; un lac n'a qu'un exutoire ; tout lac intérieur déborde vers
   le réseau. Décomposition en rivières (tronc = branche de plus grande aire), ordre de
   Strahler.
5. **Profil en long.** Surface d'eau = relief réparé − revanche (0.6 m en plaine → 2.4 m en
   versant), plafonnée par l'amont, relevée au niveau d'un lac récepteur ; les points écrits
   prennent le profil de HG. Pas une surface remplie : dans une cuvette close, elle serait
   plate jusqu'au col et l'eau déborderait au-dessus de la plaine ; ici la rivière suit le
   fond et franchit le seuil en s'y encaissant. Receveurs d'abord : un affluent
   finit au niveau de son récepteur, et aucune marche n'est tolérée — la surface ne peut être
   plus raide que 1.5 % en plaine, 10 % en versant ; une chute devient un bief encaissé.
6. **Géométrie hydraulique.** Largeur ∝ √(aire) (6 → 42 m, effilée sur les 80 premiers mètres
   d'une source), profondeur ∝ aire^0.35 (0.6 → 3 m), toutes deux non décroissantes vers
   l'aval ; vitesse de Manning (n = 0.035) sur la pente **physique** (pente rendue ÷
   exagération de la forge).
7. **Tracé.** Polylignes lissées ; l'affluent est lissé avec les premiers points de son
   récepteur (arrivée tangente) ; méandres à deux harmoniques (boucles asymétriques),
   amplitude ∝ largeur × platitude², longueur d'onde 12 × largeur, réduits s'ils montent sur
   le versant.
8. **Creusement.** Lit parabolique ; berge large et douce en plaine (≥ 3 × largeur), courte en
   colline ; levée pleine sur 10 m seulement, bornée à +0.6 m au-delà (pas de digue) ; lacs
   intérieurs en cuvette sous leur niveau, grève de 20 m ; au plus deux plaines d'inondation
   (biefs lents et ouverts, à l'écart du village et du point haut) avec deux mares latérales.
9. **Eau.** Nappe par sommet : niveau de la rivière (le plus bas des chenaux qui couvrent un
   lit : à une confluence le récepteur commande), du lac, de la mare ; toute eau non
   rattachée au réseau est asséchée. Couleurs et canaux du sol relus depuis la terre voisine
   sur la terre réparée ; humidité riveraine vers `UV1.y` ; vitesse vers `RiverFlow`.
10. **Végétation.** Hook optionnel `FRenderedHabitat::SampleRiparian` : la forêt macro
    s'éclaircit le long des rivières rendues (ripisylve), pas seulement près de l'eau de
    simulation.

Ce qui n'est **pas** touché : `Alt`, `Type`, `FlowAmt`, ressources, fertilité. Les tuiles
d'eau de simulation restent la vérité de gameplay.

## Commandes

| CVar | Effet |
|---|---|
| `anastasis.Terrain.Drainage 0/1` | 1 = réseau (défaut), 0 = eau d'avant, à l'octet ; appliqué à l'incarnation |
| `anastasis.Drainage.Debug 0..4` | lignes persistantes : largeur, profondeur, vitesse, ordre de Strahler (équivalent des *Visualize River Width / Depth / Velocity*) |
| `anastasis.Drainage.Dump <chemin>` | écrit le réseau en JSON (et `<chemin>.fields.json` : relief réparé, routage, aire, directions) |

Journal : `ANASTASIS_DRAINAGE` (résumé, contrôles, une ligne par rivière et par lac).

Preuve : `tools\unreal\hydro-network-capture.ps1 -Label <l> -States "0,1" [-Debug 1..4]`.
`r.Water.WaterMesh.ShowWireframe` n'existe pas ici (pas de Water Mesh).

## Mesures

Seed 12345, monde entier, échelle 5, HG actif. Relevé par `hydro-network-capture.ps1`
(journal `ANASTASIS_DRAINAGE` + grilles exportées), mêmes caméras avant / après.

| | avant (`Drainage 0`) | après (`Drainage 1`) |
|---|---|---|
| sommets immergés | 16 801 | 13 841 |
| dont au niveau de la mer (275 uu) | 14 948 | 6 301 (mers de bord + lac écrit) |
| dont au-dessus de la mer | 1 415 | 6 842 |
| plans d'eau disjoints (8-connexes) | 31 | 6 |
| niveaux d'eau | 1 plan + 3 rivières écrites | un profil par rivière, 4 lacs d'altitude relevés |

Réseau : 14 rivières, 9 têtes, 8 confluences, ordre de Strahler max 3, 5.7 km de cours
d'eau, largeur 4.5 → 42 m, 7 plans d'eau (2 mers de bord, le lac écrit, 4 lacs d'altitude
relevés de 14 à 25 m), 1 plaine d'inondation, 5 092 sommets de tranchée rendus à la terre.
Couche : 450–850 ms par incarnation.

Contrôles (`AnastasisDrainage::Check`, mêmes chiffres dans le journal et les tests) :

```
uphill=0 narrowing=0 confluence_narrower=0 dangling_mouths=0 isolated_water=0
lakes_without_role=0 bank_containment=0.947          (HG coupé : 0.982)
```

Chaîne principale : rivière 1 (écrite, 995 m, 5.5 → 35.5 m, profondeur 0.2 → 2.1 m) reçoit
quatre affluents et se jette dans le lac écrit ; l'exutoire (rivière 0, ordre 3, 38 → 42 m,
2.2 → 2.4 m) sort par la brèche nord. Vallée B : deux branches se joignent et descendent le
ruisseau écrit jusqu'à la mer sud-ouest. Les quatre lacs d'altitude se déversent tous dans
le réseau (un exutoire chacun).

Vitesses : 0.1–0.2 m/s dans la plaine, 1–2 m/s sur les versants, jusqu'au plafond de
3.5 m/s en sortie de lac d'altitude et sur la descente vers la mer sud-est.

## Limites connues

- **Rive en escalier.** La nappe partage la grille du sol (5 m) et le matériau de rive lit une
  profondeur par sommet bornée à 0 : le bord mouillé suit les triangles. Deux sorties, hors
  périmètre : profondeur signée dans `FillShorelineChannels`, ou ruban d'eau par rivière
  (ce que génère un WaterBodyRiver).
- **Gameplay ≠ rendu.** Une tranchée réparée reste `Type = Water` en simulation ; une rivière
  nouvelle passe sur des tuiles de terre. Ramener l'hydrologie de simulation au même profil
  exige de modifier `AnastasisHydrology` **et** `hydrology.js` (parité).
- **Lac de HG.** Gardé tel quel, forme rectangulaire comprise : c'est une donnée écrite.
- **Seed.** Calibré sur 12345 ; `Anastasis.Terrain.Drainage.OriginalForms` couvre le relief
  sans HG. D'autres seeds ne sont pas mesurés.
