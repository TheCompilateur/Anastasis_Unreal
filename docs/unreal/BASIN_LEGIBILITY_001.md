# BASIN_LEGIBILITY_001 — le lit produit-il une vallée visible ?

## Question

Les six vues fournies par Alexandre montrent un réseau d'eau, mais souvent peu de relief
qui permette de lire son bassin versant. Depuis ces vues, `WATER_NETWORK_001` a rendu
cohérentes les eaux de la simulation et du rendu sur la graine 12345. Ce travail teste
une hypothèse plus étroite : **le relief latéral des rivières principales est-il trop
faible pour se lire au-delà de la berge ?** Il ne déduit pas la réponse d'une ancienne
capture et ne change pas l'hydrologie.

La géographie de Trabzon comporte des bassins à crue et érosion sensibles
([Yıldırım 2021](https://dergipark.org.tr/tr/pub/barofd/article/894180)). Les vallées
intérieures peuvent être plus sèches que le versant maritime
([Atalay & Efe 2010](https://jeb.co.in/journal_issues/201001_jan10/paper_07.pdf)).
Ces travaux modernes motivent l'examen du relief et de ses gradients. Ils ne donnent
pas une coupe topographique chiffrée de notre vallée fictive en 1204.

## Instrument

`Anastasis.Terrain.BasinLegibility.Profile` exécute la recette canonique de la carte
entière, graine 12345, échelle 5, Human Geography et WaterLook actifs. Il contrôle
que le nombre de rivières et de lacs égale celui de `AnastasisCanonicalGeography`.
Pour chaque rivière d'ordre Strahler ≥ 2 et de longueur ≥ 250 m, il échantillonne
un point sur quatre, puis le terrain rendu à gauche et à droite, à 30, 100 et 200 m
du lit. La mesure est `min(hauteur des deux côtés) - niveau d'eau`, en mètres :
une seule rive haute ne suffit pas à fabriquer une vallée encaissée. Les profils hors
carte et ceux qui traversent une autre eau sont comptés à part.

Sortie : effectifs, médiane et déciles du relief à chaque rayon. Le test ne passe
que si le monde est calculable et si chaque rayon fournit au moins dix coupes sèches.
Ce PASS valide **l'instrument**, pas la scène, le joueur ou l'historicité.

## Décision définie avant mesure

- **Hypothèse géométrique à poursuivre** si la médiane du relief à 100 m est
  inférieure à 2 m **et** celle à 200 m inférieure à 5 m. Ces seuils sont des
  heuristiques de discrimination visuelle à l'échelle de la carte, pas des normes
  géomorphologiques pontiques.
- **Écarter un simple approfondissement du lit** si le relief à 100 et 200 m est
  déjà présent : mesurer alors la lisibilité des rives, du couvert et des matériaux
  sur les mêmes poses. Un lit plus profond ne réparera pas une information cachée.
- Dans les deux cas, une action visuelle ne sera retenue qu'après A/B/A à carte,
  graine, caméra et lumière identiques, inspection des images et mesure au-delà
  de la variance de capture. Les débits, l'accès PNJ et le verdict joueur restent
  des preuves distinctes.

## Résultat

`tools/unreal/report-tests.ps1 -Filter 'Anastasis.Terrain.BasinLegibility.Profile'`
sur le worktree `basin-legibility-001`, base `4ff9c34a4` : **PASS instrumental,
1 test, 0 échec**. Le log brut est
`Saved/CanonicalVerification/report-tests.log` (2026-10-09 16:49 UTC).

| Rayon | Coupes sèches | Hors carte | Autre eau | Hausse p10 | Hausse p50 | Hausse p90 |
|---|---:|---:|---:|---:|---:|---:|
| 30 m | 138 | 1 | 29 | 0,38 m | 2,40 m | 9,71 m |
| 100 m | 151 | 1 | 16 | 1,17 m | 4,58 m | 29,16 m |
| 200 m | 137 | 9 | 22 | 1,18 m | 5,03 m | 29,43 m |

Le test a échantillonné 168 points le long de cinq rivières d'ordre ≥ 2. La règle
préinscrite (médianes < 2 m à 100 m **et** < 5 m à 200 m) n'est pas atteinte.
**REJECT : approfondir globalement les lits ou amplifier tout le relief.** Le réseau
possède déjà du relief transversal ; une telle retouche risquerait de déplacer l'eau
canonique, le site d'ouverture et les accès pour traiter un défaut qui n'est pas
établi comme général.

Le décile bas reste peu marqué : 1,17–1,18 m à 100–200 m. Cela pointe des tronçons
locaux à examiner avec leur rive, leurs matériaux et leur couvert. Les médianes par
rayon ne sont pas des coupes appariées, donc leur faible différence entre 100 et
200 m n'est **pas** une preuve que les versants plafonnent exactement à 5 m.
L'instrument ne mesure ni silhouette à l'écran, ni contraste de matériau, ni
perception du joueur. La prochaine preuve discriminante est une capture actuelle
du tronçon plat identifié autour de `(1063 m, 1340 m)` et d'un témoin encaissé,
à même hauteur de caméra et même lumière, puis lecture des matériaux et du couvert.
