# HANDOFF: field-grain-001

## MISSION

Representer les champs de cereales de la carte par une vraie touffe 3D instanciee. Le type de culture et la quantite de `FVillage::LiveTileAt` commandent seules les instances : une parcelle epuisee ne montre plus de cereales. Aucun changement de regle agricole ou de stock.

## FILES_OWNED

- `Content/Anastasis/FieldGrain/SM_Field_GrainClump_01.uasset`
- `Source/Anastasis_UnrealV2/Village/AnastasisFieldGrainVisual.h/.cpp`
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.h/.cpp` (spawn et reset du miroir)
- `tools/unreal/create-field-grain.py/.ps1`, `tools/unreal/field-grain-pie.py`, `tools/unreal/proofs.txt`
- `AGENTS.md`, cette fiche

## COMMIT

Le commit de passation est le HEAD de `agent/field-grain-001` marque par `agent-worktree.ps1 finish` ; consulter `.handoff/field-grain-001.txt` pour son empreinte et son verdict.

## MEC

- `py -3 tools/unreal/create-field-grain.py` : 2 223 sommets, 1 782 triangles, emprise 103.14 x 96.06 x 112.25 cm, SHA256 `0f0947fa19ac69d8a1e34b86ccf016c1d1d6aab058d3f95769c9e31b531aff0e`.
- `tools/unreal/anastasis-unreal.ps1 build` : `BUILD::PASS` sur les sources stables.
- `tools/unreal/create-field-grain.ps1` : `FIELD_GRAIN::PASS`, asset sauvegarde et nombre de triangles / bornes verifies par Unreal.
- `tools/unreal/editor-batch.ps1 -Proofs field-grain-pie` : `PROOF::PASS` (90,4 s), 29 154 instances HISM au depart de la scene figee ; 29 146 apres deux vivres recoltes. La parcelle (10,53) passe de 144 a 136 touffes, le stock alimentaire total des champs de 22 808 a 22 806, le sac du fermier de 0 a 2.
- Suite sans rendu : verdict de `finish` sur le commit marque ; le journal de passation fait foi.

## PROOFS

PROOFS: field-grain-pie

## SCN

Sur les trois captures de `Saved/FieldGrainEvidence/pie/`, meme carte, meme camera et simulation figee : `01-grain-on.png`, `02-grain-off.png`, `03-grain-on-control.png`. Images inspectees : le champ garni est lisible au premier plan ; le temoin sans cereales revele le sol et l'herbe preexistante. `compare.py` : 21,79 % des pixels >16/255 entre ON et OFF, 3,82 % entre ON et ON controle. La pluie et les effets temporels expliquent une part de l'ecart temoin. L'image montre une cereale stylisee ; identification botanique precise non jugee.

## PLY

UNKNOWN : aucun jugement de marche humaine dans les champs.

## ECARTS

AUCUN : la mission ne modifie pas `Source/AnastasisSim/` ; le champ reste sous autorite `FVillage`.

## INTEGRATION_RISK

- `AGENTS.md`, `tools/unreal/proofs.txt` et `AnastasisSimulationSubsystem.*` sont des fichiers chauds ; rebaser avant le lot si `main` avance.
- L'asset est genere dans ce worktree. L'integrateur doit rejouer `field-grain-pie` dans son editeur ; les captures du worktree ne prouvent pas l'image integree.
- Le maillage est instancie par blocs de 8 x 8 tuiles, avec collision/nav desactivees et disparition progressive de 100 a 140 m. Environ 29 000 instances au demarrage de la preuve ; aucun budget GPU n'est revendique sans mesure en scene.

## STOP

Ne revendique ni exactitude botanique pontique, ni fidelite geographique de chaque parcelle du terrain rendu, ni experience joueur avant inspection de la version integree.
