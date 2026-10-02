# HANDOFF: building-capacities-001

## MISSION

Rendre observables les capacites sociales des maisons, greniers et chantiers et mesurer leurs couts d'opportunite sur trois jours simules, avec comparaison A/B reproductible. Aucun nouveau comportement de village n'est introduit.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/Sim/AnastasisBuildingCapacity.h`
- `Source/Anastasis_UnrealV2/Sim/AnastasisBuildingCapacity.cpp`
- `Source/Anastasis_UnrealV2/Sim/AnastasisBuildingCapacityTests.cpp`
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.h`
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp`
- `docs/unreal/handoffs/building-capacities-001.md`

## COMMIT

PENDING

## MEC

- BUILD: `tools/unreal/anastasis-unreal.ps1 build` → `BUILD::PASS` apres le dernier changement Source (les trois fichiers nouveaux et l'hote compiles).
- TESTS: QUEUED — `Anastasis.Gameplay.BuildingCapacities.ThreeDayAB` dans la suite du lot.
- COMMANDS:
  - `Anastasis.Village.Capacities` journalise `BUILDING_CAPACITIES` en JSON dans le jeu.
  - `UAnastasisSimulationDebugLibrary::GetBuildingCapacityStatus` expose le meme instantane aux outils Python/Blueprint.
- Mesures : maisons achevees et occupees, personnes logees, greniers et stock physique/disponible/reserve, sites actifs, intentions instantanees de batir/recolter/livrer, sessions actives de batir/cultiver, actes cumules (repas, repos, recolte, livraison, pieces posees), besoins moyens et critiques. Le test integre aussi les secondes-personnes passees en sessions de construction et de culture chaque jour.
- Test : memes monde plat, positions, besoins et quatre habitants par paire ; maison attribuee / absente, grenier garni avec poste de fermier / absent, chantier approvisionne / absent. Echantillons jours 0 a 3 et deltas structurees dans le rapport d'automation.

## PROOFS

PROOFS: (aucune)

## SCN

UNKNOWN — aucun editeur ou PIE lance dans cette mission ; la suite du lot donnera les chiffres et le verdict du test A/B.

## PLY

UNKNOWN — les mesures portent sur la simulation, pas sur ce que le joueur voit ou comprend.

## ECARTS

NON CONCERNE — `Source/AnastasisSim/` est en lecture seule pour cette mission ; l'observatoire est dans l'hote Unreal.

## INTEGRATION_RISK

- `AnastasisSimulationSubsystem.*` est un fichier partage avec d'autres missions ; verifier le conflit eventuel lors du lot.
- Le bras grenier ajoute ensemble un stock initial de 12 portions et un poste valide pour deux fermiers : son effet ne peut etre attribue aux murs seuls.
- Les buts ne sont que des intentions instantanees ; les actes cumules et besoins sont des observations. Les deltas A/B sont des differences de trajectoires, pas une causalite generale au-dela de ce scenario.
- Le test de trois jours appelle `UpdateActors` sur un village synthetique sans la file complete de minuit de `FAnastasisSimulation` ; la portee est la boucle locale PNJ/batiments.

## STOP

Ne revendique pas de stabilite sociale macro, de cout monetaire, de comportement nouveau ou de preuve joueur. La comparaison n'est valable qu'apres execution reussie de la suite en lot ; `queued` ne vaut pas `PASS`.
