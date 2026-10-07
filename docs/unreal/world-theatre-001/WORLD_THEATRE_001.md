# WORLD_THEATRE_001 — mise en scène géographique (MEGALOGRAPHIA)

Mission du 2026-10-07. Le monde n'est pas une distribution d'assets : c'est une topologie, des vues, une
hiérarchie, des vides, un horizon. Cette mission **lit** le monde réellement exécuté, **mesure** ce qui le
fait lire comme une démo procédurale, puis pose une **couche additive** strictement indépendante, coupable
d'un geste pour un A/B propre.

Aucun système d'un autre propriétaire n'est modifié (sol, eau, végétation, villages, lumière, PNJ,
gameplay). Tout vit dans `Source/Anastasis_UnrealV2/WorldTheatre/`, `docs/unreal/world-theatre-001/` et
trois scripts `tools/unreal/world-theatre-*.py`.

## Primitives Unreal : ce qui s'applique ici, et ce qui ne s'applique pas

| Demandé | État du projet | Décision |
|---|---|---|
| Data Layer `DL_WORLD_THEATRE` | `Lvl_AnastasisSlice` n'est **pas** World Partition (aucun acteur externe) ; tout le monde est transitoire, rebâti par l'incarnation | Convertir la carte partagée serait une mutation d'un asset d'autrui. Équivalent : un sous-système transitoire, **un** acteur `AnastasisWorldTheatre` étiqueté et rangé dans la couche d'éditeur `DL_WORLD_THEATRE`, et la CVar `anastasis.Theatre 0/1` (à chaud) |
| HLOD / proxies | rien ne streame (pas de WP) | Les éléments lointains **sont** déjà des proxies : un maillage procédural par couche, sans LOD à perdre ; la composition ne peut pas disparaître au streaming |
| Landscape Edit Layer | pas de Landscape : `ProceduralMeshComponent` (`ExperimentalTerrain`, `HorizonTerrain`) | Toute retouche topographique ne peut être qu'une surcouche additive coupée par la même CVar ; **aucune n'est livrée** (voir « Exception terrain ») |
| Plugin `AnastasisWorldTheatre` | un plugin ne peut pas dépendre du module de jeu, et la couche doit lire `AAnastasisWorldEmbodiment` | Code dans un dossier possédé du module de jeu : `Source/Anastasis_UnrealV2/WorldTheatre/` |
| `Content/WorldTheatre/` | aucun asset créé à ce stade | La couche réutilise **en lecture** `M_AnastasisFarTerrain` (couleur de sommet = albédo) ; `/Game/WorldTheatre/M_WorldTheatreMass` est cherché d'abord, s'il existe un jour |
| PCG | non utilisé dans le projet | Inutile : la composition est un **plan** explicite, l'exécuteur le drape |

## Architecture

```
PIE (monde du jeu)                          hors moteur (secondes)                     PIE
anastasis.Theatre.Read  ──►  relevé  ──►  world-theatre-analyze.py  ──►  plan C++ versionné  ──►  UAnastasisWorldTheatreSubsystem
 (AnastasisWorldReading)      grilles      lecture perceptuelle,          AnastasisWorldTheatrePlan.inl   drape sur le sol rendu,
 sol + eau rendus,            20 m/200 m   diagnostic, vistas,             (généré, relu en revue)          refuse et journalise
 objets posés par famille     + CSV        composition, aperçu logiciel                                     ce qui n'y tient plus
```

- **La composition n'appartient à aucun tirage** : chaque masse et le repère portent dans le plan la raison
  mesurée de leur présence (`// masse 1 (… ha) : versant 74 %, creux 62 %…`). Le seul « bruit » est le grain
  des couronnes (40 m) et le ton de peuplement (160 m) : une matière, pas une forme.
- **L'exécuteur ne décide rien** : il drape. Si un autre agent change le relief, un élément qui tombe dans
  l'eau ou sur une pente trop forte est refusé et journalisé (`WORLD_THEATRE reject <id>: <raison>`).
- **Structures** : `FWorldPerceptualSample` (une cellule : altitude, pente, courbure, TPI 300 m / 2,5 km, eau,
  densités), `FHorizonSignature` (élévation et distance de l'horizon par azimut, plans de silhouette),
  `FWorldVista`, `FLandmarkRelation` vivent dans l'analyse (`world-theatre-analyze.py`), là où elles sont
  consommées. Le C++ ne porte que ce que l'exécuteur consomme : `FMass`, `FSilhouetteSpec`, `FTrace`, `FPlan`.

## Phase 1 — lecture du monde exécuté (relevé PIE du 2026-10-07, graine 12345)

- Carte jouable 1,9 km × 1,9 km, relief 70 m, pente p50 5,9°, p95 22,8° ; eau 9,6 % de la carte.
- Anneau d'horizon : relief 3 855 m, jusqu'à 60 km. 103 227 objets posés relevés (11 096 arbres, 27 227
  arbustes, 42 911 rochers, 18 130 roseaux, 416 ruines, 4 bâtiments), 1,2 million d'instances d'herbe ignorées.
- **Axes** : ceux du ciel du projet (`AnastasisAtmosphereResolver`) — X = nord, Y = est ; yaw 0 = nord, 90 = est.
- **Géographie réelle** (horizon mesuré dans toutes les directions, de partout dans la carte) : une **muraille
  au nord-est** à 10–12 km, 10–14° au-dessus de l'œil ; une seconde chaîne à l'est-sud-est à ~21 km ;
  **la vallée s'ouvre au sud-ouest / sud** sur 22–38 km (horizon à ~2°). Plusieurs bords, regard dehors, sont
  fermés à 100–600 m par une remontée juste hors de la carte.

## Phase 3 — diagnostic anti-génératif (mesuré, seuils posés avant la mesure)

| Signature | Mesure | Seuil | Verdict |
|---|---|---|---|
| Densité partout | plus grand disque vide **60 m** ; 0 % de la carte à plus de 60 m d'un objet | < 150 m | PRÉSENTE |
| Rochers uniformes | 42 911 rochers ; 0,7 % des quadrats de 160 m sans rocher | < 5 % | PRÉSENTE |
| Répétition de fréquence | 7 familles à variantes équitirées : `SM_Rock_Low` ×3 (37 510, max/min 1,013), `SM_Shrub_Broom` ×3 (1,001)… | max/min < 1,05 | PRÉSENTE |
| Repères concurrents | ruines dans 96 % des quadrats de 400 m ; 371 sur 416 du même maillage générique | > 90 % | PRÉSENTE |
| Forêt coupée au bord | 49,8 arbres/ha dans la bande intérieure de 200 m → **0** dehors | < 10 % | PRÉSENTE |
| Monde centré sur la carte | le sol monte avec le rayon dans **16 / 24** secteurs (40 m à 1 km → 773 m à 11,5 km) | ≥ 20 / 24 | partielle (sous le seuil) |
| Horizon accidentel | aucune vista canonique ne ferme à moins de 1 km | — | absente |

Lecture : l'image « démo procédurale » vient d'abord de **l'intérieur** de la carte (semis uniforme, aucun
vide, variantes en rotation, repères partout) et de sa **couture** (forêt coupée net). Ces quatre premières
signatures appartiennent aux systèmes de végétation et de lieux composés : **non corrigées ici**, transmises.
L'avant-pays, lui, est une cuvette aux pentes en anneau (4–8 km) : une règle purement topographique y
dessinerait une **couronne de forêt centrée sur la carte** — vérifié et rejeté à la première composition.

## Phase 2 — vistas canoniques

`docs/unreal/world-theatre-001/vistas.json` (9 vistas, `z` résolu sur le sol rendu) :

| Vista | Rôle |
|---|---|
| V1 village → chaîne | enclosure, cadrage de l'horizon |
| V2 village → ouverture | release, ouverture de la vallée (horizon 29 km) |
| V3 point haut → sud-est | full reveal |
| V4 bassin → sud-ouest | fond exposé, vide |
| V5 bord est → avant-pays | continuation au-delà de la carte (H3) |
| V6 approche du village | settlement approach, partial reveal |
| V7 forêt du bord ouest → dehors | enclosed forest → release |
| V8 vue générale | lecture macro (H4) |
| V9 village → bosse ouest-nord-ouest | landmark anchoring (repère refusé, voir Phase 4) |

## Phase 4 — la couche

Itération 1 (run 1) : 14 masses dont trois de « couture » contre les bords boisés. **Rejet** de la couture : à
400 m, une enveloppe de canopée se lit comme une bâche verte au sommet droit (V5) ; vue d'avion, elle suit le
bord du carré et le **souligne** (V8). La continuité proche relève de vraies instances (système végétation).

Itération 2 : masses au-delà de 2,5 km seulement, asymétrie voulue (pied de la chaîne au nord-est ; le cône
sud-ouest, où la vallée s'ouvre, reste **vide**), ton de peuplement, et **un seul repère humain** : une tour de
guet abandonnée, placée par calcul là où elle se découpe sur le ciel depuis le village et l'approche.

## Phase 5 — exception terrain (proposition, rien livré)

Deux constats relèvent du relief, propriétaire `AnastasisTerrainHorizon` / forge :
1. la remontée à 100–600 m hors de plusieurs bords ferme la vue dehors (mesure d'horizon par azimut) ;
2. la cuvette en anneau à 4–8 km (le sol monte avec le rayon dans 16 secteurs sur 24).
Proposition : orienter la remontée de l'anneau par la tectonique (ouvrir le sud-ouest au lieu de relever tous
les côtés), derrière une CVar du propriétaire. Aucune mutation n'est faite par cette mission.

## Phase 6 — architecture des distances

| Plan | Distance | Qui le porte |
|---|---|---|
| NEAR | < 60 m | végétation, sol, contact (autres propriétaires) |
| MID | 60 m – 1 km | la carte elle-même ; le repère à 1,4 km |
| FAR | 1 – 8 km | **la couche** : masses du pied de la chaîne (au-delà de 2,5 km), silhouette |
| EXTREME | > 8 km | anneau d'horizon et atmosphère (autres propriétaires) |

Rien ne streame : la couche est un seul `ProceduralMeshComponent` résident, la composition survit à tout
déplacement du joueur. Coût mesuré au chargement : 4,2 s (relevé + drapage) en PIE, 113 340 triangles (run 1).

## v2.1 — la lumière : le lointain cesse d'être la chose la plus claire (world-theatre-light-001)

**Constat mesuré, sur la profondeur vraie** (sonde `M_WorldTheatreDepthProbe`, validée contre la géométrie :
rapport médian 0,89–1,08 sur le terrain lointain, ciel concordant à 96–99 %). Avant v2.1, le lointain est plus
clair que le plan moyen : ×2,74 du village vers la chaîne (V1, horizon ×4,6), ×1,45 vers l'ouverture (V2).

**Moyen** : `M_WorldTheatreDistance`, post-traitement avant tonemapper, non borné, priorité basse, posé sur
l'acteur du théâtre. Au-delà de `Start` km, perte de valeur (`Darken`), de saturation (`Desaturate`) et de chaleur
(teinte 0,86 / 0,94 / 1,06) jusqu'à `Full` km. Le ciel ne suit que dans sa **bande basse** (0 à 12° au-dessus de
l'horizon, à 70 %) ; le zénith reste intact. Aucun réglage d'atmosphère d'un autre propriétaire n'est touché.
CVars `anastasis.Theatre.Light` (0 par défaut) et `anastasis.Theatre.Light.*` (Start, Full, Darken, Desaturate,
Cool, Sky, Horizon), à chaud.

**Itération** : le premier réglage laissait le ciel intact. À 19 h, la chaîne assombrie se découpait alors dans une
brume du soir restée claire (papier découpé). La bande basse du ciel suit désormais le lointain.

**Mesure, 11 h** (`--luminance`, horizon / plan moyen, luminance médiane ; run final, bande de ciel comprise) :

| Vista | avant | **défaut (0,45)** | fort (0,60) |
|---|---|---|---|
| V1 village → chaîne | 4,43 | **2,35** | 1,74 |
| V2 village → ouverture | 1,60 | **0,83** | 0,58 |
| V3 point haut | 1,05 | **0,53** | 0,36 |
| V5 bord est | 1,26 | **0,59** | 0,40 |
| V6 approche | 1,22 | **0,58** | 0,39 |

Ciel médian inchangé (0,182 → 0,183) ; témoin (masses2 / masses) identique à 0,001 près. Le plan moyen de V1 est fait
des arbres sombres de la carte : le rapport y reste > 1 ; l'image montre une chaîne passée du blanc craie au gris
ardoise. **19 h** : scène très sombre (luminances 0,003–0,02), rapports instables et une valeur incohérente
(V2 « défaut » plus sombre que « fort ») : aucun chiffre revendiqué au crépuscule, verdict à l'image seulement.

**Lecture** : en assombrissant, la couche **rend le relief** que la brume blanche effaçait (crêtes, ravines de la
muraille, V5 / V6). À 19 h, la chaîne sort de la brume comme une masse sombre ; sa base, la lisière des masses
forestières, se lit encore en frise dentelée. Effet de bord mesuré : la saturation du lointain monte légèrement
à 11 h (0,29 → 0,32 en V1), la teinte froide l'emportant sur la désaturation ; à l'œil, le lointain se lit ardoise.

**Pas encore fait (v2.1 → v2.2)** : variation dans le lointain (ombres de nuages, volumes de brouillard sombres par
secteur), nuit, et tout ce qui lie la menace à la simulation.

## v2.2 — la menace, lue dans la simulation (world-theatre-threat-001)

**Règle cardinale** : aucune menace n'est inventée (PONT-HIS-02). La seule source est le monde extérieur simulé
(geopolitical-world-001, `geo-pontos-1204.json`) : insécurité (raids, enlèvements, bandes) et troupes, qui voyagent
de nœud en nœud par les routes du scénario, et nouvelles qui voyagent deux fois plus vite. Sans scénario chargé
(`Anastasis.Geo.Load`), rien ne s'allume. Dans la carte, rien ne simule d'ennemi (`bDangerNear` toujours faux, le
« risque spatial » mesure la faim et la nuit) : la menace est donc lointaine par construction.

**Ce que l'œil peut voir** : seuls les voisins du village (un jour de route : Parcharia, Matzouka). Paipert et
Cheriana, à trois jours et plus, ne se voient pas ; leur danger se montre en arrivant chez un voisin.

| Signe | Source simulée | Où |
|---|---|---|
| Fumée, étape 0 | pression vraie au-dessus de sa base chez le voisin | 10 km, source cachée derrière une crête |
| Fumée, étapes 1 → 2 | pression en route vers le village, qui se rapproche | 5 km puis 2,75 km |
| Fumée proche + effroi | exposition du village (ce qui l'a atteint) | 2,75 km ; le lointain s'assombrit (v2.1) |
| Feux de signaux (fumée claire le jour) | **nouvelle** de raid en route vers le village, du plus loin au plus près | chaîne de 4 collines à portée de vue, 8,5 → 13 km |

Caps des voisins (**hypothèse de conception**, accordée à la vallée mesurée, X = nord) : Parcharia 108° (la seconde
chaîne à l'est-sud-est, ~21 km, 7°, des pâturages « au-dessus des vallées ») ; Matzouka 40° (derrière la muraille).

**1204, ce qui est porté et ce qui ne l'est pas** (brief `docs/historicity/briefs/world-theatre-threat-001.json`) :
la frontière d'été aux pâturages, le no man's land de Cheriana et la rupture de 1204 sont dans le scénario ; la
pression turkmène systématique **n'est pas attestée pour 1204-1225** (HIS-04 : surtout après 1277 ; HIS-06 : paix
relative Konya-Nicée) : la couche n'en pose aucune, et la preuve injecte un raid **sans acteur attribué**. Les feux
de signaux sont une **analogie** avec la chaîne byzantine du IXe siècle (HIS-07), jamais un fait local.

**Mesuré sur le scénario réel** (test `ThreatNewsBeforeRaid`, raid injecté à Paipert) : nouvelle en route vers le
village au jour 2, raid en route au jour 3, ressenti au village au jour 4. Les feux ont un jour d'avance.
