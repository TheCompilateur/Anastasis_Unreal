# Resultats — ecart n05 (tolerances v1, analyse 2026-10-01)

## endurance — principal : A = `aucun`, B = `n05` — N = 40, D = 3 j

**Verdict statistique (regle v2, Holm) : RUPTURE** — nature mesuree : **rupture**  
Regle v1 (declaree, sans correction) : RUPTURE / rupture — 30 grandeur(s) DIFFERENT, 0 INDETERMINE  
**Verdict au bit : DIVERGE** (40/40 paires ; premier tick divergent median 1, min 1, max 1)

Activation (B) : 8.1e+04 par passage en moyenne (min 81000). Cout d'un passage : A 8.81 s, B 9.67 s (medianes).
Decorrelation (sep. A/B au dernier jour / sep. temoin/temoin-bis) : 0.864 [IC95 0.821 ; 0.908], n = 40. 1re separation >= 1 case : median 3 s.

| t (s) | sep. positions A/B (cases, mediane) | sep. besoins A/B | sep. positions temoin/bis |
|---:|---:|---:|---:|
| 0.0 | 0 | 0 | 0 |
| 25.0 | 7.06 | 5.92 | 3.05 |
| 49.0 | 3.59 | 11.6 | 2.03 |
| 74.0 | 2.6 | 11.8 | 1.58 |
| 98.0 | 7.97 | 13.3 | 6.02 |
| 123.0 | 6.52 | 18.2 | 7.54 |
| 147.0 | 4.78 | 24.6 | 4.54 |
| 172.0 | 5.01 | 23.5 | 5.8 |
| 196.0 | 6.21 | 19.8 | 7.22 |
| 221.0 | 3.25 | 16.4 | 3.61 |
| 245.0 | 1.04 | 15 | 0.775 |
| 270.0 | 7.19 | 14.7 | 7.67 |

Criteres de rupture declenches : ampleur : changements_but_par_habitant_jour +3.94 > 3 x delta (1.26) ; ampleur : duree_episode.gatherFood -4.16 > 3 x delta (1.28) ; ampleur : duree_episode.gatherStone -7.34 > 3 x delta (2.01) ; ampleur : duree_episode.gatherWood -6.68 > 3 x delta (2.08) ; ampleur : duree_episode.helpFarm -35.3 > 3 x delta (9.27) ; ampleur : loisir_moy +10.1 > 3 x delta (3) ; ampleur : part_but.deliver +0.0959 > 3 x delta (0.03) ; ampleur : part_but.rest +0.102 > 3 x delta (0.03) ; ampleur : part_dedans +0.172 > 3 x delta (0.03) ; ampleur : social_moy +12.7 > 3 x delta (3) ; ampleur : stock_bati_moy +8.18 > 3 x delta (2.68)

| grandeur | echelle | A | B | diff | IC95 | IC90 | delta | p | p Holm | effet (sd) | v1 | v2 (Holm) |
|---|---|---:|---:|---:|---|---|---:|---:|---:|---:|---|---|
| `changements_but_par_habitant_jour` | comportement | 8.43 | 12.4 | 3.94 | [3.42 ; 4.48] | [3.5 ; 4.37] | 1.26 | 0.00025 | 0.011 | 4.4 | DIFFERENT | DIFFERENT |
| `duree_episode.build` | comportement | 5.16 | 3.98 | -1.18 | [-1.59 ; -0.772] | [-1.53 ; -0.839] | 1.03 | 0.00025 | 0.011 | -1.2 | DIFFERENT | DIFFERENT |
| `duree_episode.deliver` | comportement | 8.85 | 6.57 | -2.29 | [-3.06 ; -1.49] | [-2.94 ; -1.65] | 1.77 | 0.00025 | 0.011 | -1 | DIFFERENT | DIFFERENT |
| `duree_episode.eat` | comportement | 11.8 | 7.28 | -4.5 | [-6.35 ; -2.69] | [-6.07 ; -2.95] | 2.36 | 0.00025 | 0.011 | -0.99 | DIFFERENT | DIFFERENT |
| `duree_episode.gatherFood` | comportement | 6.39 | 2.24 | -4.16 | [-4.68 ; -3.65] | [-4.58 ; -3.73] | 1.28 | 0.00025 | 0.011 | -2.6 | DIFFERENT | DIFFERENT |
| `duree_episode.gatherStone` | comportement | 10.1 | 2.73 | -7.34 | [-8.24 ; -6.47] | [-8.1 ; -6.6] | 2.01 | 0.00025 | 0.011 | -2.6 | DIFFERENT | DIFFERENT |
| `duree_episode.gatherWood` | comportement | 10.4 | 3.7 | -6.68 | [-7.19 ; -6.16] | [-7.12 ; -6.26] | 2.08 | 0.00025 | 0.011 | -4.2 | DIFFERENT | DIFFERENT |
| `duree_episode.helpFarm` | comportement | 46.4 | 11.1 | -35.3 | [-39.2 ; -31.2] | [-38.6 ; -31.8] | 9.27 | 0.00025 | 0.011 | -3.4 | DIFFERENT | DIFFERENT |
| `duree_episode.rest` | comportement | 22.1 | 30 | 7.96 | [6.85 ; 9.1] | [7.02 ; 8.92] | 4.41 | 0.00025 | 0.011 | 3.4 | DIFFERENT | DIFFERENT |
| `duree_episode.sell` | comportement | 5.44 | 4.71 | -0.731 | [-0.933 ; -0.538] | [-0.898 ; -0.568] | 1.09 | 0.00025 | 0.011 | -1.3 | EQUIVALENT | EQUIVALENT |
| `duree_episode.socialize` | comportement | 9.49 | 9.57 | 0.0837 | [-0.788 ; 0.901] | [-0.647 ; 0.774] | 1.9 | 0.85 | 1 | 0.033 | EQUIVALENT | EQUIVALENT |
| `duree_episode_moy_s` | comportement | 10.4 | 7.17 | -3.22 | [-3.68 ; -2.77] | [-3.6 ; -2.84] | 1.56 | 0.00025 | 0.011 | -2.7 | DIFFERENT | DIFFERENT |
| `part_but.build` | comportement | 0.0284 | 0.0242 | -0.00426 | [-0.00717 ; -0.00141] | [-0.00669 ; -0.00181] | 0.03 | 0.007 | 0.063 | -0.66 | EQUIVALENT | EQUIVALENT |
| `part_but.deliver` | comportement | 0.147 | 0.243 | 0.0959 | [0.0827 ; 0.109] | [0.0851 ; 0.107] | 0.03 | 0.00025 | 0.011 | 2.8 | DIFFERENT | DIFFERENT |
| `part_but.eat` | comportement | 0.147 | 0.0874 | -0.0596 | [-0.0809 ; -0.0388] | [-0.0774 ; -0.0419] | 0.03 | 0.00025 | 0.011 | -1 | DIFFERENT | DIFFERENT |
| `part_but.gatherFood` | comportement | 0.0502 | 0.0331 | -0.0171 | [-0.0227 ; -0.0117] | [-0.0216 ; -0.0126] | 0.03 | 0.00025 | 0.011 | -1 | EQUIVALENT | EQUIVALENT |
| `part_but.gatherStone` | comportement | 0.0874 | 0.051 | -0.0364 | [-0.045 ; -0.0279] | [-0.0436 ; -0.0292] | 0.03 | 0.00025 | 0.011 | -1.6 | DIFFERENT | DIFFERENT |
| `part_but.gatherWood` | comportement | 0.0265 | 0.0161 | -0.0104 | [-0.0124 ; -0.00863] | [-0.012 ; -0.00894] | 0.03 | 0.00025 | 0.011 | -2.4 | EQUIVALENT | EQUIVALENT |
| `part_but.helpFarm` | comportement | 0.123 | 0.0357 | -0.0873 | [-0.0959 ; -0.0786] | [-0.0945 ; -0.08] | 0.03 | 0.00025 | 0.011 | -4.2 | DIFFERENT | DIFFERENT |
| `part_but.rest` | comportement | 0.232 | 0.334 | 0.102 | [0.0877 ; 0.117] | [0.0902 ; 0.114] | 0.03 | 0.00025 | 0.011 | 2.6 | DIFFERENT | DIFFERENT |
| `part_but.sell` | comportement | 0.0602 | 0.0579 | -0.00233 | [-0.00552 ; 0.00087] | [-0.00504 ; 0.000259] | 0.03 | 0.17 | 1 | -0.23 | EQUIVALENT | EQUIVALENT |
| `part_but.socialize` | comportement | 0.0583 | 0.0894 | 0.0311 | [0.0214 ; 0.0407] | [0.0228 ; 0.0391] | 0.03 | 0.00025 | 0.011 | 1.7 | DIFFERENT | DIFFERENT |
| `part_dedans` | comportement | 0.377 | 0.548 | 0.172 | [0.151 ; 0.192] | [0.154 ; 0.189] | 0.03 | 0.00025 | 0.011 | 3.6 | DIFFERENT | DIFFERENT |
| `part_observer` | comportement | 0.00667 | 0.0037 | -0.00296 | [-0.00296 ; -0.00296] | [-0.00296 ; -0.00296] | 0.02 | 0.00025 | 0.011 | inf | EQUIVALENT | EQUIVALENT |
| `batiments_fin` | monde | 3 | 3 | 0 | [0 ; 0] | [0 ; 0] | 0.5 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `bois_preleve_tuiles` | monde | -3.9e+03 | -3.89e+03 | 11.4 | [6.4 ; 19.8] | [6.83 ; 18.4] | 585 | 0.00025 | 0.011 | 0.49 | EQUIVALENT | EQUIVALENT |
| `energie_moy` | monde | 73.9 | 82.8 | 8.98 | [7.39 ; 10.6] | [7.64 ; 10.3] | 3 | 0.00025 | 0.011 | 2 | DIFFERENT | DIFFERENT |
| `faim_moy` | monde | 36.7 | 35.2 | -1.46 | [-2.5 ; -0.383] | [-2.35 ; -0.56] | 3 | 0.013 | 0.1 | -0.51 | EQUIVALENT | EQUIVALENT |
| `hygiene_moy` | monde | 67 | 69.5 | 2.55 | [1.32 ; 3.66] | [1.56 ; 3.49] | 3 | 0.00075 | 0.011 | 0.82 | DIFFERENT | DIFFERENT |
| `liens_positifs_fin` | monde | 12.5 | 12.6 | 0.05 | [-1.1 ; 1.15] | [-0.9 ; 1] | 2.5 | 1 | 1 | 0.019 | EQUIVALENT | EQUIVALENT |
| `liens_somme_fin` | monde | 214 | 301 | 87.2 | [43.4 ; 128] | [50.4 ; 122] | 42.7 | 0.00025 | 0.011 | 1 | DIFFERENT | DIFFERENT |
| `loisir_moy` | monde | 81.1 | 91.1 | 10.1 | [9.24 ; 10.9] | [9.35 ; 10.8] | 3 | 0.00025 | 0.011 | 4.2 | DIFFERENT | DIFFERENT |
| `moral_moy` | monde | 88 | 90.5 | 2.52 | [1.67 ; 3.38] | [1.81 ; 3.25] | 3 | 0.00025 | 0.011 | 1.1 | DIFFERENT | DIFFERENT |
| `nourriture_recoltee_tuiles` | monde | -1.44e+04 | -1.42e+04 | 199 | [154 ; 245] | [161 ; 236] | 1.44e+03 | 0.00025 | 0.011 | 1.6 | EQUIVALENT | EQUIVALENT |
| `oui_dire_fin` | monde | 0.35 | 1.43 | 1.07 | [0.625 ; 1.55] | [0.675 ; 1.48] | 1 | 0.00025 | 0.011 | 1.7 | DIFFERENT | DIFFERENT |
| `part_faim_forte` | monde | 0.0601 | 0.0151 | -0.045 | [-0.0541 ; -0.0359] | [-0.0527 ; -0.0374] | 0.03 | 0.00025 | 0.011 | -1.5 | DIFFERENT | DIFFERENT |
| `part_soif_forte` | monde | 0.135 | 0.0709 | -0.0638 | [-0.0973 ; -0.0289] | [-0.0923 ; -0.0345] | 0.03 | 0.00025 | 0.011 | -0.7 | DIFFERENT | DIFFERENT |
| `pierre_prelevee_tuiles` | monde | 57.8 | 82.7 | 24.9 | [20.3 ; 29.3] | [21 ; 28.5] | 8.67 | 0.00025 | 0.011 | 2.4 | DIFFERENT | DIFFERENT |
| `population_fin` | monde | 5 | 5 | 0 | [0 ; 0] | [0 ; 0] | 0.1 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `sante_min` | monde | 85.3 | 94.2 | 8.94 | [5.89 ; 12.1] | [6.32 ; 11.6] | 5 | 0.00025 | 0.011 | 0.84 | DIFFERENT | DIFFERENT |
| `social_moy` | monde | 71.1 | 83.8 | 12.7 | [10.2 ; 15.1] | [10.6 ; 14.8] | 3 | 0.00025 | 0.011 | 2.2 | DIFFERENT | DIFFERENT |
| `soif_moy` | monde | 40 | 33 | -6.99 | [-9.49 ; -4.43] | [-9.07 ; -4.84] | 3 | 0.00025 | 0.011 | -1.1 | DIFFERENT | DIFFERENT |
| `stock_bati_moy` | monde | 26.8 | 35 | 8.18 | [5.09 ; 11.1] | [5.58 ; 10.7] | 2.68 | 0.00025 | 0.011 | 1.1 | DIFFERENT | DIFFERENT |
| `survivants_initiaux` | monde | 5 | 5 | 0 | [0 ; 0] | [0 ; 0] | 0.1 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `tresor_fin` | monde | 12.2 | 12.1 | -0.025 | [-1.2 ; 1] | [-0.976 ; 0.875] | 5 | 1 | 1 | -0.0083 | EQUIVALENT | EQUIVALENT |

Prediction : nature predite **derivant**, mesuree **rupture** → **REFUTEE**
- `bit.tDivReduite` attendu <=1 — mesure 1 — TENU
- `micro.decorrelation` attendu >=0.8 — mesure 0.864 — TENU
- `changements_but_par_habitant_jour` attendu hausse — mesure 3.94 — TENU
- `faim_moy` attendu equivalent — mesure -1.46 — TENU
- `soif_moy` attendu equivalent — mesure -6.99 — NON TENU (non demontre ou contraire)
- `survivants_initiaux` attendu equivalent — mesure 0 — TENU
- `nourriture_recoltee_tuiles` attendu equivalent — mesure 199 — TENU

## endurance — controle : A = `vue_village`, B = `n05` — N = 40, D = 3 j

**Verdict statistique (regle v2, Holm) : INDETERMINE** — nature mesuree : **indeterminee (N insuffisant)**  
Regle v1 (declaree, sans correction) : INDETERMINE / indeterminee (N insuffisant) — 0 grandeur(s) DIFFERENT, 3 INDETERMINE  
**Verdict au bit : DIVERGE** (40/40 paires ; premier tick divergent median 1375, min 1218, max 1442)

Activation (B) : 8.1e+04 par passage en moyenne (min 81000). Cout d'un passage : A 9.66 s, B 9.67 s (medianes).
Decorrelation (sep. A/B au dernier jour / sep. temoin/temoin-bis) : 0.612 [IC95 0.567 ; 0.659], n = 40. 1re separation >= 1 case : median 80 s.

| t (s) | sep. positions A/B (cases, mediane) | sep. besoins A/B | sep. positions temoin/bis |
|---:|---:|---:|---:|
| 0.0 | 0 | 0 | 0 |
| 25.0 | 0 | 0.0114 | 3.05 |
| 49.0 | 0.198 | 1.32 | 2.03 |
| 74.0 | 0.235 | 0.293 | 1.58 |
| 98.0 | 3.99 | 1.97 | 6.02 |
| 123.0 | 2.39 | 7.86 | 7.54 |
| 147.0 | 2.13 | 9.55 | 4.54 |
| 172.0 | 3.11 | 11.8 | 5.8 |
| 196.0 | 4.17 | 11.5 | 7.22 |
| 221.0 | 2.25 | 9.37 | 3.61 |
| 245.0 | 0.857 | 8.84 | 0.775 |
| 270.0 | 6.13 | 10.1 | 7.67 |

| grandeur | echelle | A | B | diff | IC95 | IC90 | delta | p | p Holm | effet (sd) | v1 | v2 (Holm) |
|---|---|---:|---:|---:|---|---|---:|---:|---:|---:|---|---|
| `changements_but_par_habitant_jour` | comportement | 11.9 | 12.4 | 0.467 | [0.0283 ; 0.887] | [0.102 ; 0.808] | 1.79 | 0.036 | 1 | 0.33 | EQUIVALENT | EQUIVALENT |
| `duree_episode.build` | comportement | 3.83 | 3.98 | 0.146 | [-0.227 ; 0.528] | [-0.173 ; 0.465] | 0.767 | 0.47 | 1 | 0.17 | EQUIVALENT | EQUIVALENT |
| `duree_episode.deliver` | comportement | 7.12 | 6.57 | -0.554 | [-1.01 ; -0.0868] | [-0.929 ; -0.157] | 1.42 | 0.024 | 1 | -0.34 | EQUIVALENT | EQUIVALENT |
| `duree_episode.eat` | comportement | 8.06 | 7.28 | -0.785 | [-2.05 ; 0.416] | [-1.83 ; 0.236] | 1.61 | 0.22 | 1 | -0.2 | INDETERMINE | INDETERMINE |
| `duree_episode.gatherFood` | comportement | 2.23 | 2.24 | 0.00794 | [-0.116 ; 0.144] | [-0.0976 ; 0.12] | 0.446 | 0.91 | 1 | 0.024 | EQUIVALENT | EQUIVALENT |
| `duree_episode.gatherStone` | comportement | 2.89 | 2.73 | -0.161 | [-0.266 ; -0.0641] | [-0.246 ; -0.0785] | 0.578 | 0.0037 | 0.16 | -0.55 | EQUIVALENT | EQUIVALENT |
| `duree_episode.helpFarm` | comportement | 12.4 | 11.1 | -1.31 | [-4.25 ; 0.964] | [-3.75 ; 0.677] | 2.48 | 0.39 | 1 | -0.086 | INDETERMINE | INDETERMINE |
| `duree_episode.rest` | comportement | 29.6 | 30 | 0.469 | [-0.515 ; 1.41] | [-0.346 ; 1.26] | 5.91 | 0.35 | 1 | 0.18 | EQUIVALENT | EQUIVALENT |
| `duree_episode.sell` | comportement | 4.74 | 4.71 | -0.0362 | [-0.135 ; 0.0753] | [-0.124 ; 0.0566] | 0.948 | 0.52 | 1 | -0.11 | EQUIVALENT | EQUIVALENT |
| `duree_episode.socialize` | comportement | 9.54 | 9.57 | 0.0283 | [-0.712 ; 0.7] | [-0.567 ; 0.595] | 1.91 | 0.94 | 1 | 0.015 | EQUIVALENT | EQUIVALENT |
| `duree_episode_moy_s` | comportement | 7.45 | 7.17 | -0.287 | [-0.541 ; -0.0352] | [-0.497 ; -0.0751] | 1.12 | 0.03 | 1 | -0.32 | EQUIVALENT | EQUIVALENT |
| `part_but.build` | comportement | 0.0235 | 0.0242 | 0.000685 | [-0.00191 ; 0.00332] | [-0.00146 ; 0.00287] | 0.03 | 0.63 | 1 | 0.11 | EQUIVALENT | EQUIVALENT |
| `part_but.deliver` | comportement | 0.252 | 0.243 | -0.00867 | [-0.0202 ; 0.00304] | [-0.0182 ; 0.00158] | 0.03 | 0.16 | 1 | -0.27 | EQUIVALENT | EQUIVALENT |
| `part_but.eat` | comportement | 0.0946 | 0.0874 | -0.00728 | [-0.0214 ; 0.00593] | [-0.0189 ; 0.00376] | 0.03 | 0.31 | 1 | -0.18 | EQUIVALENT | EQUIVALENT |
| `part_but.gatherFood` | comportement | 0.0335 | 0.0331 | -0.000389 | [-0.00257 ; 0.00189] | [-0.00222 ; 0.00146] | 0.03 | 0.75 | 1 | -0.056 | EQUIVALENT | EQUIVALENT |
| `part_but.gatherStone` | comportement | 0.0487 | 0.051 | 0.00235 | [-0.0017 ; 0.0062] | [-0.000944 ; 0.0055] | 0.03 | 0.27 | 1 | 0.23 | EQUIVALENT | EQUIVALENT |
| `part_but.helpFarm` | comportement | 0.0359 | 0.0357 | -0.000222 | [-0.00378 ; 0.0033] | [-0.0032 ; 0.00274] | 0.03 | 0.91 | 1 | -0.0087 | EQUIVALENT | EQUIVALENT |
| `part_but.rest` | comportement | 0.33 | 0.334 | 0.00426 | [-0.00882 ; 0.0174] | [-0.00667 ; 0.015] | 0.03 | 0.52 | 1 | 0.13 | EQUIVALENT | EQUIVALENT |
| `part_but.sell` | comportement | 0.0574 | 0.0579 | 0.000444 | [-0.00146 ; 0.00233] | [-0.00117 ; 0.00206] | 0.03 | 0.67 | 1 | 0.092 | EQUIVALENT | EQUIVALENT |
| `part_but.socialize` | comportement | 0.082 | 0.0894 | 0.00744 | [-0.00356 ; 0.0184] | [-0.00191 ; 0.0164] | 0.03 | 0.19 | 1 | 0.25 | EQUIVALENT | EQUIVALENT |
| `part_dedans` | comportement | 0.564 | 0.548 | -0.016 | [-0.0314 ; -0.000185] | [-0.0287 ; -0.00292] | 0.03 | 0.056 | 1 | -0.3 | EQUIVALENT | EQUIVALENT |
| `part_observer` | comportement | 0.0037 | 0.0037 | 0 | [0 ; 0] | [0 ; 0] | 0.02 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `batiments_fin` | monde | 3 | 3 | 0 | [0 ; 0] | [0 ; 0] | 0.5 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `bois_preleve_tuiles` | monde | -3.89e+03 | -3.89e+03 | 1.62 | [-1.1 ; 4.42] | [-0.676 ; 4.03] | 583 | 0.27 | 1 | 0.19 | EQUIVALENT | EQUIVALENT |
| `energie_moy` | monde | 82 | 82.8 | 0.843 | [-0.291 ; 1.96] | [-0.0964 ; 1.76] | 3 | 0.16 | 1 | 0.26 | EQUIVALENT | EQUIVALENT |
| `faim_moy` | monde | 35.2 | 35.2 | -0.00463 | [-0.559 ; 0.56] | [-0.485 ; 0.459] | 3 | 0.99 | 1 | -0.0034 | EQUIVALENT | EQUIVALENT |
| `hygiene_moy` | monde | 68.5 | 69.5 | 0.982 | [0.157 ; 1.77] | [0.293 ; 1.64] | 3 | 0.021 | 0.89 | 0.37 | EQUIVALENT | EQUIVALENT |
| `liens_positifs_fin` | monde | 12.2 | 12.6 | 0.4 | [-0.65 ; 1.4] | [-0.5 ; 1.25] | 2.43 | 0.52 | 1 | 0.13 | EQUIVALENT | EQUIVALENT |
| `liens_somme_fin` | monde | 277 | 301 | 23.9 | [-8.88 ; 56.1] | [-3.15 ; 50.3] | 55.4 | 0.15 | 1 | 0.21 | EQUIVALENT | EQUIVALENT |
| `loisir_moy` | monde | 90.9 | 91.1 | 0.265 | [-0.192 ; 0.708] | [-0.112 ; 0.637] | 3 | 0.27 | 1 | 0.15 | EQUIVALENT | EQUIVALENT |
| `moral_moy` | monde | 90.2 | 90.5 | 0.358 | [0.026 ; 0.683] | [0.0755 ; 0.626] | 3 | 0.037 | 1 | 0.24 | EQUIVALENT | EQUIVALENT |
| `nourriture_recoltee_tuiles` | monde | -1.42e+04 | -1.42e+04 | -0.95 | [-17.8 ; 15.8] | [-15 ; 13.1] | 1.42e+03 | 0.92 | 1 | -0.0074 | EQUIVALENT | EQUIVALENT |
| `oui_dire_fin` | monde | 1.32 | 1.43 | 0.1 | [-0.475 ; 0.675] | [-0.4 ; 0.575] | 1 | 0.8 | 1 | 0.078 | EQUIVALENT | EQUIVALENT |
| `part_faim_forte` | monde | 0.0204 | 0.0151 | -0.0053 | [-0.0109 ; 5.56e-05] | [-0.01 ; -0.00074] | 0.03 | 0.065 | 1 | -0.33 | EQUIVALENT | EQUIVALENT |
| `part_soif_forte` | monde | 0.0899 | 0.0709 | -0.019 | [-0.0389 ; 0.00134] | [-0.0356 ; -0.00218] | 0.03 | 0.071 | 1 | -0.31 | INDETERMINE | INDETERMINE |
| `pierre_prelevee_tuiles` | monde | 80.6 | 82.7 | 2.08 | [-1.82 ; 5.9] | [-1.1 ; 5.2] | 12.1 | 0.3 | 1 | 0.24 | EQUIVALENT | EQUIVALENT |
| `population_fin` | monde | 5 | 5 | 0 | [0 ; 0] | [0 ; 0] | 0.1 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `sante_min` | monde | 94.3 | 94.2 | -0.0729 | [-1.46 ; 1.28] | [-1.22 ; 1.07] | 5 | 0.94 | 1 | -0.026 | EQUIVALENT | EQUIVALENT |
| `social_moy` | monde | 83.4 | 83.8 | 0.419 | [-0.772 ; 1.62] | [-0.604 ; 1.41] | 3 | 0.5 | 1 | 0.082 | EQUIVALENT | EQUIVALENT |
| `soif_moy` | monde | 34.7 | 33 | -1.63 | [-3.16 ; -0.018] | [-2.89 ; -0.315] | 3 | 0.045 | 1 | -0.34 | EQUIVALENT | EQUIVALENT |
| `stock_bati_moy` | monde | 34.8 | 35 | 0.194 | [-2.47 ; 2.87] | [-2.08 ; 2.49] | 3.48 | 0.89 | 1 | 0.03 | EQUIVALENT | EQUIVALENT |
| `survivants_initiaux` | monde | 5 | 5 | 0 | [0 ; 0] | [0 ; 0] | 0.1 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `tresor_fin` | monde | 11.8 | 12.1 | 0.35 | [-0.5 ; 1.25] | [-0.4 ; 1.1] | 5 | 0.52 | 1 | 0.19 | EQUIVALENT | EQUIVALENT |

Prediction : nature predite **chaotique**, mesuree **indeterminee (N insuffisant)** → **REFUTEE**
- `micro.decorrelation` attendu >=0.8 — mesure 0.612 — NON TENU (non demontre ou contraire)
- `changements_but_par_habitant_jour` attendu equivalent — mesure 0.467 — TENU
- `faim_moy` attendu equivalent — mesure -0.00463 — TENU

## endurance — calibration_AA:aucun/aucun@bis : A = `aucun`, B = `aucun@bis` — N = 40, D = 3 j

**Verdict statistique (regle v2, Holm) : DERIVE** — nature mesuree : **derivant**  
Regle v1 (declaree, sans correction) : DERIVE / derivant — 8 grandeur(s) DIFFERENT, 3 INDETERMINE  
**Verdict au bit : DIVERGE** (40/40 paires ; premier tick divergent median 0, min 0, max 0)

Activation (B) : 0 par passage en moyenne (min 0). Cout d'un passage : A 8.81 s, B 8.25 s (medianes).
Decorrelation (sep. A/B au dernier jour / sep. temoin/temoin-bis) : 1 [IC95 1 ; 1], n = 40. 1re separation >= 1 case : median 10 s.

| t (s) | sep. positions A/B (cases, mediane) | sep. besoins A/B | sep. positions temoin/bis |
|---:|---:|---:|---:|
| 0.0 | 0 | 0 | 0 |
| 25.0 | 3.05 | 0.128 | 3.05 |
| 49.0 | 2.03 | 2.47 | 2.03 |
| 74.0 | 1.58 | 4.12 | 1.58 |
| 98.0 | 6.02 | 5.74 | 6.02 |
| 123.0 | 7.54 | 9.77 | 7.54 |
| 147.0 | 4.54 | 20.5 | 4.54 |
| 172.0 | 5.8 | 21.9 | 5.8 |
| 196.0 | 7.22 | 17 | 7.22 |
| 221.0 | 3.61 | 14.7 | 3.61 |
| 245.0 | 0.775 | 14.2 | 0.775 |
| 270.0 | 7.67 | 13.1 | 7.67 |

| grandeur | echelle | A | B | diff | IC95 | IC90 | delta | p | p Holm | effet (sd) | v1 | v2 (Holm) |
|---|---|---:|---:|---:|---|---|---:|---:|---:|---:|---|---|
| `changements_but_par_habitant_jour` | comportement | 8.43 | 9.02 | 0.588 | [0.242 ; 0.94] | [0.302 ; 0.887] | 1.26 | 0.0035 | 0.13 | 0.65 | EQUIVALENT | EQUIVALENT |
| `duree_episode.build` | comportement | 5.16 | 5.06 | -0.107 | [-0.526 ; 0.335] | [-0.461 ; 0.262] | 1.03 | 0.63 | 1 | -0.1 | EQUIVALENT | EQUIVALENT |
| `duree_episode.deliver` | comportement | 8.85 | 7.99 | -0.867 | [-1.65 ; -0.091] | [-1.53 ; -0.212] | 1.77 | 0.042 | 1 | -0.39 | EQUIVALENT | EQUIVALENT |
| `duree_episode.eat` | comportement | 11.8 | 8.49 | -3.29 | [-4.89 ; -1.82] | [-4.6 ; -2.05] | 2.36 | 0.00025 | 0.011 | -0.72 | DIFFERENT | DIFFERENT |
| `duree_episode.gatherFood` | comportement | 6.39 | 6.88 | 0.482 | [-0.358 ; 1.39] | [-0.243 ; 1.22] | 1.28 | 0.28 | 1 | 0.3 | EQUIVALENT | EQUIVALENT |
| `duree_episode.gatherStone` | comportement | 10.1 | 9.28 | -0.787 | [-2.07 ; 0.466] | [-1.85 ; 0.246] | 2.01 | 0.22 | 1 | -0.27 | EQUIVALENT | EQUIVALENT |
| `duree_episode.gatherWood` | comportement | 10.4 | 10.7 | 0.325 | [-0.281 ; 0.936] | [-0.174 ; 0.838] | 2.08 | 0.31 | 1 | 0.21 | EQUIVALENT | EQUIVALENT |
| `duree_episode.helpFarm` | comportement | 46.4 | 47.4 | 1.05 | [-4.26 ; 6.59] | [-3.59 ; 5.61] | 9.27 | 0.71 | 1 | 0.1 | EQUIVALENT | EQUIVALENT |
| `duree_episode.rest` | comportement | 22.1 | 21.8 | -0.277 | [-1.18 ; 0.66] | [-1.03 ; 0.523] | 4.41 | 0.57 | 1 | -0.12 | EQUIVALENT | EQUIVALENT |
| `duree_episode.sell` | comportement | 5.44 | 5.41 | -0.025 | [-0.265 ; 0.228] | [-0.231 ; 0.186] | 1.09 | 0.84 | 1 | -0.043 | EQUIVALENT | EQUIVALENT |
| `duree_episode.socialize` | comportement | 9.49 | 9.41 | -0.0761 | [-1.07 ; 0.901] | [-0.921 ; 0.749] | 1.9 | 0.87 | 1 | -0.03 | EQUIVALENT | EQUIVALENT |
| `duree_episode_moy_s` | comportement | 10.4 | 9.68 | -0.703 | [-1.14 ; -0.296] | [-1.06 ; -0.356] | 1.56 | 0.0032 | 0.13 | -0.6 | EQUIVALENT | EQUIVALENT |
| `part_but.build` | comportement | 0.0284 | 0.0276 | -0.000796 | [-0.00354 ; 0.00189] | [-0.00315 ; 0.00143] | 0.03 | 0.57 | 1 | -0.12 | EQUIVALENT | EQUIVALENT |
| `part_but.deliver` | comportement | 0.147 | 0.149 | 0.00154 | [-0.00994 ; 0.0128] | [-0.00809 ; 0.011] | 0.03 | 0.81 | 1 | 0.045 | EQUIVALENT | EQUIVALENT |
| `part_but.eat` | comportement | 0.147 | 0.107 | -0.0395 | [-0.0584 ; -0.0214] | [-0.0554 ; -0.0244] | 0.03 | 0.00025 | 0.011 | -0.68 | DIFFERENT | DIFFERENT |
| `part_but.gatherFood` | comportement | 0.0502 | 0.0589 | 0.00863 | [0.00133 ; 0.0159] | [0.00261 ; 0.0148] | 0.03 | 0.033 | 1 | 0.52 | EQUIVALENT | EQUIVALENT |
| `part_but.gatherStone` | comportement | 0.0874 | 0.0903 | 0.00287 | [-0.00746 ; 0.0128] | [-0.00572 ; 0.0116] | 0.03 | 0.59 | 1 | 0.12 | EQUIVALENT | EQUIVALENT |
| `part_but.gatherWood` | comportement | 0.0265 | 0.0277 | 0.00119 | [-0.00063 ; 0.00287] | [-0.000333 ; 0.00263] | 0.03 | 0.2 | 1 | 0.27 | EQUIVALENT | EQUIVALENT |
| `part_but.helpFarm` | comportement | 0.123 | 0.121 | -0.00189 | [-0.0126 ; 0.00963] | [-0.0109 ; 0.00763] | 0.03 | 0.74 | 1 | -0.092 | EQUIVALENT | EQUIVALENT |
| `part_but.rest` | comportement | 0.232 | 0.252 | 0.0205 | [0.0078 ; 0.0335] | [0.00981 ; 0.031] | 0.03 | 0.0035 | 0.13 | 0.52 | DIFFERENT | INDETERMINE |
| `part_but.sell` | comportement | 0.0602 | 0.0632 | 0.00306 | [-0.00154 ; 0.00752] | [-0.000759 ; 0.00682] | 0.03 | 0.18 | 1 | 0.31 | EQUIVALENT | EQUIVALENT |
| `part_but.socialize` | comportement | 0.0583 | 0.0622 | 0.00389 | [-0.00424 ; 0.0126] | [-0.00311 ; 0.0113] | 0.03 | 0.39 | 1 | 0.22 | EQUIVALENT | EQUIVALENT |
| `part_dedans` | comportement | 0.377 | 0.341 | -0.0354 | [-0.0548 ; -0.0178] | [-0.0511 ; -0.0204] | 0.03 | 0.001 | 0.042 | -0.73 | DIFFERENT | DIFFERENT |
| `part_observer` | comportement | 0.00667 | 0.00667 | 0 | [0 ; 0] | [0 ; 0] | 0.02 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `batiments_fin` | monde | 3 | 3 | 0 | [0 ; 0] | [0 ; 0] | 0.5 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `bois_preleve_tuiles` | monde | -3.9e+03 | -3.9e+03 | 4.45 | [-0.2 ; 12.7] | [0.1 ; 11.8] | 585 | 0.15 | 1 | 0.19 | EQUIVALENT | EQUIVALENT |
| `energie_moy` | monde | 73.9 | 76.8 | 2.96 | [1.42 ; 4.52] | [1.69 ; 4.25] | 3 | 0.0012 | 0.051 | 0.67 | DIFFERENT | INDETERMINE |
| `faim_moy` | monde | 36.7 | 37.7 | 0.971 | [-0.235 ; 2.18] | [-0.0381 ; 1.98] | 3 | 0.12 | 1 | 0.34 | EQUIVALENT | EQUIVALENT |
| `hygiene_moy` | monde | 67 | 68.2 | 1.27 | [0.205 ; 2.38] | [0.357 ; 2.2] | 3 | 0.038 | 1 | 0.41 | EQUIVALENT | EQUIVALENT |
| `liens_positifs_fin` | monde | 12.5 | 12.2 | -0.3 | [-1.55 ; 1] | [-1.35 ; 0.8] | 2.5 | 0.71 | 1 | -0.11 | EQUIVALENT | EQUIVALENT |
| `liens_somme_fin` | monde | 214 | 178 | -35.7 | [-67.1 ; -4.52] | [-61.3 ; -9.15] | 42.7 | 0.032 | 1 | -0.41 | DIFFERENT | INDETERMINE |
| `loisir_moy` | monde | 81.1 | 81.8 | 0.707 | [-0.352 ; 1.76] | [-0.162 ; 1.61] | 3 | 0.19 | 1 | 0.29 | EQUIVALENT | EQUIVALENT |
| `moral_moy` | monde | 88 | 88 | 0.0158 | [-0.757 ; 0.855] | [-0.645 ; 0.719] | 3 | 0.97 | 1 | 0.0072 | EQUIVALENT | EQUIVALENT |
| `nourriture_recoltee_tuiles` | monde | -1.44e+04 | -1.43e+04 | 28.5 | [-20 ; 77.4] | [-12 ; 69.7] | 1.44e+03 | 0.27 | 1 | 0.23 | EQUIVALENT | EQUIVALENT |
| `oui_dire_fin` | monde | 0.35 | 0.7 | 0.35 | [0 ; 0.725] | [0.05 ; 0.65] | 1 | 0.1 | 1 | 0.56 | EQUIVALENT | EQUIVALENT |
| `part_faim_forte` | monde | 0.0601 | 0.0479 | -0.0122 | [-0.0239 ; 0.000112] | [-0.0223 ; -0.00189] | 0.03 | 0.061 | 1 | -0.42 | EQUIVALENT | EQUIVALENT |
| `part_soif_forte` | monde | 0.135 | 0.0792 | -0.0555 | [-0.0859 ; -0.0264] | [-0.0809 ; -0.0308] | 0.03 | 0.00075 | 0.032 | -0.61 | DIFFERENT | DIFFERENT |
| `pierre_prelevee_tuiles` | monde | 57.8 | 61 | 3.2 | [-0.6 ; 7.42] | [-0.1 ; 6.7] | 8.67 | 0.13 | 1 | 0.31 | EQUIVALENT | EQUIVALENT |
| `population_fin` | monde | 5 | 5 | 0 | [0 ; 0] | [0 ; 0] | 0.1 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `sante_min` | monde | 85.3 | 87.3 | 2.04 | [-3.14 ; 6.95] | [-2.4 ; 6.25] | 5 | 0.43 | 1 | 0.19 | INDETERMINE | INDETERMINE |
| `social_moy` | monde | 71.1 | 72.3 | 1.12 | [-1.42 ; 3.63] | [-0.947 ; 3.27] | 3 | 0.4 | 1 | 0.2 | INDETERMINE | INDETERMINE |
| `soif_moy` | monde | 40 | 36.4 | -3.59 | [-5.71 ; -1.58] | [-5.37 ; -1.86] | 3 | 0.0022 | 0.09 | -0.56 | DIFFERENT | INDETERMINE |
| `stock_bati_moy` | monde | 26.8 | 28.2 | 1.37 | [-1.28 ; 3.72] | [-0.816 ; 3.37] | 2.68 | 0.3 | 1 | 0.18 | INDETERMINE | INDETERMINE |
| `survivants_initiaux` | monde | 5 | 5 | 0 | [0 ; 0] | [0 ; 0] | 0.1 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `tresor_fin` | monde | 12.2 | 11.9 | -0.275 | [-1.48 ; 0.65] | [-1.25 ; 0.525] | 5 | 0.69 | 1 | -0.091 | EQUIVALENT | EQUIVALENT |

## endurance — calibration_AA:aucun/aucun@200000 : A = `aucun`, B = `aucun@200000` — N = 40, D = 3 j

**Verdict statistique (regle v2, Holm) : INDETERMINE** — nature mesuree : **indeterminee (N insuffisant)**  
Regle v1 (declaree, sans correction) : DERIVE / derivant — 6 grandeur(s) DIFFERENT, 4 INDETERMINE  
**Verdict au bit : DIVERGE** (40/40 paires ; premier tick divergent median 0, min 0, max 0)

Activation (B) : 0 par passage en moyenne (min 0). Cout d'un passage : A 8.81 s, B 9.55 s (medianes).
Decorrelation (sep. A/B au dernier jour / sep. temoin/temoin-bis) : 0.978 [IC95 0.913 ; 1.05], n = 40. 1re separation >= 1 case : median 10 s.

| t (s) | sep. positions A/B (cases, mediane) | sep. besoins A/B | sep. positions temoin/bis |
|---:|---:|---:|---:|
| 0.0 | 0 | 0 | 0 |
| 25.0 | 3.16 | 0.112 | 3.05 |
| 49.0 | 2.4 | 3.1 | 2.03 |
| 74.0 | 1.77 | 4.26 | 1.58 |
| 98.0 | 5.74 | 5.45 | 6.02 |
| 123.0 | 6.2 | 10.2 | 7.54 |
| 147.0 | 5.26 | 21.4 | 4.54 |
| 172.0 | 5.4 | 24.8 | 5.8 |
| 196.0 | 8.11 | 19.5 | 7.22 |
| 221.0 | 4.06 | 15.6 | 3.61 |
| 245.0 | 0.82 | 13.7 | 0.775 |
| 270.0 | 6.84 | 13.6 | 7.67 |

| grandeur | echelle | A | B | diff | IC95 | IC90 | delta | p | p Holm | effet (sd) | v1 | v2 (Holm) |
|---|---|---:|---:|---:|---|---|---:|---:|---:|---:|---|---|
| `changements_but_par_habitant_jour` | comportement | 8.43 | 8.75 | 0.325 | [-1.79e-16 ; 0.68] | [0.0467 ; 0.618] | 1.26 | 0.081 | 1 | 0.36 | EQUIVALENT | EQUIVALENT |
| `duree_episode.build` | comportement | 5.16 | 5.01 | -0.156 | [-0.596 ; 0.262] | [-0.527 ; 0.202] | 1.03 | 0.49 | 1 | -0.15 | EQUIVALENT | EQUIVALENT |
| `duree_episode.deliver` | comportement | 8.85 | 8.27 | -0.583 | [-1.46 ; 0.24] | [-1.33 ; 0.103] | 1.77 | 0.19 | 1 | -0.26 | EQUIVALENT | EQUIVALENT |
| `duree_episode.eat` | comportement | 11.8 | 9.94 | -1.85 | [-3.42 ; -0.393] | [-3.14 ; -0.6] | 2.36 | 0.029 | 1 | -0.4 | DIFFERENT | INDETERMINE |
| `duree_episode.gatherFood` | comportement | 6.39 | 6.44 | 0.0477 | [-0.722 ; 0.82] | [-0.586 ; 0.699] | 1.28 | 0.9 | 1 | 0.03 | EQUIVALENT | EQUIVALENT |
| `duree_episode.gatherStone` | comportement | 10.1 | 9.81 | -0.262 | [-1.46 ; 0.89] | [-1.24 ; 0.688] | 2.01 | 0.67 | 1 | -0.091 | EQUIVALENT | EQUIVALENT |
| `duree_episode.gatherWood` | comportement | 10.4 | 10.2 | -0.14 | [-0.734 ; 0.449] | [-0.631 ; 0.344] | 2.08 | 0.65 | 1 | -0.088 | EQUIVALENT | EQUIVALENT |
| `duree_episode.helpFarm` | comportement | 46.4 | 41.6 | -4.81 | [-9.3 ; 0.0719] | [-8.69 ; -0.781] | 9.27 | 0.047 | 1 | -0.46 | EQUIVALENT | EQUIVALENT |
| `duree_episode.rest` | comportement | 22.1 | 22.3 | 0.268 | [-0.932 ; 1.48] | [-0.744 ; 1.28] | 4.41 | 0.66 | 1 | 0.11 | EQUIVALENT | EQUIVALENT |
| `duree_episode.sell` | comportement | 5.44 | 5.53 | 0.0899 | [-0.119 ; 0.291] | [-0.0896 ; 0.263] | 1.09 | 0.41 | 1 | 0.15 | EQUIVALENT | EQUIVALENT |
| `duree_episode.socialize` | comportement | 9.49 | 9.23 | -0.253 | [-1.48 ; 0.969] | [-1.29 ; 0.778] | 1.9 | 0.7 | 1 | -0.1 | EQUIVALENT | EQUIVALENT |
| `duree_episode_moy_s` | comportement | 10.4 | 9.98 | -0.411 | [-0.873 ; -0.00184] | [-0.787 ; -0.0579] | 1.56 | 0.079 | 1 | -0.35 | EQUIVALENT | EQUIVALENT |
| `part_but.build` | comportement | 0.0284 | 0.0281 | -0.000352 | [-0.00298 ; 0.0023] | [-0.00261 ; 0.0018] | 0.03 | 0.81 | 1 | -0.054 | EQUIVALENT | EQUIVALENT |
| `part_but.deliver` | comportement | 0.147 | 0.155 | 0.00731 | [-0.00454 ; 0.0187] | [-0.00263 ; 0.017] | 0.03 | 0.21 | 1 | 0.21 | EQUIVALENT | EQUIVALENT |
| `part_but.eat` | comportement | 0.147 | 0.118 | -0.0286 | [-0.0469 ; -0.0116] | [-0.0432 ; -0.0141] | 0.03 | 0.0037 | 0.17 | -0.49 | DIFFERENT | INDETERMINE |
| `part_but.gatherFood` | comportement | 0.0502 | 0.0523 | 0.00204 | [-0.00444 ; 0.00854] | [-0.00348 ; 0.00748] | 0.03 | 0.55 | 1 | 0.12 | EQUIVALENT | EQUIVALENT |
| `part_but.gatherStone` | comportement | 0.0874 | 0.0969 | 0.00952 | [0.000777 ; 0.0185] | [0.00224 ; 0.0169] | 0.03 | 0.042 | 1 | 0.41 | EQUIVALENT | EQUIVALENT |
| `part_but.gatherWood` | comportement | 0.0265 | 0.0257 | -0.000815 | [-0.00237 ; 0.000593] | [-0.00209 ; 0.000407] | 0.03 | 0.33 | 1 | -0.19 | EQUIVALENT | EQUIVALENT |
| `part_but.helpFarm` | comportement | 0.123 | 0.117 | -0.00554 | [-0.015 ; 0.00369] | [-0.0134 ; 0.00219] | 0.03 | 0.26 | 1 | -0.27 | EQUIVALENT | EQUIVALENT |
| `part_but.rest` | comportement | 0.232 | 0.25 | 0.0184 | [0.00296 ; 0.0338] | [0.00524 ; 0.0317] | 0.03 | 0.031 | 1 | 0.46 | DIFFERENT | INDETERMINE |
| `part_but.sell` | comportement | 0.0602 | 0.058 | -0.00222 | [-0.00602 ; 0.0012] | [-0.00535 ; 0.000685] | 0.03 | 0.25 | 1 | -0.22 | EQUIVALENT | EQUIVALENT |
| `part_but.socialize` | comportement | 0.0583 | 0.0586 | 0.000315 | [-0.00819 ; 0.00867] | [-0.0067 ; 0.0073] | 0.03 | 0.95 | 1 | 0.018 | EQUIVALENT | EQUIVALENT |
| `part_dedans` | comportement | 0.377 | 0.361 | -0.0154 | [-0.0343 ; 0.00348] | [-0.0316 ; 0.000465] | 0.03 | 0.12 | 1 | -0.32 | INDETERMINE | INDETERMINE |
| `part_observer` | comportement | 0.00667 | 0.00667 | 0 | [0 ; 0] | [0 ; 0] | 0.02 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `batiments_fin` | monde | 3 | 3 | 0 | [0 ; 0] | [0 ; 0] | 0.5 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `bois_preleve_tuiles` | monde | -3.9e+03 | -3.9e+03 | 3.77 | [-0.85 ; 12] | [-0.6 ; 11.1] | 585 | 0.44 | 1 | 0.16 | EQUIVALENT | EQUIVALENT |
| `energie_moy` | monde | 73.9 | 76.2 | 2.33 | [0.538 ; 4.07] | [0.851 ; 3.79] | 3 | 0.017 | 0.72 | 0.53 | DIFFERENT | INDETERMINE |
| `faim_moy` | monde | 36.7 | 37 | 0.359 | [-0.632 ; 1.38] | [-0.498 ; 1.23] | 3 | 0.49 | 1 | 0.13 | EQUIVALENT | EQUIVALENT |
| `hygiene_moy` | monde | 67 | 66.6 | -0.305 | [-1.62 ; 1.02] | [-1.41 ; 0.811] | 3 | 0.67 | 1 | -0.099 | EQUIVALENT | EQUIVALENT |
| `liens_positifs_fin` | monde | 12.5 | 12.1 | -0.4 | [-1.45 ; 0.7] | [-1.3 ; 0.5] | 2.5 | 0.53 | 1 | -0.15 | EQUIVALENT | EQUIVALENT |
| `liens_somme_fin` | monde | 214 | 186 | -27.9 | [-60.3 ; 4.95] | [-55.2 ; 0.00125] | 42.7 | 0.11 | 1 | -0.32 | INDETERMINE | INDETERMINE |
| `loisir_moy` | monde | 81.1 | 81.9 | 0.868 | [-0.0825 ; 1.84] | [0.0741 ; 1.7] | 3 | 0.096 | 1 | 0.36 | EQUIVALENT | EQUIVALENT |
| `moral_moy` | monde | 88 | 87.8 | -0.23 | [-1.06 ; 0.642] | [-0.936 ; 0.513] | 3 | 0.61 | 1 | -0.1 | EQUIVALENT | EQUIVALENT |
| `nourriture_recoltee_tuiles` | monde | -1.44e+04 | -1.43e+04 | 46.2 | [5.97 ; 88] | [12.3 ; 80.8] | 1.44e+03 | 0.035 | 1 | 0.38 | EQUIVALENT | EQUIVALENT |
| `oui_dire_fin` | monde | 0.35 | 0.775 | 0.425 | [0 ; 0.875] | [0.075 ; 0.775] | 1 | 0.07 | 1 | 0.68 | EQUIVALENT | EQUIVALENT |
| `part_faim_forte` | monde | 0.0601 | 0.0485 | -0.0116 | [-0.0232 ; -0.000519] | [-0.0211 ; -0.00243] | 0.03 | 0.052 | 1 | -0.4 | EQUIVALENT | EQUIVALENT |
| `part_soif_forte` | monde | 0.135 | 0.0976 | -0.0371 | [-0.0692 ; -0.00539] | [-0.0638 ; -0.0109] | 0.03 | 0.031 | 1 | -0.41 | DIFFERENT | INDETERMINE |
| `pierre_prelevee_tuiles` | monde | 57.8 | 63 | 5.2 | [1.52 ; 8.88] | [2.1 ; 8.35] | 8.67 | 0.014 | 0.6 | 0.5 | EQUIVALENT | EQUIVALENT |
| `population_fin` | monde | 5 | 5 | 0 | [0 ; 0] | [0 ; 0] | 0.1 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `sante_min` | monde | 85.3 | 89.5 | 4.21 | [0.341 ; 8.11] | [0.939 ; 7.44] | 5 | 0.041 | 1 | 0.39 | DIFFERENT | INDETERMINE |
| `social_moy` | monde | 71.1 | 71.8 | 0.69 | [-1.5 ; 3.02] | [-1.15 ; 2.65] | 3 | 0.56 | 1 | 0.12 | EQUIVALENT | EQUIVALENT |
| `soif_moy` | monde | 40 | 37.8 | -2.23 | [-4.45 ; 0.0143] | [-4.11 ; -0.398] | 3 | 0.069 | 1 | -0.35 | INDETERMINE | INDETERMINE |
| `stock_bati_moy` | monde | 26.8 | 27.5 | 0.729 | [-1.68 ; 3.11] | [-1.3 ; 2.72] | 2.68 | 0.55 | 1 | 0.096 | INDETERMINE | INDETERMINE |
| `survivants_initiaux` | monde | 5 | 5 | 0 | [0 ; 0] | [0 ; 0] | 0.1 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `tresor_fin` | monde | 12.2 | 12 | -0.15 | [-1.27 ; 0.75] | [-1.02 ; 0.625] | 5 | 0.83 | 1 | -0.05 | EQUIVALENT | EQUIVALENT |

## endurance — calibration_AA:aucun/aucun@300000 : A = `aucun`, B = `aucun@300000` — N = 40, D = 3 j

**Verdict statistique (regle v2, Holm) : INDETERMINE** — nature mesuree : **indeterminee (N insuffisant)**  
Regle v1 (declaree, sans correction) : DERIVE / derivant — 3 grandeur(s) DIFFERENT, 6 INDETERMINE  
**Verdict au bit : DIVERGE** (40/40 paires ; premier tick divergent median 0, min 0, max 0)

Activation (B) : 0 par passage en moyenne (min 0). Cout d'un passage : A 8.81 s, B 9.76 s (medianes).
Decorrelation (sep. A/B au dernier jour / sep. temoin/temoin-bis) : 1.02 [IC95 0.958 ; 1.08], n = 40. 1re separation >= 1 case : median 10 s.

| t (s) | sep. positions A/B (cases, mediane) | sep. besoins A/B | sep. positions temoin/bis |
|---:|---:|---:|---:|
| 0.0 | 0 | 0 | 0 |
| 25.0 | 3.41 | 0.158 | 3.05 |
| 49.0 | 2.16 | 4.5 | 2.03 |
| 74.0 | 1.57 | 4.62 | 1.58 |
| 98.0 | 5.92 | 6 | 6.02 |
| 123.0 | 6.46 | 10.8 | 7.54 |
| 147.0 | 4.96 | 22.8 | 4.54 |
| 172.0 | 5.58 | 24.3 | 5.8 |
| 196.0 | 7.16 | 21.2 | 7.22 |
| 221.0 | 4.56 | 16.4 | 3.61 |
| 245.0 | 0.907 | 14.6 | 0.775 |
| 270.0 | 6.69 | 14.9 | 7.67 |

| grandeur | echelle | A | B | diff | IC95 | IC90 | delta | p | p Holm | effet (sd) | v1 | v2 (Holm) |
|---|---|---:|---:|---:|---|---|---:|---:|---:|---:|---|---|
| `changements_but_par_habitant_jour` | comportement | 8.43 | 8.88 | 0.453 | [0.0817 ; 0.83] | [0.14 ; 0.768] | 1.26 | 0.026 | 1 | 0.5 | EQUIVALENT | EQUIVALENT |
| `duree_episode.build` | comportement | 5.16 | 5.03 | -0.135 | [-0.455 ; 0.195] | [-0.407 ; 0.134] | 1.03 | 0.43 | 1 | -0.13 | EQUIVALENT | EQUIVALENT |
| `duree_episode.deliver` | comportement | 8.85 | 8.32 | -0.534 | [-1.5 ; 0.412] | [-1.34 ; 0.268] | 1.77 | 0.28 | 1 | -0.24 | EQUIVALENT | EQUIVALENT |
| `duree_episode.eat` | comportement | 11.8 | 9.6 | -2.18 | [-3.92 ; -0.446] | [-3.61 ; -0.71] | 2.36 | 0.021 | 0.92 | -0.48 | DIFFERENT | INDETERMINE |
| `duree_episode.gatherFood` | comportement | 6.39 | 6.45 | 0.0521 | [-0.719 ; 0.786] | [-0.565 ; 0.668] | 1.28 | 0.9 | 1 | 0.033 | EQUIVALENT | EQUIVALENT |
| `duree_episode.gatherStone` | comportement | 10.1 | 9.28 | -0.785 | [-1.72 ; 0.146] | [-1.56 ; -0.00606] | 2.01 | 0.1 | 1 | -0.27 | EQUIVALENT | EQUIVALENT |
| `duree_episode.gatherWood` | comportement | 10.4 | 10.8 | 0.373 | [-0.192 ; 0.926] | [-0.115 ; 0.836] | 2.08 | 0.2 | 1 | 0.24 | EQUIVALENT | EQUIVALENT |
| `duree_episode.helpFarm` | comportement | 46.4 | 46.9 | 0.567 | [-4.86 ; 6.19] | [-4.07 ; 5.36] | 9.27 | 0.84 | 1 | 0.055 | EQUIVALENT | EQUIVALENT |
| `duree_episode.rest` | comportement | 22.1 | 21.8 | -0.291 | [-1.26 ; 0.727] | [-1.12 ; 0.536] | 4.41 | 0.58 | 1 | -0.12 | EQUIVALENT | EQUIVALENT |
| `duree_episode.sell` | comportement | 5.44 | 5.24 | -0.198 | [-0.472 ; 0.0653] | [-0.421 ; 0.0278] | 1.09 | 0.15 | 1 | -0.34 | EQUIVALENT | EQUIVALENT |
| `duree_episode.socialize` | comportement | 9.49 | 9.21 | -0.278 | [-1.35 ; 0.786] | [-1.18 ; 0.635] | 1.9 | 0.61 | 1 | -0.11 | EQUIVALENT | EQUIVALENT |
| `duree_episode_moy_s` | comportement | 10.4 | 9.81 | -0.572 | [-1.04 ; -0.127] | [-0.96 ; -0.197] | 1.56 | 0.02 | 0.92 | -0.49 | EQUIVALENT | EQUIVALENT |
| `part_but.build` | comportement | 0.0284 | 0.0271 | -0.00133 | [-0.00354 ; 0.000889] | [-0.00315 ; 0.000481] | 0.03 | 0.24 | 1 | -0.21 | EQUIVALENT | EQUIVALENT |
| `part_but.deliver` | comportement | 0.147 | 0.152 | 0.00439 | [-0.0107 ; 0.019] | [-0.00837 ; 0.0163] | 0.03 | 0.57 | 1 | 0.13 | EQUIVALENT | EQUIVALENT |
| `part_but.eat` | comportement | 0.147 | 0.121 | -0.0258 | [-0.0469 ; -0.0043] | [-0.0436 ; -0.00726] | 0.03 | 0.027 | 1 | -0.45 | DIFFERENT | INDETERMINE |
| `part_but.gatherFood` | comportement | 0.0502 | 0.0573 | 0.00711 | [-0.000741 ; 0.0149] | [0.000259 ; 0.0136] | 0.03 | 0.08 | 1 | 0.43 | EQUIVALENT | EQUIVALENT |
| `part_but.gatherStone` | comportement | 0.0874 | 0.0867 | -0.000741 | [-0.0109 ; 0.0106] | [-0.00937 ; 0.00856] | 0.03 | 0.9 | 1 | -0.032 | EQUIVALENT | EQUIVALENT |
| `part_but.gatherWood` | comportement | 0.0265 | 0.0277 | 0.0012 | [-0.000444 ; 0.00287] | [-0.000167 ; 0.00261] | 0.03 | 0.18 | 1 | 0.27 | EQUIVALENT | EQUIVALENT |
| `part_but.helpFarm` | comportement | 0.123 | 0.121 | -0.00248 | [-0.0159 ; 0.0114] | [-0.0136 ; 0.00898] | 0.03 | 0.73 | 1 | -0.12 | EQUIVALENT | EQUIVALENT |
| `part_but.rest` | comportement | 0.232 | 0.246 | 0.0145 | [-0.00089 ; 0.03] | [0.00131 ; 0.0274] | 0.03 | 0.079 | 1 | 0.36 | EQUIVALENT | EQUIVALENT |
| `part_but.sell` | comportement | 0.0602 | 0.0643 | 0.00413 | [-0.000556 ; 0.00857] | [0.000204 ; 0.00789] | 0.03 | 0.09 | 1 | 0.41 | EQUIVALENT | EQUIVALENT |
| `part_but.socialize` | comportement | 0.0583 | 0.0579 | -0.000444 | [-0.00857 ; 0.0083] | [-0.00754 ; 0.00706] | 0.03 | 0.92 | 1 | -0.025 | EQUIVALENT | EQUIVALENT |
| `part_dedans` | comportement | 0.377 | 0.358 | -0.0187 | [-0.0386 ; 0.00107] | [-0.0354 ; -0.00237] | 0.03 | 0.069 | 1 | -0.39 | INDETERMINE | INDETERMINE |
| `part_observer` | comportement | 0.00667 | 0.00667 | 0 | [0 ; 0] | [0 ; 0] | 0.02 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `batiments_fin` | monde | 3 | 3 | 0 | [0 ; 0] | [0 ; 0] | 0.5 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `bois_preleve_tuiles` | monde | -3.9e+03 | -3.9e+03 | 3.42 | [-1.18 ; 11.7] | [-0.9 ; 10.7] | 585 | 0.6 | 1 | 0.15 | EQUIVALENT | EQUIVALENT |
| `energie_moy` | monde | 73.9 | 75.6 | 1.78 | [-0.237 ; 3.7] | [0.0916 ; 3.41] | 3 | 0.089 | 1 | 0.4 | INDETERMINE | INDETERMINE |
| `faim_moy` | monde | 36.7 | 37.6 | 0.89 | [-0.383 ; 2.17] | [-0.18 ; 1.98] | 3 | 0.18 | 1 | 0.31 | EQUIVALENT | EQUIVALENT |
| `hygiene_moy` | monde | 67 | 67.2 | 0.257 | [-1.35 ; 1.97] | [-1.11 ; 1.69] | 3 | 0.77 | 1 | 0.083 | EQUIVALENT | EQUIVALENT |
| `liens_positifs_fin` | monde | 12.5 | 11.8 | -0.65 | [-1.7 ; 0.35] | [-1.5 ; 0.2] | 2.5 | 0.26 | 1 | -0.25 | EQUIVALENT | EQUIVALENT |
| `liens_somme_fin` | monde | 214 | 175 | -39.1 | [-73.6 ; -2.77] | [-68.6 ; -9.02] | 42.7 | 0.044 | 1 | -0.45 | DIFFERENT | INDETERMINE |
| `loisir_moy` | monde | 81.1 | 81.2 | 0.161 | [-1.17 ; 1.46] | [-0.967 ; 1.27] | 3 | 0.81 | 1 | 0.067 | EQUIVALENT | EQUIVALENT |
| `moral_moy` | monde | 88 | 87.6 | -0.393 | [-1.33 ; 0.631] | [-1.17 ; 0.474] | 3 | 0.44 | 1 | -0.18 | EQUIVALENT | EQUIVALENT |
| `nourriture_recoltee_tuiles` | monde | -1.44e+04 | -1.43e+04 | 24.1 | [-40.9 ; 88.6] | [-29.5 ; 76.3] | 1.44e+03 | 0.46 | 1 | 0.2 | EQUIVALENT | EQUIVALENT |
| `oui_dire_fin` | monde | 0.35 | 0.45 | 0.1 | [-0.175 ; 0.4] | [-0.125 ; 0.35] | 1 | 0.64 | 1 | 0.16 | EQUIVALENT | EQUIVALENT |
| `part_faim_forte` | monde | 0.0601 | 0.0572 | -0.00289 | [-0.0183 ; 0.0125] | [-0.0157 ; 0.0105] | 0.03 | 0.72 | 1 | -0.099 | EQUIVALENT | EQUIVALENT |
| `part_soif_forte` | monde | 0.135 | 0.106 | -0.0292 | [-0.0666 ; 0.00906] | [-0.0607 ; 0.00313] | 0.03 | 0.14 | 1 | -0.32 | INDETERMINE | INDETERMINE |
| `pierre_prelevee_tuiles` | monde | 57.8 | 61.3 | 3.48 | [-0.775 ; 7.83] | [-0.1 ; 7.2] | 8.67 | 0.13 | 1 | 0.33 | EQUIVALENT | EQUIVALENT |
| `population_fin` | monde | 5 | 5 | 0 | [0 ; 0] | [0 ; 0] | 0.1 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `sante_min` | monde | 85.3 | 85.5 | 0.234 | [-6.45 ; 6.7] | [-5.32 ; 5.63] | 5 | 0.95 | 1 | 0.022 | INDETERMINE | INDETERMINE |
| `social_moy` | monde | 71.1 | 71.5 | 0.33 | [-2.02 ; 2.92] | [-1.66 ; 2.47] | 3 | 0.8 | 1 | 0.058 | EQUIVALENT | EQUIVALENT |
| `soif_moy` | monde | 40 | 38 | -2.08 | [-4.74 ; 0.621] | [-4.29 ; 0.175] | 3 | 0.14 | 1 | -0.32 | INDETERMINE | INDETERMINE |
| `stock_bati_moy` | monde | 26.8 | 27.5 | 0.726 | [-2.67 ; 3.75] | [-2.09 ; 3.31] | 2.68 | 0.66 | 1 | 0.096 | INDETERMINE | INDETERMINE |
| `survivants_initiaux` | monde | 5 | 5 | 0 | [0 ; 0] | [0 ; 0] | 0.1 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `tresor_fin` | monde | 12.2 | 11.4 | -0.7 | [-1.78 ; 0.126] | [-1.57 ; 0.025] | 5 | 0.19 | 1 | -0.23 | EQUIVALENT | EQUIVALENT |

## endurance — calibration_AA:aucun@bis/aucun@200000 : A = `aucun@bis`, B = `aucun@200000` — N = 40, D = 3 j

**Verdict statistique (regle v2, Holm) : INDETERMINE** — nature mesuree : **indeterminee (N insuffisant)**  
Regle v1 (declaree, sans correction) : DERIVE / derivant — 2 grandeur(s) DIFFERENT, 3 INDETERMINE  
**Verdict au bit : DIVERGE** (40/40 paires ; premier tick divergent median 0, min 0, max 0)

Activation (B) : 0 par passage en moyenne (min 0). Cout d'un passage : A 8.25 s, B 9.55 s (medianes).
Decorrelation (sep. A/B au dernier jour / sep. temoin/temoin-bis) : 1.02 [IC95 0.936 ; 1.1], n = 40. 1re separation >= 1 case : median 10 s.

| t (s) | sep. positions A/B (cases, mediane) | sep. besoins A/B | sep. positions temoin/bis |
|---:|---:|---:|---:|
| 0.0 | 0 | 0 | 0 |
| 25.0 | 2.15 | 0.0642 | 3.05 |
| 49.0 | 1.97 | 3.21 | 2.03 |
| 74.0 | 1.58 | 4.07 | 1.58 |
| 98.0 | 6.03 | 4.76 | 6.02 |
| 123.0 | 6.86 | 9.48 | 7.54 |
| 147.0 | 5.01 | 20.1 | 4.54 |
| 172.0 | 6.31 | 21.9 | 5.8 |
| 196.0 | 7.77 | 15.3 | 7.22 |
| 221.0 | 3.92 | 13.5 | 3.61 |
| 245.0 | 0.755 | 13.1 | 0.775 |
| 270.0 | 8.06 | 13 | 7.67 |

| grandeur | echelle | A | B | diff | IC95 | IC90 | delta | p | p Holm | effet (sd) | v1 | v2 (Holm) |
|---|---|---:|---:|---:|---|---|---:|---:|---:|---:|---|---|
| `changements_but_par_habitant_jour` | comportement | 9.02 | 8.75 | -0.263 | [-0.54 ; 0.0117] | [-0.498 ; -0.03] | 1.35 | 0.069 | 1 | -0.36 | EQUIVALENT | EQUIVALENT |
| `duree_episode.build` | comportement | 5.06 | 5.01 | -0.0484 | [-0.5 ; 0.364] | [-0.435 ; 0.301] | 1.01 | 0.83 | 1 | -0.053 | EQUIVALENT | EQUIVALENT |
| `duree_episode.deliver` | comportement | 7.99 | 8.27 | 0.285 | [-0.429 ; 0.955] | [-0.29 ; 0.862] | 1.6 | 0.43 | 1 | 0.17 | EQUIVALENT | EQUIVALENT |
| `duree_episode.drink` | comportement | 3.88 | 3.68 | -0.192 | [-0.528 ; 0.146] | [-0.481 ; 0.0954] | 0.775 | 0.26 | 1 | -0.27 | EQUIVALENT | EQUIVALENT |
| `duree_episode.eat` | comportement | 8.49 | 9.94 | 1.44 | [0.298 ; 2.52] | [0.495 ; 2.36] | 1.7 | 0.019 | 0.85 | 0.71 | DIFFERENT | INDETERMINE |
| `duree_episode.gatherFood` | comportement | 6.88 | 6.44 | -0.434 | [-1.39 ; 0.439] | [-1.23 ; 0.31] | 1.38 | 0.36 | 1 | -0.2 | EQUIVALENT | EQUIVALENT |
| `duree_episode.gatherStone` | comportement | 9.28 | 9.81 | 0.525 | [-0.482 ; 1.56] | [-0.327 ; 1.38] | 1.86 | 0.31 | 1 | 0.21 | EQUIVALENT | EQUIVALENT |
| `duree_episode.gatherWood` | comportement | 10.7 | 10.2 | -0.465 | [-0.988 ; 0.0746] | [-0.909 ; -0.0167] | 2.14 | 0.09 | 1 | -0.38 | EQUIVALENT | EQUIVALENT |
| `duree_episode.helpFarm` | comportement | 47.4 | 41.6 | -5.86 | [-11.8 ; 0.123] | [-10.9 ; -0.718] | 9.49 | 0.06 | 1 | -0.39 | INDETERMINE | INDETERMINE |
| `duree_episode.rest` | comportement | 21.8 | 22.3 | 0.545 | [-0.61 ; 1.74] | [-0.433 ; 1.54] | 4.36 | 0.36 | 1 | 0.25 | EQUIVALENT | EQUIVALENT |
| `duree_episode.sell` | comportement | 5.41 | 5.53 | 0.115 | [-0.0874 ; 0.325] | [-0.0542 ; 0.293] | 1.08 | 0.26 | 1 | 0.21 | EQUIVALENT | EQUIVALENT |
| `duree_episode.socialize` | comportement | 9.41 | 9.23 | -0.177 | [-0.94 ; 0.602] | [-0.817 ; 0.468] | 1.88 | 0.65 | 1 | -0.088 | EQUIVALENT | EQUIVALENT |
| `duree_episode_moy_s` | comportement | 9.68 | 9.98 | 0.292 | [-0.0164 ; 0.602] | [0.0372 ; 0.557] | 1.45 | 0.068 | 1 | 0.37 | EQUIVALENT | EQUIVALENT |
| `part_but.build` | comportement | 0.0276 | 0.0281 | 0.000444 | [-0.00276 ; 0.0038] | [-0.0023 ; 0.0032] | 0.03 | 0.81 | 1 | 0.068 | EQUIVALENT | EQUIVALENT |
| `part_but.deliver` | comportement | 0.149 | 0.155 | 0.00578 | [-0.00693 ; 0.0182] | [-0.00496 ; 0.0163] | 0.03 | 0.39 | 1 | 0.22 | EQUIVALENT | EQUIVALENT |
| `part_but.drink` | comportement | 0.0227 | 0.0203 | -0.00235 | [-0.0047 ; 1.85e-05] | [-0.00433 ; -0.000352] | 0.03 | 0.053 | 1 | -0.44 | EQUIVALENT | EQUIVALENT |
| `part_but.eat` | comportement | 0.107 | 0.118 | 0.011 | [-0.00111 ; 0.0226] | [0.000758 ; 0.0208] | 0.03 | 0.081 | 1 | 0.46 | EQUIVALENT | EQUIVALENT |
| `part_but.gatherFood` | comportement | 0.0589 | 0.0523 | -0.00659 | [-0.0124 ; -0.00117] | [-0.0115 ; -0.00196] | 0.03 | 0.028 | 1 | -0.48 | EQUIVALENT | EQUIVALENT |
| `part_but.gatherStone` | comportement | 0.0903 | 0.0969 | 0.00665 | [-0.00356 ; 0.0166] | [-0.00196 ; 0.0152] | 0.03 | 0.2 | 1 | 0.31 | EQUIVALENT | EQUIVALENT |
| `part_but.gatherWood` | comportement | 0.0277 | 0.0257 | -0.002 | [-0.00343 ; -0.00063] | [-0.00317 ; -0.000833] | 0.03 | 0.0075 | 0.35 | -0.49 | EQUIVALENT | EQUIVALENT |
| `part_but.helpFarm` | comportement | 0.121 | 0.117 | -0.00365 | [-0.0152 ; 0.00772] | [-0.0131 ; 0.00585] | 0.03 | 0.55 | 1 | -0.13 | EQUIVALENT | EQUIVALENT |
| `part_but.rest` | comportement | 0.252 | 0.25 | -0.00207 | [-0.0139 ; 0.00941] | [-0.0121 ; 0.00765] | 0.03 | 0.74 | 1 | -0.081 | EQUIVALENT | EQUIVALENT |
| `part_but.sell` | comportement | 0.0632 | 0.058 | -0.00528 | [-0.00948 ; -0.000778] | [-0.00889 ; -0.00163] | 0.03 | 0.016 | 0.71 | -0.57 | EQUIVALENT | EQUIVALENT |
| `part_but.socialize` | comportement | 0.0622 | 0.0586 | -0.00357 | [-0.011 ; 0.00361] | [-0.00982 ; 0.00246] | 0.03 | 0.35 | 1 | -0.23 | EQUIVALENT | EQUIVALENT |
| `part_dedans` | comportement | 0.341 | 0.361 | 0.0199 | [0.000314 ; 0.0394] | [0.00331 ; 0.0363] | 0.03 | 0.058 | 1 | 0.55 | DIFFERENT | INDETERMINE |
| `part_observer` | comportement | 0.00667 | 0.00667 | 0 | [0 ; 0] | [0 ; 0] | 0.02 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `batiments_fin` | monde | 3 | 3 | 0 | [0 ; 0] | [0 ; 0] | 0.5 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `bois_preleve_tuiles` | monde | -3.9e+03 | -3.9e+03 | -0.675 | [-2.25 ; 0.9] | [-2 ; 0.65] | 584 | 0.41 | 1 | -0.19 | EQUIVALENT | EQUIVALENT |
| `energie_moy` | monde | 76.8 | 76.2 | -0.63 | [-1.85 ; 0.534] | [-1.63 ; 0.378] | 3 | 0.31 | 1 | -0.22 | EQUIVALENT | EQUIVALENT |
| `faim_moy` | monde | 37.7 | 37 | -0.612 | [-1.68 ; 0.39] | [-1.48 ; 0.25] | 3 | 0.24 | 1 | -0.25 | EQUIVALENT | EQUIVALENT |
| `hygiene_moy` | monde | 68.2 | 66.6 | -1.58 | [-2.69 ; -0.467] | [-2.5 ; -0.64] | 3 | 0.0095 | 0.44 | -0.66 | EQUIVALENT | EQUIVALENT |
| `liens_positifs_fin` | monde | 12.2 | 12.1 | -0.1 | [-1.3 ; 1.05] | [-1.1 ; 0.85] | 2.44 | 0.94 | 1 | -0.033 | EQUIVALENT | EQUIVALENT |
| `liens_somme_fin` | monde | 178 | 186 | 7.85 | [-18.3 ; 32.9] | [-14.2 ; 29.1] | 35.6 | 0.57 | 1 | 0.12 | EQUIVALENT | EQUIVALENT |
| `loisir_moy` | monde | 81.8 | 81.9 | 0.161 | [-0.852 ; 1.19] | [-0.69 ; 1.03] | 3 | 0.76 | 1 | 0.062 | EQUIVALENT | EQUIVALENT |
| `moral_moy` | monde | 88 | 87.8 | -0.246 | [-0.935 ; 0.464] | [-0.828 ; 0.332] | 3 | 0.49 | 1 | -0.13 | EQUIVALENT | EQUIVALENT |
| `nourriture_recoltee_tuiles` | monde | -1.43e+04 | -1.43e+04 | 17.7 | [-30.5 ; 67.1] | [-23 ; 58.1] | 1.43e+03 | 0.49 | 1 | 0.14 | EQUIVALENT | EQUIVALENT |
| `oui_dire_fin` | monde | 0.7 | 0.775 | 0.075 | [-0.325 ; 0.45] | [-0.25 ; 0.4] | 1 | 0.8 | 1 | 0.072 | EQUIVALENT | EQUIVALENT |
| `part_faim_forte` | monde | 0.0479 | 0.0485 | 0.000611 | [-0.0111 ; 0.0126] | [-0.00922 ; 0.0107] | 0.03 | 0.93 | 1 | 0.021 | EQUIVALENT | EQUIVALENT |
| `part_soif_forte` | monde | 0.0792 | 0.0976 | 0.0184 | [-0.00291 ; 0.0414] | [0.000611 ; 0.0376] | 0.03 | 0.11 | 1 | 0.48 | INDETERMINE | INDETERMINE |
| `pierre_prelevee_tuiles` | monde | 61 | 63 | 2 | [-1.3 ; 5.03] | [-0.7 ; 4.55] | 9.15 | 0.23 | 1 | 0.25 | EQUIVALENT | EQUIVALENT |
| `population_fin` | monde | 5 | 5 | 0 | [0 ; 0] | [0 ; 0] | 0.1 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `sante_min` | monde | 87.3 | 89.5 | 2.17 | [-2.9 ; 7.44] | [-2.28 ; 6.45] | 5 | 0.43 | 1 | 0.15 | INDETERMINE | INDETERMINE |
| `social_moy` | monde | 72.3 | 71.8 | -0.427 | [-2.34 ; 1.54] | [-2.04 ; 1.24] | 3 | 0.66 | 1 | -0.085 | EQUIVALENT | EQUIVALENT |
| `soif_moy` | monde | 36.4 | 37.8 | 1.36 | [-0.297 ; 3.11] | [-0.0313 ; 2.83] | 3 | 0.13 | 1 | 0.44 | EQUIVALENT | EQUIVALENT |
| `stock_bati_moy` | monde | 28.2 | 27.5 | -0.641 | [-2.93 ; 1.7] | [-2.52 ; 1.28] | 2.82 | 0.58 | 1 | -0.13 | EQUIVALENT | EQUIVALENT |
| `survivants_initiaux` | monde | 5 | 5 | 0 | [0 ; 0] | [0 ; 0] | 0.1 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `tresor_fin` | monde | 11.9 | 12 | 0.125 | [-0.5 ; 0.776] | [-0.4 ; 0.675] | 5 | 0.77 | 1 | 0.096 | EQUIVALENT | EQUIVALENT |

## endurance — calibration_AA:aucun@bis/aucun@300000 : A = `aucun@bis`, B = `aucun@300000` — N = 40, D = 3 j

**Verdict statistique (regle v2, Holm) : INDETERMINE** — nature mesuree : **indeterminee (N insuffisant)**  
Regle v1 (declaree, sans correction) : DERIVE / derivant — 3 grandeur(s) DIFFERENT, 3 INDETERMINE  
**Verdict au bit : DIVERGE** (40/40 paires ; premier tick divergent median 0, min 0, max 0)

Activation (B) : 0 par passage en moyenne (min 0). Cout d'un passage : A 8.25 s, B 9.76 s (medianes).
Decorrelation (sep. A/B au dernier jour / sep. temoin/temoin-bis) : 1.01 [IC95 0.948 ; 1.08], n = 40. 1re separation >= 1 case : median 10 s.

| t (s) | sep. positions A/B (cases, mediane) | sep. besoins A/B | sep. positions temoin/bis |
|---:|---:|---:|---:|
| 0.0 | 0 | 0 | 0 |
| 25.0 | 2.87 | 0.128 | 3.05 |
| 49.0 | 2.11 | 2.58 | 2.03 |
| 74.0 | 1.34 | 3.43 | 1.58 |
| 98.0 | 5.55 | 5.14 | 6.02 |
| 123.0 | 5.55 | 10.5 | 7.54 |
| 147.0 | 5.12 | 20.4 | 4.54 |
| 172.0 | 5.49 | 22.8 | 5.8 |
| 196.0 | 7.48 | 17.2 | 7.22 |
| 221.0 | 4.06 | 13.1 | 3.61 |
| 245.0 | 0.792 | 14 | 0.775 |
| 270.0 | 7.91 | 12.5 | 7.67 |

| grandeur | echelle | A | B | diff | IC95 | IC90 | delta | p | p Holm | effet (sd) | v1 | v2 (Holm) |
|---|---|---:|---:|---:|---|---|---:|---:|---:|---:|---|---|
| `changements_but_par_habitant_jour` | comportement | 9.02 | 8.88 | -0.135 | [-0.435 ; 0.162] | [-0.387 ; 0.108] | 1.35 | 0.39 | 1 | -0.19 | EQUIVALENT | EQUIVALENT |
| `duree_episode.build` | comportement | 5.06 | 5.03 | -0.0274 | [-0.411 ; 0.333] | [-0.345 ; 0.275] | 1.01 | 0.89 | 1 | -0.03 | EQUIVALENT | EQUIVALENT |
| `duree_episode.deliver` | comportement | 7.99 | 8.32 | 0.334 | [-0.392 ; 1.05] | [-0.285 ; 0.934] | 1.6 | 0.39 | 1 | 0.2 | EQUIVALENT | EQUIVALENT |
| `duree_episode.drink` | comportement | 3.88 | 3.83 | -0.0467 | [-0.365 ; 0.284] | [-0.317 ; 0.235] | 0.775 | 0.79 | 1 | -0.066 | EQUIVALENT | EQUIVALENT |
| `duree_episode.eat` | comportement | 8.49 | 9.6 | 1.11 | [0.0968 ; 2.21] | [0.26 ; 2.04] | 1.7 | 0.045 | 1 | 0.54 | DIFFERENT | INDETERMINE |
| `duree_episode.gatherFood` | comportement | 6.88 | 6.45 | -0.429 | [-1.35 ; 0.433] | [-1.2 ; 0.313] | 1.38 | 0.37 | 1 | -0.2 | EQUIVALENT | EQUIVALENT |
| `duree_episode.gatherStone` | comportement | 9.28 | 9.28 | 0.00215 | [-0.921 ; 0.89] | [-0.756 ; 0.758] | 1.86 | 1 | 1 | 0.00087 | EQUIVALENT | EQUIVALENT |
| `duree_episode.gatherWood` | comportement | 10.7 | 10.8 | 0.0479 | [-0.626 ; 0.733] | [-0.523 ; 0.626] | 2.14 | 0.9 | 1 | 0.039 | EQUIVALENT | EQUIVALENT |
| `duree_episode.helpFarm` | comportement | 47.4 | 46.9 | -0.487 | [-7.07 ; 5.87] | [-6 ; 4.85] | 9.49 | 0.88 | 1 | -0.032 | EQUIVALENT | EQUIVALENT |
| `duree_episode.rest` | comportement | 21.8 | 21.8 | -0.014 | [-0.917 ; 0.886] | [-0.748 ; 0.749] | 4.36 | 0.98 | 1 | -0.0064 | EQUIVALENT | EQUIVALENT |
| `duree_episode.sell` | comportement | 5.41 | 5.24 | -0.173 | [-0.435 ; 0.0828] | [-0.387 ; 0.0474] | 1.08 | 0.2 | 1 | -0.32 | EQUIVALENT | EQUIVALENT |
| `duree_episode.socialize` | comportement | 9.41 | 9.21 | -0.202 | [-1.04 ; 0.626] | [-0.901 ; 0.484] | 1.88 | 0.64 | 1 | -0.1 | EQUIVALENT | EQUIVALENT |
| `duree_episode_moy_s` | comportement | 9.68 | 9.81 | 0.131 | [-0.19 ; 0.454] | [-0.126 ; 0.403] | 1.45 | 0.44 | 1 | 0.17 | EQUIVALENT | EQUIVALENT |
| `part_but.build` | comportement | 0.0276 | 0.0271 | -0.000537 | [-0.00332 ; 0.00239] | [-0.00287 ; 0.00185] | 0.03 | 0.72 | 1 | -0.082 | EQUIVALENT | EQUIVALENT |
| `part_but.deliver` | comportement | 0.149 | 0.152 | 0.00285 | [-0.00969 ; 0.0151] | [-0.00778 ; 0.0131] | 0.03 | 0.66 | 1 | 0.11 | EQUIVALENT | EQUIVALENT |
| `part_but.drink` | comportement | 0.0227 | 0.0214 | -0.00126 | [-0.00363 ; 0.00117] | [-0.00324 ; 0.000778] | 0.03 | 0.33 | 1 | -0.24 | EQUIVALENT | EQUIVALENT |
| `part_but.eat` | comportement | 0.107 | 0.121 | 0.0137 | [0.00272 ; 0.0263] | [0.00411 ; 0.0243] | 0.03 | 0.02 | 0.95 | 0.57 | EQUIVALENT | EQUIVALENT |
| `part_but.gatherFood` | comportement | 0.0589 | 0.0573 | -0.00152 | [-0.0087 ; 0.0048] | [-0.00726 ; 0.0038] | 0.03 | 0.66 | 1 | -0.11 | EQUIVALENT | EQUIVALENT |
| `part_but.gatherStone` | comportement | 0.0903 | 0.0867 | -0.00361 | [-0.0133 ; 0.00674] | [-0.012 ; 0.00502] | 0.03 | 0.5 | 1 | -0.17 | EQUIVALENT | EQUIVALENT |
| `part_but.gatherWood` | comportement | 0.0277 | 0.0277 | 1.85e-05 | [-0.0017 ; 0.00183] | [-0.00141 ; 0.00152] | 0.03 | 1 | 1 | 0.0045 | EQUIVALENT | EQUIVALENT |
| `part_but.helpFarm` | comportement | 0.121 | 0.121 | -0.000593 | [-0.0127 ; 0.0124] | [-0.0106 ; 0.0104] | 0.03 | 0.93 | 1 | -0.02 | EQUIVALENT | EQUIVALENT |
| `part_but.rest` | comportement | 0.252 | 0.246 | -0.00602 | [-0.0172 ; 0.00465] | [-0.0156 ; 0.003] | 0.03 | 0.3 | 1 | -0.24 | EQUIVALENT | EQUIVALENT |
| `part_but.sell` | comportement | 0.0632 | 0.0643 | 0.00107 | [-0.00313 ; 0.00511] | [-0.00252 ; 0.00446] | 0.03 | 0.62 | 1 | 0.12 | EQUIVALENT | EQUIVALENT |
| `part_but.socialize` | comportement | 0.0622 | 0.0579 | -0.00433 | [-0.0117 ; 0.00302] | [-0.0106 ; 0.00185] | 0.03 | 0.27 | 1 | -0.28 | EQUIVALENT | EQUIVALENT |
| `part_dedans` | comportement | 0.341 | 0.358 | 0.0166 | [0.000389 ; 0.0338] | [0.00266 ; 0.031] | 0.03 | 0.064 | 1 | 0.46 | DIFFERENT | INDETERMINE |
| `part_observer` | comportement | 0.00667 | 0.00667 | 0 | [0 ; 0] | [0 ; 0] | 0.02 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `batiments_fin` | monde | 3 | 3 | 0 | [0 ; 0] | [0 ; 0] | 0.5 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `bois_preleve_tuiles` | monde | -3.9e+03 | -3.9e+03 | -1.02 | [-2.35 ; 0.226] | [-2.1 ; 0.025] | 584 | 0.14 | 1 | -0.29 | EQUIVALENT | EQUIVALENT |
| `energie_moy` | monde | 76.8 | 75.6 | -1.18 | [-2.84 ; 0.316] | [-2.53 ; 0.0896] | 3 | 0.15 | 1 | -0.42 | EQUIVALENT | EQUIVALENT |
| `faim_moy` | monde | 37.7 | 37.6 | -0.0804 | [-1.22 ; 1.06] | [-1.04 ; 0.866] | 3 | 0.89 | 1 | -0.033 | EQUIVALENT | EQUIVALENT |
| `hygiene_moy` | monde | 68.2 | 67.2 | -1.01 | [-2.43 ; 0.484] | [-2.24 ; 0.257] | 3 | 0.19 | 1 | -0.43 | EQUIVALENT | EQUIVALENT |
| `liens_positifs_fin` | monde | 12.2 | 11.8 | -0.35 | [-1.55 ; 0.8] | [-1.35 ; 0.65] | 2.44 | 0.64 | 1 | -0.12 | EQUIVALENT | EQUIVALENT |
| `liens_somme_fin` | monde | 178 | 175 | -3.38 | [-29.4 ; 22.2] | [-25.1 ; 19.2] | 35.6 | 0.82 | 1 | -0.052 | EQUIVALENT | EQUIVALENT |
| `loisir_moy` | monde | 81.8 | 81.2 | -0.545 | [-1.98 ; 0.794] | [-1.72 ; 0.581] | 3 | 0.45 | 1 | -0.21 | EQUIVALENT | EQUIVALENT |
| `moral_moy` | monde | 88 | 87.6 | -0.409 | [-1.11 ; 0.342] | [-1 ; 0.201] | 3 | 0.27 | 1 | -0.22 | EQUIVALENT | EQUIVALENT |
| `nourriture_recoltee_tuiles` | monde | -1.43e+04 | -1.43e+04 | -4.42 | [-63.1 ; 52.8] | [-54.3 ; 44] | 1.43e+03 | 0.89 | 1 | -0.036 | EQUIVALENT | EQUIVALENT |
| `oui_dire_fin` | monde | 0.7 | 0.45 | -0.25 | [-0.75 ; 0.275] | [-0.675 ; 0.2] | 1 | 0.41 | 1 | -0.24 | EQUIVALENT | EQUIVALENT |
| `part_faim_forte` | monde | 0.0479 | 0.0572 | 0.0093 | [-0.00491 ; 0.025] | [-0.0028 ; 0.0225] | 0.03 | 0.24 | 1 | 0.32 | EQUIVALENT | EQUIVALENT |
| `part_soif_forte` | monde | 0.0792 | 0.106 | 0.0264 | [0.00178 ; 0.0524] | [0.00572 ; 0.0486] | 0.03 | 0.051 | 1 | 0.68 | DIFFERENT | INDETERMINE |
| `pierre_prelevee_tuiles` | monde | 61 | 61.3 | 0.275 | [-3.45 ; 4.08] | [-2.88 ; 3.42] | 9.15 | 0.9 | 1 | 0.034 | EQUIVALENT | EQUIVALENT |
| `population_fin` | monde | 5 | 5 | 0 | [0 ; 0] | [0 ; 0] | 0.1 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `sante_min` | monde | 87.3 | 85.5 | -1.81 | [-8.71 ; 4.67] | [-7.59 ; 3.66] | 5 | 0.6 | 1 | -0.13 | INDETERMINE | INDETERMINE |
| `social_moy` | monde | 72.3 | 71.5 | -0.787 | [-3.23 ; 1.46] | [-2.8 ; 1.12] | 3 | 0.52 | 1 | -0.16 | EQUIVALENT | EQUIVALENT |
| `soif_moy` | monde | 36.4 | 38 | 1.51 | [-0.208 ; 3.34] | [0.0252 ; 3.07] | 3 | 0.11 | 1 | 0.49 | INDETERMINE | INDETERMINE |
| `stock_bati_moy` | monde | 28.2 | 27.5 | -0.644 | [-3.35 ; 1.86] | [-2.88 ; 1.45] | 2.82 | 0.62 | 1 | -0.13 | INDETERMINE | INDETERMINE |
| `survivants_initiaux` | monde | 5 | 5 | 0 | [0 ; 0] | [0 ; 0] | 0.1 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `tresor_fin` | monde | 11.9 | 11.4 | -0.425 | [-0.9 ; 0.025] | [-0.825 ; -0.05] | 5 | 0.11 | 1 | -0.33 | EQUIVALENT | EQUIVALENT |

## endurance — calibration_AA:aucun@200000/aucun@300000 : A = `aucun@200000`, B = `aucun@300000` — N = 40, D = 3 j

**Verdict statistique (regle v2, Holm) : INDETERMINE** — nature mesuree : **indeterminee (N insuffisant)**  
Regle v1 (declaree, sans correction) : DERIVE / derivant — 1 grandeur(s) DIFFERENT, 2 INDETERMINE  
**Verdict au bit : DIVERGE** (40/40 paires ; premier tick divergent median 0, min 0, max 0)

Activation (B) : 0 par passage en moyenne (min 0). Cout d'un passage : A 9.55 s, B 9.76 s (medianes).
Decorrelation (sep. A/B au dernier jour / sep. temoin/temoin-bis) : 1.02 [IC95 0.926 ; 1.12], n = 40. 1re separation >= 1 case : median 10 s.

| t (s) | sep. positions A/B (cases, mediane) | sep. besoins A/B | sep. positions temoin/bis |
|---:|---:|---:|---:|
| 0.0 | 0 | 0 | 0 |
| 25.0 | 3.16 | 0.066 | 3.05 |
| 49.0 | 2.4 | 4.11 | 2.03 |
| 74.0 | 2.06 | 3.87 | 1.58 |
| 98.0 | 6.29 | 6.23 | 6.02 |
| 123.0 | 6.04 | 8.92 | 7.54 |
| 147.0 | 4.53 | 20.3 | 4.54 |
| 172.0 | 5.36 | 23.7 | 5.8 |
| 196.0 | 7.39 | 16.4 | 7.22 |
| 221.0 | 4.17 | 13.9 | 3.61 |
| 245.0 | 0.771 | 13.4 | 0.775 |
| 270.0 | 7.82 | 13 | 7.67 |

| grandeur | echelle | A | B | diff | IC95 | IC90 | delta | p | p Holm | effet (sd) | v1 | v2 (Holm) |
|---|---|---:|---:|---:|---|---|---:|---:|---:|---:|---|---|
| `changements_but_par_habitant_jour` | comportement | 8.75 | 8.88 | 0.128 | [-0.155 ; 0.405] | [-0.115 ; 0.36] | 1.31 | 0.39 | 1 | 0.17 | EQUIVALENT | EQUIVALENT |
| `duree_episode.build` | comportement | 5.01 | 5.03 | 0.021 | [-0.388 ; 0.423] | [-0.322 ; 0.352] | 1 | 0.93 | 1 | 0.02 | EQUIVALENT | EQUIVALENT |
| `duree_episode.deliver` | comportement | 8.27 | 8.32 | 0.0492 | [-0.6 ; 0.731] | [-0.497 ; 0.621] | 1.65 | 0.9 | 1 | 0.029 | EQUIVALENT | EQUIVALENT |
| `duree_episode.drink` | comportement | 3.68 | 3.83 | 0.145 | [-0.179 ; 0.483] | [-0.137 ; 0.423] | 0.737 | 0.4 | 1 | 0.21 | EQUIVALENT | EQUIVALENT |
| `duree_episode.eat` | comportement | 9.94 | 9.6 | -0.334 | [-1.5 ; 0.971] | [-1.34 ; 0.756] | 1.99 | 0.6 | 1 | -0.11 | EQUIVALENT | EQUIVALENT |
| `duree_episode.gatherFood` | comportement | 6.44 | 6.45 | 0.00444 | [-0.826 ; 0.767] | [-0.667 ; 0.655] | 1.29 | 0.99 | 1 | 0.0023 | EQUIVALENT | EQUIVALENT |
| `duree_episode.gatherStone` | comportement | 9.81 | 9.28 | -0.523 | [-1.44 ; 0.38] | [-1.28 ; 0.234] | 1.96 | 0.28 | 1 | -0.22 | EQUIVALENT | EQUIVALENT |
| `duree_episode.gatherWood` | comportement | 10.2 | 10.8 | 0.512 | [-0.0734 ; 1.12] | [0.00871 ; 1.01] | 2.05 | 0.12 | 1 | 0.43 | EQUIVALENT | EQUIVALENT |
| `duree_episode.helpFarm` | comportement | 41.6 | 46.9 | 5.38 | [0.48 ; 10.6] | [1.15 ; 9.78] | 8.31 | 0.041 | 1 | 0.5 | DIFFERENT | INDETERMINE |
| `duree_episode.rest` | comportement | 22.3 | 21.8 | -0.559 | [-1.66 ; 0.624] | [-1.48 ; 0.416] | 4.47 | 0.34 | 1 | -0.19 | EQUIVALENT | EQUIVALENT |
| `duree_episode.sell` | comportement | 5.53 | 5.24 | -0.288 | [-0.553 ; -0.0327] | [-0.511 ; -0.0715] | 1.11 | 0.034 | 1 | -0.57 | EQUIVALENT | EQUIVALENT |
| `duree_episode.socialize` | comportement | 9.23 | 9.21 | -0.0251 | [-0.961 ; 0.865] | [-0.784 ; 0.733] | 1.85 | 0.96 | 1 | -0.012 | EQUIVALENT | EQUIVALENT |
| `duree_episode_moy_s` | comportement | 9.98 | 9.81 | -0.161 | [-0.477 ; 0.158] | [-0.424 ; 0.106] | 1.5 | 0.33 | 1 | -0.18 | EQUIVALENT | EQUIVALENT |
| `part_but.build` | comportement | 0.0281 | 0.0271 | -0.000981 | [-0.00352 ; 0.00154] | [-0.00311 ; 0.00113] | 0.03 | 0.49 | 1 | -0.13 | EQUIVALENT | EQUIVALENT |
| `part_but.deliver` | comportement | 0.155 | 0.152 | -0.00293 | [-0.0141 ; 0.00813] | [-0.0125 ; 0.00635] | 0.03 | 0.62 | 1 | -0.11 | EQUIVALENT | EQUIVALENT |
| `part_but.drink` | comportement | 0.0203 | 0.0214 | 0.00109 | [-0.00144 ; 0.00372] | [-0.00109 ; 0.00328] | 0.03 | 0.43 | 1 | 0.19 | EQUIVALENT | EQUIVALENT |
| `part_but.eat` | comportement | 0.118 | 0.121 | 0.00276 | [-0.0104 ; 0.0177] | [-0.00826 ; 0.015] | 0.03 | 0.71 | 1 | 0.09 | EQUIVALENT | EQUIVALENT |
| `part_but.gatherFood` | comportement | 0.0523 | 0.0573 | 0.00507 | [-0.00172 ; 0.0116] | [-0.000611 ; 0.0105] | 0.03 | 0.14 | 1 | 0.36 | EQUIVALENT | EQUIVALENT |
| `part_but.gatherStone` | comportement | 0.0969 | 0.0867 | -0.0103 | [-0.022 ; 0.000889] | [-0.0198 ; -0.00074] | 0.03 | 0.093 | 1 | -0.49 | EQUIVALENT | EQUIVALENT |
| `part_but.gatherWood` | comportement | 0.0257 | 0.0277 | 0.00202 | [0.000556 ; 0.00346] | [0.000795 ; 0.00322] | 0.03 | 0.011 | 0.51 | 0.82 | EQUIVALENT | EQUIVALENT |
| `part_but.helpFarm` | comportement | 0.117 | 0.121 | 0.00306 | [-0.00928 ; 0.0156] | [-0.007 ; 0.0133] | 0.03 | 0.63 | 1 | 0.15 | EQUIVALENT | EQUIVALENT |
| `part_but.rest` | comportement | 0.25 | 0.246 | -0.00394 | [-0.0174 ; 0.00994] | [-0.0156 ; 0.00783] | 0.03 | 0.6 | 1 | -0.12 | EQUIVALENT | EQUIVALENT |
| `part_but.sell` | comportement | 0.058 | 0.0643 | 0.00635 | [0.00235 ; 0.0101] | [0.00306 ; 0.00965] | 0.03 | 0.0042 | 0.2 | 0.8 | EQUIVALENT | EQUIVALENT |
| `part_but.socialize` | comportement | 0.0586 | 0.0579 | -0.000759 | [-0.00722 ; 0.0062] | [-0.00624 ; 0.00513] | 0.03 | 0.84 | 1 | -0.045 | EQUIVALENT | EQUIVALENT |
| `part_dedans` | comportement | 0.361 | 0.358 | -0.00328 | [-0.02 ; 0.0134] | [-0.0176 ; 0.0113] | 0.03 | 0.7 | 1 | -0.072 | EQUIVALENT | EQUIVALENT |
| `part_observer` | comportement | 0.00667 | 0.00667 | 0 | [0 ; 0] | [0 ; 0] | 0.02 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `batiments_fin` | monde | 3 | 3 | 0 | [0 ; 0] | [0 ; 0] | 0.5 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `bois_preleve_tuiles` | monde | -3.9e+03 | -3.9e+03 | -0.35 | [-1.95 ; 1.23] | [-1.68 ; 0.975] | 584 | 0.7 | 1 | -0.1 | EQUIVALENT | EQUIVALENT |
| `energie_moy` | monde | 76.2 | 75.6 | -0.552 | [-2.24 ; 1.09] | [-1.96 ; 0.833] | 3 | 0.52 | 1 | -0.17 | EQUIVALENT | EQUIVALENT |
| `faim_moy` | monde | 37 | 37.6 | 0.531 | [-0.471 ; 1.59] | [-0.293 ; 1.4] | 3 | 0.31 | 1 | 0.29 | EQUIVALENT | EQUIVALENT |
| `hygiene_moy` | monde | 66.6 | 67.2 | 0.562 | [-0.816 ; 2.05] | [-0.585 ; 1.81] | 3 | 0.44 | 1 | 0.21 | EQUIVALENT | EQUIVALENT |
| `liens_positifs_fin` | monde | 12.1 | 11.8 | -0.25 | [-1.35 ; 0.75] | [-1.15 ; 0.6] | 2.42 | 0.71 | 1 | -0.13 | EQUIVALENT | EQUIVALENT |
| `liens_somme_fin` | monde | 186 | 175 | -11.2 | [-36.4 ; 16] | [-33.2 ; 11.7] | 37.2 | 0.43 | 1 | -0.2 | EQUIVALENT | EQUIVALENT |
| `loisir_moy` | monde | 81.9 | 81.2 | -0.707 | [-1.9 ; 0.461] | [-1.71 ; 0.287] | 3 | 0.25 | 1 | -0.33 | EQUIVALENT | EQUIVALENT |
| `moral_moy` | monde | 87.8 | 87.6 | -0.163 | [-0.871 ; 0.59] | [-0.767 ; 0.457] | 3 | 0.67 | 1 | -0.11 | EQUIVALENT | EQUIVALENT |
| `nourriture_recoltee_tuiles` | monde | -1.43e+04 | -1.43e+04 | -22.1 | [-76.8 ; 28.8] | [-66.2 ; 21.2] | 1.43e+03 | 0.41 | 1 | -0.27 | EQUIVALENT | EQUIVALENT |
| `oui_dire_fin` | monde | 0.775 | 0.45 | -0.325 | [-0.875 ; 0.225] | [-0.775 ; 0.125] | 1 | 0.3 | 1 | -0.26 | EQUIVALENT | EQUIVALENT |
| `part_faim_forte` | monde | 0.0485 | 0.0572 | 0.00869 | [-0.00533 ; 0.0236] | [-0.00308 ; 0.0207] | 0.03 | 0.24 | 1 | 0.4 | EQUIVALENT | EQUIVALENT |
| `part_soif_forte` | monde | 0.0976 | 0.106 | 0.00791 | [-0.019 ; 0.0354] | [-0.0146 ; 0.0317] | 0.03 | 0.58 | 1 | 0.14 | INDETERMINE | INDETERMINE |
| `pierre_prelevee_tuiles` | monde | 63 | 61.3 | -1.73 | [-5.73 ; 2.45] | [-5.17 ; 1.77] | 9.45 | 0.42 | 1 | -0.21 | EQUIVALENT | EQUIVALENT |
| `population_fin` | monde | 5 | 5 | 0 | [0 ; 0] | [0 ; 0] | 0.1 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `sante_min` | monde | 89.5 | 85.5 | -3.98 | [-9.57 ; 1.36] | [-8.56 ; 0.502] | 5 | 0.15 | 1 | -0.43 | INDETERMINE | INDETERMINE |
| `social_moy` | monde | 71.8 | 71.5 | -0.36 | [-2.61 ; 1.83] | [-2.22 ; 1.48] | 3 | 0.76 | 1 | -0.087 | EQUIVALENT | EQUIVALENT |
| `soif_moy` | monde | 37.8 | 38 | 0.145 | [-1.83 ; 2.07] | [-1.52 ; 1.8] | 3 | 0.89 | 1 | 0.033 | EQUIVALENT | EQUIVALENT |
| `stock_bati_moy` | monde | 27.5 | 27.5 | -0.00343 | [-2.63 ; 2.39] | [-2.15 ; 2.01] | 2.75 | 1 | 1 | -0.0006 | EQUIVALENT | EQUIVALENT |
| `survivants_initiaux` | monde | 5 | 5 | 0 | [0 ; 0] | [0 ; 0] | 0.1 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `tresor_fin` | monde | 12 | 11.4 | -0.55 | [-1.1 ; -0.075] | [-1 ; -0.15] | 5 | 0.042 | 1 | -0.37 | EQUIVALENT | EQUIVALENT |

## genese — principal : A = `aucun`, B = `n05` — N = 30, D = 3 j

**Verdict statistique (regle v2, Holm) : RUPTURE** — nature mesuree : **rupture**  
Regle v1 (declaree, sans correction) : RUPTURE / rupture — 24 grandeur(s) DIFFERENT, 4 INDETERMINE  
**Verdict au bit : DIVERGE** (30/30 paires ; premier tick divergent median 1, min 1, max 1)

Activation (B) : 9.5e+04 par passage en moyenne (min 80296). Cout d'un passage : A 8.89 s, B 12.2 s (medianes).
Decorrelation (sep. A/B au dernier jour / sep. temoin/temoin-bis) : 1.11 [IC95 1.01 ; 1.21], n = 30. 1re separation >= 1 case : median 2 s.

| t (s) | sep. positions A/B (cases, mediane) | sep. besoins A/B | sep. positions temoin/bis |
|---:|---:|---:|---:|
| 0.0 | 0 | 0 | 0 |
| 25.0 | 9.05 | 1.74 | 4.84 |
| 49.0 | 6.99 | 8.37 | 4.64 |
| 74.0 | 5.33 | 13.8 | 4.77 |
| 98.0 | 8.92 | 16.1 | 8.73 |
| 123.0 | 9.46 | 16.8 | 7.96 |
| 147.0 | 3.33 | 17.1 | 3.32 |
| 172.0 | 7.59 | 15.4 | 6.33 |
| 196.0 | 8.75 | 15.4 | 8.9 |
| 221.0 | 7.03 | 15.1 | 6.41 |
| 245.0 | 2.43 | 14.7 | 2.33 |
| 270.0 | 10.2 | 15.7 | 8.9 |

Criteres de rupture declenches : ampleur : duree_episode.observer -2.54 > 3 x delta (0.748) ; ampleur : part_dedans +0.0979 > 3 x delta (0.03)

| grandeur | echelle | A | B | diff | IC95 | IC90 | delta | p | p Holm | effet (sd) | v1 | v2 (Holm) |
|---|---|---:|---:|---:|---|---|---:|---:|---:|---:|---|---|
| `changements_but_par_habitant_jour` | comportement | 7.69 | 10.7 | 3.02 | [2.34 ; 3.69] | [2.44 ; 3.58] | 1.15 | 0.00025 | 0.012 | 2.1 | DIFFERENT | DIFFERENT |
| `duree_episode.build` | comportement | 20.8 | 14.8 | -5.95 | [-8.8 ; -3.51] | [-8.29 ; -3.8] | 4.15 | 0.00025 | 0.012 | -0.83 | DIFFERENT | DIFFERENT |
| `duree_episode.deliver` | comportement | 7.76 | 4.23 | -3.53 | [-5.13 ; -1.96] | [-4.88 ; -2.21] | 1.55 | 0.0005 | 0.016 | -0.79 | DIFFERENT | DIFFERENT |
| `duree_episode.drink` | comportement | 4.3 | 2.48 | -1.82 | [-2.18 ; -1.44] | [-2.13 ; -1.51] | 0.86 | 0.00025 | 0.012 | -1.8 | DIFFERENT | DIFFERENT |
| `duree_episode.eat` | comportement | 9.37 | 5.32 | -4.05 | [-6.1 ; -2.33] | [-5.73 ; -2.57] | 1.87 | 0.00025 | 0.012 | -0.81 | DIFFERENT | DIFFERENT |
| `duree_episode.gatherStone` | comportement | 9.81 | 4.25 | -5.57 | [-7.13 ; -4.13] | [-6.89 ; -4.33] | 1.96 | 0.00025 | 0.012 | -1.2 | DIFFERENT | DIFFERENT |
| `duree_episode.haulJob` | comportement | 18.9 | 13.4 | -5.49 | [-8.56 ; -2.71] | [-7.98 ; -3.07] | 3.77 | 0.001 | 0.029 | -0.67 | DIFFERENT | DIFFERENT |
| `duree_episode.helpFarm` | comportement | 36.3 | 20.5 | -15.8 | [-28.4 ; -4.73] | [-26.1 ; -6.21] | 7.26 | 0.013 | 0.28 | -0.56 | DIFFERENT | INDETERMINE |
| `duree_episode.observer` | comportement | 3.74 | 1.2 | -2.54 | [-3.41 ; -1.69] | [-3.26 ; -1.82] | 0.748 | 0.00025 | 0.012 | -1.1 | DIFFERENT | DIFFERENT |
| `duree_episode.rest` | comportement | 20.6 | 22.2 | 1.54 | [-0.0263 ; 3.11] | [0.242 ; 2.9] | 4.13 | 0.074 | 1 | 0.48 | EQUIVALENT | EQUIVALENT |
| `duree_episode.shelterRain` | comportement | 20.7 | 16.2 | -4.54 | [-9.9 ; 1.32] | [-9.09 ; 0.309] | 4.4 | 0.14 | 1 | -0.44 | INDETERMINE | INDETERMINE |
| `duree_episode.socialize` | comportement | 7.79 | 6.64 | -1.15 | [-2.09 ; -0.182] | [-1.94 ; -0.317] | 1.56 | 0.033 | 0.6 | -0.52 | DIFFERENT | INDETERMINE |
| `duree_episode.visitFamily` | comportement | 19.7 | 17.3 | -2.42 | [-6.25 ; 1.28] | [-5.61 ; 0.761] | 4.02 | 0.22 | 1 | -0.25 | INDETERMINE | INDETERMINE |
| `duree_episode_moy_s` | comportement | 11.5 | 8.23 | -3.27 | [-4.02 ; -2.5] | [-3.91 ; -2.62] | 1.72 | 0.00025 | 0.012 | -1.6 | DIFFERENT | DIFFERENT |
| `part_but.build` | comportement | 0.0868 | 0.0697 | -0.0171 | [-0.0279 ; -0.00651] | [-0.0264 ; -0.00804] | 0.03 | 0.0055 | 0.12 | -0.45 | EQUIVALENT | EQUIVALENT |
| `part_but.deliver` | comportement | 0.048 | 0.0779 | 0.0298 | [0.0206 ; 0.0394] | [0.022 ; 0.0377] | 0.03 | 0.00025 | 0.012 | 1.4 | DIFFERENT | DIFFERENT |
| `part_but.drink` | comportement | 0.0271 | 0.0158 | -0.0113 | [-0.0139 ; -0.00892] | [-0.0135 ; -0.00932] | 0.03 | 0.00025 | 0.012 | -1.5 | EQUIVALENT | EQUIVALENT |
| `part_but.eat` | comportement | 0.117 | 0.0765 | -0.0408 | [-0.0599 ; -0.0238] | [-0.0563 ; -0.0266] | 0.03 | 0.0005 | 0.016 | -0.92 | DIFFERENT | DIFFERENT |
| `part_but.gatherStone` | comportement | 0.061 | 0.0435 | -0.0175 | [-0.0259 ; -0.00878] | [-0.0247 ; -0.0101] | 0.03 | 0.0005 | 0.016 | -0.71 | EQUIVALENT | EQUIVALENT |
| `part_but.haulJob` | comportement | 0.145 | 0.149 | 0.00411 | [-0.0159 ; 0.0246] | [-0.0127 ; 0.0212] | 0.03 | 0.69 | 1 | 0.097 | EQUIVALENT | EQUIVALENT |
| `part_but.helpFarm` | comportement | 0.0563 | 0.0514 | -0.00499 | [-0.0189 ; 0.00837] | [-0.0164 ; 0.00592] | 0.03 | 0.47 | 1 | -0.16 | EQUIVALENT | EQUIVALENT |
| `part_but.observer` | comportement | 0.021 | 0.00594 | -0.015 | [-0.0201 ; -0.00994] | [-0.0193 ; -0.0107] | 0.03 | 0.00025 | 0.012 | -1.1 | EQUIVALENT | EQUIVALENT |
| `part_but.rest` | comportement | 0.262 | 0.297 | 0.035 | [0.0178 ; 0.0517] | [0.0205 ; 0.0491] | 0.03 | 0.0012 | 0.035 | 1.1 | DIFFERENT | DIFFERENT |
| `part_but.shelterRain` | comportement | 0.0361 | 0.0266 | -0.00943 | [-0.0194 ; 0.00108] | [-0.0177 ; -0.000661] | 0.03 | 0.09 | 1 | -0.3 | EQUIVALENT | EQUIVALENT |
| `part_but.socialize` | comportement | 0.0465 | 0.0463 | -0.000154 | [-0.00849 ; 0.00875] | [-0.0072 ; 0.00732] | 0.03 | 0.97 | 1 | -0.0077 | EQUIVALENT | EQUIVALENT |
| `part_but.visitFamily` | comportement | 0.0435 | 0.0594 | 0.0159 | [0.00207 ; 0.0296] | [0.00416 ; 0.0275] | 0.03 | 0.03 | 0.58 | 0.59 | EQUIVALENT | EQUIVALENT |
| `part_dedans` | comportement | 0.337 | 0.435 | 0.0979 | [0.0691 ; 0.129] | [0.0735 ; 0.123] | 0.03 | 0.00025 | 0.012 | 1.5 | DIFFERENT | DIFFERENT |
| `part_observer` | comportement | 0.021 | 0.00594 | -0.015 | [-0.0201 ; -0.00994] | [-0.0193 ; -0.0107] | 0.02 | 0.00025 | 0.012 | -1.1 | EQUIVALENT | EQUIVALENT |
| `batiments_fin` | monde | 6.87 | 7.63 | 0.767 | [0.433 ; 1.1] | [0.5 ; 1.03] | 0.5 | 0.00075 | 0.022 | 1.2 | DIFFERENT | DIFFERENT |
| `bois_preleve_tuiles` | monde | -5.09e+03 | -5.08e+03 | 12.4 | [-35.1 ; 57.8] | [-27.3 ; 50.5] | 763 | 0.61 | 1 | 0.0069 | EQUIVALENT | EQUIVALENT |
| `energie_moy` | monde | 77.6 | 81.6 | 4.02 | [2.32 ; 5.83] | [2.58 ; 5.54] | 3 | 0.00025 | 0.012 | 0.86 | DIFFERENT | DIFFERENT |
| `faim_moy` | monde | 37.2 | 36.3 | -0.889 | [-2.14 ; 0.393] | [-1.96 ; 0.179] | 3 | 0.17 | 1 | -0.32 | EQUIVALENT | EQUIVALENT |
| `hygiene_moy` | monde | 63.6 | 67.2 | 3.57 | [1.7 ; 5.45] | [2.01 ; 5.15] | 3 | 0.0012 | 0.035 | 0.59 | DIFFERENT | DIFFERENT |
| `liens_positifs_fin` | monde | 20.9 | 23.5 | 2.57 | [0.1 ; 5.07] | [0.533 ; 4.63] | 4.18 | 0.06 | 1 | 0.43 | DIFFERENT | INDETERMINE |
| `liens_somme_fin` | monde | 685 | 766 | 80.9 | [23.9 ; 138] | [33.9 ; 130] | 137 | 0.013 | 0.28 | 0.7 | EQUIVALENT | EQUIVALENT |
| `loisir_moy` | monde | 80 | 85.2 | 5.16 | [3.36 ; 7.17] | [3.61 ; 6.79] | 3 | 0.00025 | 0.012 | 0.9 | DIFFERENT | DIFFERENT |
| `moral_moy` | monde | 89.4 | 91.5 | 2.13 | [1.2 ; 3.06] | [1.35 ; 2.9] | 3 | 0.00025 | 0.012 | 0.79 | EQUIVALENT | EQUIVALENT |
| `nourriture_recoltee_tuiles` | monde | -6.74e+03 | -6.86e+03 | -119 | [-195 ; -41.8] | [-183 ; -53] | 674 | 0.0052 | 0.12 | -0.036 | EQUIVALENT | EQUIVALENT |
| `oui_dire_fin` | monde | 1.93 | 2.17 | 0.233 | [-0.967 ; 1.53] | [-0.767 ; 1.33] | 1 | 0.78 | 1 | 0.11 | INDETERMINE | INDETERMINE |
| `part_faim_forte` | monde | 0.0328 | 0.0242 | -0.00864 | [-0.0196 ; 0.00179] | [-0.0179 ; 0.00032] | 0.03 | 0.13 | 1 | -0.31 | EQUIVALENT | EQUIVALENT |
| `part_soif_forte` | monde | 0.0394 | 0.0295 | -0.00994 | [-0.0237 ; 0.00388] | [-0.0217 ; 0.00153] | 0.03 | 0.17 | 1 | -0.27 | EQUIVALENT | EQUIVALENT |
| `pierre_prelevee_tuiles` | monde | 93.2 | 124 | 30.7 | [0.433 ; 65.1] | [5.5 ; 59.9] | 14 | 0.075 | 1 | 0.21 | DIFFERENT | INDETERMINE |
| `population_fin` | monde | 6.7 | 6.83 | 0.133 | [-0.167 ; 0.433] | [-0.133 ; 0.4] | 0.1 | 0.55 | 1 | 0.15 | INDETERMINE | INDETERMINE |
| `sante_min` | monde | 76.2 | 76.2 | -0.0255 | [-2.45 ; 2.4] | [-2.05 ; 2.02] | 5 | 0.98 | 1 | -0.0047 | EQUIVALENT | EQUIVALENT |
| `social_moy` | monde | 72.9 | 78.7 | 5.83 | [2.38 ; 9.25] | [2.98 ; 8.72] | 3 | 0.0042 | 0.1 | 0.66 | DIFFERENT | INDETERMINE |
| `soif_moy` | monde | 35.3 | 32.8 | -2.51 | [-3.94 ; -1.06] | [-3.74 ; -1.27] | 3 | 0.0017 | 0.045 | -0.62 | DIFFERENT | DIFFERENT |
| `stock_bati_moy` | monde | 143 | 152 | 8.55 | [5.42 ; 11.9] | [5.88 ; 11.4] | 14.3 | 0.00025 | 0.012 | 1.7 | EQUIVALENT | EQUIVALENT |
| `survivants_initiaux` | monde | 5 | 5 | 0 | [0 ; 0] | [0 ; 0] | 0.1 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `tresor_fin` | monde | 338 | 282 | -55.9 | [-91.6 ; -23.1] | [-85.5 ; -27.5] | 33.8 | 0.0025 | 0.062 | -0.81 | DIFFERENT | INDETERMINE |

Prediction : nature predite **derivant**, mesuree **rupture** → **REFUTEE**
- `bit.tDivReduite` attendu <=1 — mesure 1 — TENU
- `micro.decorrelation` attendu >=0.8 — mesure 1.11 — TENU
- `changements_but_par_habitant_jour` attendu hausse — mesure 3.02 — TENU
- `faim_moy` attendu equivalent — mesure -0.889 — TENU
- `survivants_initiaux` attendu equivalent — mesure 0 — TENU

## genese — controle : A = `vue_village`, B = `n05` — N = 30, D = 3 j

**Verdict statistique (regle v2, Holm) : INDETERMINE** — nature mesuree : **indeterminee (N insuffisant)**  
Regle v1 (declaree, sans correction) : DERIVE / derivant — 1 grandeur(s) DIFFERENT, 6 INDETERMINE  
**Verdict au bit : DIVERGE** (30/30 paires ; premier tick divergent median 766, min 115, max 2604)

Activation (B) : 9.5e+04 par passage en moyenne (min 80296). Cout d'un passage : A 11.6 s, B 12.2 s (medianes).
Decorrelation (sep. A/B au dernier jour / sep. temoin/temoin-bis) : 1.07 [IC95 0.949 ; 1.19], n = 30. 1re separation >= 1 case : median 21 s.

| t (s) | sep. positions A/B (cases, mediane) | sep. besoins A/B | sep. positions temoin/bis |
|---:|---:|---:|---:|
| 0.0 | 0 | 0 | 0 |
| 25.0 | 2.22 | 0.0275 | 4.84 |
| 49.0 | 2.22 | 3.12 | 4.64 |
| 74.0 | 2.15 | 5.61 | 4.77 |
| 98.0 | 7.47 | 9.44 | 8.73 |
| 123.0 | 6.53 | 12.9 | 7.96 |
| 147.0 | 2.85 | 13 | 3.32 |
| 172.0 | 6.66 | 11 | 6.33 |
| 196.0 | 7.23 | 12.5 | 8.9 |
| 221.0 | 7.15 | 12.1 | 6.41 |
| 245.0 | 3.32 | 12.9 | 2.33 |
| 270.0 | 8.68 | 12.8 | 8.9 |

| grandeur | echelle | A | B | diff | IC95 | IC90 | delta | p | p Holm | effet (sd) | v1 | v2 (Holm) |
|---|---|---:|---:|---:|---|---|---:|---:|---:|---:|---|---|
| `changements_but_par_habitant_jour` | comportement | 11.1 | 10.7 | -0.373 | [-0.848 ; 0.126] | [-0.783 ; 0.043] | 1.66 | 0.15 | 1 | -0.26 | EQUIVALENT | EQUIVALENT |
| `duree_episode.build` | comportement | 14.4 | 14.8 | 0.392 | [-1.06 ; 1.82] | [-0.823 ; 1.59] | 2.88 | 0.6 | 1 | 0.1 | EQUIVALENT | EQUIVALENT |
| `duree_episode.deliver` | comportement | 4.15 | 4.23 | 0.0789 | [-0.577 ; 0.674] | [-0.457 ; 0.576] | 0.83 | 0.83 | 1 | 0.045 | EQUIVALENT | EQUIVALENT |
| `duree_episode.eat` | comportement | 4.95 | 5.32 | 0.377 | [-0.396 ; 1.09] | [-0.261 ; 1] | 0.99 | 0.33 | 1 | 0.15 | INDETERMINE | INDETERMINE |
| `duree_episode.explore` | comportement | 3.86 | 4.61 | 0.746 | [0.0115 ; 1.5] | [0.125 ; 1.38] | 0.773 | 0.065 | 1 | 0.55 | DIFFERENT | INDETERMINE |
| `duree_episode.gatherStone` | comportement | 4.48 | 4.25 | -0.238 | [-0.759 ; 0.219] | [-0.675 ; 0.152] | 0.897 | 0.37 | 1 | -0.11 | EQUIVALENT | EQUIVALENT |
| `duree_episode.haulJob` | comportement | 12.5 | 13.4 | 0.87 | [-0.419 ; 2.19] | [-0.237 ; 1.99] | 2.5 | 0.2 | 1 | 0.21 | EQUIVALENT | EQUIVALENT |
| `duree_episode.helpFarm` | comportement | 21.6 | 20.5 | -1.08 | [-8.51 ; 6.62] | [-7.44 ; 5.22] | 4.32 | 0.8 | 1 | -0.048 | INDETERMINE | INDETERMINE |
| `duree_episode.rest` | comportement | 21.2 | 22.2 | 0.959 | [-0.285 ; 2.28] | [-0.0873 ; 2.07] | 4.24 | 0.17 | 1 | 0.25 | EQUIVALENT | EQUIVALENT |
| `duree_episode.shelterRain` | comportement | 14 | 16.8 | 2.81 | [-2.1 ; 7.9] | [-1.28 ; 7.03] | 2.67 | 0.29 | 1 | 0.36 | INDETERMINE | INDETERMINE |
| `duree_episode.socialize` | comportement | 6.96 | 6.64 | -0.322 | [-1.22 ; 0.635] | [-1.09 ; 0.46] | 1.39 | 0.51 | 1 | -0.15 | EQUIVALENT | EQUIVALENT |
| `duree_episode.visitFamily` | comportement | 16.9 | 17.3 | 0.466 | [-3.53 ; 4.59] | [-2.9 ; 3.91] | 3.38 | 0.83 | 1 | 0.046 | INDETERMINE | INDETERMINE |
| `duree_episode_moy_s` | comportement | 7.98 | 8.23 | 0.251 | [-0.0959 ; 0.583] | [-0.0385 ; 0.541] | 1.2 | 0.16 | 1 | 0.23 | EQUIVALENT | EQUIVALENT |
| `part_but.build` | comportement | 0.0681 | 0.0697 | 0.00163 | [-0.00431 ; 0.00728] | [-0.00333 ; 0.00646] | 0.03 | 0.6 | 1 | 0.064 | EQUIVALENT | EQUIVALENT |
| `part_but.deliver` | comportement | 0.0752 | 0.0779 | 0.00264 | [-0.00477 ; 0.00988] | [-0.00356 ; 0.00884] | 0.03 | 0.5 | 1 | 0.086 | EQUIVALENT | EQUIVALENT |
| `part_but.eat` | comportement | 0.0713 | 0.0765 | 0.00514 | [-0.00659 ; 0.0161] | [-0.00446 ; 0.0143] | 0.03 | 0.41 | 1 | 0.17 | EQUIVALENT | EQUIVALENT |
| `part_but.explore` | comportement | 0.0204 | 0.0212 | 0.000765 | [-0.00592 ; 0.00772] | [-0.00468 ; 0.00651] | 0.03 | 0.85 | 1 | 0.054 | EQUIVALENT | EQUIVALENT |
| `part_but.gatherStone` | comportement | 0.0481 | 0.0435 | -0.00454 | [-0.00912 ; -0.00013] | [-0.00835 ; -0.000863] | 0.03 | 0.058 | 1 | -0.23 | EQUIVALENT | EQUIVALENT |
| `part_but.haulJob` | comportement | 0.159 | 0.149 | -0.0105 | [-0.0322 ; 0.0108] | [-0.0289 ; 0.00716] | 0.03 | 0.36 | 1 | -0.21 | EQUIVALENT | EQUIVALENT |
| `part_but.helpFarm` | comportement | 0.0475 | 0.0514 | 0.00389 | [-0.00668 ; 0.0141] | [-0.00464 ; 0.0125] | 0.03 | 0.48 | 1 | 0.12 | EQUIVALENT | EQUIVALENT |
| `part_but.rest` | comportement | 0.297 | 0.297 | -0.000472 | [-0.0154 ; 0.0154] | [-0.0133 ; 0.0127] | 0.03 | 0.95 | 1 | -0.012 | EQUIVALENT | EQUIVALENT |
| `part_but.shelterRain` | comportement | 0.023 | 0.0266 | 0.00367 | [-0.00207 ; 0.0104] | [-0.00131 ; 0.00916] | 0.03 | 0.28 | 1 | 0.15 | EQUIVALENT | EQUIVALENT |
| `part_but.socialize` | comportement | 0.0449 | 0.0463 | 0.00135 | [-0.00785 ; 0.0106] | [-0.00642 ; 0.00931] | 0.03 | 0.78 | 1 | 0.066 | EQUIVALENT | EQUIVALENT |
| `part_but.visitFamily` | comportement | 0.0645 | 0.0594 | -0.00511 | [-0.0167 ; 0.00677] | [-0.015 ; 0.00506] | 0.03 | 0.44 | 1 | -0.18 | EQUIVALENT | EQUIVALENT |
| `part_dedans` | comportement | 0.431 | 0.435 | 0.00419 | [-0.0139 ; 0.0232] | [-0.0108 ; 0.0199] | 0.03 | 0.67 | 1 | 0.066 | EQUIVALENT | EQUIVALENT |
| `part_observer` | comportement | 0.00552 | 0.00594 | 0.000423 | [-0.000377 ; 0.00133] | [-0.000278 ; 0.00119] | 0.02 | 0.36 | 1 | 0.24 | EQUIVALENT | EQUIVALENT |
| `batiments_fin` | monde | 7.6 | 7.63 | 0.0333 | [-0.233 ; 0.3] | [-0.2 ; 0.267] | 0.5 | 1 | 1 | 0.043 | EQUIVALENT | EQUIVALENT |
| `bois_preleve_tuiles` | monde | -5.08e+03 | -5.08e+03 | 8.5 | [-16.8 ; 38.1] | [-13.5 ; 32.3] | 763 | 0.62 | 1 | 0.0047 | EQUIVALENT | EQUIVALENT |
| `energie_moy` | monde | 81.7 | 81.6 | -0.066 | [-1.2 ; 1.1] | [-1.01 ; 0.892] | 3 | 0.91 | 1 | -0.017 | EQUIVALENT | EQUIVALENT |
| `faim_moy` | monde | 36.7 | 36.3 | -0.347 | [-1.19 ; 0.463] | [-1.05 ; 0.318] | 3 | 0.41 | 1 | -0.11 | EQUIVALENT | EQUIVALENT |
| `hygiene_moy` | monde | 67.5 | 67.2 | -0.355 | [-1.45 ; 0.69] | [-1.27 ; 0.559] | 3 | 0.53 | 1 | -0.087 | EQUIVALENT | EQUIVALENT |
| `liens_positifs_fin` | monde | 23.7 | 23.5 | -0.2 | [-2.87 ; 2.43] | [-2.47 ; 2.07] | 4.73 | 0.91 | 1 | -0.029 | EQUIVALENT | EQUIVALENT |
| `liens_somme_fin` | monde | 776 | 766 | -10.1 | [-57.9 ; 43] | [-50.9 ; 34.3] | 155 | 0.7 | 1 | -0.088 | EQUIVALENT | EQUIVALENT |
| `loisir_moy` | monde | 85.9 | 85.2 | -0.722 | [-1.72 ; 0.339] | [-1.55 ; 0.167] | 3 | 0.18 | 1 | -0.18 | EQUIVALENT | EQUIVALENT |
| `moral_moy` | monde | 91.7 | 91.5 | -0.221 | [-0.787 ; 0.381] | [-0.693 ; 0.293] | 3 | 0.47 | 1 | -0.11 | EQUIVALENT | EQUIVALENT |
| `nourriture_recoltee_tuiles` | monde | -6.85e+03 | -6.86e+03 | -14.8 | [-84.6 ; 53] | [-71.8 ; 40.8] | 685 | 0.67 | 1 | -0.0043 | EQUIVALENT | EQUIVALENT |
| `oui_dire_fin` | monde | 1.8 | 2.17 | 0.367 | [-0.867 ; 1.53] | [-0.602 ; 1.33] | 1 | 0.6 | 1 | 0.13 | INDETERMINE | INDETERMINE |
| `part_faim_forte` | monde | 0.0235 | 0.0242 | 0.000649 | [-0.00903 ; 0.0107] | [-0.00753 ; 0.00896] | 0.03 | 0.9 | 1 | 0.025 | EQUIVALENT | EQUIVALENT |
| `part_soif_forte` | monde | 0.027 | 0.0295 | 0.00248 | [-0.00938 ; 0.0153] | [-0.00733 ; 0.0133] | 0.03 | 0.7 | 1 | 0.059 | EQUIVALENT | EQUIVALENT |
| `pierre_prelevee_tuiles` | monde | 129 | 124 | -4.93 | [-16 ; 6.47] | [-14.3 ; 4.23] | 19.3 | 0.43 | 1 | -0.031 | EQUIVALENT | EQUIVALENT |
| `population_fin` | monde | 6.83 | 6.83 | 0 | [-0.333 ; 0.367] | [-0.267 ; 0.3] | 0.1 | 1 | 1 | 0 | INDETERMINE | INDETERMINE |
| `sante_min` | monde | 74.9 | 76.2 | 1.32 | [-0.984 ; 3.66] | [-0.614 ; 3.3] | 5 | 0.29 | 1 | 0.23 | EQUIVALENT | EQUIVALENT |
| `social_moy` | monde | 79.8 | 78.7 | -1.07 | [-2.9 ; 0.762] | [-2.56 ; 0.448] | 3 | 0.27 | 1 | -0.21 | EQUIVALENT | EQUIVALENT |
| `soif_moy` | monde | 32.4 | 32.8 | 0.41 | [-0.642 ; 1.44] | [-0.481 ; 1.29] | 3 | 0.44 | 1 | 0.11 | EQUIVALENT | EQUIVALENT |
| `stock_bati_moy` | monde | 151 | 152 | 1.12 | [-1.22 ; 3.67] | [-0.872 ; 3.23] | 15.1 | 0.4 | 1 | 0.14 | EQUIVALENT | EQUIVALENT |
| `survivants_initiaux` | monde | 5 | 5 | 0 | [0 ; 0] | [0 ; 0] | 0.1 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `tresor_fin` | monde | 284 | 282 | -2.27 | [-16.7 ; 14.7] | [-14.7 ; 11.8] | 28.4 | 0.8 | 1 | -0.019 | EQUIVALENT | EQUIVALENT |

Prediction : nature predite **chaotique**, mesuree **indeterminee (N insuffisant)** → **REFUTEE**
- `micro.decorrelation` attendu >=0.8 — mesure 1.07 — TENU
- `changements_but_par_habitant_jour` attendu equivalent — mesure -0.373 — TENU

## genese — calibration_AA:aucun/aucun@bis : A = `aucun`, B = `aucun@bis` — N = 30, D = 3 j

**Verdict statistique (regle v2, Holm) : INDETERMINE** — nature mesuree : **indeterminee (N insuffisant)**  
Regle v1 (declaree, sans correction) : DERIVE / derivant — 1 grandeur(s) DIFFERENT, 8 INDETERMINE  
**Verdict au bit : DIVERGE** (30/30 paires ; premier tick divergent median 0, min 0, max 0)

Activation (B) : 0 par passage en moyenne (min 0). Cout d'un passage : A 8.89 s, B 9.1 s (medianes).
Decorrelation (sep. A/B au dernier jour / sep. temoin/temoin-bis) : 1 [IC95 1 ; 1], n = 30. 1re separation >= 1 case : median 7 s.

| t (s) | sep. positions A/B (cases, mediane) | sep. besoins A/B | sep. positions temoin/bis |
|---:|---:|---:|---:|
| 0.0 | 0 | 0 | 0 |
| 25.0 | 4.84 | 0.23 | 4.84 |
| 49.0 | 4.64 | 3.34 | 4.64 |
| 74.0 | 4.77 | 6.81 | 4.77 |
| 98.0 | 8.73 | 10.9 | 8.73 |
| 123.0 | 7.96 | 13.6 | 7.96 |
| 147.0 | 3.32 | 15.9 | 3.32 |
| 172.0 | 6.33 | 14.2 | 6.33 |
| 196.0 | 8.9 | 13.8 | 8.9 |
| 221.0 | 6.41 | 14.2 | 6.41 |
| 245.0 | 2.33 | 14.2 | 2.33 |
| 270.0 | 8.9 | 14.7 | 8.9 |

| grandeur | echelle | A | B | diff | IC95 | IC90 | delta | p | p Holm | effet (sd) | v1 | v2 (Holm) |
|---|---|---:|---:|---:|---|---|---:|---:|---:|---:|---|---|
| `changements_but_par_habitant_jour` | comportement | 7.69 | 7.77 | 0.0795 | [-0.284 ; 0.431] | [-0.223 ; 0.379] | 1.15 | 0.67 | 1 | 0.055 | EQUIVALENT | EQUIVALENT |
| `duree_episode.build` | comportement | 20.8 | 21.1 | 0.312 | [-2.86 ; 3.42] | [-2.36 ; 2.89] | 4.15 | 0.85 | 1 | 0.043 | EQUIVALENT | EQUIVALENT |
| `duree_episode.deliver` | comportement | 7.76 | 6.96 | -0.8 | [-1.78 ; 0.0608] | [-1.6 ; -0.0585] | 1.55 | 0.093 | 1 | -0.18 | INDETERMINE | INDETERMINE |
| `duree_episode.drink` | comportement | 4.3 | 4.54 | 0.246 | [-0.179 ; 0.687] | [-0.117 ; 0.616] | 0.86 | 0.28 | 1 | 0.24 | EQUIVALENT | EQUIVALENT |
| `duree_episode.eat` | comportement | 9.37 | 9.43 | 0.0558 | [-1.34 ; 1.24] | [-1.06 ; 1.08] | 1.87 | 0.93 | 1 | 0.011 | EQUIVALENT | EQUIVALENT |
| `duree_episode.gatherStone` | comportement | 9.81 | 10.1 | 0.264 | [-0.904 ; 1.48] | [-0.738 ; 1.34] | 1.96 | 0.68 | 1 | 0.059 | EQUIVALENT | EQUIVALENT |
| `duree_episode.haulJob` | comportement | 18.9 | 19.5 | 0.586 | [-4.35 ; 6.6] | [-3.67 ; 5.61] | 3.77 | 0.85 | 1 | 0.072 | INDETERMINE | INDETERMINE |
| `duree_episode.helpFarm` | comportement | 36.3 | 38.4 | 2.12 | [-14 ; 23.3] | [-12.1 ; 19] | 7.26 | 0.87 | 1 | 0.076 | INDETERMINE | INDETERMINE |
| `duree_episode.observer` | comportement | 3.74 | 3.7 | -0.0448 | [-0.156 ; 0.0711] | [-0.14 ; 0.0524] | 0.748 | 0.46 | 1 | -0.019 | EQUIVALENT | EQUIVALENT |
| `duree_episode.rest` | comportement | 20.6 | 21.5 | 0.832 | [-0.54 ; 2.22] | [-0.326 ; 2.01] | 4.13 | 0.26 | 1 | 0.26 | EQUIVALENT | EQUIVALENT |
| `duree_episode.shelterRain` | comportement | 21.5 | 21.8 | 0.328 | [-5.34 ; 6.15] | [-4.59 ; 5.09] | 4.4 | 0.92 | 1 | 0.031 | INDETERMINE | INDETERMINE |
| `duree_episode.socialize` | comportement | 7.79 | 8.01 | 0.223 | [-0.613 ; 1.1] | [-0.49 ; 0.955] | 1.56 | 0.64 | 1 | 0.1 | EQUIVALENT | EQUIVALENT |
| `duree_episode.visitFamily` | comportement | 20.3 | 24.4 | 4.11 | [-0.409 ; 8.91] | [0.154 ; 8.12] | 4.02 | 0.11 | 1 | 0.41 | INDETERMINE | INDETERMINE |
| `duree_episode_moy_s` | comportement | 11.5 | 11.5 | -0.0383 | [-0.49 ; 0.436] | [-0.428 ; 0.354] | 1.72 | 0.88 | 1 | -0.019 | EQUIVALENT | EQUIVALENT |
| `part_but.build` | comportement | 0.0868 | 0.0775 | -0.00925 | [-0.0219 ; 0.00201] | [-0.0199 ; 0.000414] | 0.03 | 0.15 | 1 | -0.24 | EQUIVALENT | EQUIVALENT |
| `part_but.deliver` | comportement | 0.048 | 0.0478 | -0.00019 | [-0.00733 ; 0.00719] | [-0.00628 ; 0.00594] | 0.03 | 0.96 | 1 | -0.0086 | EQUIVALENT | EQUIVALENT |
| `part_but.drink` | comportement | 0.0271 | 0.0272 | 7.24e-05 | [-0.00227 ; 0.00257] | [-0.00192 ; 0.00213] | 0.03 | 0.96 | 1 | 0.0096 | EQUIVALENT | EQUIVALENT |
| `part_but.eat` | comportement | 0.117 | 0.117 | -0.000538 | [-0.0138 ; 0.0122] | [-0.0116 ; 0.0103] | 0.03 | 0.94 | 1 | -0.012 | EQUIVALENT | EQUIVALENT |
| `part_but.gatherStone` | comportement | 0.061 | 0.0666 | 0.00566 | [-0.00463 ; 0.0159] | [-0.00296 ; 0.0144] | 0.03 | 0.3 | 1 | 0.23 | EQUIVALENT | EQUIVALENT |
| `part_but.haulJob` | comportement | 0.145 | 0.139 | -0.00567 | [-0.0205 ; 0.00779] | [-0.0182 ; 0.00598] | 0.03 | 0.46 | 1 | -0.13 | EQUIVALENT | EQUIVALENT |
| `part_but.helpFarm` | comportement | 0.0563 | 0.0595 | 0.00316 | [-0.011 ; 0.0184] | [-0.00884 ; 0.0155] | 0.03 | 0.7 | 1 | 0.1 | EQUIVALENT | EQUIVALENT |
| `part_but.observer` | comportement | 0.021 | 0.0202 | -0.000819 | [-0.00221 ; 0.000641] | [-0.00199 ; 0.000382] | 0.03 | 0.3 | 1 | -0.06 | EQUIVALENT | EQUIVALENT |
| `part_but.rest` | comportement | 0.262 | 0.264 | 0.00224 | [-0.0125 ; 0.0157] | [-0.00965 ; 0.0138] | 0.03 | 0.76 | 1 | 0.068 | EQUIVALENT | EQUIVALENT |
| `part_but.shelterRain` | comportement | 0.0361 | 0.0325 | -0.00357 | [-0.0118 ; 0.00473] | [-0.0105 ; 0.00354] | 0.03 | 0.42 | 1 | -0.11 | EQUIVALENT | EQUIVALENT |
| `part_but.socialize` | comportement | 0.0465 | 0.0474 | 0.000931 | [-0.00565 ; 0.00761] | [-0.00462 ; 0.00659] | 0.03 | 0.78 | 1 | 0.046 | EQUIVALENT | EQUIVALENT |
| `part_but.visitFamily` | comportement | 0.0435 | 0.0457 | 0.00225 | [-0.00708 ; 0.0122] | [-0.00569 ; 0.0107] | 0.03 | 0.66 | 1 | 0.083 | EQUIVALENT | EQUIVALENT |
| `part_dedans` | comportement | 0.337 | 0.34 | 0.00338 | [-0.0146 ; 0.021] | [-0.0115 ; 0.0187] | 0.03 | 0.72 | 1 | 0.052 | EQUIVALENT | EQUIVALENT |
| `part_observer` | comportement | 0.021 | 0.0202 | -0.000819 | [-0.00221 ; 0.000641] | [-0.00199 ; 0.000382] | 0.02 | 0.3 | 1 | -0.06 | EQUIVALENT | EQUIVALENT |
| `batiments_fin` | monde | 6.87 | 6.93 | 0.0667 | [-0.2 ; 0.3] | [-0.133 ; 0.267] | 0.5 | 0.79 | 1 | 0.11 | EQUIVALENT | EQUIVALENT |
| `bois_preleve_tuiles` | monde | -5.09e+03 | -5.13e+03 | -37.5 | [-86.1 ; 8.27] | [-77.8 ; 1.27] | 763 | 0.13 | 1 | -0.021 | EQUIVALENT | EQUIVALENT |
| `energie_moy` | monde | 77.6 | 77.9 | 0.343 | [-1.22 ; 1.94] | [-0.97 ; 1.68] | 3 | 0.68 | 1 | 0.073 | EQUIVALENT | EQUIVALENT |
| `faim_moy` | monde | 37.2 | 36.8 | -0.379 | [-1.6 ; 0.86] | [-1.41 ; 0.633] | 3 | 0.57 | 1 | -0.14 | EQUIVALENT | EQUIVALENT |
| `hygiene_moy` | monde | 63.6 | 63.7 | 0.133 | [-1.91 ; 2.06] | [-1.56 ; 1.78] | 3 | 0.9 | 1 | 0.022 | EQUIVALENT | EQUIVALENT |
| `liens_positifs_fin` | monde | 20.9 | 21.8 | 0.9 | [-1.5 ; 3.5] | [-1.17 ; 3.07] | 4.18 | 0.51 | 1 | 0.15 | EQUIVALENT | EQUIVALENT |
| `liens_somme_fin` | monde | 685 | 692 | 6.78 | [-44 ; 61.3] | [-35.7 ; 51.1] | 137 | 0.8 | 1 | 0.059 | EQUIVALENT | EQUIVALENT |
| `loisir_moy` | monde | 80 | 80.1 | 0.116 | [-1.43 ; 1.64] | [-1.18 ; 1.38] | 3 | 0.88 | 1 | 0.02 | EQUIVALENT | EQUIVALENT |
| `moral_moy` | monde | 89.4 | 88.7 | -0.666 | [-1.38 ; 0.0614] | [-1.26 ; -0.0629] | 3 | 0.088 | 1 | -0.25 | EQUIVALENT | EQUIVALENT |
| `nourriture_recoltee_tuiles` | monde | -6.74e+03 | -6.73e+03 | 8.57 | [-42.4 ; 56.7] | [-33.8 ; 50.2] | 674 | 0.75 | 1 | 0.0026 | EQUIVALENT | EQUIVALENT |
| `oui_dire_fin` | monde | 1.93 | 1.77 | -0.167 | [-1.17 ; 0.967] | [-1.03 ; 0.767] | 1 | 0.8 | 1 | -0.077 | INDETERMINE | INDETERMINE |
| `part_faim_forte` | monde | 0.0328 | 0.0333 | 0.000462 | [-0.014 ; 0.0144] | [-0.0117 ; 0.0119] | 0.03 | 0.95 | 1 | 0.017 | EQUIVALENT | EQUIVALENT |
| `part_soif_forte` | monde | 0.0394 | 0.0448 | 0.00544 | [-0.0101 ; 0.0221] | [-0.00777 ; 0.0191] | 0.03 | 0.53 | 1 | 0.15 | EQUIVALENT | EQUIVALENT |
| `pierre_prelevee_tuiles` | monde | 93.2 | 81 | -12.2 | [-45.5 ; 16.1] | [-39 ; 11.3] | 14 | 0.5 | 1 | -0.082 | INDETERMINE | INDETERMINE |
| `population_fin` | monde | 6.7 | 7.07 | 0.367 | [-0.0333 ; 0.8] | [0.0333 ; 0.733] | 0.1 | 0.13 | 1 | 0.42 | INDETERMINE | INDETERMINE |
| `sante_min` | monde | 76.2 | 71.2 | -5.02 | [-9.25 ; -1.3] | [-8.48 ; -1.78] | 5 | 0.013 | 0.65 | -0.93 | DIFFERENT | INDETERMINE |
| `social_moy` | monde | 72.9 | 72.2 | -0.702 | [-3.25 ; 1.99] | [-2.85 ; 1.6] | 3 | 0.63 | 1 | -0.08 | EQUIVALENT | EQUIVALENT |
| `soif_moy` | monde | 35.3 | 35.6 | 0.247 | [-1.05 ; 1.55] | [-0.831 ; 1.33] | 3 | 0.73 | 1 | 0.061 | EQUIVALENT | EQUIVALENT |
| `stock_bati_moy` | monde | 143 | 144 | 0.945 | [-0.12 ; 2.12] | [0.0261 ; 1.93] | 14.3 | 0.13 | 1 | 0.19 | EQUIVALENT | EQUIVALENT |
| `survivants_initiaux` | monde | 5 | 5 | 0 | [0 ; 0] | [0 ; 0] | 0.1 | 1 | 1 | 0 | EQUIVALENT | EQUIVALENT |
| `tresor_fin` | monde | 338 | 335 | -2.37 | [-21.9 ; 17.2] | [-18.4 ; 14.2] | 33.8 | 0.81 | 1 | -0.034 | EQUIVALENT | EQUIVALENT |
