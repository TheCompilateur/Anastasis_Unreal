# HANDOFF: shore-reed-003

## MISSION

Évaluer un seul mesh Ecotone sur une petite rive du terrain livré à l'étape 3.
A/B : échelle native contre largeur réduite et hauteurs variées, mêmes racines.

## FILES_OWNED

- Content/Anastasis/Ecotone/SM_Ecotone_Reed_01.uasset (reprise identique de fb68398)
- tools/unreal/capture-shore-reeds.py
- tools/unreal/capture-shore-reeds.ps1
- docs/unreal/SHORE_REED_003.md
- docs/unreal/handoffs/shore-reed-003.md

## COMMIT

Voir git log agent/shore-reed-003. Base 85e0627, sans intégration canonique.

## MEC

- BUILD: PASS par Build.bat, UE 5.8.2 CL 56702186, exit 0, 82,67 s.
- TESTS: aucune nouvelle suite C++, aucun C++ modifié.
- Captures: 6 obtenues, six caméras vérifiées, éditeur fermé, SHA256 enregistrés.
- COMMANDS:
  - tools/unreal/capture-shore-reeds.ps1 -OutDir <nouveau-dossier-absolu>

## SCN

REJECT comme correction artistique : les tiges amincies deviennent moins lisibles.
Voir docs/unreal/SHORE_REED_003.md pour images, preuves et portée.

## PLY

UNKNOWN ; PLAYER NOT_IMPLEMENTED.

## INTEGRATION_RISK

- Claude a repris TileWorldSize=400 pendant le test. Preuve limitée à la base 1 m.
- Aucun système de dressing livré : acteurs temporaires, aucune sauvegarde de carte.
- Vent non gelé, contact mesuré avant déplacement matériel, performances inconnues.
- L'opérateur build/finish du dépôt refuse le chemin du worktree géré par Codex.
  Pas de modification de ce portail ; build direct rapporté séparément.

## STOP

Étude terminée avec résultat négatif. Pas de scène finale revendiquée, pas de merge.
Rejouer une étude sur la base 4 m livrée avant de valider une composition de rive.
