# PLAYER_HELP_REQUEST_001 — demander de l'aide, recevoir une réponse, payer le temps

## Décision de mission

**Expérience visée.** Le joueur reprend sa journée d'habitant devant un chantier qui appartient
à la vie du village. Il peut demander à deux personnes nommées de venir aider. L'une peut être
prise par son propre travail, l'autre se sentir tenue d'aider. Il doit aller leur parler, entendre
une réponse motivée, puis choisir entre donner lui-même du temps au chantier et poursuivre ses
autres engagements. Avant la fin d'une scène courte, quelqu'un vient travailler, refuse, ou le
chantier avance sans lui. Le village continue dans les deux cas.

Ce n'est pas une quête de chef, une récompense de dialogue ni un bouton qui ajoute des ouvriers.
Le fait de **demander**, la possibilité de **refuser** et le coût de **tenir parole** sont le gameplay.
La maison sert de révélateur des relations et des occupations ; elle n'est pas la boucle unique.

**Autorité de la Bible canonique reçue le 2026-10-07.** §0.2 (habitant ordinaire et mêmes verbes),
§2 (arc local de trente minutes avec arbitrage et réaction), §15 (chantier comme radiographie
sociale), §19 (symétrie), §22 (`ASK`, `WORK`, `GO`), §24–26 (affordance, acte, transaction),
§29 (`evaluateRequest` retourne acceptation et raison dominante ; le tour du village est le
gameplay), §43 (WHY), §63 (`INV-PLAYER-001/002/003`, `INV-SOCIAL-002`), §72 (entrée/sortie de
campagne). La matrice §14.4 place la demande et la réponse en IMMEDIATE, le temps travaillé en
ACCUMULATIVE, les changements sociaux durables en CONSOLIDATED.

## CURRENT vérifié sur `main` 45791989

- `FVillage` sait incarner un habitant et lui donner `socialize` ou `build`. Les liens, réunions
  et quelques mémoires de personnes existent. `FirstSite` ouvre un chantier de banc et y place
  des bâtisseurs autonomes.
- Le compagnon de `socialize` est choisi par `PickSocialCompanion` ; le joueur ne peut pas encore
  adresser une demande à une personne précise. Aucun `ASK`, `evaluateRequest`, engagement daté,
  `IntentAck` ou `DayOverlay` n'apparaît dans `Source/AnastasisSim/`.
- `FirstSite` peut livrer son devis directement : c'est un banc de chantier, pas une économie ou
  une obligation sociale canonique. `player-food-loop-001` est encore en file d'intégration ;
  cette mission n'en dépend pas.

## Contrat à construire

1. Une demande typée `ASK(helpWork, siteId, minutes)` est disponible aux deux providers. Sa
   cible est un **PersonId** vivant et nommé ; les arguments, l'heure et l'ordre sont stables.
2. La validation vérifie proximité audible, accessibilité du chantier, disponibilité du temps
   et préconditions de travail. Échec : réponse explicite, zéro mutation. Succès : demande et
   réponse inscrites immédiatement dans l'état autoritaire de la journée, avec révision et
   provenance ; l'Histoire les inscrit au commit du soir.
3. `evaluateRequest` calcule `{accept, reason, total, factors}`. La raison est le terme causal
   dominant réellement calculé. Occupation, besoin urgent, lien et obligation doivent pouvoir
   se contredire. Une réputation scalaire ne décide jamais seule du nombre de bras.
4. Accepter crée un engagement borné, **pas du travail instantané**. L'habitant doit encore
   choisir ou maintenir `GO` puis `WORK` sous les mêmes portes que les autres, se déplacer et
   payer son temps. Il peut manquer son engagement ; ce manquement est un fait, pas un effacement.
5. Refuser est un résultat ordinaire. La raison remonte au joueur et à `why(requestId)`. Les
   relations et la mémoire réagissent à un fait observé, sans `standing += X` dans le dialogue.
6. L'affordance « demander de l'aide » est publiée par la simulation seulement lorsqu'elle
   est exécutable maintenant. L'interface montre la réponse de la transaction ; elle ne crée
   ni capacité ni vérité.

## Filtre du §75 : quatorze réponses avant code

| # | Réponse pour cette mission |
|---|---|
| 1 | Aller voir des personnes, risquer un refus motivé, arbitrer son propre temps, observer qui vient. |
| 2 | `FVillage` (personnes, relations, site, travail) ; la transaction d'intention manque. |
| 3 | Primitive `ASK`, suivie de `GO` et `WORK`. Un engagement accepté est une structure sociale distincte. |
| 4 | Demande (acteur, destinataire, site, durée, heure), réponse, facteurs, engagement, échéance, état. |
| 5 | Réponse et occupation courantes : chaudes ; engagement et provenance : froids. |
| 6 | Simulation seule ; l'interface ne modifie pas l'état. Il faut un équivalent vérifié de DayOverlay et IntentAck. |
| 7 | Demande/réponse IMMEDIATE ; minutes de travail ACCUMULATIVE ; relation et manquement CONSOLIDATED. |
| 8 | Réaction locale C1 ; engagement et manquement C2 possibles. Aucun papillon C3 dans ce ticket. |
| 9 | `why(requestId)` : réponse, facteurs pondérés, motif dominant, acteurs, site et temps. |
| 10 | `INV-PLAYER-001/002/003`, `INV-SOCIAL-002`, invariants de transaction et de provenance. |
| 11 | Banc identique joueur→PNJ, PNJ→PNJ et PNJ→joueur ; cas accepte/refuse/invalidé ; A/B avec et sans demande. |
| 12 | Aucune source aléatoire nouvelle dans le jugement initial ; ordre canonique des demandes et des candidats. |
| 13 | État nouveau à sérialiser et rejouer ; versionnement requis avant fermeture. |
| 14 | Une intention, une réponse et un déplacement visibles ; aucune animation ou bâtiment nouveau requis au départ. |

## Une seule branche causale, critères de sortie

**Hypothèse.** À état initial et graine identiques, demander à une personne disponible et liée au
demandeur change sa présence et ses minutes de travail au chantier. Demander à une personne
occupée peut échouer, avec une raison dominante cohérente. Sans demande, les deux personnes
gardent leur agence ; le chantier peut progresser autrement.

**MEC — KEEP** : contrat `ASK` symétrique, refus sans mutation, WHY fidèle aux facteurs,
engagement conservé, replay déterministe, preuve que la réponse ne modifie pas directement les
minutes de chantier. **REJECT** si un PNJ est téléporté, si le dialogue écrit le travail, si la
raison est un texte préfabriqué ou si le résultat dépend d'un `isPlayer` dans le résolveur.

**SCN — KEEP** : deux candidats contrastés issus de leurs états (pas de réponse forcée dans le
script), demande/absence de demande sur même état initial, déplacement et travail effectivement
observés, un refus possible, temps payé, village autonome dans le bras témoin.

**PLY — KEEP** : dans l'éditeur, une personne visible et nommée, une affordance adressée à elle,
un aller physique, une réponse compréhensible avant la fin de la session, puis un changement
perceptible de sa conduite ou du chantier. Le verdict doit venir d'un essai au clavier et à
hauteur d'humain ; les commandes et JSON restent des preuves de mécanisme.

## Porte d'entrée et découpage

Le code actuel ne possède pas la transaction `Intent → validation atomique → état autoritaire de
la journée → IntentAck` exigée au §26. **Ne pas ajouter une commande Unreal qui contourne cette
porte.** Premier ticket de réalisation : établir ce trajet pour une demande typée, avec rejet sans
mutation, accusé et replay. Deuxième ticket : `evaluateRequest`, engagement et conduite autonome.
Troisième ticket : affordance, présentation et preuve PLY. Chaque ticket se ferme sur ses propres
tests ; la mission complète ne revendique une scène jouable qu'après les trois.

La sélection de ces tickets est une décision de production. Les contrats de la Bible ne sont pas
amendés ici ; les écarts du portage Unreal seront déclarés dans `ECARTS.md` dès leur entrée en code.
