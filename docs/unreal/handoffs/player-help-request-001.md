# HANDOFF: player-help-request-001

## MISSION

Définir la prochaine mission de gameplay joueur à partir de la Bible canonique : demander de
l'aide à une personne pour un chantier, recevoir une réponse motivée, payer le temps et voir
un effet social. Cette passation est une **décision de périmètre**, pas son implémentation.

## FILES_OWNED

- `docs/unreal/PLAYER_HELP_REQUEST_001.md` : cible de scène, contrat, filtre §75, critères de
  preuve et découpage en tickets réalisables.
- `docs/unreal/handoffs/player-help-request-001.md` : cette passation.

## COMMIT

Commit de la branche `agent/player-help-request-001` portant ces deux documents.

## MEC

- Lecture de la Bible `.docx` fournie par Alexandre : §0.2, 2, 3, 15, 19, 22–26, 27–29,
  43, 63, 71–76 et tables des primitives, symétries, effets et invariants.
- Inspection de `main` 45791989 : `FVillage` possède `socialize`, `build`, des liens partiels
  et `FirstSite`, mais ni `ASK`, ni `evaluateRequest`, ni `IntentAck`/`DayOverlay`.
- Aucun code ou asset Unreal modifié. Build et suite sans objet pour cette mission documentaire.

## PROOFS

PROOFS: (aucune)

## SCN

UNKNOWN — le scénario A/B est spécifié, pas encore construit ni exécuté.

## PLY

UNKNOWN — aucune scène jouée au clavier ou évaluée à hauteur d'humain.

## ECARTS

AUCUN — `Source/AnastasisSim/` n'est pas modifié.

## INTEGRATION_RISK

- Cette fiche n'est pas une preuve que l'architecture de la Bible existe dans le portage Unreal.
- Les futurs tickets toucheront des fichiers centraux de `Source/AnastasisSim/` et doivent être
  isolés et ordonnés avec les missions de portage déjà actives.
- `player-food-loop-001` reste en file d'intégration et n'est pas une dépendance de cette mission.

## STOP

Ne pas annoncer un système d'obligations, un refus de PNJ validé, une scène jouable ni une boucle
de trente minutes. Le premier travail de code doit fermer la transaction d'intention, avant
une commande ou une interface de demande d'aide.
