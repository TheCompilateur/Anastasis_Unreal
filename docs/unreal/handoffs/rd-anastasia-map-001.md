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
- Capture de contrôle tentée sur la racine canonique à `e681e629` : arrêt du seul éditeur de cette capture après le démarrage d'un nouveau lot ; `CAPTURE::FAIL` (5 images absentes). Ce run n'est pas une preuve de scène.
- `git diff --cached --check` : PASS après indexation des deux fichiers possédés.

## PROOFS

PROOFS: (aucune)

## SCN

Le rapport identifie la carte Git, sa génération et les représentations utilisées. Inspection vivante de la scène : `UNKNOWN`. La capture interrompue ne soutient aucun verdict visuel ; Alexandre a ensuite demandé de laisser la capture de côté.

## PLY

UNKNOWN : aucun trajet joueur construit, aucune mesure de hitch attribuable aux PSO.

## ECARTS

AUCUN : `Source/AnastasisSim` inchangé.

## INTEGRATION_RISK

- Documentation seule ; aucun effet sur le rendu, la simulation, les binaires ni la map.
- Base de référence figée : dernier `main` intégré `e681e629`. Un nouveau lot peut déplacer `main` avant le versement ; la conclusion historique du dossier reste bornée à ce SHA et doit être réévaluée si le niveau, la configuration PSO ou les consommateurs pertinents changent.
- Les propositions de branches parallèles ne sont pas des dépendances de cette mission.

## STOP

Pas de nouveau cache PSO, de migration Mass, de changement d'IA, de terrain ou de rendu. Le diagnostic sur jeu construit est défini mais pas encore exécuté : ne pas rapporter de gain de performance ni de preuve joueur.
