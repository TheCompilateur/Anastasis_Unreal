# HANDOFF: pontic-water-micro-001

## MISSION

Etape 1 du pilote de microvie pontique demande par Alexandre : mousse, prele, tussilage et
grenouille sur le trajet du sous-bois vers l'eau. Trois petites formes de berge sont
selectionnees dans le plan de micro-ecologie existant ; la mousse remplace la laiche
ecrasee qui servait de substitut au pied de certains troncs.

Regle PONT-ECO-01. Brief : `docs/historicity/briefs/pontic-water-micro-001.json`.
KEEP si les quatre meshes sont reels, les trois formes de berge apparaissent dans les
poches compatibles, le retour A/B/A est identique, les images proches les montrent sans
aspect de semis uniforme et le cout GPU n'augmente pas de plus de 1 ms par vue cible.
REJECT si l'une manque, flotte, se lit comme un objet pose arbitrairement, ou si le cout
depasse ce budget. Une grenouille immobile ne vaut que presence visuelle.

Decision au worktree : quatre assets livrables comme pilote visuel ; KEEP final de la
couche en scene non prononce, car la mesure GPU est brouillee par des editeurs concurrents.
Les 3 nouvelles familles de berge sont decalees de 135 cm des props qui ont fourni leur
habitat, puis le sol, l'eau, la pente et la distance aux props sont reevalues au site pose.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisPonticWaterMicro.h/.cpp` et `AnastasisPonticWaterMicroTests.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.h/.cpp`
- `tools/unreal/create-pontic-water-micro.ps1/.py`, `ground-cover-capture.py`, `proofs.txt`
- `Content/Anastasis/PonticMicro/SM_Pontic_{Moss,Horsetail,Coltsfoot,Frog}_01.uasset`
- `AGENTS.md`, `.claude/skills/anastasis-capture/SKILL.md`, `.claude/skills/anastasis-realisme/fiches/vegetation.md`
- `docs/historicity/SOURCES.md`, `docs/historicity/briefs/pontic-water-micro-001.json`, cette fiche

## COMMIT

Commit de cette fiche et des chemins FILES_OWNED (SHA dans Git et marqueur `finish`).

## MEC

- BUILD: PASS (`anastasis-unreal.ps1 build`, apres la derniere modification C++)
- ASSETS: PASS (`create-pontic-water-micro.ps1`, quatre meshes et trois LOD chacun) :
  - Moss `[540,90,45]`, Horsetail `[403,120,45]`, Coltsfoot `[264,168,96]`, Frog `[1440,576,288]` triangles.
- PREUVE EDITEUR: `PROOF::PASS pontic-water-micro-capture (227.0s)` sur
  `Saved/EditorBatch/20261007-175447/editor-batch.log` et
  `Saved/PonticWaterMicroEvidence/` dans ce worktree.
- A/B/A: 213 composants et meme inventaire de 1187 placements au retour ON ;
  OFF sans placement de berge ; mousse Pontic -> laiche existante -> mousse Pontic.
- TESTS: QUEUED (`Anastasis.PonticWaterMicro.HabitatSelection` dans la suite du lot)
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\create-pontic-water-micro.ps1`
  - `tools\unreal\editor-batch.ps1 -Proofs pontic-water-micro-capture`
  - `python tools/historicity/check-brief.py docs/historicity/briefs/pontic-water-micro-001.json`

## PROOFS

PROOFS: pontic-water-micro-capture

## SCN

PARTIAL : les vues proches montrent prele, tussilage et grenouille distincts des
roseaux, pierres et touffes de reference ; ils disparaissent au OFF et reviennent au ON2.
La mousse forme une petite tache visible seulement de pres au pied d'un tronc : encore
faible comme signal de scene. La grenouille reste une silhouette statique, partiellement
camouflee par l'herbe existante. Les six vues a 1,7 m et les images ON/OFF/ON2 sont dans
`Saved/PonticWaterMicroEvidence/` (ignore Git). Leurs regions cibles changent plus entre
ON/OFF qu'entre ON/ON2 (erreur moyenne pixels : mousse 2.3/0.5, prele 7.9/1.6,
tussilage 14.4/0.5, grenouille 3.3/0.9). Le cout GPU est UNKNOWN : des editeurs concurrents
ont fait varier le GPU d'environ 11 a 49 ms a camera fixe, trop pour juger le seuil de 1 ms.

## PLY

UNKNOWN : pas de parcours joueur ni de comportement animal revendique.

## ECARTS

AUCUN — `Source/AnastasisSim/` inchange.

## INTEGRATION_RISK

- `AnastasisWorldEmbodiment.cpp` et `ground-cover-capture.py` sont des fichiers chauds ; le
  branchement est un bloc dedie conditionne par `anastasis.Dressing.PonticWaterMicro`.
- Les quatre meshes binaires sont crees uniquement sous `Content/Anastasis/PonticMicro/`.
  Aucun asset existant ni materiau existant n'est regenere.
- Le lot doit rejouer la preuve sur son propre arbre ; le build du worktree ne valide pas
  l'apparence ni la version canonique.
- `KEEP` scene/performance attend une mesure GPU quiescente ; ne pas convertir ce
  `PROOF::PASS` instrumental en approbation artistique ou joueur.

## STOP

Pas de locomotion ou interaction de la grenouille, pas de distribution de population
ecologique, pas d'attestation botanique de cette vallee en 1204, pas de preuve PLY.
