# FAMILLES_FEU_001 — « Valmire pousse toute seule », mission 2 : les familles du premier soir

## Décision d'Alexandre (2026-10-08)

Mission 2 du jalon décrit dans `CHRONIQUE_VILLAGE_001.md`, dans ses mots :

> **Scène** : Bible §9 et §10.6, le feu du premier soir.
> **Je dois voir** des familles (parents, enfants, un frère, un apprenti), pas des habitants isolés, chacune avec un passé, un objet et une dette morale.
> **Hors sujet** : le joueur, les dialogues, l'apparence.
> **Fini quand** : la chronique du soir 1 présente chaque famille en deux phrases, et ces phrases changent d'une partie à l'autre.

Ses choix, le même jour :

| Question | Réponse |
|---|---|
| Langue des habitants | **français d'abord** ; la langue inventée du JS (romaïka) viendra par-dessus |
| À quoi servent les répliques | **le soir au feu, demander de l'aide, la vie de tous les jours** |
| Combien de familles | **4 familles, environ 12 personnes** |
| D'où partent les familles | **4 familles neuves** (pas les fondateurs `ROMAN_FOUNDERS` du JS) |
| Base de répliques | **reprendre les ~1 300 répliques du JS et ajouter** |
| Les quatre familles proposées | **« Oui, on part là-dessus »** |
| Qui pose les trois questions au feu | **un moine de passage** (une 14e personne) |

## Les fondateurs

`Content/Anastasis/Scenario/valmire-fondateurs.json`. Une famille par matrice d'origine de la Bible (§10.2) ;
une seule vient de la Ville (`docs/historicity/MODELE.md`, PONT-HIS-02). Brief : `docs/historicity/briefs/familles-feu-001.json`.

| Famille | Matrice | Membres |
|---|---|---|
| la maison du Scribe | réfugiés de la Ville | Konstantinos le Scribe (41), Eudokia, sa femme (36), Michael, leur fils (12) |
| la famille de Georgios | paysans de la vallée | Georgios (50), Theodora, sa femme (47), Ioannes, leur fils (19), Basileios, son frère (44) |
| la maison de Niketas | maison militaire | Niketas du fort (38), Zoe, sa femme (30), Leon, son pupille (16) |
| la maisonnée de Maria | gens des routes | Maria, veuve de muletier (34), Euphrosyne, sa fille (9), Sabas, muletier engagé (22) |
| — | moine de passage | Arsenios (61) |

Pour chacune des trois questions (où étais-tu quand la Ville est tombée, qu'as-tu emporté, qui n'est pas venu),
chaque famille a deux ou trois réponses ; la première est celle qu'Alexandre a validée. La partie en tire une par
sa graine : les familles ne changent pas, leur histoire si. Sur vingt graines, le test compte les récits différents.

Tout cela est une **hypothèse de conception** : personnes, biographies et objets inventés, plausibles, à faire relire.

## Ce qui est construit

| Où | Quoi |
|---|---|
| `tools/migration/gen-talk-lines.mjs` | recopie les répliques de la référence épinglée (fee66ae) dans `Content/Anastasis/Dialogue/repliques-reference.json` : 280 situations, 1 302 répliques, sans accents (ton de la référence) |
| `Content/Anastasis/Dialogue/repliques-valmire.json` | 156 répliques neuves, accentuées : le moine au feu, demander / accepter / refuser l'aide (occupé, son propre toit, dette, rancune, faiblesse, inconnu, pas les moyens), vie de tous les jours (saluts, faim, soif, pluie, travail, puits, deuil, ragots, la Ville) |
| `Sim/AnastasisDialogueLines.*` | la base, lue une fois ; une réplique se choisit par une clé (graine, qui parle, situation), jamais par le flux aléatoire de la simulation ; trous `{nom}`, `{absent}`… |
| `AnastasisSim` (écart n°44) | `FNpc::Name`, `FamilyName`, `Gender`, `Age`, `FamilyId`, `KinRole` ; `FVillage::FFamily` (adultes, dépendants, maison) ; `AddFamily`, `JoinFamily`, `SetIdentity` ; haché par `StateDigest`. Des données seulement : aucune décision ne les lit encore |
| `Sim/AnastasisValmireFounders.*` | lit le scénario, pose les fondateurs autour du premier puits, chaque famille de son côté, **sur des cases d'où le puits est atteignable** ; compose la scène du feu |
| `UAnastasisSimulationSubsystem` | `anastasis.Village.Founders` (défaut 1) : le village du lancement est celui des fondateurs ; l'ordre de pose donne les métiers d'ouverture ; la chronique s'ouvre sur les familles puis le feu |
| chronique | vrais noms ; habitants et récits **famille par famille** ; la scène du feu, le soir du jour 1 ; quelques répliques de la base citées (faim, soif, deuil, amitié) |
| portraits | `CategoryFor` / `PersonPool` : un habitant dont la simulation connaît le sexe et l'âge porte un portrait de sa catégorie (femme, homme, ancien, enfant), sans répéter un visage déjà porté |

## Ce qui n'est pas fait

- La vie familiale de la référence (couples, naissances, enfants qui ne travaillent pas, liens de parenté dans
  `bonds.js`) : écart n°44, `goals-family-001`.
- Le passé reste dans l'hôte : la simulation ne sait pas encore qui a emporté quoi, ni qui doit quoi à qui (mission 3).
- Les répliques ne sont pas encore dites dans le jeu (pas de bulles) : elles vivent dans la chronique et la base.
- Les répliques de la référence restent sans accents.
- La maison achevée n'est attribuée à personne pendant un saut (`AssignCompletedOpeningHome` n'est appelé que par `Tick`), constat de la mission 1.
