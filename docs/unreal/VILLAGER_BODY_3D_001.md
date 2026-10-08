# VILLAGER_BODY_3D_001 -- des corps qui marchent

> État historique au 2026-10-01. La règle de présentation a changé le 2026-10-08 :
> les portraits sont des références de style, les PNJ en jeu gardent un corps 3D à
> toute distance, et une ressource 3D manquante ne déclenche plus de carte PNG.
> Voir `docs/unreal/PLAYABLE_WINDOWS_001.md` (Correction PNJ). Les visages et
> vêtements en volume restent à modéliser.

Mission : critique d'Alexandre du 2026-10-01 sur VILLAGER_PNG_001 -- « ce sont des images mises sur des
objets en mouvement ; leur visage ne bouge pas, leurs pieds non plus, leurs mains non plus ». Option C
retenue : **corps 3D animes de pres, cartes portrait au loin**. Sans toucher au simulateur.

## 1. Constat (lu dans le code, pas suppose)

`AAnastasisVillagerVisual` etait un quad procedural avec UNE texture : il pivotait vers la camera
(lacet seul) et se retournait en miroir selon le sens de marche. Aucun squelette, aucune frame, aucun
balancement. Les pieds glissaient a 3 m/s, l'habitant etait toujours vu de face, eclaire par une
lumiere peinte.

## 2. Ce qui est fait

- **Corps** : le mannequin d'Epic deja dans le projet (`/Game/Characters/Mannequins`) -- `SKM_Manny_Simple`
  pour les hommes, `SKM_Quinn_Simple` pour les femmes -- joue en noeud unique sur `BS_Idle_Walk_Run`.
  Aucun Animation Blueprint : la position dans le blend space est la vitesse a laquelle la carte est
  dessinee (cm/s, lissee), ramenee a la taille du mannequin.
- **Cap** : le corps se tourne vers sa direction de deplacement (300 deg/s), garde son cap a l'arret.
  L'acteur continue de tourner vers la camera pour la carte ; le corps est en rotation absolue.
- **Tenue** : `M_AnastasisVillagerBody` lit la position PRE-SKINNING (pose de liaison) et peint par
  bandes : sandales, jambes nues, vetement de l'ourlet au cou, ceinture, bordure d'ourlet, manches
  courtes, avant-bras et mains nus, visage, cheveux. Les seuils sont MESURES sur les os de Manny par
  `create-villager-body.py`, pas devines.
- **Qui porte quoi** : `AnastasisVillagerLooks::BodyLookFor(Look)`, fonction pure du portrait.
  - **Teintes mesurees sur le dessin** (`villager-png.py colours` -> `villager-colours.json` ->
    `import-villagers.py` -> `FAnastasisVillagerLook::BodyGarment / BodySkin / BodyHead`) : le vetement
    (k-means a 3 groupes sur la poitrine, trait d'encre ecarte, le groupe le plus peuple), la peau (visage,
    sinon bras), la tete (cheveux, voile ou bonnet). Planche de controle :
    `docs/visual/villager-body-3d-001/A_teintes_mesurees.png`. Peau mesuree desaturee d'un tiers (peinte
    en pleine lumiere, elle tourne a l'orange sous le soleil du jeu).
  - **Palette de secours** si rien n'est mesure : laine ecrue, lin, laine grise, garance, guede passee,
    brun fonce, olive.
  - **Jamais un vetement couleur de peau** (`MinGarmentContrast` = 48 sur un canal sRGB) : la palette prend
    la teinte suivante, une teinte mesuree est eclaircie ou assombrie, sa nuance gardee.
  - Chiton au genou pour un homme, plus bas pour un aine, peplos a la cheville et cheveux longs pour une
    femme ; stature ~168 cm (homme) / ~158 cm (femme), -3 % pour un aine, +-3 % entre individus (Manny
    et Quinn mesurent TOUS DEUX 180 cm) ; cadence de marche +-5 %, -10 % pour un aine.
- **Bascule** : `anastasis.Village.Bodies` (0 cartes, 1 corps de pres -- defaut, 2 corps partout),
  `anastasis.Village.BodyDistance` (80 m). Au-dela, une personne fait une dizaine de pixels : la carte.
- **Registre** : `VillagerBodyMale`, `VillagerBodyFemale`, `VillagerLocomotion`, `VillagerBodyMaterial`,
  avec des defauts C++ : `DA_AnastasisPresentation` n'a pas ete reecrit. Une piece manquante -> la carte
  seule, a toute distance, et un Warning `ANASTASIS_VILLAGE villager bodies: missing`.

## 3. Preuves

### 3.1 Mesures du mannequin (`create-villager-body.ps1`, 2026-10-01)

Manny 180,5 cm, Quinn 180,2 cm, 2 emplacements de materiau chacun ; mannequin face a +Y, bras le long
de X. Seuils graves dans `M_AnastasisVillagerBody` (fraction de la hauteur) : cou 0,868, ceinture 0,575,
epaule 0,111, manche 0,154, cheville 0,041. `BS_Idle_Walk_Run` : blend space 2D, axe 0 = Direction
(-180..180), axe 1 = Speed (0..600) ; idle a 0, marche a 300, course a 600.

Defauts vus et corriges avant le PASS : `PreSkinnedPosition` n'existe qu'au vertex shader (« not available
in the Pixel shader ») -> `VertexInterpolator` ; la vitesse etait envoyee sur l'axe Direction ; premier
corps ocre sur peau halee -> l'habitant paraissait nu (regle de contraste, testee).

### 3.2 PIE (`villager-body-pie.ps1`)

| Run | Marche (pieds, balayage) | A l'arret | Bascule | Verdict |
|---|---|---|---|---|
| 1 | gauche 92,7 cm, droit 75,7 cm, cap 0 deg d'ecart | **non prouve** : l'habitant choisi s'etait remis en marche (299,7 cm/s) | 2 corps / 10 cartes, puis 12 cartes a 120 m, 0 erreur | PASS du script, mais mesure d'arret invalide -> script corrige |
| 2 | gauche 35,7 cm, droit 37,6 cm | vitesse 0 : main 1,95 cm, tete 2,16 cm (respiration de l'idle) | idem, 0 erreur | **FAIL** : 6 echantillons de marche en 3 s (machine saturee, ~2 images/s) -> la mesure attend 10 echantillons |
| 3 (teintes mesurees) | gauche 88,3 cm, droit 83,5 cm, 10 echantillons, cap 0 deg d'ecart | vitesse 0 : main 2,85 cm, tete 1,80 cm, 108 echantillons | 2 corps / 10 cartes ; 12 cartes a 120 m ; 0 erreur | **`VILLAGER_BODY_PIE::PASS`** |

Images du run 3 : `docs/visual/villager-body-3d-001/` -- B et C la marche de profil (foulee differente), D un aine
immobile de face, E corps de pres et carte au loin dans la meme image, F tout en cartes a 120 m.
Regard franc sur ces images : le corps marche et respire ; les teintes du dessin le rendent souvent tout brun
(planches au lavis sepia) ; aucun visage ; le vetement est une peinture sans volume.

### 3.3 Tests

A completer (`Anastasis.Village.Villagers.BodyLook`, `.Body`).

## 4. Limites -- a dire franchement

- **Pas de visage.** Le mannequin d'Epic n'a ni yeux, ni bouche, ni nez. Le visage est une surface
  couleur peau. C'est la limite la plus visible de pres.
- **Vetement peint, pas modele** : le chiton n'a pas de volume, pas de plis, la robe d'une femme est
  peinte sur ses jambes (pas de jupe). Lisible a 5 m, faux a 1 m.
- **Mannequin moderne** : proportions et demarche d'un personnage de jeu d'action ; une seule
  animation de repos et de marche pour tout le monde (cadence variee seulement).
- **Pas de gestes d'activite** : boire, manger, se reposer, travailler -> la meme pose de repos. Le
  pack ne contient pas ces animations.
- **Ressemblance au portrait limitee aux teintes** : vetement, peau, tete. Les planches sont lavees de sepia :
  beaucoup de vetements mesures sortent bruns, et un bleu passe se lit gris. Le reste (coupe, manteau,
  tablier, barbe, coiffe) n'existe pas sur le corps.
- **Objets portes** (fourche, panier) : absents du corps.

## 5. Suite (par ordre d'effet)

Le mannequin a atteint son plafond : on ne lui donnera ni visage ni etoffe en volume.

1. **`villager-metahuman-001`** -- le moteur installe embarque MetaHuman Creator (beta) et MetaHuman Crowd
   (experimental) ; leurs exemples Python (`Engine/Plugins/MetaHuman/MetaHumanCharacter/Content/Python/examples`)
   creent, sculptent, habillent et assemblent un MetaHuman par script. Un habitant prototype, teint / age /
   cheveux pris sur son portrait (`villager-colours.json`), assemble OPTIMIZED / MEDIUM, sur la meme marche et
   la meme bascule. Mesurer memoire et GPU avant d'en mettre douze. Risques : le rig du visage et les textures
   passent probablement par le cloud Epic (connexion d'Alexandre requise) ; aucune tenue antique fournie.
2. **Vetements antiques modelises** (chiton, peplos, himation) : Fab, ou modeles et pondere dans le moteur
   (`SkeletalMeshModelingTools`). Le plus gros chantier artistique.
3. Animations d'activite (puiser, porter, manger, s'asseoir) pilotees par `FNpc::Activity`.
4. Objets portes attaches aux mains (`hand_r`) selon le metier.
