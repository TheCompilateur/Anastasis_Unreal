# AAA_CONTACT_REALISM_001 -- la peau de contact

Objet : a un a vingt metres de la camera, le monde se lit comme des **objets poses sur un terrain**
(arbre + sol, rocher + sol, eau + sol). Cette couche pose la matiere qui les raccorde : un sol qui
s'assombrit sous les racines, une vase au bord d'une eau calme, un sediment au pied d'un rocher, une
roseliere sur un sol organique. Elle ne redessine ni le relief, ni les rivieres, ni la distribution des
arbres, ni l'atmosphere.

**Verdict : PARTIEL** (voir « Preuve » et « Limites »). Le raccord de pied d'arbre et le lustre humide
des rives se voient a 1-3 m ; a 80 m l'ecart est quasi nul, ce qui est voulu pour le calme du paysage mais
ne prouve pas l'integration a cette distance.

## Ce qui existe deja, et ce que la couche ajoute

| Deja la (non touche) | Ce qu'il fait | Ce qui manquait |
|---|---|---|
| `AnastasisRiverbank` | vase / gravier peints en couleur de sommet, roseaux, galets, blocs | une matiere **sur** le sol qui suive la berge : bande humide, vase luisante, limon ; rien n'etait un decalque |
| `AnastasisTrunkContact` | touffes de litiere et coussin de mousse (maillages) | le sol lui-meme reste uniforme sous la touffe : pas de zone racinaire sombre, pas de litiere en nappe |
| `AnastasisMicroEcology` | billes, souches, tas de branches, racines, roseaux, buissons | ces objets sont poses sur un sol propre : pas d'ombre de contact |
| sol `M_AnastasisGround` (soil-crusade, soil-slope) | variation de matiere par pente, humidite, famille de litiere | un champ continu ; rien au droit de chaque objet |
| `AnastasisPlaces` / `AnastasisUnderstory` | rochers, blocs | une collerette de sediment, des cailloux au pied |

Il n'y avait **aucun decalque** dans le projet. La couche est donc additive : elle lit, elle ne reecrit rien.

## Architecture

```
AAnastasisWorldEmbodiment (inchange)           AnastasisTerrainForge / AnastasisDrainage (lecture)
   HISM : arbres, rochers, roseaux, billes ...        sol, eau, vitesse de l'eau
                  \                                   /
        UAnastasisContactRealismSubsystem  (nouveau, UTickableWorldSubsystem)
           GatherAnchors  ->  AnastasisContactRealism::Build  (nouveau, pur, deterministe)  -> PLAN (tout le monde)
                                   |
        UpdateLive : seuls les decalques a < anastasis.Contact.Radius du point de vue sont des composants
        UDecalComponent (MI_ACR_<famille>, DBuffer)  +  3 HISM de cailloux (SM_Rock_Low existants)
        dans un acteur transitoire qui appartient a la couche
```

- `Source/Anastasis_UnrealV2/WorldView/AnastasisContactRealism.{h,cpp}` : planificateur pur. Aucun
  UObject ; tout par `FInputs` (hauteur du sol, hauteur de l'eau, calme de l'eau, ancres).
- `AnastasisContactRealismSubsystem.{h,cpp}` : lit les HISM de l'incarnation, pose les composants. En
  monde de jeu il se reconstruit seul quand l'incarnation est stable (trois releves a 0,5 s) et diffuse
  autour du premier joueur. Monde d'editeur : `anastasis.Contact.Rebuild` apres `EmbodyCanonical`,
  `anastasis.Contact.Focus x y` pour poser le point de vue.
- `tools/unreal/aaa-contact-realism.{ps1,py}` : source d'autorite des materiaux
  (`/Game/Anastasis/AAAContactRealism/`) : `M_ACR_Decal` + onze `MI_ACR_<famille>`.
- `tools/unreal/contact-realism-capture.{ps1,py}` : A/B `anastasis.Contact.Realism` 1 / 0 / 1.
- `AnastasisContactRealismTests.cpp` : `Anastasis.ContactRealism.{ShoreStates,ShorePlan,Anchors}`.

### Diffusion autour du point de vue (pourquoi le plan n'est pas plafonne a 3200)

Premier essai : plafond global de 3200 decalques sur 83 045 ancres. Resultat : **151 arbres** sur ~6 000
recevaient un raccord (2,5 % des ancres vues avant le plafond). Le plan couvre donc tout le monde
(23 826 decalques sur la graine 12345, `plan_ms` 210-330 ms, une fois par incarnation) et seuls les
plus proches du point de vue (rayon 90 m, 3200 au plus) existent comme composants : 345 a 640 vivants
aux cameras de preuve. Hysterese de 15 % a la limite ; le point de vue doit bouger de 15 m pour rediffuser.

## Les cinq etats de rive

Calcules sur une grille de 1,5 m : sol, nappe, **distance a l'eau** (chamfer 8 voisins), niveau de la
nappe voisine. L'etat vient du contexte local, pas d'un anneau :

| Etat | Condition (`ClassifyShore`) | Matiere posee |
|---|---|---|
| `SHORE_REED` | >= 2 tiges de roseau **reelles** dans la case voisine, eau calme | `ReedBed` sous chaque plaque (centroide), `WetBand` sombre sur la rive |
| `SHORE_STONY` | pente > 0,28, ou eau vive (calme < 0,4), ou banc de gravier (bruit > 0,78 et pente > 0,08) | `StoneWet` + 1 a 4 cailloux |
| `SHORE_MUDDY` | eau calme > 0,6, pente < 0,14, revanche < 1,3 m, bruit > 0,32 | `Mud` (noire, luisante) + halo `Halo` |
| `SHORE_DAMP` | cas general dans la bande | `WetBand` + halo `Halo` |
| `SHORE_DRY` | bruit de plaque < 0,2 (troncon de berge dure), ou au-dela de la bande | rien, ou un lit de depot pale (`Deposit`) rare |

Bande locale 1,0 a 4,5 m (bruit de plaque a 25 m, plus large sur replat). Chaque famille a une
probabilite < 1 : **le vide est requis**. Sur la graine 12345 : `SHORE_DRY` 199/15 656 candidats,
`DAMP` 179/1 581, `MUDDY` 51/457, `REED` 589/2 932, `STONY` 262/3 008 (decalques / examines).

### Porte d'eau

Les decalques de rive sont **centres sur le niveau de l'eau** (pas sur le sol) et le materiau retranche
tout ce qui est sous ce niveau (`smoothstep(-6, 4, WorldPos.z - ObjectPos.z)`, parametre `Gate`). Sans
elle, le fond immerge, vu par transparence a travers Single Layer Water, devenait des ovales cyan
cernes de blanc (capture `second`). Un objet immerge ne recoit aucun raccord.

## Regles de contact

| Regle | Mise en oeuvre |
|---|---|
| Pied d'arbre | zone racinaire sombre decalee en aval + nappe de litiere plus large, moins pres de l'eau ; 78 % des adultes ; pas d'anneau (decalage, lacet aleatoire, bord casse par le bruit) |
| Pied de rocher | collerette de limon (`RockDirt`) allongee en aval + 0 a 9 cailloux enfonces a 35-60 %, plus nombreux en aval ; l'exclusion de l'herbe existe deja (les rochers entrent dans `Canopy`) |
| Ligne d'eau | cinq etats ci-dessus |
| Roseaux / sol mouille | `ReedBed` : sol organique + tiges paille couchees, sous les roseaux reels |
| Objets couches (billes, driftwood) | zone sombre allongee sur toute leur longueur, selon le lacet reel |
| Arbustes, souches, racines | contact sombre proportionnel a l'emprise |
| Prairie | creux plus humides (courbure), lits de depot pales sur replats, rigoles d'erosion sur pentes moyennes : tres rare |
| Vegetation | `SetReceivesDecals(false)` sur tous les HISM de l'incarnation (sinon le decalque noircit herbe, roseaux et troncs) ; etat d'origine restitue a `Clear()` |

Les taches (feuilles, graviers, tiges) sont des cellules en **centimetres monde** (feuille ~11 cm,
gravier 4-5 cm, tige 7 cm etiree x3), pas en UV : un decalque de 7 m aurait eu des feuilles de 50 cm.

## Preuve

Capture `Saved/ContactRealismEvidence/<Label>/` (non versionnee ; jeu de preuves `sixth`), graine 12345,
ciel epingle (jour 1, 16 h 30), memes cameras pour les trois etats `contact` / `base` / `contact2`.

| Vue | Ce qu'on voit | px > 16/255 contact vs base (derive contact vs contact2) |
|---|---|---|
| `z1_arbre` (1,7 m, 5 m) | zone racinaire plus sombre et luisante, taches de litiere ocre sur le sable | 18,3 % (4,0 %) |
| `z2_rive` (1,1 m, 3 m) | sable de rive plus humide, plus luisant ; cailloux et eau inchanges | 25,4 % (14,7 %, l'eau bouge) |
| `z1_rocher`, `z2_rocher` | collerette de limon discrete | 13,5 % / 12,9 % |
| `z2_paysage_80m` | quasi identique : le paysage reste calme | 4,7 % (2,6 %) |

L'ecart d'eau (vagues animees) domine les pourcentages de rive ; la lecture se fait sur les planches
`cmp_<vue>.png` (contact | base | difference x4). **GPU p50 par vue, contact / base / contact2** (jeu `sixth`,
RTX 3060, machine partagee) : `z1_arbre` 19,0 / 18,9 / 18,8 ms, `z1_rive` 19,7 / 19,0 / 18,9, `z2_arbre`
14,7 / 14,8 / 15,2, `z2_rive` 17,7 / 17,0 / 17,9, `z2_pierre` 17,4 / 17,3 / 19,2, `z2_paysage_80m`
16,2 / 16,1 / 17,0. Le cout de la couche (contact - base, de -0,2 a +0,7 ms) est dans la derive de la
machine (base - contact2 jusqu'a 1,9 ms) : **non mesure plus finement que ca**, un seul echantillon par vue.
`Anastasis.ContactRealism.*` : 3 tests sur 3 verts (editeur sans rendu).

## Cout et budget

- Composants vivants : `anastasis.Contact.MaxDecals` (3200) dans `anastasis.Contact.Radius` (9000 uu) ;
  fondu a `anastasis.Contact.FadeScreenSize` (0,012). Un decalque DBuffer coute sa surface a l'ecran.
- Cailloux : 3 HISM (un par variante de `SM_Rock_Low`), culling a 60 m, pas d'ombre, 6 000 au plus.
- Planification : ~210 ms une fois par incarnation (`plan_ms` du journal `ANASTASIS_CONTACT`).
- `anastasis.Contact.Realism 0` rend le monde d'avant ; en jeu la bascule est reprise au releve suivant.

## CVars et commandes

| | |
|---|---|
| `anastasis.Contact.Realism` | 1 (defaut) / 0 |
| `anastasis.Contact.Pebbles` | 1 / 0 : cailloux, applique a la reconstruction |
| `anastasis.Contact.FadeScreenSize` | fondu des decalques |
| `anastasis.Contact.MaxDecals` | plafond de decalques **vivants** |
| `anastasis.Contact.Radius` | rayon de diffusion autour du point de vue (uu) |
| `anastasis.Contact.PlanDecals` | plafond du plan (40 000) |
| `anastasis.Contact.InAutomation` | 0 : pas de reconstruction automatique pendant un test |
| `anastasis.Contact.Rebuild` / `.Status` / `.Dump <fichier>` / `.Focus <x> <y>\|off` | reconstruire / journaliser / ecrire le plan / forcer le point de vue |

## Limites connues

- **Effet discret.** Sur sol deja sombre (rive a l'ombre, vase peinte par `Riverbank`) la couche ne se voit
  presque pas ; son effet principal est un sol plus humide et plus luisant a 1-3 m. A 80 m, a peine.
- **Pas de recul sur le plan sans point de vue** : en editeur sans `Focus`, les decalques les plus proches du
  centre du sol rendu l'emportent.
- **Zone d'epreuve unique par graine.** Une seule graine (12345), deux zones ; aucun test sur le monde complet
  en PIE, aucun test d'un joueur qui marche (rediffusion en mouvement non mesuree).
- **Herbe** : brins non coupes (`GroundCover` hors perimetre) ; ils traversent un decalque.
- **RVT, PCG, decalques de maillage** : non utilises (voir plus bas).
- Les tests ne couvrent pas le rendu : seul le planificateur est teste.

## Pas fait, et pourquoi

- **RVT** : `DANGEROUS_TO_CHANGE` sur ce terrain (`fiches/sol.md`, `AAA_VISUAL_TARGET_LAB.md`). Les
  decalques DBuffer font le meme travail ici sans toucher au terrain ni au materiau de sol.
- **PCG** : le plugin n'est pas active ; l'activer modifie le `.uproject`, fichier partage. Les memes regles
  ecologiques (distance a l'eau, pente, contexte) sont dans le planificateur pur.
- **Decalques de maillage** : non utilises ; les projections restent bornees a pente <= 0,75.

## Pieges rencontres (a ne pas refaire)

Rangés aussi dans la memoire projet `ue58-decalques-pieges` :

- UE 5.8 : `DecalBlendMode` est obsolete (propriete protegee) ; le DBuffer se deduit des sorties branchees.
- Un `VectorParameter` sort du RGB : lire `.w` dans un Custom ne compile pas, et le moteur met le materiau par
  defaut (rectangle noir). `-nullrhi` ne compile aucun shader : seule une capture avec rendu le voit.
- `-ExecCmds` separe par des virgules.
- Ajouter des fichiers a `Source/` redecoupe les groupes unity : `FPlan` non qualifie dans
  `AnastasisMicroEcologyTests.cpp` est devenu ambigu (corrige d'un jeton).

Planches : `docs/visual/aaa-contact-realism-001/<vue>.png` (gauche = couche active, centre = couche coupee, droite = difference x4, memes cameras, ciel epingle).
