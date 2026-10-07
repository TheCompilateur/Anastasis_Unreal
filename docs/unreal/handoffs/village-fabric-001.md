# HANDOFF: village-fabric-001

## MISSION

Seconde croisade bâtiments, mandat d'Alexandre du 2026-10-07, en parallèle d'architecture-crusade-001 et
coordonnée avec elle par message (partage : elle garde le bâtiment, celle-ci le tissu entre les bâtiments).
Grammaire d'implantation pontique, côté présentation :
- ruelles caladées qui suivent les courbes et deviennent escaliers au-delà de 16 % ;
- réseau en arbre qui pousse depuis la placette du puits, avec des troncs communs élargis ;
- murets de terrasse en pierre sèche (soutènement aval, déblai amont) ;
- platane de placette.

Géométrie construite à l'exécution, aucun asset généré, défrichement réversible. Rien n'est écrit dans la
simulation. Détail : `docs/unreal/VILLAGE_FABRIC_001.md`.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/Village/AnastasisVillageFabric.{h,cpp}` (grammaire pure)
- `Source/Anastasis_UnrealV2/Village/AnastasisVillageFabricActor.{h,cpp}` (géométrie, défrichement réversible)
- `Source/Anastasis_UnrealV2/Village/AnastasisVillageFabricSubsystem.{h,cpp}` (mise à jour, CVars, commandes, lecteur Python)
- `Source/Anastasis_UnrealV2/Village/AnastasisVillageFabricTests.cpp`
- `tools/unreal/village-fabric-pie.py`, `tools/unreal/village-fabric-capture.py`
- `docs/unreal/VILLAGE_FABRIC_001.md`, cette fiche
- `AGENTS.md` (deux lignes d'index), `tools/unreal/proofs.txt` (trois lignes en fin)

## COMMIT

PENDING

## MEC

- BUILD: PASS (`anastasis-unreal.ps1 build`, worktree ; adaptatif, et unity sur l'arbre commite 2cef02d)
- TESTS: PASS 4/4, joues dans un editeur de ce worktree (`Automation RunTests Anastasis.Village.Fabric`) :
  `Flat`, `Determinism`, `Slope`, `EdgeCases` = Success. Aucun KNOWN_EXPECTED_FAILURE. Suite complete
  `Anastasis` NON jouee ici : elle attend le lot.
  - Slope (pente 30 %) : `laneGrade=0.120 fallGrade=0.300 maxGrade=0.254 steps=38m/105m walls=13 wallMax=288cm intrusions=0`
- PIE `village-fabric-pie` : un premier run reel a tout valide sauf le compte defriche (747 puis 753 : l'herbe
  change entre deux passages). Il a aussi montre un repli qui posait le puits a 600 m des maisons (500 m de
  calade, grammaire 350 a 570 ms). Corriges : tolerance de 5 %, portee `MaxReachCells` = 8, repli en hameau
  autour du puits du lancement. Le run corrige n'a PAS ete rejoue (file d'editeurs saturee, arret a la demande
  d'Alexandre) : il attend le lot, `TESTS::QUEUED`.
- Captures `village-fabric-capture` : script pret, annule a la demande d'Alexandre. Aucune image produite.
## PROOFS

PROOFS: village-fabric-pie, village-fabric-capture

## SCN

Aucune commande de scénario nouvelle. La preuve utilise `Anastasis.Village.Hamlet 6 0` (architecture-crusade-001)
et se replie sur `FirstWell 0` + `FirstHouse 0` si la commande n'existe pas.
Nouveaux réglages : `anastasis.Village.Fabric`, `anastasis.Village.FabricClear`, `anastasis.Village.FabricTree`.
Nouvelles commandes : `Anastasis.Village.FabricReport`, `Anastasis.Village.FabricRebuild`.

## PLY

Calades et murets ont une collision (requête et physique). Le pawn du joueur marche sur la calade et bute
contre un muret de terrasse. Les habitants ne sont pas concernés : leur Z vient du terrain seul.

## ECARTS

AUCUN — `Source/AnastasisSim/` n'est pas touché. La grammaire ne fait aucun tirage, et sa seule variation est
un hachage FNV-1a de l'identifiant du bâtiment.

## INTEGRATION_RISK

- **Ordre de lot : après architecture-crusade-001.** La preuve PIE s'appuie sur son `Anastasis.Village.Hamlet`
  (repli prévu, mais un hameau de 6 maisons est la preuve voulue).
- Aucun fichier partagé avec elle, hormis `AGENTS.md` et `proofs.txt`, où chacun ajoute en fin de section.
- **Défrichement.** Le mien ne touche que les instances HISM/ISM posées sur la chaussée, et ignore le carré
  ±1020 cm de chaque parcelle bâtie, qui est le domaine du défrichement de l'architecture. Il est rendu à
  l'identique quand le tissu change, en `EndPlay` et sous `Fabric 0`. Un système qui réécrit ces instances
  entre-temps garde sa valeur : la restauration ne touche qu'une instance encore à l'échelle zéro.
- Mon acteur `AAnastasisVillageFabric` n'a aucun HISM : rien à exclure dans le défrichement de l'architecture.
- **Coût.** La grammaire trace le relief en chaque nœud de 1 m de la grille du hameau (environ 50 000 traces
  pour 6 maisons), seulement quand l'ensemble des bâtiments change. La durée est journalisée
  (`ANASTASIS_FABRIC built … grammarMs=`), elle n'a pas encore été mesurée en PIE.
- **Matériau.** La pierre est `MI_WeatheredStone`, sinon `M_AnastasisStone`. Les couleurs de sommet par pierre
  ne sont lues par aucun des deux : la variation se voit par la géométrie (joints, fruit, assises), pas
  par la teinte.

## STOP

- **Aucun verdict visuel.** `village-fabric-pie` est instrumental. Le jugement d'image (calade à 1,7 m,
  muret au bord d'une terrasse, platane) reste à faire par capture A/B (`anastasis.Village.Fabric 0/1`), après
  le versement.
- Le seuil est posé dans l'axe de l'accès de la simulation, pas encore sur la porte réelle de l'archétype
  (`EntryLocal`). L'adaptateur viendra après le versement d'architecture-crusade-001.
- La simulation ne choisit pas ses parcelles d'après ce tissu. Aucune position de bâtiment n'est proposée ni
  changée.
