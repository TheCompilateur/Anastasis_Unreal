# HANDOFF: lake-shoreline-contour-001

## MISSION

Hypothese testee puis **REJETEE** : decouper les triangles de nappe lacustre a l'intersection exacte eau/relief devait retirer les berges anguleuses de la World Map actuelle. Aucun changement de rendu n'est conserve.

## FILES_OWNED

- `docs/unreal/handoffs/lake-shoreline-contour-001.md` seulement.

## COMMIT

Voir `git log` de `agent/lake-shoreline-contour-001`.

## MEC

- Prototype C++ avec CVar A/B/A compile deux fois : `BUILD::PASS` dans le worktree.
- Le prototype a cree 2 298 sommets de contour et retire 234 triangles entierement secs, mais la triangulation partielle a porte la section d'eau de 21 846 a 22 761 triangles.
- Test d'automation cible non conclu : un lancement Unreal a ete interrompu avant decouverte des tests (`TOTAL 0`, `RUN_INCOMPLET`).
- Le prototype, son test et le script de capture ont ete retires du code suivi apres le verdict visuel.

## PROOFS

PROOFS: (aucune)

## SCN

Capture editor A/B/A sur le plus grand lac interieur de `Lvl_AnastasisSlice`, graine 12345, 1 905 cellules, vue de berge a 1,7 m et oblique. `Saved/LakeContourEvidence/` conserve localement les six images, `metrics.json`, `site.json` et les trois exports du reseau ; `Saved/EditorBatch/20261007-144410/editor-batch.log` contient `LAKE_CONTOUR_CAPTURE COMPLETE`. Ces artefacts ne sont pas commites.

- Les trois exports hydrologiques sont identiques une fois exclu le champ `summary`, dont seul le temps de calcul `ms` varie.
- Vue oblique : 0,26 % des pixels changent au-dela de 16/255 entre reference et contour ; 0,40 % changent entre reference et retour temoin. Effet inferieur a la variance de capture.
- Vue a hauteur humaine : 17,97 % entre reference et contour, 18,64 % entre les deux temoins ; la variation temporelle de l'eau domine l'image. Aucune amelioration de rive defendable.
- GPU p50 de la vue oblique : 14,118 ms (reference), 14,173 ms (contour), 14,095 ms (retour temoin). Pas de gain de performance mesure.

## PLY

UNKNOWN : aucune marche du joueur au bord du lac. L'hypothese est rejetee a l'etape scene, avant une preuve joueur.

## ECARTS

AUCUN : aucun changement de `Source/AnastasisSim/`.

## INTEGRATION_RISK

Document seul. Ne pas reappliquer ce decoupage comme correction visuelle sans une nouvelle observation qui montre une nappe visible sur du terrain sec. Les triangles excedentaires sont normalement occultes par la section de sol, d'ou l'absence d'effet mesurable.

## STOP

Arret de cette branche causale. Reorienter l'effort vers un defaut visible du sol ou de l'ecosysteme, avec image A/B/A et retour temoin.
