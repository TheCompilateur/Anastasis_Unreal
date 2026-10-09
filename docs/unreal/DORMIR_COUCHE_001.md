# DORMIR_COUCHE_001 — on dort couché, et la nuit l'intérieur n'est plus blanc

## Décision d'Alexandre (2026-10-09)

Après la cabane (`MA_CABANE_001.md`), deux défauts relevés sur ses images, et une consigne : « ainsi pour tout le
monde, PNJ ».

| Défaut | Cause | Correction |
|---|---|---|
| l'intérieur la nuit est blanc | la nuit, l'exposition du jeu est réglée pour le clair de lune (EV100 vers −1) ; le foyer, à deux mètres, est des centaines de fois plus fort | dans le volume habité d'un logis, l'exposition est celle d'une pièce éclairée par son feu (EV100 5,5 la nuit, 9 en plein jour) |
| on dort debout | le joueur restait debout près du banc ; les habitants, eux, étaient **cachés** dès qu'ils entraient | qui dort (« dort », « repose ») est couché sur le dos, sur une banquette-lit de son logis, ou sur une natte au sol quand elles sont prises |

## Où l'on dort

Chaque logis a ses banquettes-lits (sedir) dans le rapport du générateur (`benches`) et son volume habité (`interior`) ;
le catalogue C++ les reprend (`AnastasisArchitecture`, test `Anastasis.Village.Architecture.Rapport`).

| Logis | Banquettes | Places sur les banquettes |
|---|---|---|
| cabane | 1 (3,7 m) | 2 |
| maison pauvre | 1 (3,8 m) | 2 |
| maison moyenne | 2, à l'étage (4,9 et 3,7 m) | 3 |
| ferme | 2, à l'étage (4,3 et 3,2 m) | 3 |

Une place fait 185 cm. Au-delà, des nattes au sol, rang après rang le long de la première banquette, vers le milieu
de la pièce (jamais au-delà de la banquette : une cloison peut couper la pièce). Les dormeurs d'un logis prennent les
places dans l'ordre de leur identifiant : la même nuit, chacun garde la sienne (`SleepSpots`, test
`Anastasis.Village.Architecture.Couchages`).

Les autres activités d'intérieur (manger, se détendre) cachent encore le corps, comme avant.

## L'œil dedans

Un post-process borné par le volume habité de chaque logis, prioritaire sur le volume d'exposition du ciel, fixe
l'exposition entre `anastasis.Village.InteriorNightEV` (5,5) et `anastasis.Village.InteriorDayEV` (9) selon la lumière
du jour ; fondu de 60 cm à la porte. `anastasis.Village.InteriorLight 0` rend l'exposition du dehors (A/B).

## Comment le voir

| Pour | Comment |
|---|---|
| la preuve rejouable | `tools\unreal\editor-batch.ps1 -Proofs sommeil-pie` → `Saved/SleepEvidence/pie/` (quatre images + `sleep.json`) |
| lire l'état | `AnastasisSimulationDebugLibrary.get_sleep_status` (JSON) |
| régler à l'œil | `anastasis.Village.InteriorNightEV`, `InteriorDayEV`, `InteriorLight`, `HearthCandela` |

## Ce qui n'est pas fait

- Couché, le corps garde l'animation de repos debout (bras le long du corps, léger souffle) : pas d'animation de
  sommeil.
- Ce que voit le joueur pendant qu'il dort n'est pas traité (ni fondu au noir, ni regard au plafond).
- Manger, se détendre dedans : le corps reste caché.
