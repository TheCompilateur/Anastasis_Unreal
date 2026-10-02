# Paysage anthropique — tranche 001

## Décision et portée

Première connexion implémentée : déplacement extérieur observé d'un PNJ → mémoire locale de présentation → herbe abaissée après répétition → récupération graduelle. Le terrain, le village, la navigation, les ressources et leurs sauvegardes restent les autorités existantes. Cette tranche ne ferme pas la boucle paysage modifié → décisions humaines.

État : EXPÉRIMENTAL, `anastasis.Anthropic.Memory 0` par défaut. Ne pas activer par défaut avant preuve PIE, A/B et mesure du coût. Pas de claim de victoire artistique.

## Observations du code et des images existantes

- `FVillage::Bind`, `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp` : Settlement initial = centre du monde. Le lancement crée son premier puits autour de ce centre. Ce n'est pas une sélection de site fondée sur une analyse du terrain rendu.
- `AnastasisNavGrid.cpp` : coûts par type de tuile et obstacles. La pente de TerrainForge n'entre pas dans cette grille. Un trajet correct dans la simulation peut contredire le relief de présentation.
- `AnastasisWorld.h` : eau, altitude sémantique, humidité, fertilité, forêt, champs, ressource et quantité existent. `Road` est un type reconnu ; cela ne fournit pas un compteur de passages.
- `FNpc` expose identité, X/Y, intérieur et chemin courant. Un chemin projeté n'est pas un passage effectué. Seuls les changements de position observés alimentent cette couche.
- `AnastasisTerrainSurface` expose Worked (champs), Wetness et familles de sol. La couche proposée ne repeint pas ces données.
- Les captures suivies `docs/visual/human-occupation-001/distance.png` et `ground.png` ont été regardées pendant cette mission : établissement proche de l'eau sur espace ouvert, mais sentier en rectangles clairs discontinus. Ce sont des images archivées d'un niveau séparé, pas une inspection du village actuel ni une preuve de notre changement.
- La fiche `human-occupation-001.md` décrit un site à (102000,109000) cm, berge à 36 m et forêt intérieure à 146 m. Valeurs rapportées par cette fiche, non remesurées ici. Son sentier est composé, sans connexion PNJ.

## Carte des interventions et frontières

| Sujet demandé | Appui réellement disponible | Décision dans cette tranche |
|---|---|---|
| Eau et accès | eau sémantique, puits, positions, drainage rendu | traces uniquement là où des déplacements extérieurs sont observés ; aucun gué inventé |
| Plat, sols, champs | altitudes, fertilité, Field, relief rendu | candidats inspectables, aucune nouvelle parcelle ni déplacement de village |
| Passages et corridors | positions, navigation et obstacles | mémoire des petits déplacements ; aucun segment déduit d'un grand intervalle |
| Lisière exploitée | forêt et ressources ; arbres décoratifs distincts | ne pas convertir une proximité en preuve de coupe ; aucune souche ajoutée |
| Pâturage | pas de troupeau raccordé à cette représentation | réserver le vocabulaire herbe haute/basse/sol exposé ; aucune exploitation attribuée |
| Gradient d'usage | répétition locale des positions observées | gradient de force à bord doux, non anneau concentrique autour du village |
| Mémoire humaine | observations depuis activation | atténuation des traces ; pas de mémoire ancienne préchargée ni persistance intersession |
| Retour écologique vers société | aucun contrat de retour de cette couche | hors tranche ; ne pas modifier coût de route ou fertilité |

## Documenté / plausible / spéculatif

DOCUMENTÉ (écologie générale, pas histoire locale) : les expériences de David N. Cole montrent une réponse au piétinement et une récupération qui varient avec la végétation. Sources primaires : [Cole 1995 I](https://research.fs.usda.gov/treesearch/24581), [Cole 1995 II](https://research.fs.usda.gov/treesearch/23582). Elles portent sur des milieux américains contemporains, pas sur une pratique pontique/byzantine.

PLAUSIBLE : une circulation répétée vers un accès réellement utilisé laisse une végétation plus basse ; un accès abandonné perd graduellement sa lisibilité. Cette traduction graphique simplifiée ne reconstitue pas une communauté végétale.

SPÉCULATIF : les réglages de ce prototype (seuil de 2 m cumulés dans une cellule, pleine force à 8 m, demi-vie de huit jours simulés) sont des paramètres artistiques, pas des résultats de Cole et pas une métrologie de pas humains. Aucun nombre de générations ni pratique historique locale n'est affirmé.

## Contrat de la couche

- Nouveau sous-système WorldView, aucune écriture dans `Source/AnastasisSim/`.
- Coordonnées : X/Y simulés × TileWorldSize × SpatialScale réellement incarné.
- Observations séparées par au plus 1/60 s simulée ; au-delà, compter un gap et reprendre depuis la position présente. Plus de 2 m par observation = saut refusé. Le temps accéléré peut donc ne rien dessiner : couverture partielle explicite.
- Ni immobilité, ni répétition d'une frame, ni intérieur, ni disparition/réapparition d'un identifiant ne créent une trace.
- Mémoire de distance cumulée, pas compteur exact de passages. Intersections segment/grille exactes ; noyau doux de 75 cm autour du point moyen observé.
- Au plus 4096 cellules observées, les 256 traces les plus fortes rendues et 8192 instances de végétation modifiées. Tout débordement apparaît dans le statut. Ce plafond borne une expérience locale ; ce n'est pas une mémoire territoriale exhaustive.
- Seuls les HISM `GroundCover_*` de l'incarnation sont raccourcis, au plus une fois par seconde. Aucun mesh ajouté, aucune collision changée, aucun asset enregistré.
- `off`, ResetCanonical et fin de monde restaurent les transforms encore possédés. Vérification par relecture. Un composant reconstruit ou un transform changé ailleurs ne reçoit pas une ancienne valeur.
- Le reset explicite du simulateur efface aussi cette mémoire, même si graine et heure sont identiques.
- API Python : `unreal.AnastasisAnthropicDebugLibrary.get_status(world)` ; cellules, instances, gaps, sauts, débordements, coût CPU de la dernière application, restaurations relues. `apply_ms` n'est pas le coût GPU ni un benchmark.

## Preuve et critère de décision

Tests ajoutés à `Anastasis.Anthropic.*` : immobilité, temps dupliqué, petits déplacements, téléportation, lacune temporelle, intérieur, disparition, recul d'horloge, répétition, récupération, saturation et conservation de distance sous subdivision. Exécution attendue au lot.

`anthropic-memory-pie` attend des déplacements réels dans Lvl_AnastasisSlice à vitesse normale, exige des instances d'herbe effectivement modifiées, coupe la couche puis exige mémoire vide et transforms restaurés relus. L'absence d'effet est FAIL, pas un succès de chargement. Ce test ne prouve pas une qualité visuelle.

KEEP seulement si : effet local lisible à 1,7 m et en oblique, continuité organique sans damier, réponse confinée aux trajets réellement observés, restauration correcte, coût acceptable mesuré dans le même cadrage et mêmes conditions. REJECT visuel si petites zones écrasées incohérentes, trous rectangulaires ou coût excessif ; laisser alors la CVar à 0.

Prochaine preuve discriminante : batch PIE puis A/B figé aux mêmes caméras près du puits réellement simulé (off / traces accumulées / off restauré), accompagnés d'un témoin. Il faut distinguer absence de trafic répété, absence d'herbe sur ce trajet et défaillance de rendu.

Les coupes, pâturages, champs choisis, vestiges, générations et la rétroaction écologique sont encore NON RÉALISÉS. Cette tranche n'est pas la croisade complète.
