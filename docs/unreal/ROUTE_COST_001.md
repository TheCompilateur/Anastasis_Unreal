# ROUTE_COST_001 — Le terrain paie le temps des trajets PNJ

## Contrat de jeu

L'A* de `AnastasisSim` lit déjà `FNavGrid::MoveCost` pour choisir ses cases.
`anastasis.Village.RouteCost=1` (défaut dans l'hôte Unreal) fait dépenser ce
même multiplicateur au budget de marche des PNJ. Pour un segment de longueur
`d`, le budget consomme `d × MoveCost[case du prochain waypoint]`. Avec une
vitesse de base identique, la route (`0,65`) se franchit plus vite que l'herbe
(`1`) et l'herbe humide de type marais (`2,4`) plus lentement. La météo garde
son facteur de vitesse existant.

La décision est bornée : aucun nouveau graphe, aucune création de route,
aucun tirage aléatoire, aucun changement du calcul A* ou des stocks. Une
livraison arrive plus tôt ou plus tard parce que le fermier a réellement passé
plus ou moins de temps à marcher. La boucle récolte → sac → grenier doit
conserver exactement la nourriture.

`anastasis.Village.RouteCost=0` reprend le temps uniforme de la référence JS.
Le village C++ est désactivé par défaut ; l'hôte Unreal active la règle en PIE
et lors des sauts de temps. L'écart est inscrit sous n° 25 dans `ECARTS.md`.

## Preuve et limites

`Anastasis.Sim.Village.Recolte.RouteCostDelivery` compare quatre mondes
identiques sauf le multiplicateur de terrain : route, herbe, herbe humide,
et témoin herbe en mode référence. La preuve doit retrouver une livraison
conservée dans chaque monde et l'ordre des temps `route < herbe < humide`.
Le lot d'intégration rejoue aussi `gather-deliver-pie`, avec le coût actif par
défaut, pour vérifier la récolte, le retour, deux livraisons et la conservation.

Ce coût provient de la **grille sémantique** du simulateur. Il n'intègre pas
encore la pente ni les collisions du maillage `ExperimentalTerrain`, la foule,
les charges portées ou le temps de trajet du joueur incarné. Une route visuelle
sans tuile de route sémantique ne reçoit pas `0,65`. La preuve de navigation
joueur et l'effet économique sur plusieurs jours restent distincts.
