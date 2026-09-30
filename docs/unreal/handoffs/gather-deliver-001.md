# HANDOFF: gather-deliver-001

## MISSION

Fermer la boucle nourriture : un habitant cueille aux champs et livre au grenier, fidèlement à la
référence. Le seul chemin « champ → grenier » du JS est un **fermier dont le poste est le grenier** :
c'est lui qui est porté. Détail : `docs/unreal/GATHER_DELIVER_001.md`.

## FILES_OWNED

Créés :

- `Source/AnastasisSim/Public/Work/AnastasisGather.h`, `Private/Work/AnastasisGather.cpp`
- `Source/AnastasisSim/Private/Tests/AnastasisGatherTests.cpp`, `AnastasisGatherVectors.inl` (généré), `AnastasisVillageGatherTests.cpp`
- `tools/migration/parity/gather.mjs`, `tools/unreal/gather-deliver-pie.ps1`, `tools/unreal/gather-deliver-pie.py`
- `docs/unreal/GATHER_DELIVER_001.md`, cette fiche

Modifiés :

- `Source/AnastasisSim/{Public,Private}/Village/AnastasisVillage.*`
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.*`, `Village/AnastasisVillagePresentation.cpp`
- `tools/migration/parity-kit.mjs` (plusieurs modules par déclaration), `Source/AnastasisSim/PORTAGE.md`, `AGENTS.md` (index)

## COMMIT

BRANCH_HEAD

## MEC

- BUILD: PASS — `tools\unreal\anastasis-unreal.ps1 build` sur `9e2057c` (base `main@5161e91`), unity :
  `Module.AnastasisSim.cpp`, `Module.Anastasis_UnrealV2.{1,2}.cpp` compilés.
- TESTS: PASS — `tools\unreal\report-tests.ps1 -Filter Anastasis`, run complet (162 annoncés, 162 exécutés) :
  ```
  PASS                  : 158
  KNOWN_EXPECTED_FAILURE: 4   (le registre : Sim.Parite.Fbm, Sim.Parite.SemantiqueJs, 2 x AI.Toolsets.AnastasisInspect)
  FAIL                  : 0
  ```
  Nouveaux, tous PASS : `Sim.Parite.Recolte` ; `Sim.Village.Recolte.Perception` (9 gisements vus,
  oubli de l'épuisé, capacité 26), `.Selection` (ligne `gatherFood` recomposée depuis les fonctions
  prouvées : mêmes bits ; le sans-métier ne cueille pas), `.Cueillette` (1er coup à arrivée + 0,36 s,
  2 par coup au printemps, compétence au bit près, retour à 10), `.Livraison` (grenier 0 -> 10 -> 20,
  dehors, moral +1, compétence de marché au bit près), `.Epuisement` (2 + 2 + 1, jachère, monde
  généré intact, le sac de 5 livré), `.MultiAgents` (3 fermiers, 3 postes distincts, 37 cueillis),
  `.Destruction` (en route et pendant la récolte : aucune référence, sac gardé), `.Plein` (295 + 10 :
  5 entrent, 5 restent), `.Hote` (monde canonique). Conservation vérifiée à chaque tick.
  Les tests du puits, de la maison, du grenier et `FoodSupply.*` (Codex) restent PASS sans modification.
  En cours de route : un run bloqué au démarrage (deux autres éditeurs sur la machine), relancé.
- PARITÉ : `node tools/migration/gen-parity.mjs gather.mjs` contre `fee66ae` — 2011 vecteurs ;
  régénérés depuis `git archive fee66ae` : identiques. `Anastasis.Sim.Parite.Recolte` PASS, 0 écart.

## SCN

PASS — `tools\unreal\gather-deliver-pie.ps1` (éditeur discret) en PIE sur `Lvl_AnastasisSlice`,
`GATHER_DELIVER PASS`, piloté par l'état de la simulation :

- `FirstFarmer 1` -> `building-0` (granary) en (47,43), champ (46,46) à 19 portions, 1 fermier.
- t=40,2 : session ouverte, sac 2 ; t=42,9 : sac 10 -> `deliver` ; t=44,1 : 1re livraison, grenier 10 ;
  t=56,2 : 2e livraison, grenier 21.
- champs + sacs + stocks + repas constants à chaque échantillon.
- Captures (`Saved/SliceEvidence/gather-deliver/01..05.png`, planche légendée
  `planche-gather-deliver.png`) : prises par `Shot` — `HighResShot` ne rend pas les tracés de debug.
- Un premier lancement a manqué de mémoire au chargement de la carte (autres éditeurs ouverts) ; relancé.

## PLY

UNKNOWN — aucun contrôle humain ; les habitants sont des tracés de debug. `PLAYER` reste NOT_IMPLEMENTED.

## INTEGRATION_RISK

- **Deux circuits de récolte dans `FVillage`** : celui-ci (fidèle, fermier au grenier) et
  l'extension food-supply-001 de Codex (`ec5322f`, non fidèle, opt-in par `ActivateFoodSource`).
  Ils cohabitent (le fermier suit la référence, les autres l'extension ; une tuile ouverte n'a
  qu'une vérité). Décision à prendre : garder ou retirer l'extension.
- **Référence** : la copie de travail JS porte, non commité, `load > 11` (au lieu de 9) pour rentrer
  livrer. Ce portage suit le commit `fee66ae`. Si c'est commité : changer `HaulLoadAbove`, régénérer.
- Monde généré immuable : l'état vivant des tuiles est dans le village (`LiveTileAt`), le rendu du
  terrain ne montre pas l'épuisement (le tracé de debug, si).
- Le passage à la table calculée ne touche que les fermiers ; les tests du puits, de la maison, du
  grenier et de food-supply restent PASS sans modification.

## STOP

- Pas de marché, d'or, de vente : un sans-métier ne cueille pas (ses lignes restent au plancher).
- Pas d'exploration ni de rumeurs, pas de raté de coup, pas de repousse des champs.
- Pas d'autres métiers (steward, porter, farm, fishery…), pas de marché de l'emploi.
- Pas de parité de trajectoire : la table reste réduite (buts non portés = 42 + rythme).
