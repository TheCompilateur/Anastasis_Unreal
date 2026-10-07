# HANDOFF: places-component-lifecycle-001

## MISSION

Stabiliser les composants HISM des lieux composes de la World Map quand `AAnastasisWorldEmbodiment` incarne plusieurs fois le meme monde. Observation initiale sur `Lvl_AnastasisSlice`, graine 12345 : plan constant de 19 lieux et 356 pieces, mais `ANASTASIS_PLACES components` vaut successivement 44, 63, 82, 101. Le nom demande a `MakeUniqueObjectName` peut etre suffixe ; la recherche ulterieure par nom de base ne retrouve donc pas le composant. La correction retrouve le HISM par le `UStaticMesh` charge, au sein du tableau de composants Places uniquement, et continue de vider ses instances plutot que de detruire un arbre HISM asynchrone.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisPlaces.cpp` : cle de reutilisation des composants.
- `tools/unreal/places-lifecycle.py` : preuve de quatre incarnations dans le meme editeur.
- `tools/unreal/proofs.txt` : entree `places-lifecycle`.
- `AGENTS.md` : ligne d'index du script.
- `docs/unreal/handoffs/places-component-lifecycle-001.md`.

## COMMIT

Voir `git log` de la branche `agent/places-component-lifecycle-001`.

## MEC

- Syntaxe Python : `PYTHON_AST::PASS` ; `git diff --check` sans erreur d'espacement.
- Build Editor Win64 Development : `BUILD::PASS` (18 actions, 524,39 s, premier build du worktree).
- Suite `Anastasis` complete : reservee au lot d'integration.

## PROOFS

PROOFS: places-lifecycle

## SCN

`editor-batch.ps1 -Proofs places-lifecycle` : `PROOF::PASS` (80,0 s). Sur le monde actuel, `on -> repeat -> off -> return`, les 44 identites de composants restent identiques ; le nombre d'instances vaut `356 -> 356 -> 0 -> 356`, les composants actifs `44 -> 44 -> 0 -> 44`. Le log `Saved/EditorBatch/20261007-152154/editor-batch.log` et `Saved/PlacesLifecycleEvidence/lifecycle.json` conservent les mesures locales non commitees. Le plan reste a 19 lieux et 356 pieces ; aucun mesh manquant et aucun placement sans sol.

## PLY

UNKNOWN : aucune mesure en marche joueur. Le defaut vise la reincarnation de la map, dans l'editeur ou un runtime qui la reconstruit.

## ECARTS

AUCUN : ne touche pas `Source/AnastasisSim/`.

## INTEGRATION_RISK

- Reutilisation par identite de `UStaticMesh` seulement dans `PlaceMeshes`, sans affecter les HISM de l'ecologie ou des berges. Les meshes de `AnastasisPlaces::MeshPath` sont distincts par famille/variante.
- Si une session vivante contient deja des doublons vides accumules avant le patch, ils restent attaches et vides jusqu'a la fermeture de l'editeur ; la croissance s'arrete. Le prochain lancement propre repart du minimum.
- Conserver `ClearInstances()` et la reutilisation en place : detruire/recreer les HISM pendant la construction de leur arbre asynchrone a deja declenche `InstanceReorderTable`.

## STOP

S'arreter a `HANDOFF_READY::YES`. Integration reservee a l'integrateur unique ; aucune preuve joueur ou gain FPS chiffre n'est revendique.
