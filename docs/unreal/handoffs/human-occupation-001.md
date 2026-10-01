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

PENDING

## MEC

- BUILD: voir finish
- TESTS: voir finish. La mission ne change pas le C++.
- COMMANDS:
  - `tools\unreal\human-occupation-001.ps1 -Out Saved\HumanOccupationEvidence`
  - Passe retenue, terrain d'avant rebase : HUMAN_OCCUPATION::PASS
  - site 102000, 109000 cm, franc-bord 584 cm, relief 7,9 cm, berge a 3600 cm
  - 16 StaticMeshActor, 28 decals, niveau sauve, Lvl_AnastasisSlice non sauvee
  - frame pendant capture : 20 ms jeu, 52 ms GPU, machine chargee, pas un benchmark

## SCN

OBSERVED. Captures 1600x900 dans docs/visual/human-occupation-001 : sol, distance, riviere vers le groupe, groupe vers la foret, lisiere lointaine vers le groupe, heure 8 et 17,5. Le sentier est une suite de decals de terre, lisible depuis la riviere et d'en haut. L'arbre debout le plus proche cote interieur est a 186 m : la clairiere du bassin est deja vide.

## PLY

UNKNOWN. Aucun joueur.

## INTEGRATION_RISK

Nouveau .umap et un materiau de decal. Ne remplace pas Lvl_AnastasisSlice. Les hauteurs ont ete echantillonnees sur le terrain d'avant les passes foret/sol de main : rejouer la recette apres rebase, sinon les batiments peuvent flotter. AGENTS.md n'ajoute qu'une ligne d'index.

## STOP

Pas de ville, pas de champ, pas de cloture, pas de lien avec le village simule, pas de modification des PNJ, du brouillard, de la lumiere ou de la meteo. Le sentier n'est pas une trace continue.
