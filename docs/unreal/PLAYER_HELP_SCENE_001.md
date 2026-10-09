# PLAYER_HELP_SCENE_001 — une demande d'aide jouable dans Valmire

## Objectif

Une famille a ouvert un chantier de maison. Le joueur incarne son chef, peut y consacrer son propre
temps, rencontrer une autre personne et lui demander de venir aider. La reponse vient de
`FVillage::EvaluateHelp` : une raison calculée, un accord qui admet la personne au chantier sans
la teleporter ni poser de piece, ou un refus dont le demandeur se souvient. Le village continue
de decider et de travailler ensuite.

## Entree et commandes

Dans `Lvl_AnastasisSlice`, lorsque les fondateurs de Valmire sont poses :

| Touche | Acte |
|---|---|
| Entree | ouvrir la premiere parcelle familiale si elle n'existe pas, incarner le chef de cette famille |
| deplacement / regard | marcher avec le corps simule, regarder une personne a moins de six cases |
| E | demander de l'aide a cette personne pour le chantier |
| F | tenir une intention `build` sous les memes portes de la table que les autres habitants |
| X | retirer l'intention |
| J | afficher ou fermer les dernieres notes entendues du carnet |

Un panneau Slate en bas a gauche fournit l'entree, les actes, la reponse et la situation du toit.
Il est visible dans le village fondateur ou pendant cette scene. Il reprend le carnet existant ;
il ne calcule ni besoin, ni raison, ni travail. La touche E emprunte la meme methode `AskHelp`
que peut appeler n'importe quel habitant ; la routine nocturne garde son chemin anterieur.

## Contrat de la demande

`AskHelp(fromId, toId, siteId)` lit toutes ses preconditions avant d'ecrire : personnes presentes,
demandeur admis sur une maison de famille ouverte, destinataire non deja sollicite, adulte,
proximite vocale, chemin du destinataire vers le site. Un rejet rend un motif et conserve
`StateDigest`. Une demande valide ajoute un `FHelpAnswer` a `HelpLog`, avec le jugement existant.
Un oui ajoute seulement la personne a `AllowedBuilders`. Le trajet et les pieces appartiennent
ensuite a sa decision ordinaire. Un non produit le souvenir `refusedHelp` deja connu du village.

Le panneau indique ce qui est accessible au joueur : la personne devant lui, le chantier de sa
famille, la reponse qu'il vient d'entendre, les aides qu'il a obtenues, et les recits que son carnet
a effectivement retenus. L'absence de pieces apres un oui est affichee explicitement.

## Preuve et limites

- **MEC** : `Anastasis.Sim.Episodes.DemandeParlee` distingue rejet sans mutation, oui motive sans
  piece instantanee, doublon sans mutation et refus motive.
- **SCN instrumentale** : `player-help-scene-pie` ouvre la scene, verifie la creation du panneau,
  deplace le joueur si la cible est hors de portee, adresse une demande, lit le motif et verifie
  qu'aucune piece ni teleportation n'est produite par la reponse. Les deux captures `Shot`
  ne montrent pas le panneau PIE ; la seconde montre une geometrie vert vif au premier plan.
  Elles ne valident pas l'image joueur.
- **PLY** : reste `UNKNOWN` tant qu'une personne n'a pas joue au clavier, a hauteur d'humain,
  sans console, et compris la reponse puis la conduite reelle des habitants.

Cette premiere tranche ne donne pas encore de duree chiffree a une demande, ne reserve pas
une plage horaire, et ne promet pas qu'un oui sera tenu. Elle ne dessine pas de menu principal
de commercialisation. Le panneau est un contrat d'action et de lecture a tester avant de
multiplier les surfaces. Le carnet montre ses quatre notes les plus recentes ; la comparaison
complete des versions reste dans son texte existant.

**KEEP** si le clavier seul permet de commencer, de demander, de travailler et de lire une
reponse causale ; si le PNJ accepte, son travail futur doit etre constate separement.
**REJECT** si la touche E agit a distance, si un oui cree des pieces, si la raison ne vient pas
du calcul, ou si l'interface annonce un effet que la simulation n'a pas produit.
