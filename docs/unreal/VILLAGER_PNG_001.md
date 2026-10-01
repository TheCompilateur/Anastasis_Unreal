# VILLAGER_PNG_001 -- une population visuelle pour les habitants simules

Mission : donner un visage aux habitants de la simulation, par des PNG detoures, **sans toucher
au simulateur**. 32 individus distincts : 8 hommes adultes, 8 femmes adultes, 4 hommes ages,
4 femmes agees, 4 garcons, 4 filles.

## 1. Ce qui existait (inspecte, pas suppose)

| Question | Constat |
|---|---|
| Representation graphique des PNJ | **Aucune.** `FAnastasisVillagePresentation::DrawDebug` dessine une sphere de debug par habitant (couleur = soif) et une ligne de texte. `Sync` ne cree des acteurs que pour les batiments. Aucun sprite, billboard, Paper2D, widget ni texture de personnage dans `Source/` ou `Content/` ; seuls les Mannequins du template. |
| Demographie dans le simulateur | **Aucune.** `AnastasisVillage::FNpc` n'a ni age, ni sexe, ni stade de vie, ni identifiant visuel. L'ecart n°8 de `AnastasisVillage.h` le declare : « Tous les habitants sont des adultes sans metier de garde, sans famille ». Attributs individuels : `Id` (`npc-N`), `JobId`, `TraitIndex`. |
| Images de reference | Hors du projet Unreal : `C:\dev\Jeux IV Kingdoms\assets\references\npc\` (planche homme, planche femme, planche « PNJ integres au decor ») et `...\.worktrees\vanilla-unification\references\npc\` (+ 12 variantes low-poly). Planches de concept sur fond plein, un meme corps decline : des references de STYLE, pas une source de 32 individus. |
| Ou ranger une table visuelle | `UAnastasisPresentationRegistry` (`DA_AnastasisPresentation`) : « l'asset qu'une passe artistique edite », inconnu de la simulation. Reutilise, pas de seconde architecture. |

## 2. Decisions (prises avec Alexandre le 2026-09-30)

- **Source des images** : generees par Alexandre dans ChatGPT (l'outil des planches), un prompt par
  individu tire du manifeste ; cette session n'a aucun generateur d'images. Fiche :
  `docs/unreal/VILLAGER_PNG_001_PROMPTS.md`.
- **Rattachement aux PNJ** : table de presentation seule, deterministe sur l'identifiant `npc-N`.
  Les 32 portraits sont crees et visibles sur la planche ; **en jeu, seuls adultes et aines sont
  attribues**, puisque la simulation n'a que des adultes. Les enfants entreront en jeu le jour ou
  la simulation aura des enfants -- c'est une ligne dans `AnastasisVillagerLooks::IsAssignableInVillage`.

## 3. Chaine

```
SourceArt/Characters/villager-population.json   autorite : id, categorie, age, stature, physionomie
        | villager-png.py prompts       -> docs/unreal/VILLAGER_PNG_001_PROMPTS.md (ChatGPT)
SourceArt/Characters/Raw/CHR_*.png               depot des images generees (jamais modifie)
        | villager-png.py prep          -> SourceArt/Characters/PNG/<Categorie>/CHR_*.png
        | villager-png.py board/check   -> docs/visual/villager-png-001/ ; ressemblance par paire
        | import-villagers.ps1          -> /Game/Anastasis/Characters/PNG/<Categorie>/CHR_*
        |                                  /Game/Anastasis/Characters/M_AnastasisVillager
        |                                  DA_AnastasisPresentation.Villagers / .VillagerMaterial
        v
UAnastasisSimulationSubsystem::Tick -> FAnastasisVillagePresentation::SyncVillagers
        -> AAnastasisVillagerVisual (une carte par npc-N, pieds = SimToUnreal)
```

**Canevas commun** : 512 x 1024 px = 100 x 200 cm, pieds sur l'axe vertical, 16 px au-dessus du
bord bas. La stature du manifeste est appliquee au detourage (5,12 px/cm) : un enfant de 106 cm
et un homme de 181 cm ont la meme carte, pas la meme taille. Une carte = toujours la meme taille
monde.

**Detourage** (`prep`) : alpha natif s'il existe ; sinon fond uni modelise (quadratique, ajuste sur
les bords), propagation depuis les bords, alpha des lisieres par projection sur l'axe
fond -> couleur du sujet voisine, puis decontamination `F = (I - (1-a)B)/a` (plus de liseré clair).
Trous fermes (main sur la hanche) retires seulement sur fond vert d'incrustation : sur un fond
gris, une piece de lin ecru a la couleur du fond. Couleur saignee sous l'alpha nul (mips propres).
Statut `A_REVOIR` si : sujet coupe par un bord, halo mesure, trous fermes non traites.

**Textures** : BC7 (contours d'un visage de 60 px), sRGB, groupe Character, Clamp, couverture alpha
conservee dans les mips (seuil 0,5 = celui du masque). Verifie par relecture a chaque import.

**Carte** (`AAnastasisVillagerVisual`) : quad procedural (UV, normale et pivot explicites), materiau
masque deux faces, lacet seul vers la camera du joueur (billboard cylindrique : reste debout sous
une camera haute). Les portraits regardent a gauche ; marcher vers la droite de l'ecran retourne
la carte (`Mirror`), l'arret garde le dernier sens. Habitant dedans -> carte cachee. Acteurs
transients, jamais sauves.

**Attribution** (`AnastasisVillagerLooks`) : pool = portraits adultes et aines ; chaque categorie
ordonnee par CRC de l'id (independant de l'ordre d'import), puis les categories entrelacees en
proportion de leur taille : homme, femme, homme age, femme agee, homme, femme... Tout village de N
habitants reflete donc la population. `npc-N` -> N-ieme du pool modulo sa taille : les 24 premiers
habitants ont 24 visages differents. Aucun tirage dans le RNG de la
simulation, aucune ecriture : le digest du village ne peut pas en dependre (teste).

`anastasis.Village.Portraits 0` retire les cartes ; les spheres restent sous `anastasis.Village.Debug`.

## 4. Code

Aucun code de simulation PNJ n'a ete modifie (`Source/AnastasisSim/` intact).

Presentation (`Source/Anastasis_UnrealV2/`) :

| Fichier | Changement |
|---|---|
| `WorldView/AnastasisPresentationRegistry.h` | `EAnastasisVillagerCategory`, `FAnastasisVillagerLook`, `Villagers`, `VillagerMaterial` |
| `Village/AnastasisVillagerLooks.h/.cpp` | attribution pure |
| `Village/AnastasisVillagerVisual.h/.cpp` | la carte |
| `Village/AnastasisVillagePresentation.h/.cpp` | `SyncVillagers`, `FindVillager`, `Clear` les retire |
| `Sim/AnastasisSimulationSubsystem.h/.cpp` | appel dans `Tick`, CVar `anastasis.Village.Portraits`, `GetVillagerCards` (debug, lecture seule) |
| `Village/AnastasisVillagerTests.cpp` | `Anastasis.Village.Villagers.LookPool`, `.Presentation` |

## 5. Preuves

### 5.1 Chaine validee sur 32 portraits PROVISOIRES (2026-10-01)

Les images de la population ne sont pas encore generees. La chaine a ete prouvee de bout en bout
sur 32 silhouettes provisoires (formes plates, id ecrit dessus, nez vers la gauche pour lire
l'orientation), **jamais commitees** : ni les PNG, ni les textures, ni le registre qui les reference.
Ces preuves disent que la machinerie marche ; elles ne disent rien de la qualite artistique.

| Preuve | Valeurs |
|---|---|
| `build` | `BUILD::PASS` (worktree, Editor Win64 Development) |
| `villager-png.py prep` (banc synthetique) | fond gris, vert, alpha natif : `halo=0.000` partout ; tache claire de tunique conservee ; trou ferme bras/torse retire sur vert, signale `A_REVOIR` sur gris |
| `villager-png.py check` (banc synthetique) | 8 silhouettes clonees par construction : toutes signalees (IoU silhouette jusqu'a 0,997) -- le detecteur voit les clones |
| `import-villagers.ps1` | `VILLAGERS_IMPORT::PASS imported=32 registry=32 failures=0` ; chaque texture 512x1024 BC7 sRGB Character Clamp hors streaming couverture alpha ; `MATERIAL_OK` (0 echec apres la compilation finale) |
| `Anastasis.Village.Villagers.LookPool` | Success |
| `Anastasis.Village.Villagers.Presentation` | Success (digest du village identique avant/apres) |
| `villager-lineup.ps1` | `VILLAGER_LINEUP::PASS shots=5/5` : cartes debout, detourees, ombre portee partant des pieds, statures lisibles contre le temoin 180 cm |
| `villager-pie.ps1` | `VILLAGER_PIE PASS 12 habitants, 12 cartes, portraits distincts, retrait suivi` ; la sphere de debug de `npc-0` et sa carte coincident (02-debug) |

Corrections apportees par ces runs, chacune vue avant d'etre corrigee :

1. Detourage : liseré clair d'un pixel (seuil absolu) -> alpha par projection, decontamination.
2. Detourage : piece de lin clair percee sur fond gris -> trous fermes seulement sur fond vert ; la
   fiche de prompts demande le vert en repli (envoyee corrigee a Alexandre).
3. Materiau : `float4 -> float2` sur les UV (masques de canaux incomplets), et un `PASS` qui ne
   verifiait pas la compilation -> quatre canaux explicites, le `.ps1` refuse un echec apres la
   compilation finale.
4. Eclairage : carte noire a contre-jour -> normale monde penchee vers le haut (eclairee comme le sol).
5. Banc : scene au sol dans l'ombre de l'anneau d'horizon -> scene a 300 m, soleil fixe du banc.
6. Attribution : un tri CRC seul donnait 8 femmes sur les 12 premiers habitants -> entrelacement
   proportionnel, teste (autant d'hommes que de femmes a toute taille paire).
7. Preuve PIE : camera sur le barycentre de 12 habitants disperses sur 280 m, cartes de dix pixels
   -> cadrage sur un habitant ; et streaming des textures (mip flou a la premiere apparition) ->
   portraits hors streaming.

### 5.2 Population reelle

A faire des que les images sont deposees dans `SourceArt/Characters/Raw/` :
`villager-png.py prep` -> `board` -> `check` -> `import-villagers.ps1` -> `villager-lineup.ps1`
-> `villager-pie.ps1`, et ici les valeurs, les planches et le verdict « habitants ou clones ».

## 6. Prochaines etapes (graphisme seulement)

- davantage d'individus par categorie (le pool cycle au-dela de 24 habitants) ;
- vetements et metiers : une variante de portrait par metier du simulateur (`JobId` farmer -> outil, chapeau) ;
- saisons (manteaux d'hiver), statuts sociaux, blessures, vieillissement ;
- vues de dos et de profil pour une carte qui tourne a 8 directions au lieu d'un miroir ;
- enfants en jeu des que la simulation en a.
