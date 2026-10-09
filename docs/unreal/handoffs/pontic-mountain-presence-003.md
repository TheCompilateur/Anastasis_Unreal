# Reprise : pontic-mountain-presence-003

## MISSION

Rendre la chaine principale intimidante et materiellement credible depuis le bassin,
a hauteur humaine. La reference `S045` occupe deja le ciel, mais ses versants sont
une paroi bleu-blanc continue. Les huit meshes pontiques sont dans `main`, sans pose
validee. Cette etape teste **uniquement** la matiere rocheuse de l'anneau lointain.

## FILES_OWNED

- `tools/unreal/far-terrain-material.py` : candidat `MountainRockDetail` 0/1.
- `tools/unreal/mountain-rock-capture.ps1/.py` : capture 0/1/0 a une camera.
- `AGENTS.md` : index des outils.
- cette note.

## CURRENT

- Worktree `C:\dev\ANASTASIS_WORKTREES\pontic-mountain-presence-003`, branche
  `agent/pontic-mountain-presence-003`, base `e038b7d0e`.
- Premier build isole `BUILD::PASS` (19 actions, 252 s). Aucun C++ modifie.
- Le script de materiau est **un candidat non execute**. Le `.uasset` existant n'a
  pas ete regenere ; la carte affiche donc toujours l'ancien materiau.
- Aucun editeur de cette mission n'a ete lance, aucune capture nouvelle, aucun
  verdict SCN ni PLY. Ne pas appeler `finish` ou integrer a ce stade.

## REPRISE BORNEE

1. Verifier `agent-worktree.ps1 status` et la porte memoire ; attendre une place
   libre dans la file de l'editeur, sans fermer celui d'un autre agent.
2. Dans ce worktree, lancer `tools\unreal\far-terrain-material.ps1`. Verifier
   `FAR_TERRAIN_MATERIAL::PASS`, le graphe reel, puis la sauvegarde du `.uasset`.
3. Lancer `tools\unreal\mountain-rock-capture.ps1 -Label trial-01`. Trois
   images `Saved\MountainRockEvidence\trial-01\S045_{A,B,A2}.png` doivent
   avoir un ciel valide et la meme camera. A/B/A modifie le seul parametre
   `MountainRockDetail` dans un editeur.
4. Ouvrir les trois images, calculer A/B et A/A2 avec
   `.claude\skills\anastasis-capture\compare.py`, examiner les versants plutot
   que l'image entiere, et relever les ms GPU. REJECT si motif periodique,
   roche factice, ciel noir, cout non borne ou effet dans la variance temoin.
5. Si KEEP, capturer les huit azimuts avec le materiau livre et verifier les
   limites de la foret/neige ; puis commit, `finish` et passation a l'integrateur.
   Si REJECT, restaurer le script et l'asset, documenter le resultat.

## MEC / SCN / PLY

MEC : build initial PASS seulement ; compilation du shader UNKNOWN.
SCN : UNKNOWN ; aucune image du candidat.
PLY : UNKNOWN ; aucun parcours joueur.

## PROOFS

PROOFS: (aucune)

## INTEGRATION_RISK

Ce commit de preparation ne doit pas etre integre seul. Il change le script
d'autorite, pas le materiau charge en jeu. `pontic-ground-context-002` touche
`AnastasisWorldEmbodiment.cpp` mais pas ce script ; revalider sur le main actif.

## STOP

Mission mise en veille a la demande d'Alexandre. Aucun PASS visuel ni
photoréalisme revendique.
