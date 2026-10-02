# HANDOFF: reconsider-mutations-001

## MISSION

Clôture de reconsider-001 (versée dans `main` en 5be9ed6 avant ses mutations), demandée par « Simulateur IV
Kingdoms migration phase 3 » :

1. prouver par des mutations que les six tests réécrits en invariants (Recolte.Cueillette, Livraison,
   Epuisement, Destruction, Plein, MeteoHabitants.Orage) attrapent encore les vraies régressions, et
   renforcer un invariant s'il ne détecte rien ;
2. corriger la preuve PIE `gather-deliver-pie`, qui exigeait un sac > 9 au retour ;
3. corriger la fiche n° 2, qui disait le collant de but non porté.

Aucun changement de comportement du jeu : seuls des tests, une preuve, la fiche n° 2 et la documentation
changent. Le C++ de la simulation est intact (mutations posées puis retirées).

## FILES_OWNED

- `Source/AnastasisSim/Private/Tests/AnastasisVillageGatherTests.cpp` : `Recolte.Cueillette` gagne un
  contrôle isolé du retour forcé (sans pensée)
- `Source/AnastasisSim/Private/Tests/AnastasisVillageWeatherTests.cpp` : `MeteoHabitants.Orage`, la première
  décision sous l'orage doit l'envoyer à l'abri
- `Source/AnastasisSim/ECARTS.md` (n° 2), `Source/AnastasisSim/Public/Village/AnastasisVillage.h` (note n° 2,
  commentaire seulement)
- `tools/unreal/gather-deliver-pie.py`, `tools/unreal/proofs.txt` (`gather-deliver-pie` inscrite), `AGENTS.md`
  (ligne d'index de `gather-deliver-pie`)
- `docs/unreal/handoffs/reconsider-001.md` (tableau des mutations), cette fiche

## COMMIT

Le commit qui porte cette fiche sur `agent/reconsider-mutations-001`.

## MEC

- BUILD : `tools\unreal\anastasis-unreal.ps1 build` → `BUILD::PASS`.
- TESTS (`report-tests.ps1 -Filter Anastasis.Sim`, sans mutation, tests renforcés) : **PASS 120,
  KNOWN_EXPECTED_FAILURE 2** (`Parite.Fbm`, `Parite.SemantiqueJs`), **FAIL 0**, 122/122.
- MUTATIONS : une à la fois, posée, build, suite `Anastasis.Sim` entière, retirée (`mutate.py`, script de
  scratchpad). Tableau complet dans `docs/unreal/handoffs/reconsider-001.md`, section MUTATIONS.

| Test réécrit | Ancienne assertion | Nouvel invariant | Mutation | Détectée par (message) |
|---|---|---|---|---|
| `Recolte.Cueillette` | `5 coups : sac 10` | sac = cueilli, 2 ≤ sac ≤ 10 ; **renforcé** : sans pensée (aucune reconsidération possible), retour forcé à sac 10 | M1 — retour forcé à sac > 9 supprimé | `Recolte.Cueillette` : `sans pensee : retour force a sac 10 (5 coups)` to be 10, but it was 44 ; aussi `Village.Endurance` (solitude critique) |
| `Recolte.Livraison` | `grenier 0 -> 10`, `livre` 10, marché 10, `grenier 20` | grenier = livré = sac du retour ; marché = sac ; grenier = tout ce qui a été livré, deuxième voyage > sac | M2 — livrer sac − 1 | `Recolte.Livraison` : `grenier 0 -> le sac` to be 4, but it was 3 ; `sac vide` 0 / 1 ; `livre` 4 / 3 |
| `Recolte.Epuisement` | `sac 5 : sous le seuil`, `grenier 5` | 5 cueillis ; sac + grenier = 5 ; le grenier finit à 5 | M2 — livrer sac − 1 | `Recolte.Epuisement` : `champ vide` to be 0, but it was 1 |
| `Recolte.Plein` | une livraison, `5 livres`, `5 au sac` | grenier plein à 300 ; 5 livrés ; reste au sac = cueilli − 5 | M2 — livrer sac − 1 | `Recolte.Plein` : `plein a 300` 300 / 299 ; `5 livres` 5 / 4 ; `le reste au sac` 5 / 1 |
| `Recolte.Destruction` | `le sac reste plein` = 10 | le sac reste celui d'avant la démolition | M7 — sac vidé à la démolition | `Recolte.Destruction` : `le sac reste plein` to be 4, but it was 0 |
| `MeteoHabitants.Orage` | `shelterRain` gagne la table, pas de porte, pas de but repris ; reprise : observer | deux chemins selon la table (ligne ou porte + `shelterResumeGoal` = gatherFood) ; **renforcé** : la PREMIÈRE décision sous l'orage l'envoie à l'abri ; reprise = but retenu par la porte | M3 — porte d'orage ignorée | `Orage` : `storm: his first decision under the storm drops the harvest for shelter` (goal=gatherFood) |
| `MeteoHabitants.Orage` | (idem) | (idem) | M4 — `shelterResumeGoal` oublié | `Orage` : `and keeps the harvest to resume` to be "gatherFood", but it was "" ; aussi `TempsSec` (`every storm-gate decision keeps an exposed goal to resume`) |
| — (reconsidération) | — | — | M5 — échelle 0,42 du tirage retirée (`CommitReconsiderScale` = 1) | `Village.Reconsideration` : `tick 165 npc-2 (build) : chance 0.396000 / 0.166320` (puis 74 autres) |
| — (collant) | — | — | M6 — collant de but retiré de la table (`Row.Value += Bonus` supprimé) | `Orage` : première décision sous l'orage = `deliver` (goal=deliver), que la porte d'orage exclut ; avec le collant, la cueillette garde la table et la porte l'envoie à l'abri |

- Deux invariants ne détectaient rien et ont été renforcés (pas les mutations) :
  - `Recolte.Cueillette` : avec la reconsidération, le fermier rentre à sac 4 avant que le retour forcé
    ne joue. Le retour forcé supprimé (M1) passait donc. Ajout d'un contrôle isolé : le même fermier
    sans pensée (`AiThinkAt` repoussé à chaque tick), donc sans reconsidération ; seul
    `shouldHaulGatherLoad` peut le ramener, à sac 10. M1 : `to be 10, but it was 44`.
  - `MeteoHabitants.Orage` acceptait les deux chemins vers l'abri sans dire QUAND. Sans la porte d'orage
    (M3), le fermier y allait plus tard par la ligne de la table. Renforcé : sa PREMIÈRE décision sous
    l'orage doit l'envoyer à l'abri (la porte le garantit, npc.js l. 2055-2071).
- PREUVE PIE `gather-deliver-pie` (corrigée : `03-retour` = but `deliver` avec 1 à 10 au sac, au-delà de 10
  échec explicite ; `04-livre` = le sac du retour entré en entier au grenier, repas compris ; conservation à
  chaque échantillon inchangée), inscrite au registre, rejouée par `editor-batch.ps1 -Proofs gather-deliver-pie` :
  **`PROOF::PASS gather-deliver-pie (38.8s)`**. Étapes : `02-recolte` sac 2 ; `03-retour` sac 10 (t = 42,97) ;
  `04-livre` grenier 10, sac 0 ; `05-large` grenier 20 après le deuxième voyage. Ce run est rentré à sac
  plein ; le motif accepte aussi un retour plus tôt.

## PROOFS

PROOFS: gather-deliver-pie, village-weather-pie

## SCN

Aucune scène, aucun asset.

## PLY

Sans objet.

## ECARTS

- modifié : n° 2 — le collant de but est porté (reconsider-001, 1 944 vecteurs `ReconsiderStickiness`) ; reste
  le relâchement de cible en fin de chantier et de session sociale ; titre, référence, cpp et fermeture
  corrigés (fermeture : à attribuer).

## INTEGRATION_RISK

- Tests seulement côté C++ : `AnastasisVillageGatherTests.cpp` et `AnastasisVillageWeatherTests.cpp`, touchés
  aussi par toute mission de récolte ou de météo.
- `gather-deliver-pie` entre au registre : le lot la rejouera pour les missions qui la déclarent.

## STOP

- Ne revendique aucun nouveau portage. Les mutations sont posées puis retirées ; aucune ne reste dans le code.
