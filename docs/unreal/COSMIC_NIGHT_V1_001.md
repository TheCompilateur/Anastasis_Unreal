# COSMIC_NIGHT_V1_001 — une nuit qui a une forme

## Décision artistique

La V0 contient les bons éléments, mais la rivière est faible, les plans se lisent peu et les
événements peuvent passer inaperçus. La V1 donne au ciel une silhouette : coeur galactique,
bras sombres, baies de nuit noire. Le clair n'occupe pas tout le dôme. La lune ouvre autour
d'elle une poche sombre et un halo fin ; elle n'est plus un disque collé sur la texture.

La carte n'est pas éditée. Le même AAnastasisWorldAtmosphere transitoire reste propriétaire
du ciel en PIE, sur toute version de niveau qui l'emploie. L'horloge, la météo et le seed
continuent de décider quand le phénomène existe. La V1 change la présentation uniquement.

## Matière V1

- Source : ArtSource/Celestial/cosmic_river_v1.png, panorama 2:1 original généré avec
  imagegen, bords gauche/droit volontairement sombres pour la projection 360 degrés.
- Shader : ArtSource/Celestial/cosmic_sky_v1.hlsl ; la galaxie dessinée, étoiles lointaines
  et étoiles-guides sont trois échelles. Pas de bloom ou de LUT ajoutés.
- Lune : texture V0 conservée, mais halo, liseré et réserve sombre autour du disque.
- Étoiles filantes : tête, traîne fine et traîne diffuse distinctes ; durée ~2 s au rythme
  normal de 90 s/jour, contre ~1,1 s en V0.
- Voile rare : filament cyan et frange améthyste, lent et assez long pour être guetté.
- A/B direct : anastasis.Sky.CosmicArtVersion 0/1 ; 1 par défaut. L'asset V0 reste présent.
  anastasis.Sky.Cosmic 0 coupe tout le dôme comme auparavant.
- Forge : tools/unreal/cosmic-sky-material.ps1 -Version 1 -Rebuild crée
  T_CosmicRiverV1 et M_AnastasisCosmicSkyV1 sans écraser la V0.

## Treize motifs de nuit

Ces motifs forment la réserve artistique. La comète patiente (#3) est codée dans la
V1.1 ; les douze autres restent des directions. Le calendrier du monde et la météo
décident de leur apparition, sans toucher à la simulation.

| # | Nom | Signe visible | Rythme envisagé |
|---|---|---|---|
| 1 | La conjonction des aiguilles | Trois étoiles-guides s'alignent une heure | saisonnier |
| 2 | La couronne de cendre | Anneau froid et poussiéreux autour de la lune | après pluie humide |
| 3 | La comète patiente | Même chevelure se déplace sur plusieurs nuits | codée V1.1 : quatre nuits |
| 4 | La rivière obscurcie | Une faille de poussière traverse le coeur galactique | rare |
| 5 | La pluie transverse | Plusieurs météores partent d'un même radiant | nuit annoncée par quelques précurseurs |
| 6 | La nacre basse | L'horizon ouvre une bande iridescente entre les nuages | bref après l'orage |
| 7 | Le puits sans étoiles | Une zone ronde de la voûte perd ses points lumineux | très rare |
| 8 | La poussière de lune | De fins filaments apparaissent seulement près du halo lunaire | lune haute, air clair |
| 9 | Le pulsar lointain | Une étoile bleue émet deux battements très espacés | périodique, discret |
| 10 | L'arche rompue | La rivière céleste semble se séparer en deux bras | saisonnier |
| 11 | L'étoile témoin | Une unique étoile résiste dans une trouée de nuages | couvert variable |
| 12 | La marée froide | Une lente onde pâle traverse le ciel et se reflète dans l'eau | exceptionnel |
| 13 | La seconde ombre | Pendant quelques minutes, l'ombre lunaire change de direction | anomalie rarissime |

Le météore, le voile et la comète sont les trois formes mises en matière. Les douze autres
ne seront admises qu'une par une, avec une silhouette reconnaissable et un coût GPU mesuré.

## La comète patiente (V1.1)

Un début rare du calendrier donne quatre soirées consécutives avec la même comète. Elle
avance de cinq degrés par nuit par rapport aux étoiles, gagne en éclat puis décline. Son
noyau, sa queue ionique et sa poussière sont dessinés dans le matériau V1. La couverture
nuageuse ou le jour l'effacent à l'écran sans supprimer la date de l'événement. La CVar
`anastasis.Sky.CosmicEvent 3` permet de la prévisualiser sur une soirée épinglée ; `-1`
redonne le calendrier déterministe. Ce motif ne crée aucune ressource ni croyance de PNJ.

## Preuve et limite

Le script cosmic-night-pie vérifie le matériau V1, le retour V0 et les paramètres nocturnes,
pas l'émerveillement. Alexandre a dispensé les captures automatiques le 7 octobre.
La première inspection humaine à hauteur d'oeil devra juger quatre points : lisibilité
de la rivière, profondeur, composition lune/galaxie et distinction des nuits rares.
