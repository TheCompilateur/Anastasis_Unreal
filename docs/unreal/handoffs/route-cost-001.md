# HANDOFF: route-cost-001

## MISSION

Rendre le coût de la grille de navigation causal dans le temps de trajet des PNJ : même A*, même monde et mêmes ressources ; une route avance une livraison, une herbe humide la retarde.

## FILES_OWNED

- `Source/AnastasisSim/Public/Village/AnastasisVillage.h`
- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp`
- `Source/AnastasisSim/Private/Tests/AnastasisVillageGatherTests.cpp`
- `Source/AnastasisSim/ECARTS.md`
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp`
- `tools/unreal/proofs.txt`
- `tools/unreal/gather-deliver-pie.py`
- `docs/unreal/ROUTE_COST_001.md`
- `docs/unreal/handoffs/route-cost-001.md`

## COMMIT

PENDING

## MEC

- BUILD: `tools/unreal/anastasis-unreal.ps1 build` → `BUILD::PASS` sur fichiers Source figés. Une compilation antérieure a trouvé une erreur de type dans le test, corrigée ; deux passages ensuite invalidés par l'empreinte Source changée pendant leur fenêtre, non revendiqués comme PASS.
- TESTS: `Anastasis.Sim.Village.Recolte.RouteCostDelivery` compilé ; exécution dans la suite du lot, résultat encore `UNKNOWN`.
- `git diff --check`: PASS (avertissements Git CRLF seulement).
- `python -B -c "import ast; ..."` sur `gather-deliver-pie.py`: `PYTHON_AST::PASS`.
- `node tools/migration/check-ecarts.mjs -base main -handoff docs/unreal/handoffs/route-cost-001.md`: `ECARTS::PASS` ; avertissements préexistants et portée du nouveau cas déclarée.

## PROOFS

PROOFS: gather-deliver-pie

## SCN

`UNKNOWN` sur le commit de passation. Le lot doit rejouer `gather-deliver-pie` dans son éditeur commun. Son motif `PASS` signifie récolte, retour, deux livraisons et conservation observés ; il ne mesure pas la pente du terrain rendu.

## PLY

`UNKNOWN`. Ni le mouvement du joueur incarné ni la lisibilité de l'effet économique sur plusieurs jours ne sont prouvés par cette mission.

## ECARTS

- ouvert : n° 29 — temps de trajet payé au coût du terrain dans le jeu Unreal (A_TRANCHER). Le village C++ seul reste désactivé pour le harnais JS.
- empile dessous (versement de l'integrateur) : n° 28 de mortality-001, verse dans le meme lot ; cette mission ne le modifie pas.

## INTEGRATION_RISK

- `AnastasisVillage.h/.cpp` sont des fichiers actifs dans plusieurs missions ; rebaser sans écraser leur travail.
- L'A* et la marche utilisent les coûts sémantiques (`FNavGrid`), pas la pente du maillage rendu. Les comparaisons de carte doivent conserver cette distinction.
- `anastasis.Village.RouteCost` vaut 1 en jeu et 0 rétablit la marche uniforme. Les délais de scripts PIE antérieurs peuvent changer ; le lot doit vérifier `gather-deliver-pie`.
- La preuve automatisée compare des mondes posés à la main. La livraison PIE sert à vérifier le branchement dans l'hôte, pas une économie mature.

## STOP

Pas de revendication de preuve joueur, de coût de foule/charge, de convergence JS ni d'effet macro sur le village.
