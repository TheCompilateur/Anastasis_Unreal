# HANDOFF: human-occupation-001

## MISSION

Micro-implantation visuelle dans le bassin habitable, dans un niveau isole. Trois maisons, un grenier, un abri, un puits, du stockage et un sentier vers la riviere. Aucune logique de PNJ, de simulation, de ciel ou de meteo.

## FILES_OWNED

- tools/unreal/human-occupation-001.ps1
- tools/unreal/human-occupation-001.py
- Content/Anastasis/Maps/Lvl_HumanOccupation.umap
- Content/Anastasis/HumanOccupation/M_HO_Tread.uasset
- docs/visual/human-occupation-001/
- AGENTS.md (une ligne d'index)
- docs/unreal/handoffs/human-occupation-001.md

## COMMIT

HEAD de agent/human-occupation-001 (rejeu inclus).

## MEC

- BUILD: BUILD::PASS apres rebase (10 actions, 51 s).
- TESTS: voir finish. La mission ne change pas le C++.
- COMMANDS:
  - `tools\unreal\human-occupation-001.ps1 -Out Saved\HumanOccupationEvidence`
  - Rejeu apres rebase sur main : HUMAN_OCCUPATION::PASS
  - site 102000, 109000 cm, franc-bord 584 cm, relief 7,9 cm, berge a 3600 cm
  - bassin 106000, 106000, z 882,4 cm — memes hauteurs qu'avant rebase
  - 16 StaticMeshActor, 27 decals, niveau sauve, Lvl_AnastasisSlice non sauvee
  - arbre debout le plus proche : 14581 cm (la passe foret de main a rapproche la lisiere)
  - frame pendant capture : 14 ms jeu, 19 ms GPU, pas un benchmark

## SCN

OBSERVED. Captures 1600x900 dans docs/visual/human-occupation-001, rejouees apres rebase : sol, distance, riviere vers le groupe, groupe vers la foret, lisiere vers le groupe, heure 8 et 17,5. Le sentier est une suite de decals de terre. L'arbre debout le plus proche cote interieur est a 146 m.

## PLY

UNKNOWN. Aucun joueur.

## INTEGRATION_RISK

Nouveau .umap et un materiau de decal. Ne remplace pas Lvl_AnastasisSlice. Recette rejouee apres rebase : le site et les hauteurs n'ont pas bouge, un decal de moins, lisiere a 146 m au lieu de 186 m. AGENTS.md n'ajoute qu'une ligne d'index.

## STOP

Pas de ville, pas de champ, pas de cloture, pas de lien avec le village simule, pas de modification des PNJ, du brouillard, de la lumiere ou de la meteo. Le sentier n'est pas une trace continue.
