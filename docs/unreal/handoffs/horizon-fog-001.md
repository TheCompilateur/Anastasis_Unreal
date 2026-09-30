# HANDOFF: horizon-fog-001

## MISSION

Étape 2 de horizon-ring-001 : régler la brume à l'échelle du monde de 1,9 km pour que
l'anneau lointain se fonde dans le ciel.

**Mission réduite en cours de route, sur décision d'Alexandre (2026-09-30)** : la mission
`env-realism-001` modifie déjà `AnastasisAtmosphereProfile`, `AnastasisWorldAtmosphere` et
leurs tests pour la même chose (perspective aérienne ×3, diffusion de Mie, albédo du sol,
nuages volumétriques, brume de vallée en seconde couche, lune). Deux versions concurrentes
du même réglage dans les mêmes fichiers = conflit assuré. Cette mission ne verse donc que
l'outil de mesure, et laisse l'atmosphère à env-realism-001.

Livré : `capture-horizon.ps1 -Atmosphere`. Dans l'éditeur, `Lvl_AnastasisSlice` montre
l'éclairage enregistré dans la map ; en PIE, le GameMode applique `DA_AnastasisAtmosphere`
(`Apply()`, puis `ApplyMist()` après l'incarnation). L'option reproduit cet ordre avant les
captures et journalise la brume et le SkyAtmosphere réellement en place (`HORIZON_FOG`,
`HORIZON_SKY`). Rien n'est sauvé.

## FILES_OWNED

- `tools/unreal/capture-horizon.ps1`, `tools/unreal/capture-horizon.py`

## COMMIT

PENDING

## MEC

- BUILD: voir `finish` (outil seul, aucun C++ touché)
- TESTS: voir `finish`
- COMMANDS:
  - `tools\unreal\capture-horizon.ps1 -Label pie-baseline -Atmosphere`

Mesure de référence sous l'atmosphère du jeu (profil actuel : densité 0,012, départ 15 m,
opacité max 0,85, perspective aérienne 1,0, 69 poches de brume), part de pixels « vide »,
anneau coupé (A) puis actif (B) :

| Vue | A | B |
|---|---|---|
| H1 30 m au-dessus du bassin | 0,6 % | 0,5 % |
| H2 15 m au-dessus du point haut | 18,5 % | 18,4 % |
| H3 au bord, regard dehors | 14,6 % | 0,0 % |
| H4 vue générale | 36,6 % | 1,8 % |
| H5 150 m au-dessus du bassin | 1,8 % | 0,1 % |

H2 : ombres de sous-bois comptées par le seuil quasi-noir, pas du vide (identique A/B).
La brume du profil cache déjà une partie du vide en A, pas assez : l'anneau reste
nécessaire.

## SCN

`Lvl_AnastasisSlice`, graine 12345, `EmbodyCanonical`, atmosphère du profil appliquée.

## PLY

NOT_IMPLEMENTED

## INTEGRATION_RISK

Aucun sur le code : deux fichiers d'outil, option désactivée par défaut.

Constaté en passant, à transmettre à qui tient l'atmosphère (env-realism-001 ou
ATMOSPHERE_002) : sous l'atmosphère du jeu, les 69 poches de brume de `ApplyMist()` se
voient comme des boules blanches opaques posées au sol — « nuages de coton » sur la vue
générale, voile blanc sur la moitié basse de la vue à 150 m. Réglées avant le passage à
l'échelle 5 (rayon = fraction de cellule, extinction max 0,65).

## STOP

- Brume, perspective aérienne, nuages : non touchés, domaine d'env-realism-001.
- Quand env-realism-001 est sur `main` : relancer `capture-horizon.ps1 -Atmosphere` et
  juger la jonction carte / anneau et le bleuissement de la chaîne lointaine.
