# HANDOFF : pontic-mountain-presence-003 — candidat roche rejete

## MISSION

Tester si un detail de strates et fissures dans le materiau de l'anneau lointain rend la chaine principale plus credible depuis le bassin, a 1,7 m. Une seule variable : `MountainRockDetail` 0/1/0 sur le `main` actuel au moment de l'essai.

## FILES_OWNED

- `tools/unreal/far-terrain-material.py` : repare la regeneration du graphe ; `MountainRockDetail` reste present dans le script deja verse sur `main`, mais son defaut passe a 0 apres REJECT.
- `tools/unreal/mountain-rock-capture.py` : utilise le ciel existant du niveau, sans ajouter un second acteur d'atmosphere.
- cette fiche. Aucun `.uasset`, C++, carte ou valeur CVar livree.

## COMMIT

Branche `agent/pontic-mountain-presence-003` rebasee sur `0af22c5fcae8c622ef409e4dc22997b140ff1031` ; consulter `HEAD` pour le commit final. La preparation des deux scripts etait deja presente sur `main` avant cette reprise.

## MEC

- Build du worktree rebase : `BUILD::PASS` (15 actions, 142,44 s).
- Premier essai du generateur : echec `graphe non vide apres nettoyage : 2`. Correction locale par recreation de l'asset, comme `world-theatre-light-material.py` ; deuxieme essai `FAR_TERRAIN_MATERIAL::PASS` avec le candidat actif. Le `.uasset` experimental a ensuite ete restaure a l'identique du `main`.
- Le script final met le defaut du parametre a 0 ; ce defaut final n'a pas ete regenere en asset, car le candidat visuel est rejete. `PYTHON_SYNTAX::PASS` avait ete obtenu avant ce seul changement de scalaire ; revérifier au portail final.
- `mountain-rock-capture.ps1 -Label current-0af22c5` : `MOUNTAIN_ROCK::CAPTURE_COMPLETE`, camera `(106000,106000,1052)`, pitch 3, yaw 45, un acteur d'incarnation, ciel de niveau valide. Images locales `Saved/MountainRockEvidence/current-0af22c5/S045_{A,B,A2}.png`.
- GPU p50 rapportes : A 18,24 ms, B 18,02 ms, A2 17,38 ms ; machine partagee, aucun gain de performance revendique.

## SCN

**REJECT.** La chaine reste une paroi bleu-blanc continue. Le detail ajoute une faible modulation au centre, sans roche lisible ni changement de la grande face est.

- Image entiere, pixels dont un canal differe de plus de 16/255 : A/B 2,63 %, A/A2 1,97 %.
- Zone massif central `(560,330)-(820,455)` : A/B 0,64 %, A/A2 0 % ; moyenne RGB 138,06 -> 135,82 -> 138,08.
- Zone massif est `(1370,295)-(1740,485)` : A/B 0 %, A/A2 0 %.
- Le ciel est valide, et le temoin A2 revient. Le changement reste insuffisant pour la cible d'emerveillement.

Diagnostic separe, sur le `main` canonique `0af22c5` en PIE a 11 h : `WorldTheatre.Light` 0/1/0 (masses constantes) change 65,71 % de la zone de chaine centrale contre 0,30 % entre temoins, et 0,66 % de la vallee contre 1,36 % entre temoins. Les images `Saved/WorldTheatreEvidence/mountain-current-0af22c5/V1_village_chaine_{masses,light,masses2}.png` montrent une baisse de valeur du lointain, mais toujours une paroi continue. Ce levier tonal n'est pas active par defaut dans cette mission.

## PLY

UNKNOWN. L'A/B/A roche est en editeur de niveau ; l'A/B/A lumiere est en PIE a camera fixe. Aucun parcours joueur ni jeu package ne juge l'emerveillement.

## ECARTS

AUCUN — `Source/AnastasisSim/` non touche.

## PROOFS

PROOFS: (aucune)

## INTEGRATION_RISK

Passation d'outil et de diagnostic. Le materiau enregistre n'est pas modifie. Regenerer plus tard via `far-terrain-material.ps1` ajoutera un parametre de detail a 0, equivalent visuellement au graphe precedent ; le candidat a 1 reste rejete. Ne pas annoncer une amelioration des montagnes sur la base du build, de la compilation du shader ou de ces captures instrumentales.

## STOP / NEXT

Arreter la branche de detail rocheux. Le levier de valeur a ete isole ; la forme de la chaine et la repartition de roche/neige restent les obstacles visuels. Ouvrir une mission de composition du relief avec une seule vue hero et des temoins, avant tout nouveau detail de surface.