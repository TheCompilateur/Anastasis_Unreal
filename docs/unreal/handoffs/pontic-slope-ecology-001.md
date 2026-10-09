# HANDOFF: pontic-slope-ecology-001

## MISSION

Intervenir dans la scene, pas seulement dans sa mesure : la penalite de Wetness
uniforme du placement arboré clairsemait aussi les versants rendus. Sur une pente
de 8 a 24 degres, reduire progressivement cette penalite, afin que l'humidite
puisse soutenir des peuplements irreguliers. La plaine plate conserve le tirage
historique. Hypothese artistique de conception, non carte botanique de 1204.

Brief : `docs/historicity/briefs/pontic-slope-ecology-001.json` (ECO-01, ECO-02,
PONT-ECO-01; `BRIEF::PASS` de tracabilite seulement).

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisEcologicalDressing.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisPonticSlopeEcologyTests.cpp`
- `tools/pontic-slope-ecology/capture.py` et `capture.ps1`
- `tools/unreal/proofs.txt` : une nouvelle ligne de preuve.
- `docs/historicity/briefs/pontic-slope-ecology-001.json`
- `docs/unreal/handoffs/pontic-slope-ecology-001.md`

## COMMIT

Voir le commit portant cette fiche (`git log -1 --format=%H -- docs/unreal/handoffs/pontic-slope-ecology-001.md`).

## MEC

- `tools/unreal/anastasis-unreal.ps1 build` : PASS, compilation C++ du nouveau test et du placement.
- `Anastasis.Ecology.PonticSlopeEcology` : NON EXECUTE. Attente `EDITOR_GATE::WAIT` jusqu'a 6/45 min, file de quatre puis trois demandes, 2 editeurs actifs et moins de 2,5 Go libres. La demande a ete interrompue proprement, sans toucher aux editeurs des autres missions. Commande de reprise : `tools/unreal/report-tests.ps1 -Filter 'Anastasis.Ecology.PonticSlopeEcology'`.
- Syntaxe de `tools/pontic-slope-ecology/capture.py` et `capture.ps1` : PASS statique. Inscription de la preuve : `editor-batch.ps1 -Proofs pontic-slope-ecology-capture -DryRun` PASS. Capture reelle NON EXECUTEE.
- CVar `anastasis.Dressing.PonticSlopeEcology` : 0 = ancien tirage, 1 = versant humide soutenu; valeur par defaut 1. La difference ne s'applique qu'au mode macro et a pente > 8 degres; saturation a 24 degres.
- Aucune nouvelle instance hors des regles d'habitat, d'eau, de pente, de route, de bassin ou d'espacement.

## PROOFS

PROOFS: pontic-slope-ecology-capture

## SCN

UNKNOWN tant que les six images A/B/A du worktree ne sont pas regardees et comparees. Commande de reprise, apres fermeture des editeurs concurrents et liberation de la porte memoire : `tools/pontic-slope-ecology/capture.ps1 -Label pilot-001`. Examiner les six PNG et `capture.json`, puis mesurer `reference/candidate` face a `reference/reference2` avec `.claude/skills/anastasis-capture/compare.py`. Le PASS du script certifie la completion et le retour d'inventaire, pas la qualite de la scene.

## PLY

UNKNOWN : aucune traversée joueur revendiquee.

## ECARTS

AUCUN — `Source/AnastasisSim/` intact.

## INTEGRATION_RISK

- La CVar ajoute des arbres possibles dans les pentes humides et peut augmenter le cout GPU. Son activation par defaut n'a PAS encore de verdict SCN. Cette branche ne doit pas etre admise dans un lot avant test cible, examen A/B/A et decision KEEP/REJECT. Aucun `finish` n'a ete lance.
- `tools/unreal/proofs.txt` peut recevoir d'autres lignes dans les lots concurrents; fusion par union d'une seule ligne finale.
- Aucun fichier de relief, hydrologie, sol, asset d'arbre ou `AnastasisWorldEmbodiment` possede par une autre mission n'est modifie.

## STOP

Build et test de placement ne prouvent ni photorealisme, ni fidelite botanique au XIIIe siecle, ni effet joueur. Ne retenir l'activation par defaut qu'apres examen SCN et cout GPU.
