# PONTIC_HORIZON_MATERIAL_003 — reprise

## Etat au 2026-10-09

- Branche `agent/pontic-horizon-material-003`, worktree `C:\dev\ANASTASIS_WORKTREES\pontic-horizon-material-003`, depart `e038b7d0e`.
- Aucun changement de rendu, de carte, de matériau ou de code de jeu. Aucun build ni capture dans cette branche.
- Le diagnostic precedent `pontic-horizon-realism-002` (commit `952d0f498`) montre sur `S045_B.png` une paroi bleu-blanc. Ses tests de masque neige et de brouillard ne l'expliquent pas ; son marquage magenta montre que l'anneau contribue. La variation `aerial_scale=0.5` a produit un ciel noir et une mesure GPU invalide : elle est rejetee.
- Le script `capture-horizon.ps1/.py` de `952d0f498` ajoute `-View S045`. Ne pas reutiliser une copie temporaire de ces fichiers ; verifier d'abord si ce commit a ete integre et respecter sa propriete de fichiers.
- Blocage externe : deux editeurs d'autres missions, PID 23284 (`sprites-vegetation-gpt-001`) et 51688 (`player-start-002`), etaient encore ouverts lors du dernier controle. Alexandre a demande d'attendre leur fin. Aucun processus d'attente n'est laisse actif.

## Reprise

1. Verifier `MAIN.lock`, les processus `UnrealEditor*`, la RAM libre et l'etat de `main`. Attendre la fin des deux sessions et ne fermer aucun editeur d'autrui.
2. Mettre a jour cette branche depuis `main` suivant le protocole du projet, si necessaire. Verifier la presence de `-View S045` dans l'outil de capture ; ne pas ecraser la mission `pontic-horizon-realism-002`.
3. Builder ce worktree : `tools/unreal/anastasis-unreal.ps1 build`.
4. Avec un seul editeur utile, capturer la meme carte, graine 12345, vue S045, profil d'atmosphere et heure :
   - A : `tools/unreal/capture-horizon.ps1 -Label pontic-003-material-a -Mode skyline -View S045 -States B -Atmosphere -PreCmds 'anastasis.Terrain.HorizonFarMaterial 1'`
   - B : meme commande, label `pontic-003-material-b`, valeur `0`.
   - A2 : meme commande, label `pontic-003-material-a2`, valeur `1`.
5. Inspecter les trois PNG, les journaux (`HORIZON_COMPLETE`, `HORIZON_SKY`, camera, acteur unique) et les materiaux effectifs des sections de `HorizonTerrain` dans l'editeur. Rejeter les captures au ciel noir ou au temps GPU incoherent. Comparer avec `.claude/skills/anastasis-capture/compare.py` sur les memes poses.
6. Si la variation du materiau corrige vraiment la paroi sans degrader le ciel ni les autres vues, modifier l'autorite native de `/Game/Anastasis/Materials/M_AnastasisFarTerrain` (`tools/unreal/far-terrain-material.py`), produire un A/B du correctif et verifier le cout GPU. Sinon, revenir a la valeur par defaut et isoler la perspective aerienne comme branche causale suivante.
7. Commit, fiche de passation, `finish` et arret a `HANDOFF_READY::YES` seulement apres preuves. Le verdict joueur et l'integration restent UNKNOWN jusque-la.

## Frontiere de preuve

Observation d'ancienne branche, pas preuve courante. `MEC=UNKNOWN`, `SCN=UNKNOWN`, `PLY=UNKNOWN` pour cette mission. Aucune amelioration du monde jouable n'est revendiquee.
