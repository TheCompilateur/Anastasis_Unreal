# Terrain access 001 : diagnostic des chemins et du relief

## Mandat et observation source

Le cout de navigation de `AnastasisNavGrid.cpp::TerrainMoveCostOf` utilise le type de
terrain et l'humidite, pas la pente du maillage rendu. Cela motive un audit ; cela ne
prouve pas qu'un habitant traverse actuellement un obstacle.

## Instrument

`terrain-access-pie` dans le registre du lot. Aucun editeur autonome.

1. PIE sur Lvl_AnastasisSlice, village initial vide, temps suspendu.
2. Survey existant : terrain du meme monde PIE, graine/dimensions verifiees,
   transformation identite exigee. Choix du site existant, scenario FirstFarmer,
   ajout d'une maison accessible et attribution au fermier.
3. Trois chemins demandes au vrai pathfinder avec son budget par defaut : seuil
   maison -> premier point d'eau semantiquement accessible (ordre par distance
   euclidienne), seuil maison -> champ, champ -> premier seuil du grenier.
   Il s'agit d'une fixture construite sur la carte reelle, pas de l'ouverture naturelle.
4. Export des sections visibles 0 (sol), 1 et 2 (eau) en metres, transformees en
   coordonnees monde, composant identifie et SHA256 avant/apres.
5. Le long des chemins : echantillons espaces d'au plus 0.5 m, altitude barycentrique
   et pente du triangle, pente entre echantillons, denivele positif/negatif echantillonne. Seuil diagnostic de pente 18 degres, repris de la politique
   SettlementSite::RouteSlope, pas une limite biomecanique universelle.
6. Observation autonome pendant 180 secondes simulees, TimeScale 0.5 : positions
   simulees, but, recolte/livraison/boisson, acteur visuel XYZ et visibilite. Aucun but force.
   Les segments entre observations ne sont examines que si meme PNJ, dehors,
   0 < delta t <= 0.1 s, deplacement <= 5 m. Sinon UNKNOWN.

## Resultats et limites

Sortie horodatee Saved/TerrainAccessEvidence : geometry.json (geometrie brute),
audit.json (chemins, points, anomalies avec coordonnees, observations), routes.svg
(superposition XY des trois chemins ; points rouges = drapeaux geometriques).

- UNKNOWN : couverture sol absente, chemin semantique absent, intervalle non exploitable.
- ANOMALY : pente >18 degres ou eau a moins de 10 cm sous le sol / au-dessus.
  Le dernier cas est water_or_low_freeboard, pas une affirmation de submersion.
  Un ruban d'eau enterre profondement sous la berge ne suffit pas a lever ce drapeau.
- NO_ANOMALY_AT_SAMPLES : aucun drapeau aux seuls echantillons, jamais preuve continue.

INSTRUMENT_PASS exige rapport produit, geometrie identique a la fin, trois entrees
pour les chemins (dont une peut etre UNKNOWN), au moins 10 m de mouvement simule
sur segments admissibles, 10 m de deplacement de l'acteur visuel visible et une
recolte observee. Il n'exige pas l'absence d'anomalies : les trouver est le but.
Un chemin absent reste explicitement UNKNOWN meme si l'instrument termine.

Les trois chemins planifies ne sont PAS declares parcourus : le trajet autonome
observe est conserve separement, avec son but. Actor XYZ fournit une observation de
presentation, pas une preuve de locomotion physique avec collision. Aucun PLY.
Le premier seuil du grenier peut etre inaccessible meme si un autre seuil fonctionne ;
le resultat ne condamne donc pas tout le batiment. L'eau est un point reconnu par
AtDrinkSpot, pas une preuve de boire effectivement a la rive rendue.

Surface superieure en projection XY : surplombs/cavites/triangles verticaux ne sont
pas resolus. Les obstacles plus fins que le pas peuvent etre manques. Aucun test
capsule, aucune correction de nav, aucun nivellement, aucun asset sauvegarde.
Les CVars modifiees sont restaurees en sortie. Etat runtime initial : QUEUED.

## Decision

Une anomalie echantillonnee devient un candidat avec coordonnees et trajet. Confirmer
son effet sur le mouvement observe avant toute correction. Une zone non mesuree
reste UNKNOWN. Ne pas corriger la navigation sur la seule absence de pente dans la
fonction de cout. En cas de changement de politique de simulation, declarer l'ecart
JS dans une mission suivante ; celle-ci ne modifie pas AnastasisSim.
