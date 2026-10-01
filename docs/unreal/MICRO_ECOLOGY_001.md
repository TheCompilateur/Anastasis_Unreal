# MICRO_ECOLOGY_001 — berges, prairie, lisière, sous-bois

Mission `micro-ecologie-001`. Worktree `C:\dev\ANASTASIS_WORKTREES\micro-ecologie-001`,
branche `agent/micro-ecologie-001`, base `main` à `e70871b`.

La topographie ne bouge pas. Cette passe ajoute la microstructure qui manquait entre
les masses déjà en place : des poches, pas un ruban, pas un tapis, pas un cercle.

## Audit (ce qui existait déjà)

Il n'y a pas, dans le code ni dans les scripts, de Landscape Unreal, de Runtime Virtual
Texture, de graphe PCG, de Foliage Type, de Grass Type, ni de Procedural Foliage. Le
placement est une grammaire C++ déterministe, incarnée en HISM.

| | EXISTING | REUSABLE ici | MISSING |
|---|---|---|---|
| Sol | `M_AnastasisGround` / `M_AnastasisSlice`, couleurs de sommets Rock / Litter / Worked / Wetness (`AnastasisTerrainSurface`, drainage) | lecture seule ; teinte de poche par-dessus, sans remplacer le matériau | mousse, gravier dédié, décal de boue |
| Herbe | `AnastasisGroundCover` : haute, basse, laîche, touffe d'éboulis, callune, en taches | laissée en place ; elle porte déjà la hauteur d'herbe | fleurs, fougères, joncs de rive (H4), herbacées de sous-bois (H5) |
| Forêt | `AnastasisEcologicalDressing` : masses, clairières, espacement. En mode macro, pas de strate jeune | couronnes déjà posées, lues, pas réécrites | — |
| Lieux | `AnastasisPlaces` : source, marais, vieux bois… | meshes Ecotone et Rock | distribution continue (les lieux restent des compositions) |
| Meshes réutilisés | `SM_Rock_Low`, `SM_Ecotone_Reed`, `ShoreTuft`, `Driftwood`, `BranchPile`, `Bush_Low`, `Sapling`, `FallenLog`, `Stump`, `ExposedRoots` | oui, par `AnastasisPlaces::MeshPath` | jeunes arbres distincts des gaulis, fleurs, mousse |
| Eau / relief | forge, drainage, réseau | échantillonnés | micro-relief (non fait : conflit avec la forge) |
| PNJ, ciel, atmosphère | ailleurs | — | hors mandat, non touchés |

## Ce que la passe ajoute

`AnastasisMicroEcology` décide un plan pur (`Build`), puis des HISM (`Embody`) et une
teinte de sommet (`ApplySoil`). CVar `anastasis.Dressing.MicroEcology` (défaut 1) et
`anastasis.MicroEcology.Soil` (défaut 1). Coupée pendant l'automatisation, comme l'herbe.

Règles, toutes lues sur le sol rendu :

- **Berge.** Entre 8 et 90 cm au-dessus de la nappe, ou jusqu'à 2,2 m si l'humidité de
  rive dépasse 0,45. Le bruit (16 m, cassé à ~4 m) choisit un état : propre, rocheux,
  boueux, végétalisé, bois flotté. L'état change le long de la rive et dans sa largeur.
  Propre = rien. Rocheux = galets (`Rock_Low`, échelle 0,22–0,52). Boueux = quelques
  touffes de rive, le sol porte la vase. Végétalisé = roseaux et touffes, en paquets.
  Bois flotté = dérive et fagots. Au-delà de 36° la berge ne garde que la roche ; au-delà
  de 48° elle ne pose plus rien (le lithos garde les falaises).
- **Prairie.** Au-delà de 2,55 rayons de couronne, pente ≤ 18°, espace ouvert. La plupart
  des cellules restent vides (l'herbe existante suffit). Des taches : sol nu, sec, pierres
  et buissons rares, creux humide (teinte seule — la laîche existe déjà).
- **Lisière.** Anneaux 1,12–2,28 rayons, seulement là où un bruit de 9 m dépasse 0,64 :
  des arcs, pas un cercle. Gaulis plus près du tronc, buissons plus loin.
- **Sous-bois.** 0,30–0,80 rayon. Des groupes de bois mort ou de gaulis, et des secteurs
  vides. Pas d'ajout massif d'arbres : la forêt macro reste la quantité.
- **Sol.** La couleur déjà écrite est tirée vers la vase, la roche, la terre nue, le sec
  ou l'humide, par poche. Végétalisé et bois flotté ne repeignent pas : les plantes portent
  l'état. Les sommets immergés (alpha) ne sont pas touchés.
- **Hameau.** Un lieu `Hamlet` reste une clairière vide.

## Performance

Garde-fou `MaxInstances` = 20 000, puis éclaircissage déterministe (le hash garde la
même proportion de chaque rôle). HISM par (rôle, variante, tuile de 160 m), sans
collision ni navigation. Galets, touffes et fagots sans ombre, coupés vers 42–48 m.
Buissons, gaulis et troncs coupés vers 70–78 m.

Mesure à l'incarnation du monde, ligne `ANASTASIS_MICRO_ECOLOGY` du 2026-10-01 :
`instances=19993` (`truncated=1`), `components=1196`, `missing_meshes=0`,
`tinted=8537`, `plan_ms=613`, `total_ms=956`. Répartition après éclaircissage :
galets 1124, roseaux 247, touffes 318, dérive 31, fagots 18, pierres de prairie 113,
buissons de prairie 18, buissons de lisière 4478, gaulis de lisière 3408, troncs 1958,
souches 1642, fagots de sous-bois 1811, racines 1585, gaulis de sous-bois 3242.
Le plafond est donc atteint : la lisière et le sous-bois portent l'essentiel, les
berges et la prairie restent présentes. Le coût GPU par frame n'a pas été capturé.
À côté du million de touffes d'herbe, 20 000 instances HISM restent une couche petite ;
les 1196 composants sont le point à surveiller si le monde s'élargit.

## Preuve

`BUILD::PASS`. `tools\unreal\report-tests.ps1 -Filter Anastasis.MicroEcology` :
PASS 7, KNOWN_EXPECTED_FAILURE 0, FAIL 0, lanceur exit 0.

`Anastasis.MicroEcology.Determinism`, `BankPockets` (rive : propre 5, rocheux 32,
boueux 25, végétalisé 18, dérive 5, désaccord intérieur/extérieur 0,39),
`MeadowClusters` (pierres 11, buissons 3, sol nu 74, sec 26, humide 1),
`ForestEdge` (buissons 14, gaulis 14, secteurs vides 2 / 6, sous-bois 10 dont 3 vides),
`SlopeAndClearing`, `SoilTint`, `Rejects`.

Pas de capture avant/après : le verdict visuel se fait en A/B sur
`anastasis.Dressing.MicroEcology` et `anastasis.MicroEcology.Soil`, avec les caméras
déjà prévues pour le relief et la rive. Le micro-relief (phase 6) n'est pas incarné.

## Fichiers volontairement non touchés

Atmosphère, ciel, brume, village, PNJ, `AnastasisEcologicalDressing` (forêt macro),
`AnastasisGroundCover` (règles d'herbe), `AnastasisTerrainForge` (relief),
`AnastasisPlaces` (compositions), matériau `M_AnastasisGround`, carte `.umap`.

## Intégration

Le seul point chaud est `AnastasisWorldEmbodiment` : un appel après `PlaceGroundCover`,
les CVars, le tableau de HISM. Rebase si `main` a bougé ce fichier. Ne pas fusionner en
même temps qu'une réécriture de l'incarnation.

La forêt macro ne pose toujours pas de jeune strate (`ELayer::Young` est ignoré quand
`bMacroForest` est vrai). Cette passe compense à la lisière par le gaulis existant. Si
la grammaire forestière apprend à poser ses jeunes arbres, baisser
`anastasis.Dressing.MicroEcology` le temps de voir si les gaulis se doublonnent.
