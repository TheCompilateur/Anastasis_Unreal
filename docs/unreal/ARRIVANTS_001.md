# ARRIVANTS_001 — « Valmire pousse toute seule », mission 4 : les arrivants et le conseil du soir

## Décision d'Alexandre (2026-10-08)

Après la mission 3, la chronique montrait un village qui s'endort au jour 7, une fois les maisons debout.
Proposition retenue : **« Les arrivants »** (« Go »).

| Question | Réponse |
|---|---|
| Qui décide d'accueillir | **les chefs de famille** : le soir, au feu, chacun dit oui ou non avec sa raison ; la majorité l'emporte ; le moine peut parler, il ne décide pas |
| Les refusés | **ils repartent** : le village en garde le souvenir, et ce souvenir pèse sur la prochaine décision |
| D'où ils viennent | **du monde extérieur** : il tourne pendant la partie ; une crise au loin pousse de vrais groupes sur la route, avec une vraie cause |

> **Scène** : l'arrivée des étrangers. Des gens arrivent par la route, poussés par ce qui se passe au loin, et Valmire doit décider qui il devient.
> **Je dois voir** des arrivants qui demandent à rester ; le village qui en débat, chacun avec ses souvenirs ; une décision, oui ou non, avec ses raisons ; ceux qu'on accueille qui bâtissent à leur tour et demandent de l'aide à des inconnus ; leurs histoires qui circulent, jusqu'à devenir des légendes.
> **Le joueur** entend le débat et les récits des arrivants : tout entre dans son carnet.
> **Hors sujet** : le rendu, une interface de vote, le combat.
> **Fini quand** : la chronique de 60 jours raconte au moins une arrivée accueillie et une refusée, chacune avec ses raisons ; une maison levée par des arrivants avec l'aide d'anciens ; au moins une légende ; et le village ne s'endort plus après le jour 7.

## Ce qui est construit

### 1. Le monde extérieur s'ouvre avec Valmire

Quand les fondateurs sont posés, l'hôte charge le monde extérieur (`anastasis.Geo.AutoLoad 1`, écart n°38) et
tient prêts les noms des groupes qui viendront (`Content/Anastasis/Scenario/valmire-arrivants.json` : six groupes,
le premier parle pour les siens). Le scénario `geo-pontos-1204.json` gagne trois chocs datés, chacun avec sa
provenance (`docs/historicity/briefs/arrivants-001.json`) et la façon dont ceux qui l'ont fui le racontent (`recit`) :

| Jour | Choc | Statut |
|---|---|---|
| 1 | la prise de la Ville par les Latins | VERIFIED (l'événement) ; la fuite jusqu'au Pont PLAUSIBLE |
| 2 | les pâturages d'été disputés : des bergers chassés des hauts de Cheriana | PLAUSIBLE (« disputed in May ») |
| 4 | un seigneur des marches pille les hameaux hauts de Paipert | PLAUSIBLE (dynastes pillards attestés en 1404) |
| 40 | les Grands Comnènes lèvent des hommes à Trébizonde | PLAUSIBLE |

La pression voyage de nœud en nœud ; un groupe arrive des jours plus tard (vers les jours 10, 14, 25, 44).

### 2. L'arrivée (écart n°53)

Le groupe devient une famille **en attente** : nommée, avec ses membres, et chacun de ses adultes se souvient de
ce qu'il a fui (`fled`). Il passe sa journée au village : il boit, mange, et peut raconter ce qu'il a vécu.

### 3. Le conseil du soir (écart n°53)

Le minuit suivant, chaque chef d'une famille accueillie vote. Une voix est une somme de raisons, et la plus forte
est celle que dit le chef :

| Raison | Sens | Poids |
|---|---|---|
| `nous_aussi` | il a vécu l'exil (la Ville, la route, ceux qu'on a laissés) | +25 |
| `remords` | il se souvient d'avoir dit non à d'autres | +25 par refus vécu, +8 entendu (40 au plus) |
| `leur_histoire` | il a entendu l'un d'eux raconter sa route | +12 |
| `bras` | deux adultes ou plus | +8 |
| `grenier_plein` | 15 portions ou plus par bouche, nouveaux venus compris | +12 |
| `grenier` | moins de 8 portions par bouche (−20), moins de 4 (−45) ; ×1,5 pour qui a des enfants | −20 / −45 |
| `peur` | l'insécurité que le village subit du dehors | −50 × insécurité |
| `nombre` | au-delà de trois personnes | −6 par personne |
| `toits` | sa propre famille dort dehors (−20), et chaque autre famille sans toit (−5) | |
| `inconnu` | des étrangers | −10 |

La majorité stricte accueille (à égalité : non, on garde le grain). **Accueilli**, le groupe devient un foyer
comme les autres : il trace sa parcelle et va demander de l'aide (écart n°48). Le soir même, chacun de ceux qui ont
dit oui, puis les arrivants des vagues d'avant, raconte à l'un des nouveaux ce que le village se raconte : l'histoire qui a déjà le plus voyagé (la fondation, une
maison levée, un refus).
Le groupe suivant l'entendra de la bouche du précédent, à la troisième bouche : c'est ainsi que naissent les
légendes (dans un village de vingt personnes, sans nouveaux venus, tout le monde sait une histoire avant qu'elle ait
fait trois bouches, et elle s'oublie en deux semaines). **Refusé**, il repart au matin ;
ceux qui ont dit non s'en souviennent (`hostingRefusal`), et ce remords parlera au conseil suivant.

### 4. Ce qu'on en lit

- **La chronique** : « Par la route de Paipert, un groupe arrive : Theophilos, Anna et Kyranna, la famille de
  Theophilos. Ils fuient le pillage des hameaux hauts de Paipert. Theophilos demande à rester : « … » », puis
  « Le soir, au feu, les chefs de famille parlent de la famille de Theophilos. Arsenios : « Je ne décide pas… » »,
  la voix de chaque chef (« Georgios : non. « Le grenier ne nourrira pas tout le monde. » »), et le verdict.
- **Le carnet** : le joueur au village entend chaque voix du conseil (page « au conseil »).
- **Le pourquoi** : `AnastasisSimulationDebugLibrary.get_arrivals_status` rend chaque groupe et chaque voix avec
  le détail de sa somme (« nous_aussi=25 grenier=-20 peur=-19 toits=-24 inconnu=-10 »).

## Comment le lire

| Pour | Comment |
|---|---|
| soixante jours sans ouvrir le jeu | test `Anastasis.Arrivants.SoixanteJours` → `Saved/Chronicle/test-arrivants-60-jours.txt` (monde de test, plus pauvre que la carte) |
| le jeu | lancer PIE, `Anastasis.Sim.Advance 60d`, `Anastasis.Chronicle.Write` |
| la preuve rejouable | `tools\unreal\editor-batch.ps1 -Proofs arrivants-pie` → `arrivants-pie-60-jours.txt`, `Saved/ArrivalsEvidence/pie/arrivals.json` |

## Ce qui n'est pas fait

- Le joueur ne vote pas : il écoute. Un groupe ne demande pas à un foyer précis (`tryHostGuest` n'est pas porté).
- Les refusés disparaissent de la carte au matin ; on ne les revoit pas sur la route.
- Les arrivants n'apportent ni bêtes ni grain ; le monde extérieur ne pousse ni commerce ni impôt jusqu'au village.
