# HANDOFF: rd-anastasia-map-001

## MISSION

Évaluer le « Résumé exécutif » R&D fourni par Alexandre depuis la map de jeu intégrée, sans ouvrir de chantier moteur par priorité documentaire seule. Produire le triage des vingt pistes et un protocole discriminant sur les saccades PSO.

## FILES_OWNED

- `docs/unreal/RD_ANASTASIA_MAP_001.md`
- `docs/unreal/handoffs/rd-anastasia-map-001.md`

## COMMIT

Voir le commit de cette fiche.

## MEC

- `Config/DefaultEngine.ini` lie le démarrage et le jeu à `Lvl_AnastasisSlice`.
- `Anastasis_UnrealV2.uproject`, `WorldView`, `AnastasisSimulationSubsystem`, `AnastasisSoundscapeSubsystem` et les procédures de performance lus sur la base Git indiquée dans le dossier.
- Sources Epic 5.8 vérifiées pour PSO, Mass, StateTree, AI Perception et passe personnalisée ; les renvois numériques sans URL du PDF ne sont pas promus en preuve primaire.
- Aucun code Unreal modifié ; build et suite non nécessaires pour la validité d'une décision documentaire.
- `git diff --check` : à renseigner après la dernière mise à jour.

## PROOFS

PROOFS: (aucune)

## SCN

Le rapport identifie la carte Git, sa génération et les représentations utilisées. Inspection vivante de la scène : à compléter après libération du lot et de l'éditeur.

## PLY

UNKNOWN : aucun trajet joueur construit, aucune mesure de hitch attribuable aux PSO.

## ECARTS

AUCUN : `Source/AnastasisSim` inchangé.

## INTEGRATION_RISK

- Documentation seule ; aucun effet sur le rendu, la simulation, les binaires ni la map.
- La base de travail a été ouverte pendant le lot `pie-advance-001,soil-matrix-004,crossing-site-001,lived-paths-001`. Recaler le dossier sur le `main` résultant et mettre à jour son SHA avant `finish`.
- Les propositions de branches parallèles ne sont pas des dépendances de cette mission.

## STOP

Pas de nouveau cache PSO, de migration Mass, de changement d'IA, de terrain ou de rendu. Le diagnostic sur jeu construit est défini mais pas encore exécuté : ne pas rapporter de gain de performance ni de preuve joueur.
