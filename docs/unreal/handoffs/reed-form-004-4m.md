# HANDOFF: reed-form-004-4m

## MISSION

Reprendre la forme et le matériau d'une touffe de roseaux puis comparer dans Unreal
sur le terrain 4 m livré par Claude (82a337b), sans intégration canonique.

## FILES_OWNED

- tools/unreal/create-reed-form.py
- tools/unreal/capture-reed-form.py
- tools/unreal/capture-reed-form.ps1
- Content/Anastasis/Ecotone/SM_Ecotone_Reed_01.uasset (reprise identique de fb68398)
- docs/unreal/REED_FORM_004.md
- docs/unreal/handoffs/reed-form-004-4m.md
- Sorties locales non commitées : Content/Anastasis/EcotoneReview004/ (2 uasset)

## COMMIT

Voir git log agent/reed-form-004-4m. Étude initiale 1 m conservée par a9b6426.

## MEC

- BUILD: PASS via Build.bat Editor Win64 Development, exit 0, 34,11 s.
- TESTS: aucun C++ modifié ; aucune nouvelle suite C++ revendiquée.
- Géométrie: 2172 sommets, 3360 triangles/touffe ; assertions aires/normales/pied.
- Sauvegarde puis relecture dans un processus neuf : mesh + matériau WPO=0 vérifiés.
- 8 captures obtenues, hashes enregistrés, transforms et caméras relus.
- COMMANDS:
  - tools/unreal/capture-reed-form.ps1 -OutDir <nouveau-dossier-absolu>

## SCN

KEEP comme base de forme, PARTIAL pour la scène. Le vent coupé ne résout pas tous
les défauts fins. Pas de photoréalisme revendiqué. Voir docs/unreal/REED_FORM_004.md.

## PLY

UNKNOWN ; PLAYER NOT_IMPLEMENTED, pas de PIE.

## INTEGRATION_RISK

- Base exacte 82a337b ; modifications suivantes de Claude exclues de la preuve.
- Matériau candidat statique ; animation et coût GPU non vérifiés.
- Acteurs temporaires ; pas de branchement au dressing, pas de carte sauvegardée.
- Deux assets générés laissés locaux et non commités. Recette refuse d'écraser le mesh.
- Opérateur build/finish du dépôt incompatible avec le chemin du worktree géré par
  Codex ; build direct rapporté, portail finish non franchi.

## STOP

Lot local terminé, aucun merge/push. Prochaine décision : composition moins régulière
et contact visuel au sol, sur une base terrain explicitement livrée.
