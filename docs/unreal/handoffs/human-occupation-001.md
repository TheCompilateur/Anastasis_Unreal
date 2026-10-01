# HANDOFF: human-occupation-001

## MISSION

Micro-implantation visuelle dans le bassin habitable, dans un niveau isole. Trois maisons, un grenier, un abri, un puits, du stockage et un sentier vers la riviere. Aucune logique de PNJ, de simulation, de ciel ou de meteo.

## FILES_OWNED

- tools/unreal/human-occupation-001.ps1
- tools/unreal/human-occupation-001.py
- Content/Anastasis/Maps/Lvl_HumanOccupation.umap
- Content/Anastasis/HumanOccupation/M_HO_Tread.uasset
- Content/Anastasis/HumanOccupation/M_HO_Building.uasset
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.h
- docs/visual/human-occupation-001/
- AGENTS.md (une ligne d'index)
- docs/unreal/handoffs/human-occupation-001.md

## COMMIT

HEAD de agent/human-occupation-001.

## MEC

- BUILD: voir finish. Le C++ ajoute des clairieres d'herbe la ou des acteurs portent HO01_Tread ou HO01_Yard.
- TESTS: voir finish.
- COMMANDS:
  - `tools\unreal\human-occupation-001.ps1 -Out Saved\HumanOccupationEvidence`
  - Derniere passe : HUMAN_OCCUPATION::PASS
  - site 102000, 109000 cm, franc-bord 584 cm, relief 7,9 cm, berge a 3600 cm
  - 16 StaticMeshActor, 123 decals (sentier + tablier au pied), Lvl_AnastasisSlice non sauvee
  - M_HO_Building sur maisons, grenier et puits seulement
  - frame pendant capture : 12 ms jeu, 19 ms GPU, pas un benchmark

## SCN

OBSERVED. Captures 1600x900 dans docs/visual/human-occupation-001 : sol, distance, riviere, foret. Le sentier et le tablier sont une suite de decals de terre. Le bardage, la pierre et les tuiles se lisent a hauteur d'homme. L'arbre debout le plus proche cote interieur est a 146 m.

## PLY

UNKNOWN. Aucun joueur.

## INTEGRATION_RISK

Nouveau .umap, M_HO_Tread, M_HO_Building. Ne remplace pas Lvl_AnastasisSlice. L'incarnation n'ecarte l'herbe que si des acteurs HO01_Tread ou HO01_Yard sont dans le niveau : la carte principale n'en a pas. Les meshes partages du village gardent leur materiau.

## STOP

Pas de ville, pas de champ, pas de cloture, pas de lien avec le village simule, pas de modification des PNJ, du brouillard, de la lumiere ou de la meteo. Le sentier n'est pas une trace continue.
