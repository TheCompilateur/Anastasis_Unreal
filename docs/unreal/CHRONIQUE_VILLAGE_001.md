# CHRONIQUE_VILLAGE_001 — « Valmire pousse toute seule », mission 1 : la chronique

## Décision d'Alexandre (2026-10-08)

Conversation de cadrage du 2026-10-08 : priorité au **gameplay**. Le jeu de référence est
[KINGDOMS](https://store.steampowered.com/app/409590/KINGDOMS/) (Oreol, 2015, abandonné) :
un monde qui vit sans le joueur, où l'on est une personne ordinaire. Le rêve : le ressusciter en 1204,
avec ce qui lui manquait, la profondeur de la Bible canonique (oikos, mémoire, terre, dettes).

Choix d'Alexandre, dans l'ordre :

| Question | Réponse |
|---|---|
| Ce qui fait rêver dans KINGDOMS | les quatre : le monde vit sans moi, être un simple habitant, fonder mon village, explorer |
| Si la première version jouable n'en porte qu'un | **le monde vit sans moi** |
| Le début de partie | **le premier soir au feu** (Bible §10.6, les trois questions) |
| Construire une maison | **les deux** : seul pour un abri, à plusieurs pour une vraie maison |
| Le jalon et ses trois missions ci-dessous | « Tout est good, go » |

## Le jalon : « Valmire pousse toute seule »

Trente jours de simulation, sans joueur :

1. Le premier soir, au feu, chaque famille fondatrice reçoit son passé : d'où elle vient, ce qu'elle a emporté, qui n'est pas venu.
2. Chaque famille monte d'abord un abri, vite, avec ses propres membres.
3. Puis elle veut une vraie maison : elle trace sa parcelle et va demander de l'aide aux autres familles, qui acceptent ou refusent, et disent pourquoi.
4. Une aide crée une dette. Rendue, elle crée de la confiance ; oubliée, de la rancune.
5. On regarde le village en jeu, et on lit sa chronique en texte.

**Fini quand** Alexandre lit les trente jours, a envie de savoir ce qui se passe au jour 31, et que deux
parties ne racontent pas la même histoire.

Le joueur entre ensuite comme une famille de plus, avec les mêmes règles (Bible §19). Fonder ailleurs et
explorer viennent après, sur cette base. Pendant ces trois missions, pas de mission de rendu.

## Les trois missions, dans les mots validés par Alexandre

**Mission 1 — La chronique** (cette mission, `chronique-village-001`)
> **Scène** : Bible §36, lire l'histoire du village sans image.
> **Je dois pouvoir** lancer trente jours et lire en français ce qui s'est passé, famille par famille.
> **Hors sujet** : tout rendu, tout nouvel asset.
> **Fini quand** : je lis la chronique et je comprends qui a fait quoi, et pourquoi.

**Mission 2 — Les familles du premier soir**
> **Scène** : Bible §9 et §10.6, le feu du premier soir.
> **Je dois voir** des familles (parents, enfants, un frère, un apprenti), pas des habitants isolés, chacune avec un passé, un objet et une dette morale.
> **Hors sujet** : le joueur, les dialogues, l'apparence.
> **Fini quand** : la chronique du soir 1 présente chaque famille en deux phrases, et ces phrases changent d'une partie à l'autre.

**Mission 3 — L'abri, puis la maison à plusieurs**
> **Scène** : Bible §29, le tour du village pour demander de l'aide.
> **Je dois voir** chaque famille monter son abri seule, puis demander de l'aide pour sa maison, recevoir des oui et des non motivés, et garder ses dettes en mémoire.
> **Hors sujet** : l'architecture, les matériaux, le joueur.
> **Fini quand** : la chronique raconte au moins une maison levée grâce à une aide, et un refus dont je comprends la raison.

La mission 3 fait d'abord la demande d'aide **entre PNJ** : le joueur prendra ensuite le même verbe
(`PLAYER_HELP_REQUEST_001` en décrit le côté joueur).

## Ce que fait la chronique

`Source/Anastasis_UnrealV2/Sim/AnastasisVillageChronicle.*`. Un observateur : à chaque frame, et toutes
les quatre heures simulées pendant un `Anastasis.Sim.Advance`, il lit le village et raconte ce qui a changé.

| Ce qui est raconté | D'où ça vient dans la simulation |
|---|---|
| la fondation : qui est là, ce qui est debout, ce qui est en chantier, qui a déjà un toit ou un métier | premier passage avec des habitants |
| arrivées, départs | habitants nouveaux ou disparus sans être morts |
| morts **et leur cause** (de soif, de faim, d'épuisement, de faiblesse) | `FVillage::GetDeaths()` (MORTALITY_001) |
| chantier ouvert, bâtiment posé, achevé **grâce à qui** | `Progress`, et les pièces posées / matériaux apportés de chacun |
| qui met la main à quel chantier (une fois par personne et par chantier) | `PiecesPlaced`, `MaterialsDelivered`, `BuildBinding` |
| qui s'installe où, **et s'il a aidé à le bâtir** | `HomeId` |
| métiers | `JobId`, `WorkplaceId` |
| amitiés, brouilles | `Relations` au seuil des liens (`FriendAt`, `RivalAt`) |
| faim **et pourquoi** (pas de grenier, grenier vide, ou grenier qui a encore de quoi), soif, faiblesse | `Needs`, stock des greniers |
| grenier vide, de nouveau garni, réserves trop maigres pour les bouches | `FoodAvailable()` |
| un bilan chaque soir : habitants, maisons, chantiers, grenier, sans-toit, morts | état lu en fin de jour |

Le texte a trois parties : **les habitants** (métier, toit, amis, mort), **les jours** (chaque ligne datée
« le matin », « le soir »…, puis le bilan), et **ce qu'a vécu chacun** (le récit d'une personne).

### Ce qu'elle ne sait pas encore, et pourquoi

- **Les familles** : la simulation n'en a pas (écart n°7, foyer sans famille). Mission 2. Le récit est donc
  « habitant par habitant » ; il deviendra « famille par famille » quand les familles existeront.
- **Les vrais noms** : la simulation n'en porte pas. Les noms sont **provisoires**, des prénoms baptismaux
  byzantins tirés d'une liste, stables pour une partie, différents d'une graine à l'autre, accordés au
  portrait que le jeu donne à l'habitant (homme, femme, ancien). La mission 2 les remplacera.
- **Les demandes d'aide, les refus, les dettes** : non portés. Mission 3. Le seul « pourquoi » raconté
  aujourd'hui est celui que la simulation connaît : cause de mort, faim et état du grenier, aide au chantier.

### Lecture seule

La chronique prend la simulation en `const` et vit dans `Anastasis_UnrealV2` : elle n'est pas un écart de
portage (`PROTOCOLE_ECARTS.md` : ce qui lit sans écrire n'en est pas un). Le test
`Anastasis.Chronique.LectureSeule` compare l'empreinte d'état (`StateDigest`) de deux villages identiques
après cinq jours, l'un raconté et l'autre non. `anastasis.Chronicle.Enabled 0` arrête la lecture.

## Comment la lire

| Pour | Comment |
|---|---|
| trente jours sans ouvrir le jeu | test `Anastasis.Chronique.TrenteJours` → `Saved/Chronicle/test-chronique-30-jours.txt` |
| le village du jeu, en PIE | `Anastasis.Sim.Advance 30d` puis `Anastasis.Chronicle.Write` → `Saved/Chronicle/chronique-<graine>-jour-<jour>.txt` |
| les dernières lignes dans le log | `Anastasis.Chronicle.Print [jours]` |
| la preuve rejouable | `tools\unreal\editor-batch.ps1 -Proofs chronicle-pie` → `Saved/Chronicle/chronique-pie-30-jours.txt` |
| depuis Python | `unreal.AnastasisSimulationDebugLibrary.get_chronicle_text(world)`, `get_chronicle_status`, `write_chronicle` |

`SeedStartVillage` (nouveau, public) pose le village du lancement sur une case déjà choisie : c'est la
séquence que le début de partie lançait en ligne après l'arpentage du site, sortie telle quelle pour qu'un
test la lance sans rendu.
