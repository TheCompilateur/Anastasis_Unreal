# HANDOFF: labor-social-pie-001 (ERGON)

## MISSION

Prouver en PIE, dans le monde Unreal charge, que le bûcheron livre physiquement le bois coupe a un puits en chantier, que le bâtisseur le consomme et qu'un autre habitant utilise ensuite le puits. Mission de preuve seulement : aucune regle de production changee.

## FILES_OWNED

- `tools/unreal/labor-social-pie.py`
- `tools/unreal/proofs.txt` (une ligne `labor-social-pie`)
- `AGENTS.md` (une ligne d'index)
- `docs/unreal/handoffs/labor-social-pie-001.md`

## COMMIT

Branche `agent/labor-social-pie-001`, creee depuis `main` `4be1a0f6d810405fbcf0e46bc4826d8eeb0a54bc`, rebasee sur `main` avant `finish`. Le marqueur de passation doit porter le HEAD final exact.

## MEC

- Premier build isole dans le worktree : `BUILD::PASS`, 19 actions, 326,51 s. Aucun C++ ni asset modifie.
- Syntaxe Python par `ast.parse` : PASS apres correction de la seconde version du script.
- Premiere tentative `editor-batch.ps1 -Proofs labor-social-pie` : FAIL, non revendiquee. Le bûcheron a coupe 12 bois, le bilan de 29 984 unites est reste conservé, mais aucun bois n'a atteint le chantier ; sans puits ni nourriture initiaux, les besoins vitaux ont préempté le travail et un observateur a disparu vers 411 s simulées. Ce run a également révélé que les cartes visuelles ne sont pas une source fiable de coordonnées au montage (0,0) ; la seconde version lit les positions autoritaires et leur projection monde.
- Seconde tentative avec puits et grenier initiaux, sans bois injecté et sans porteur : `Saved/EditorBatch/20261008-190727/editor-batch.log`, `LABOR_SOCIAL_PIE FAIL wall_timeout`. Le chantier avait ete place automatiquement a (74,36), loin des ressources vitales. Le bûcheron a coupe et livre 8 bois ; le chantier a consomme ces 8 bois et 8 pierres, avec une masse de bois conservee a 29 984. Le puits n'a pas ete acheve (8/10 bois et 8/18 pierres consommes). C'est une preuve de livraison et transformation partielles, pas de la chaine complete.
- Troisieme tentative : chantier explicitement fixe a (46,47), `Saved/EditorBatch/20261008-193702/editor-batch.log`. Le puits est reellement acheve : 10/10 bois et 18/18 pierres consommes ; 21 bois coupes, 10 livres, masse conservee a 29 984. Le script initial a affiche `PROOF::PASS`, mais ce verdict est rejete : le compteur de boisson de l'observateur est monte de 2 a 4 loin du nouveau puits, puis le PNJ s'en est approche plus tard. L'assertion melangeait deux instants et un rayon de 5000 uu couvrant potentiellement les deux puits. `other_well_use` etait donc un faux positif de metrologie.
- Quatrieme tentative : `Saved/EditorBatch/20261008-195437/editor-batch.log`, `PROOF::PASS` en 106,5 s. L'observateur `npc-1` est distinct du bûcheron `npc-3` ; apres l'achevement, son compteur de boisson passe de 6 a 8 a 524 s simulees, but `drink`, position (46,914 ; 46,509), a 1,074 tuile du nouveau puits (46,47). Les hausses anterieures, a 6,722 et 3,513 tuiles, sont correctement exclues. 21 bois coupes, 10 livres et consommes, 18 pierres consommees, bois total conserve a 29 984.
- Cinquieme tentative, script final : `Saved/EditorBatch/20261008-195800/editor-batch.log`, `PROOF::PASS labor-social-pie (73,2 s)`, `EDITOR_BATCH::PASS 1/1`. JSON local `Saved/LaborSocialPieEvidence/labor-social-pie.json` : 67 echantillons ; bilans bois toujours 29 984 et pierres toujours 18 ; 21 bois coupes, 10 livres et consommes, 18 pierres consommees, puits acheve. Apres achevement, l'observateur `npc-1` passe de 2 a 3 boissons a 254,17 s simulees, but `drink`, a 0,899 tuile du puits construit. Toutes les assertions du script final sont vraies.

## PROOFS

PROOFS: labor-social-pie

## SCN

LOCAL_FIX : ajout d'une preuve rejouable dans le registre ; aucune modification du mecanisme ERGON deja integre. Les trois premiers essais ont discrimine besoins vitaux, localisation et faux positif de metrologie.

SYSTEM_EFFECT : PASS local en PIE sur la carte chargee : foret -> inventaire du bûcheron -> trajet -> stock du chantier -> consommation par bâtisseur -> nouveau puits -> boisson d'un autre PNJ. Le bûcheron, le but, le trajet, le travail et la boisson evoluent sans commandes directes apres montage. Le test C++ integre `Anastasis.Sim.Wood.AutonomousWellChain` reste une preuve distincte du simulateur. Le montage est explicite : chantier a (46,47), 18 pierres creditees en condition initiale, puits et grenier de survie existants, porteur initial retire ; il ne prouve pas la naissance spontanee du chantier ni la production de pierre.

## PLY

PLAYER_EFFECT : UNKNOWN. Le script ne juge pas la lisibilité visuelle pour un joueur.

## ECARTS

AUCUN — aucun fichier `Source/AnastasisSim/` modifié dans cette mission.

## INTEGRATION_RISK

- `AGENTS.md` et `tools/unreal/proofs.txt` sont partagés avec d'autres missions ; rebase manuel pour `AGENTS.md` si conflit, une seule ligne définitive dans le registre.
- Le lot doit rejouer `labor-social-pie` dans son éditeur. Un résultat local ne prouve pas l'empilement futur.
- Le contrat ecrit ERGON/OIKOS `C:\dev\ANASTASIS_WORKTREES\.coordination\labor-social-001-oikos-vivant-001.md` reste l'interface ; accusé de reception OIKOS inconnu, aucune dependance bloquante, aucun fichier domestique ou politique de production touche.
- Prochaine interface possible, hors mission : exposer la demande et la provenance du bois dans les explications sociales visibles au joueur ; aucun code ajoute ici.

## STOP

Le `PROOF::PASS` local est acquis pour le script final, mais le lot doit le rejouer apres empilement. Aucun verdict de lisibilite joueur ou de sauvegarde/rechargement dans cette mission. Pas de nouveau metier, stockage, devis, rendement ou interface domestique.
