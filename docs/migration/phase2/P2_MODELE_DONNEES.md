# Modèle de données et format de sauvegarde — décisions

PORTAGE.md posait la question sans la trancher : « c'est ici qu'on décide si Unreal relit
les sauvegardes JS existantes ou repart d'un format propre — décision à prendre
explicitement ». Voici les mesures, les deux décisions, et ce qu'elles coûtent.

Elles précèdent la vague 4 parce que les prendre après avoir porté trois systèmes revient
à réécrire les trois.

## Ce que la référence fait réellement

Mesuré sur `C:/dev/Jeux IV Kingdoms` (`fee66ae`), graine 33344 :

| | |
|---|---|
| Champs d'un habitant sérialisé | **151**, dont 40 objets imbriqués, 51 nombres, 25 chaînes |
| Champs d'un bâtiment | 22 |
| Taille JSON, jour 1 / 4 / 8 / 12 | 170 / 502 / 648 / **748 Ko** (5 → 9 habitants) |
| Part des habitants dans la sauvegarde | 43 % au jour 1, **56 % au jour 12** |
| Identifiants | `npc-0`, `building-3` — compteur monotone `_nextId` |
| Ajout | `actors.push(npc)` — toujours à la fin |
| Retrait | `actors.splice(index, 1)` — **décalage, ordre préservé** |
| Références | objets directs (`npc.home` EST la maison), converties en identifiants par `serialize` |
| Index | `_actorsById`, construit paresseusement, invalidé à chaque mutation |
| Où vivent les sauvegardes | `localStorage["anastasis-save-v1"]`, plus un export fichier |
| Tolérance de version | `SAVE_VERSION = 1` : `deserialize` **lève** si la version diffère |

Deux de ces lignes commandent tout le reste.

**L'ordre du tableau est observable.** `splice` décale, la boucle itère dans l'ordre, et
deux habitants qui convoitent la même ressource sont départagés par leur rang. `serialize`
émet le tableau dans l'ordre, donc l'empreinte du harnais en dépend directement.

**Un habitant a 151 champs.** Ce n'est pas une structure qu'on redessine en passant.

## Décision 1 — Le modèle de données

### Tableau de structures, pas structure de tableaux

Le réflexe « simulation » est le SoA : un tableau par champ, pour le cache et la
vectorisation. Ici, non.

Le contrat de ce portage est la **parité bit à bit**, pas le débit. Le SoA est une
optimisation qui ne s'achète qu'une fois la parité acquise — et 151 champs dont 40 objets
imbriqués donneraient 151 tableaux parallèles, c'est-à-dire une réécriture, là où le
mandat est une traduction. Une traduction se relit ligne à ligne contre sa référence ; une
réécriture, non.

Le jour où un profil réclamera du SoA sur une boucle chaude, il sera temps — avec le
harnais pour prouver que la transformation n'a rien changé. C'est exactement ce pour quoi
il a été construit.

### La table d'entités tient la sémantique JS

`Source/AnastasisSim/Public/World/AnastasisEntityTable.h` : tableau ordonné + index
paresseux par identifiant. Elle n'expose **pas** de retrait par échange.

`RemoveAtSwap` est le geste C++ correct — retirer en O(1) en échangeant avec le dernier —
et c'est précisément ce qui ferait diverger le portage : mêmes habitants, même état, ordre
différent. Le test `Anastasis.Sim.Entites.Ordre` le vérifie sur quatre éléments, parce
qu'avec trois l'échange donne par hasard le bon résultat.

### Les références sont des identifiants

En JS, `npc.home` est la maison elle-même. En C++, ni pointeur ni index :

- un pointeur dans un `TArray` meurt au premier realloc — la leçon est déjà inscrite dans
  `AnastasisSpatialGrid` ;
- un index meurt au premier `splice`, et il meurt **en silence**, en désignant le voisin.

Donc l'identifiant, résolu par l'index paresseux — ce que `serialize` fait déjà au
passage, et ce que `_actorsById` fait déjà en mémoire.

### Tout en double, toujours

Rappel du contrat de module, ici pour mémoire : pas de `float`, y compris pour les
positions. Une simulation qui tourne des heures accumule l'écart jusqu'à la divergence.

### La représentation interne est libre ; la projection ne l'est pas

C'est le point qui donne de l'air. Le harnais ne contraint que ce que `FStateWriter` émet
— la projection canonique. À l'intérieur, un identifiant peut être un `int32` et devenir
`"npc-7"` à la projection ; un `TMap` peut remplacer un objet JS.

Ce qui n'est pas libre : l'ordre des séquences, et la valeur de chaque nombre.

## Décision 2 — Le format de sauvegarde

### Le format de jeu est natif Unreal

Versionné, binaire, propriété du C++, libre d'évoluer. Trois raisons :

1. **Le format JS est déjà sur une trajectoire de mur.** 748 Ko de JSON au jour 12 avec
   9 habitants, dont 56 % d'habitants — soit ~46 Ko par habitant. À 60 habitants, la
   projection donne ~3 Mo, à lire et écrire par `JSON.stringify` dans un `localStorage`
   dont la limite usuelle est de 5 Mo. Ce n'est pas un problème à hériter.
2. **Lire le format JS gèlerait le modèle C++ sur la forme du JSON, accidents compris** :
   les champs supprimés à l'emballage, `piecesPlaced: undefined`, les valeurs par défaut
   « si la sauvegarde vient d'avant le pathfinding ». Ces cicatrices appartiennent à
   l'histoire du dépôt JS, pas à celle du jeu Unreal.
3. **Personne ne détient de corpus de sauvegardes à préserver.** Elles vivent dans le
   `localStorage` d'un navigateur, sur une machine. (Voir la question ouverte ci-dessous :
   c'est le seul point qui pourrait renverser la décision.)

### Le format JS est lu — comme instrument, pas comme promesse

Un lecteur du JSON de `serialize` vers l'état C++, **en lecture seule**, et sans aucune
promesse de compatibilité joueur.

Sa valeur n'est pas dans le jeu, elle est dans le harnais : sans lui, une divergence au
jour 30 exige de rejouer 81 000 ticks des deux côtés avant de commencer à chercher. Avec
lui, on charge l'état JS du jour 30 dans le C++, on avance d'un tick de chaque côté, et on
compare. C'est la différence entre une boucle de diagnostic de plusieurs minutes et une
boucle de quelques secondes, sur ce qui sera le travail quotidien pendant des mois.

Combiné à `FStateWriter`, qui sait déjà émettre la projection canonique, cela donne
l'aller-retour complet : charger un état JS, le reprojeter, comparer l'empreinte. Un
désaccord à ce stade accuse le **lecteur**, avant même qu'un tick n'ait été simulé —
c'est-à-dire au seul moment où le diagnostic est trivial.

### Ce que cela change dans PORTAGE.md

La première justification du contrat de parité y est : « une sauvegarde JS doit pouvoir se
rejouer dans Unreal et donner le même village ». Le contrat de parité ne bouge pas — c'est
lui qui rend le portage fidèle. Sa justification, elle, se reformule : ce n'est pas le
**jeu** qui relit une sauvegarde JS, c'est le **harnais**. La propriété reste vérifiable,
et elle reste vérifiée ; elle cesse d'être une promesse faite au joueur.

## Ce qu'on renonce

- **Un joueur ne pourra pas reprendre sa partie navigateur dans Unreal.** Assumé.
- **Le SoA est remis à plus tard**, donc un coût mémoire et cache qu'on ne mesurera pas
  avant d'avoir de quoi le mesurer. Le harnais rendra la transformation sûre le moment
  venu.
- **Le décalage à chaque retrait est en O(n).** Sur des tableaux de quelques centaines
  d'habitants, c'est du bruit ; et c'est le prix de la parité, pas une négligence.

## Ce qui reste à décider — et qui n'est pas de mon ressort

**Quelqu'un détient-il une sauvegarde JS qui compte ?** Une partie de test longue, une
partie témoin, quelque chose qu'on voudrait revoir tourner. Si oui, la décision 2 ne
change pas de nature mais change de priorité : le lecteur JS devient un livrable de
migration daté, pas seulement un instrument. Si non, il reste ce qu'il est ici — un outil
de diagnostic, écrit quand le harnais en aura besoin.

C'est le seul point que les mesures ne tranchent pas.

## État

| | |
|---|---|
| `World/AnastasisEntityTable.h` | écrit, testé |
| `Anastasis.Sim.Entites.Ordre` / `.Index` | **PASS** (2026-09-13) |
| Suite `Anastasis.Sim` | 14 PASS, 2 KNOWN_EXPECTED_FAILURE, 0 FAIL |
| Structures d'entités (habitant, bâtiment, animal) | vague 4 — 151 champs à porter avec leurs systèmes, pas d'avance |
| Lecteur du format JS | à écrire quand le harnais aura un état C++ à charger |
| Format de sauvegarde natif | à écrire quand il y aura un état à sauver |
