# MEMOIRE_DECISIONS_001 — « Valmire pousse toute seule », mission 3 : la mémoire, le carnet, les décisions

## Décision d'Alexandre (2026-10-08)

Après avoir lu la chronique des familles :

> On doit créer [un] système de mémoire d'attachement à ce que disent les PNJ = [ça] devient leur histoire, que le joueur enregistre.
> […] il se passe qu'ils sont incapables de prendre [une] décision.

| Question | Réponse |
|---|---|
| Ce que le joueur enregistre | **un carnet qui se remplit** : il ne sait que ce qu'il a entendu lui-même, page par habitant |
| Une histoire racontée plusieurs fois | **elle change, on peut comparer** (Bible §35) |
| Par quoi commencer | **les deux liés** : la mémoire de ce qu'on s'est dit sert aux décisions |
| La mission (cinq lignes ci-dessous) | **« Oui, go »** |

> **Scène** : Bible §35 et §29 : on se souvient de ce qu'on s'est fait, on le raconte, et ça décide de la suite.
> **Je dois voir** des habitants qui retiennent ce qu'ils ont vécu et entendu, qui le racontent (et le déforment), qui décident de bâtir leur maison et de demander de l'aide, et qui acceptent ou refusent en se souvenant (« tu m'as aidé », « tu m'as refusé »).
> **Le joueur** a un carnet qui se remplit de ce qu'il entend, PNJ par PNJ, avec les versions qui changent.
> **Hors sujet** : le rendu, les bulles à l'écran, l'interface du carnet (il se lit en texte pour commencer).
> **Fini quand** : la chronique raconte une maison levée grâce à une aide, un refus motivé par un souvenir, et une histoire qui a changé en passant de bouche en bouche ; et je peux lire le carnet.

## Pourquoi ils ne décidaient rien

La table de décision des habitants (25 envies, reprise du JS) ne porte que les envies du corps : manger, boire,
dormir, s'abriter, parler, se détendre, et travailler si on leur a donné un métier ou ouvert un chantier.
Les envies qui font un projet (ouvrir un chantier, bâtir sa maison, demander de l'aide) n'étaient pas portées
(écart n°18) : quand l'une d'elles gagnait, l'habitant regardait.

## Ce qui est construit

### 1. La mémoire (écart n°47, port de `src/ai/episodes.js`)

« Un habitant ne retient QUE ce qu'il a vécu, vu de ses yeux, ou entendu d'un autre. » Porté fidèlement :

- huit souvenirs au plus, du plus lourd au plus léger, le vécu avant l'entendu ; l'oubli de minuit (−0,7 par jour, trente jours) ;
- les **témoins** (six cases, quatre au plus, pas les enfants) ; le **deuil** (le foyer, puis les témoins), l'**arrivée**, le **chantier achevé** (« c'est son œuvre ») ;
- le **récit** quand deux habitants se parlent (dans l'échange déjà porté, `shareRumors`) : un souvenir par rencontre, et seulement ce qui « vaut la peine » ;
- la **déformation** à chaque bouche : le poids baisse, le chiffre enfle, le lieu dérive ; au bout de **trois bouches, c'est une légende** que plus personne n'a vécue ;
- le **ressenti du soir** : ce qu'on a vécu avec quelqu'un teinte la relation ;
- le **poids sur les décisions** : chaque souvenir pousse ou freine certaines envies (un deuil, se reposer ; un chantier achevé, rebâtir).

Ajouté pour Valmire : **ce qui est dit au feu devient mémoire**. Chaque répondant retient où il était, ce qu'il a
porté, qui n'est pas venu ; tous les autres l'ont entendu de sa bouche. Ces histoires partent ensuite dans le
village, et se déforment : le psautier de Konstantinos peut finir en « trésor sauvé de la Ville », ou en relique.

Particularité de la référence, gardée : quand une histoire perd le nom de quelqu'un, l'étape suivante le lui rend
(`??` lit le nom perdu comme absent). Dans le JS comme ici, le nom ne se perd jamais vraiment.

### 2. Le carnet du joueur

`Sim/AnastasisNotebook.*`. Le carnet ne note que ce que le joueur entend : ce qu'on lui raconte, ou ce qui se
raconte à portée de voix (six cases) entre deux habitants. Deux parties : **ce que chacun m'a dit** (une page par
habitant), puis **les histoires, version par version**, avec qui l'a racontée, à combien de bouches de la source,
et « racontée autrement » quand la version diffère de la précédente. Sans joueur incarné, il reste vide.
`Anastasis.Carnet.Write` l'écrit dans `Saved/Chronicle/`.

### 3. Les décisions (écart n°48, EXTENSION ; Bible §29)

- **Décider de bâtir** : chaque soir, une famille sans maison décide d'en bâtir une ; son chef trace la parcelle
  près de lui, sur une case d'où il atteint le puits et qui ne coupe personne de l'eau. Une famille dont un seul a
  un toit (Georgios, dans la maison d'ouverture, les siens dehors) bâtit aussi, après celles qui n'en ont aucun.
- **Seul pour un abri, à plusieurs pour une vraie maison** : la famille monte les murs ; le toit (la moitié des
  pièces) attend qu'un aidant du dehors soit venu y poser les siennes. La chronique le dit : « les murs sont debout ;
  … attend des bras pour le toit ».
- **Le tour du village** : chaque jour, le chef va demander de l'aide à deux personnes de plus, d'abord celles
  qu'il apprécie. Chacune pèse sa réponse et garde **la raison dominante** :

  | Raison | Sens | Poids |
  |---|---|---|
  | `dette_rendue` | « Tu m'as aidé, toi aussi » : il se souvient d'une aide **qu'il a reçue** de celui qui demande (une aide seulement racontée n'oblige pas) | +40 |
  | `amitie` | la relation | ×0,5 |
  | `voisin` | l'entraide d'un village qui commence | +8 |
  | `refus_rendu` | « Tu m'as dit non quand je te l'ai demandé » : il se souvient d'un refus **qu'il a essuyé** | −40 |
  | `son_toit` | sa propre famille bâtit encore | −45 |
  | `dette` | il l'a déjà aidé, et n'a jamais été aidé en retour | −25 |
  | `occupe` | il travaille déjà sur un autre chantier (−20), ou son métier a ses journées : le champ, le chantier (−10) | −20 / −10 |
  | `faible` | santé basse ou trop de faim | −40 |
  | `inconnu` | ni lien, ni histoire de lui | −12 |

- **Le refus se retient** : le chef s'en souvient (`refusedHelp`), et ce souvenir dira non à son tour.
- **Seuls la famille et ceux qui ont dit oui** travaillent sur la maison d'une famille (avant, tout le monde
  travaillait sur tout chantier ouvert, sans qu'on le lui demande).
- **La maison levée revient à sa famille**, et le chef se souvient de **chacun de ceux qui l'ont aidé**
  (`helped`) : la prochaine fois qu'ils lui demanderont, il dira oui, et pourquoi.

La chronique raconte tout cela : la famille qui décide, chaque demande (« Georgios va demander de l'aide à
Niketas… Niketas refuse : « Je viendrai quand mon propre toit sera levé. » »), les histoires qui circulent
(« Maria raconte à Theodora : … »), les légendes qui naissent, la maison qui revient à sa famille.

## Comment le lire

| Pour | Comment |
|---|---|
| trente jours sans ouvrir le jeu | test `Anastasis.Memoire.TrenteJours` → `Saved/Chronicle/test-memoire-30-jours.txt` |
| le jeu, avec le joueur | `Anastasis.Player.Arrive`, `Anastasis.Sim.Advance 30d`, `Anastasis.Chronicle.Write`, `Anastasis.Carnet.Write` |
| la preuve rejouable | `tools\unreal\editor-batch.ps1 -Proofs memory-pie` → `memoire-pie-30-jours.txt`, `carnet-pie-30-jours.txt` |

## Ce qui n'est pas fait

- Le tour du village ne se marche pas encore : la demande se fait le soir, sans que le chef aille physiquement
  frapper aux portes. Le joueur ne peut pas encore demander lui-même (même verbe, prochaine étape).
- Les croyances sur les personnes (`socialMemory.js`), la culture, la famine, le vol, les naissances : non portés (écart n°47).
- Le carnet est un texte, sans interface ; les répliques ne sont pas encore dites à l'écran.
- Les répliques de la référence restent sans accents (« J'ai acheve un chantier »).
- Le joueur incarné meurt de faim vers le jour 8 dans la preuve, même quand on lui pose « boire » ou « manger » :
  à partir du jour 3 il n'exécute plus le but posé (défaut du joueur, tâche séparée). Le carnet s'arrête donc à sa mort.
