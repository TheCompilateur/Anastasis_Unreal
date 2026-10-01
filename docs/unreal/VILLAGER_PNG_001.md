# VILLAGER_PNG_001 -- une population visuelle pour les habitants simules

Mission : donner un visage aux habitants de la simulation, par des PNG detoures, **sans toucher au
simulateur**, et les faire apparaitre dans le jeu.

**Etat (2026-10-01)** : 104 individus decoupes des planches d'Alexandre, importes, attribues ; au
lancement du jeu, 12 habitants portent deja leur portrait, sans aucune commande.

## 1. Ce qui existait (inspecte, pas suppose)

| Question | Constat |
|---|---|
| Representation graphique des PNJ | **Aucune.** `FAnastasisVillagePresentation::DrawDebug` dessinait une sphere de debug par habitant. `Sync` ne creait des acteurs que pour les batiments. Aucun sprite, billboard, Paper2D ou texture de personnage. |
| Demographie dans le simulateur | **Aucune.** `AnastasisVillage::FNpc` n'a ni age, ni sexe, ni stade de vie. Ecart n°8 de `AnastasisVillage.h` : « Tous les habitants sont des adultes ». |
| Habitants au lancement | **Aucun.** `OnWorldBeginPlay` remet la simulation a zero sur un village vide ; les habitants n'existaient que par `Anastasis.Village.First*` en console. |
| Ou ranger une table visuelle | `UAnastasisPresentationRegistry` (`DA_AnastasisPresentation`), deja « l'asset qu'une passe artistique edite ». Reutilise. |

## 2. Decisions (avec Alexandre, 2026-09-30 / 10-01)

- **Images** : planches dessinees par Alexandre (series 1, 2, 4, 5 : `SourceArt/Characters/Sheets/`),
  decoupees ici. Elles font autorite : le manifeste dit ou est chacun.
- **Rattachement** : table de presentation seule, deterministe sur `npc-N`. Toutes les categories
  existent ; en jeu, seuls adultes et aines **debout** sont attribues (la simulation n'a que des
  adultes ; un aine assis sur un banc glisserait a travers le village).
- **Le jeu ne s'ouvre plus vide** : « fais tout ce qui doit etre fait pour que les PNJ apparaissent
  dans le jeu ». Voir 3.4.

## 3. Chaine

```
SourceArt/Characters/Sheets/serie-{1,2,4,5}.png   les planches (autorite)
SourceArt/Characters/villager-population.json      panneaux, ids, poses assises, statures moyennes
   | villager-png.py sheets  -> Raw/<id>.png (decoupe brute)  + villager-extract.json (statures mesurees)
   | villager-png.py prep    -> PNG/<Categorie>/<id>.png      (canevas commun)
   | villager-png.py board / check                            (planches, detecteur de clones)
   | import-villagers.ps1    -> /Game/Anastasis/Characters/PNG/<Categorie>/CHR_*, M_AnastasisVillager,
   |                            DA_AnastasisPresentation.Villagers / .VillagerMaterial
   v
UAnastasisSimulationSubsystem::Tick -> FAnastasisVillagePresentation::SyncVillagers
   -> AAnastasisVillagerVisual (une carte par npc-N, pieds = SimToUnreal)
```

### 3.1 Decoupe des planches (`sheets`)

- Chaque panneau est detoure **d'un bloc** sur son fond creme (modele de fond quadratique, propagation
  depuis les bords), puis chaque piece va a l'etiquette `CHR_*` la plus proche. Couper en colonnes au
  milieu de deux etiquettes coupait le sac de `M_Adult_005` et le baton de `M_Adult_006`. Une piece qui
  couvre deux etiquettes (deux figures qui se touchent) serait coupee et signalee : aucune sur 104.
- Etiquettes : bande de texte au bas du panneau (deux lignes en series 4 et 5), coupee aux k-1 plus
  grands ecarts. Pire marge relevee : 8 px entre deux etiquettes contre 2 px dans une etiquette.
- L'ombre portee du dessin est du **fond des la propagation** (meme teinte que le creme, plus sombre,
  bas de la silhouette) : retiree apres coup, elle murait l'espace entre les jambes.
- Trous fermes de la couleur exacte du fond perces (mailles des filets de peche).
- Filets verticaux entre deux cases (serie 4) ecartes : moins de 5 px de large.
- Stature = stature moyenne de la categorie x hauteur du **corps** dessinee / mediane du panneau
  (la pointe d'une lance ne compte pas : premiere ligne assez large pour etre une tete). Assis :
  72 % de la stature debout, le dessin ne disant rien (serie 5 dessine ses aines assis aussi hauts
  qu'un aine debout).

**Corrections de numerotation** (les planches portent des doublons) :

| Planche | Etiquettes | Ids retenus |
|---|---|---|
| serie 2, garcons | 004, 005, 006, 007, **007**, 008 | 004 a 009, dans l'ordre |
| serie 4, filles | 009 a 014 (009 existe en serie 2) | 010 a 015 |
| serie 5, garcons | 009 a 014 (009 existe en serie 2) | 010 a 015 |

### 3.2 Canevas et textures

Canevas commun : 512 x 1024 px = **128 x 256 cm** (une lance de garde depasse la tete d'environ
60 cm), pieds sur l'axe vertical, 16 px au-dessus du bord. Une carte = toujours la meme taille monde ;
la stature est dans l'image. Textures BC7, sRGB, groupe Character, Clamp, **hors streaming** (104
portraits ~70 Mo ; streames, la premiere apparition servait un mip flou), couverture alpha conservee
dans les mips.

### 3.3 La carte (`AAnastasisVillagerVisual`)

Quad procedural (UV, normale, pivot explicites), materiau masque deux faces, lacet seul vers la camera
(billboard cylindrique). Eclairee par une normale monde penchee vers le haut : comme le sol, jamais
noire a contre-jour. Portrait tourne a gauche ; marcher vers la droite de l'ecran le retourne
(`Mirror`). Dedans un batiment : cachee. Acteurs transients.

### 3.4 Le village du lancement

`anastasis.Village.StartVillagers` (12 ; 0 = village vide comme avant) : au debut de partie,
`SeedFirstWell` -- le chemin existant des scenarios, aucun code de simulation neuf -- pose le puits et
les habitants. Le premier scenario explicite (`FirstWell`, `FirstHouse`, `FirstGranary`, `FirstFarmer`,
`FoodSupply`) **remplace** ce village : simulation remise a zero sur la meme graine, acteurs retires.
Les preuves PIE des autres missions retrouvent donc exactement l'etat d'avant.

### 3.5 Attribution (`AnastasisVillagerLooks`)

Pool = portraits adultes et aines, `bInGame` (debout). Chaque categorie ordonnee par CRC de l'id,
categories entrelacees en proportion : tout village de N habitants reflete la population (autant
d'hommes que de femmes a toute taille paire, teste). `npc-N` -> N-ieme du pool : les 62 premiers
habitants ont 62 visages differents. Aucun tirage dans le RNG de la simulation, aucune ecriture.

## 4. Code

**Aucun code de simulation PNJ n'a ete modifie** (`Source/AnastasisSim/` intact). Presentation et hote :

| Fichier | Changement |
|---|---|
| `WorldView/AnastasisPresentationRegistry.h` | `EAnastasisVillagerCategory`, `FAnastasisVillagerLook` (`bInGame`), `Villagers`, `VillagerMaterial` |
| `Village/AnastasisVillagerLooks.h/.cpp` | attribution pure |
| `Village/AnastasisVillagerVisual.h/.cpp` | la carte |
| `Village/AnastasisVillagePresentation.h/.cpp` | `SyncVillagers`, `FindVillager`, `Clear` les retire |
| `Sim/AnastasisSimulationSubsystem.h/.cpp` | appel dans `Tick`, CVars `anastasis.Village.Portraits` et `anastasis.Village.StartVillagers`, village du lancement et son remplacement, `GetVillagerCards` (debug, lecture seule) |
| `Village/AnastasisVillagerTests.cpp` | `Anastasis.Village.Villagers.LookPool`, `.Presentation` |

## 5. Preuves (2026-10-01, valeurs relevees)

| Preuve | Valeurs |
|---|---|
| `villager-png.py sheets` | 104 figures, 0 fusion, 12 poses assises ; `SHEETS::TOTAL Adult_Male=24 Adult_Female=24 Elder_Male=13 Elder_Female=13 Child_Male=15 Child_Female=15 total=104 en_jeu=62` |
| `villager-png.py prep` | 104 portraits ; 91 OK, 13 `A_REVOIR` sur le score de halo (0,16 a 0,22 pour un seuil de 0,15) : verifies a l'oeil sur fond sombre, ce sont les bords clairs du dessin (peau de mouton, voiles, lin), pas un liseré |
| `villager-png.py check` | aucun visage proche (correlation max 0,85 garcons, 0,72 hommes ; seuil 0,92) ; 7 paires signalees sur la silhouette seule : 4 aines assis (hors jeu), robes longues |
| `import-villagers.ps1` | `VILLAGERS_IMPORT::PASS imported=104 registry=104 failures=0`, `MATERIAL_OK` |
| tests `Anastasis.Village` | 7/7 Success, dont `Villagers.LookPool` (pose assise exclue) et `Villagers.Presentation` (digest du village inchange) |
| `villager-lineup.ps1 -Label population-v1` | `VILLAGER_LINEUP::PASS shots=13/13` (population entiere puis lots de douze) |
| `villager-pie.ps1` | `VILLAGER_PIE PASS` : **12 habitants et 12 cartes au lancement, sans commande** (`00-demarrage`) ; `FirstWell 12` remplace ce village (12 habitants, pas 24) ; portraits distincts, aucun assis ; retrait suivi |

Images : `docs/visual/villager-png-001/` -- A a E planches hors moteur ; F `F_jeu_demarrage_sans_commande`,
G sphere de simulation et carte aux memes pieds, H voisins devant le puits, I a K planches dans Unreal.

Ce que les runs ont corrige, chaque defaut vu avant d'etre corrige : liseré clair (alpha par
projection) ; piece de lin percee sur fond gris ; materiau `float4 -> float2` et un `PASS` qui ne
verifiait pas la compilation ; carte noire a contre-jour ; banc dans l'ombre de l'anneau d'horizon ;
8 femmes sur 12 habitants (entrelacement) ; colonnes qui coupaient les objets portes ; ombres au sol
qui muraient l'entre-jambes ; mailles de filet creme ; `bInGame` expose a Python sous `game` ; prises
PIE vides (gel PUIS cadrage). La premiere prise `02-debug` vide n'a pas de cause etablie : le log ne
montre aucune reprise de vue par le controleur ; elle est passee au run suivant, habitant immobile.

## 6. Limites connues

- **Hommes adultes** : le groupe le plus repetitif des planches (bruns barbus de 30-40 ans, distingues
  surtout par le couvre-chef et la silhouette). Le detecteur ne crie pas au clone ; l'oeil, un peu.
- **Resolution** : figures de 160 a 300 px sur les planches, agrandies x2 a x3,5 : douces de pres.
- **Vitesse** : un habitant avance de 4 tuiles/s, une tuile fait 20 m a l'ecran (400 cm x echelle 5) :
  ~80 m/s a `anastasis.Sim.Speed 1`. Ecart d'echelle simulation/rendu anterieur a cette mission.
- **Sens du regard** : les planches melangent figures tournees a gauche, de face, a droite ; la carte
  suppose « a gauche » pour le miroir. Aucun retournement n'a ete impose (`overrides.facing`).

## 7. Prochaines etapes (graphisme seulement)

- portraits par metier du simulateur (`JobId` farmer -> fourche, panier) ;
- vues de dos et de profil (les planches en montrent) pour une carte a plusieurs directions ;
- saisons, statuts sociaux, blessures, vieillissement ;
- plus de diversite chez les hommes adultes (ages, carrures, glabres) ;
- enfants en jeu des que la simulation en a.
