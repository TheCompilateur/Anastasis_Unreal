# GEO_MEASURE_001 — l'écart simulation ↔ rendu, mesuré

Chantier C2 d'IRON_CRUSADE_001, lancé par Alexandre le 2026-10-07. Il mesure l'écart et ne change rien
au jeu. Base : `main` = `f64aeac8`, worktree `geo-measure-001`.

Un seul éditeur a tourné, par `editor-batch.ps1`, lot du 2026-10-07 à 22:12 UTC
(`Saved/EditorBatch/20261007-181144/`).

```powershell
tools\unreal\editor-batch.ps1 -Proofs geography-concordance-pie,terrain-access-pie,river-use-pie,settlement-sensitivity-pie
```

| Instrument | Verdict de l'instrument | Durée |
|---|---|---|
| `geography-concordance-pie` | `INSTRUMENT_PASS` | 57 s |
| `terrain-access-pie` | `INSTRUMENT_PASS` | 429 s |
| `river-use-pie` | `FAIL route_crosses_steep_ground` : le **résultat** de l'expérience, pas un instrument cassé | 56 s |
| `settlement-sensitivity-pie` (nouveau) | `INSTRUMENT_PASS` (témoin reproductible) | 155 s |

## 1. L'eau : 833 cases sur 9 216 en désaccord

`geography-concordance-pie` classe le centre de chaque tuile de la carte de 96 × 96 (tuiles de 20 m) :

| Classe | Cases |
|---|---|
| sèche pour les deux | 7 754 |
| eau pour les deux | 629 |
| **eau pour la simulation seulement** | **569** |
| **eau pour le rendu seulement** | **264** |
| inconnue | 0 |

- **Désaccord** : 833 cases, 9,0 % de la carte, soit environ 33 ha. 47,5 % de l'eau de la simulation
  n'est pas de l'eau rendue (569 sur 1 198).
- **Stabilité** : chiffres identiques au run du lot du 2026-10-02 (`_integration-2`). L'écart est
  stable, pas un artefact de mesure.
- **Ce que ça veut dire en jeu** :
  - 569 cases bloquent la marche dans la simulation (eau infranchissable) alors qu'on les voit sèches ;
  - 264 cases rendues en eau sont franchissables à pied pour la simulation.

## 2. Le relief que les habitants traversent

`terrain-access-pie` porte sur le site d'ouverture : un fermier, une maison et un grenier, 180 s
simulées d'observation autonome. La limite de pente de l'instrument est de 18°.

**Trois chemins planifiés par le vrai A\*** :

| Chemin | Longueur | Pente max du triangle | Pente gravie max | Statut |
|---|---|---|---|---|
| maison → eau | 0 m (seuil déjà au point d'eau) | 26,2° | — | ANOMALY |
| maison → champ | 56,6 m | 30,7° | 14,7° | ANOMALY |
| champ → grenier | 76,6 m | 30,7° | **20,7°** | ANOMALY |

**Trajet autonome observé** : 10 800 segments, dont 4 856 inconnus (pas admissibles) et 5 944 mesurables :

| Mesure | Segments | Part des mesurables |
|---|---|---|
| sans anomalie aux échantillons | 3 376 | 57 % |
| **pente du triangle sous le pas > 18°** | 2 527 | 42,5 % |
| **pente réellement gravie > 18°** | 126 | 2 % |
| **dans l'eau rendue ou à moins de 10 cm d'elle** | 41 | 0,7 % |

Lecture : le relief rendu est rugueux sous les pas (×3,6, ravines, érosion), mais la pente vraiment
gravie le long du trajet reste rarement au-dessus de 18°. Le coût de marche de la simulation ignore la
pente (`AnastasisNavGrid.cpp`, `TerrainMoveCostOf` : type et humidité seulement). 41 segments font
marcher un habitant dans l'eau qu'on voit.

## 3. Boire à la rivière

`river-use-pie` : un habitant assoiffé, sans puits, près d'une rivière rendue.

- Il choisit `drink` sur la **rive de la simulation** : cible en (74,5 ; 37,5) tuiles, soit (1 490 ; 750) m.
- La berge rendue retenue par l'instrument est en (1 545 ; 832) m. **Écart : environ 99 m.**
- La route vers sa cible traverse un sol trop raide : verdict `route_crosses_steep_ground`. Au dernier
  échantillon, il n'a pas bu (soif 80,4, `at_drink_spot` faux).

Il va donc boire là où la simulation croit qu'est l'eau, pas là où le joueur la voit.

## 4. Le rendu choisit le village : F3 démontré

Expérience nouvelle, `settlement-sensitivity-pie` : quatre PIE dans un seul éditeur, même graine et même
carte, une seule CVar de **rendu** changée à la fois. Le terrain est reconstruit au `BeginPlay`
(`EmbodyFromConsoleVariables`). Le site d'ouverture est choisi par `AnastasisSettlementSurvey`, qui lit
ce maillage.

| État | Site (tuile) | Score | Éligibles | Désaccord sur l'eau (sim seule / rendu seul) |
|---|---|---|---|---|
| `ref` (défauts) | **(74 ; 36)** | 89,4 | 59 | 833 (569 / 264) |
| `anastasis.Terrain.Drainage 0` | **(50 ; 40)** — MOVED, ≈ 490 m | 93,3 | 101 | **204** (96 / 108) |
| `anastasis.Terrain.HumanGeography 0` | **(43 ; 23)** — MOVED, ≈ 670 m | 93,7 | 297 | 885 (561 / 324) |
| `ref2` (témoin) | **(74 ; 36)** | 89,4 | 59 | 833 (569 / 264) |

- **Démontré** : un réglage qui ne touche que le rendu déplace de 0,5 à 0,7 km le point de départ de la
  société simulée, donc toute sa trajectoire. Le témoin écarte le hasard : même site, même score,
  mêmes comptes.
- **Démontré aussi** : le réseau de drainage rendu (`HYDRO_NETWORK_001`) est la source principale du
  désaccord sur l'eau. Sans lui, celui-ci passe de 833 à 204 cases (−75 %).
- **Non démontré** :
  - l'effet d'une retouche **fine** du relief (une mission de berge, de sol) : seules deux CVars
    entières ont été basculées ;
  - l'effet sur la suite de la partie au-delà du site.

## 5. Ce que ces chiffres décident, et ce qu'ils ne décident pas

Le plan IRON (C2) proposait deux familles : **(a)** la simulation reste l'autorité et le rendu s'y
contraint ; **(b)** le relief rendu devient l'autorité. Les mesures penchent vers (a), et désignent où
agir en premier :

1. **Le site de départ ne doit pas dépendre des CVars de rendu** (chantier C3). Deux options :
   - le sondage lit une surface **dérivée de la simulation** ;
   - ou le choix du site passe dans la sim, avec ses propres données.

   C'est le défaut le plus grave mesuré ici : un agent qui règle une berge peut, sans le savoir,
   changer de village.
2. **Le drainage rendu ne doit pas assécher ni créer de l'eau à l'insu de la simulation.** C'est lui
   qui produit 75 % du désaccord. Deux voies possibles :
   - soit il ne s'écarte plus des tuiles d'eau de la sim (contrainte côté `WorldView`, parité intacte) ;
   - soit son réseau devient une donnée de la sim (écart déclaré, parité `hydrology.js` à rouvrir).

   Décision d'Alexandre.
3. **La pente** : la pente gravie dépasse rarement 18° (2 % des segments). Elle justifie un coût de
   pente dans la sim (écart à déclarer), mais n'est pas la priorité.
4. **Boire à la rivière** suit de 2 : tant que la rive de la sim est à 99 m de la berge rendue, aucun
   réglage de comportement ne rendra la scène crédible.

Sorties brutes (non versées, dans le worktree) :
- `Saved/GeographyConcordanceEvidence/opening-water.{json,svg}` ;
- `Saved/TerrainAccessEvidence/1791411229059495100/{audit,geometry}.json` et `routes.svg` ;
- `Saved/RiverUseEvidence/1791411657866687300/{journey,geometry}.json` ;
- `Saved/SettlementSensitivityEvidence/<horodatage>/sensitivity.json`.
