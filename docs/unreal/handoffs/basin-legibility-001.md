# HANDOFF: basin-legibility-001

## MISSION

Mesurer sur le maillage courant si les rivières principales ont deux flancs lisibles
au-delà de leur berge, avant de changer le relief ou l'hydrologie. Une seule hypothèse
géométrique ouverte ; aucun changement du monde joué.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisBasinLegibilityTests.cpp` : test diagnostic nouveau.
- `docs/unreal/BASIN_LEGIBILITY_001.md` : question, protocole, seuils choisis avant mesure, résultat.
- `docs/unreal/handoffs/basin-legibility-001.md` : cette fiche.

## COMMIT

Voir le commit portant cette fiche (`git log -1 --format=%H -- docs/unreal/handoffs/basin-legibility-001.md`).

## MEC

- Premier build isolé : `tools/unreal/anastasis-unreal.ps1 build` → PASS, 209,08 s.
- Build après le nouveau test : même commande → PASS, 76,16 s.
- Test ciblé : `tools/unreal/report-tests.ps1 -Filter 'Anastasis.Terrain.BasinLegibility.Profile'`
  → `TESTS::PASS` (1/1), 0 FAIL ; porte mémoire ouverte après 5,4 min.
- Valeurs : cinq rivières d'ordre ≥ 2, 168 points ; hausse médiane du flanc le plus bas
  à 30/100/200 m = 2,40/4,58/5,03 m ; p10 = 0,38/1,17/1,18 m.
- Décision préinscrite : `REJECT` pour un approfondissement global du relief ou des lits ;
  le seuil conjoint < 2 m à 100 m et < 5 m à 200 m n'est pas atteint.
- Raw log : `Saved/CanonicalVerification/report-tests.log`, lignes `BASIN_PROFILE` et `radius_m=`.
- Suite complète locale : après build PASS, attente `EDITOR_GATE::WAIT` pendant 15 min,
  toujours quatre demandes devant et moins de 2,5 Go disponibles ; arrêt propre de
  cette attente. `finish -Queue` demande la suite au lot. **Aucun PASS complet local.**

## PROOFS

PROOFS: (aucune)

## SCN

UNKNOWN : aucune nouvelle image du commit ; la mesure géométrique ne juge pas la lisibilité à l'œil.

## PLY

UNKNOWN : aucun parcours joueur ou usage de l'eau n'est testé ici.

## ECARTS

AUCUN — `Source/AnastasisSim/` intact.

## INTEGRATION_RISK

- Le test ajoute un calcul complet de la géographie de référence à la suite Unreal.
- Aucun fichier déjà possédé par une mission eau, relief lointain ou végétation n'est modifié.
- Le lot `player-start-002` tenait `MAIN.lock` à l'ouverture de la mission ; vérifier la base
  et les conflits au moment de l'admission.
- Une mission future peut viser le décile plat proche de `(1063 m, 1340 m)` ; ne pas
  généraliser cette mesure locale à toute la carte.

## STOP

Un PASS de l'instrument n'est ni une correction de carte, ni un verdict historique,
SCN ou PLY. La cause visuelle reste à trancher avec une capture actuelle.
