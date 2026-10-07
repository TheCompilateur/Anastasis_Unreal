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
