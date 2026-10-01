# AAA Visual Target Lab

Worktree `C:\dev\ANASTASIS_WORKTREES\aaa-visual-lab`, branche `agent/aaa-visual-lab`, au-dessus de `main` `6371405`.
Port MCP de ce worktree : `http://localhost:8559/mcp`.

Réponse : **PARTIELLEMENT**. Le pipeline de rendu peut porter une scène haut de gamme. La base visuelle actuelle, elle, est un prototype procédural cohérent à moyenne distance, et elle casse dès que la caméra s'approche d'un corps, d'une feuille ou d'une arête.

Créé le 2026-10-01 dans l'éditeur du worktree (`AAA_LAB::CAPTURE_COMPLETE`). Le script a ouvert `/Engine/Maps/Entry`, pas la map du jeu. Le décal `M_AAA_Lab_Stain` n'est pas dans le niveau : `DecalBlendMode` est protégé en Python, le vertex paint reste la couche d'imperfection.

Audit : **115** static meshes sous `/Game/Anastasis` (hors personnages et map). Classes : **B 34, C 79, D 2, A 0**. Nanite activé sur **0** mesh de production. Dans le labo, Nanite est sur la pierre, le mur, la poutre et les dalles. Pas sur le mannequin ni sur la dalle de sol.

Les deux D : `SM_Ecotone_Bush_Low_01` (464 triangles, 47 cm) et `SM_Ecotone_Sapling_01` (144 triangles, 118 cm). Les arbres de la forêt sont en B (3 LOD, 7 000 à 11 000 triangles au LOD0) mais leur boîte native fait **100 cm** : la forêt les met à l'échelle à l'instance. Posés à l'échelle 1 dans le labo, l'understory mesure 62 × 58 × 100 cm. Ce n'est pas la hauteur en jeu.

Le sol posé est `SM_AAA_Ground_30m`, dalle de 8 cm, 20 160 triangles, Nanite off, photo `T_Ground_Worked` (lecture seule, déjà dans `GroundTextures`). La pierre lit `T_Ground_Rock`. Run 012 : terre granuleuse, ombre au pied du mur, taches `SM_AAA_Contact` (pied de mur, bout de poutre, flaque). Les icônes de lumière de l'éditeur sont encore dans l'image. La lecture des CVars par `ConsoleManager` a échoué (`unreal` n'a pas cet attribut) : le pipeline reste celui de `DefaultEngine.ini`, pas un relevé runtime.

## 1. Emplacement

Niveau prévu, créé seulement par le script d'éditeur :

`/Game/Anastasis/LookDev/AAA_Lab/Lvl_AAA_VisualLab`

Zone : 30 m × 30 m, centrée sur l'origine. Le script ouvre `/Engine/Maps/Entry`, puis `new_level` vers ce chemin, et il refuse de continuer si le monde courant ne contient pas `AAA_Lab`.

## 2. Fichiers créés (recette ; les `.uasset` attendent un éditeur qui finit)

| Fichier | Rôle |
|---|---|
| `tools/unreal/aaa-visual-lab.py` | Audit lecture seule, matériau maître, meshes du labo, niveau, trois caméras, captures |
| `tools/unreal/aaa-visual-lab.ps1` | Éditeurs via `Start-AnastasisEditor`. Refuse si la RAM libre est sous 4 Go |
| `docs/unreal/AAA_VISUAL_TARGET_LAB.md` | Ce rapport |
| `docs/unreal/handoffs/aaa-visual-lab.md` | Fiche de passation |

Assets que le script écrira, uniquement sous `/Game/Anastasis/LookDev/AAA_Lab/` :

- `M_AAA_Lab_Surface`, `MI_AAA_Soil`, `MI_AAA_Stone`, `MI_AAA_Wood`, `MI_AAA_Scale`
- `M_AAA_Lab_Stain` si le décal différé compile
- `SM_AAA_Ground_30m`, `SM_AAA_Stone_Hero`, `SM_AAA_Timber_Wall`, `SM_AAA_Timber_Beam`, `SM_AAA_Path_Stones`, `SM_AAA_Scale_180cm`
- `Lvl_AAA_VisualLab`

## 3. Fichiers modifiés

- `AGENTS.md` : une ligne d'index pour `aaa-visual-lab.ps1` + `.py`

`Config/DefaultEngine.ini` n'est pas modifié.

## 4. Fichiers évités

La racine canonique `C:\dev\ANASTASIS_UNREAL` n'a pas été écrite. `download.png` y reste non suivi, intact.

Non ouverts pour édition, parce que d'autres worktrees les portent :

- `Lvl_AnastasisSlice` et tout `Content/Anastasis/Maps`
- `M_AnastasisGround`, `M_AnastasisGrass`, `M_AnastasisBark`, `M_AnastasisLithos`, l'eau, le ciel
- `Source/` atmosphère, brouillard, hydrologie, forêt, village, PNJ
- Blueprints de jeu, `Config/DefaultEngine.ini`
- meshes existants : le labo les **instancie**, il ne les réécrit pas

Worktrees visuels présents au contrôle : `atmosphere-fog-coherence`, `atmosphere-mist-002`, `sky-transitions-001`, `forest-structure-001`, `understory-001`, `macro-forest-001`, `ground-texture-001`, `hydro-network-claude-01`, `hydrology-surface`, `lithos_forge_001`, `terrain-forge-001`, `ecotone-forge-001`, `villager-png-001`, `village-buildings-001`, `world-dressing-*`, `env-realism-001`, `river-look-001`.

## 5. Configuration renderer observée

Source : `Config/DefaultEngine.ini` sur `main`, plus le handoff `lumen-hit-lighting-001`. Les CVars runtime seront relus par le script dans `pipeline.json`. Elles ne sont pas devinées ici quand l'ini ne les fixe pas.

| Réglage | Valeur dans le projet | Classe |
|---|---|---|
| Moteur | UE 5.8.2, CL 56702186 | AAA_READY |
| RHI | DX12, SM6 | AAA_READY |
| Shading | `r.Substrate=1`, format GBuffer projet 0. Pas de `r.ForwardShading` | AAA_READY, substrat différé |
| GI | `r.DynamicGlobalIlluminationMethod=1` (Lumen) | AAA_READY |
| Reflets | `r.ReflectionMethod=1` (Lumen) | AAA_READY |
| Ombres | `r.Shadow.Virtual.Enable=1` | AAA_READY |
| Ray tracing | `r.RayTracing=1`, proxies projet activés | AAA_READY si le GPU le porte |
| Lumen hit lighting | `r.Lumen.HardwareRayTracing.LightingMode=1` | AAA_READY, déjà mesuré |
| Lumière statique | `r.AllowStaticLighting=False` | AAA_READY pour un soleil mobile |
| Distance fields | générés | AAA_READY |
| Cible matérielle | Desktop, `DefaultGraphicsPerformance=Maximum` | AAA_READY |
| Exposition locale | highlight 0.8, shadow 0.8 | LIMITING si on l'empile avec un autre contraste |
| Anti-aliasing, TSR, screen percentage | absents de l'ini | à lire au lancement, ne pas les forcer dans l'ini |
| Nanite projet | absent de l'ini | DANGEROUS_TO_CHANGE tant que l'audit éditeur n'a pas classé les meshes |
| Virtual textures / RVT | absents | DANGEROUS_TO_CHANGE : le sol et l'eau ont déjà leur arbitrage de canaux |
| Streaming textures | défaut moteur (le seuil dans l'ini est audio) | LIMITING seulement le jour où des textures photo existent |

Mesure déjà publiée, **sur la map de jeu**, RTX 3060, pas sur ce labo (`docs/unreal/handoffs/lumen-hit-lighting-001.md`) :

- sous-bois, GPU 17,50 ms (cache) → 20,86 ms (impacts), +3,36 ms
- le terrain procédural et les HISM d'arbres n'ont pas de cartes Lumen ; en mode cache, Lumen les voit noirs

### DANGEROUS_TO_CHANGE

- le bloc `[/Script/Engine.RendererSettings]`
- `r.Substrate` (recompile tous les matériaux)
- `M_AnastasisGround` (pente, humidité, couleur de sommet partagées avec l'hydrologie)
- activer Nanite ou le RVT sur le terrain procédural
- le brouillard, le ciel et le cycle du jeu : le labo a son propre `SkyAtmosphere` moteur, local au niveau

### LIMITING

- `UProceduralMeshComponent` sans cartes Lumen
- feuillage en cartes masquées + WPO : incompatible avec un simple « activer Nanite »
- matériaux procéduraux à une seule fréquence sur les bâtiments et les props (sinus dans un nœud Custom)
- silhouettes : cartes PNG, lames d'herbe, cône de `SM_Tree_Generic_01` là où il est encore la masse lointaine

### AAA_READY

Lumen, Virtual Shadow Maps, SM6, substrat, ray tracing matériel avec éclairage à l'impact, lumière dynamique. La question « est-ce que le moteur peut » a déjà sa réponse dans la config. La question ouverte est « est-ce que les surfaces le peuvent ».

## 6. Classification des assets

Heuristique du script (`classify`), appliquée au moment de l'éditeur et écrite dans `asset-audit.json`. En attendant ce JSON, la classe vient des recettes et des handoffs, pas d'un recomptage des `.uasset`.

| Classe | Sens | Exemples argumentés |
|---|---|---|
| A — AAA-CAPABLE | proche caméra sans honte : volume, normale de détail, rugosité variée, échelle humaine | aucun asset de production aujourd'hui |
| B — ACCEPTABLE WITH MATERIAL IMPROVEMENT | la forme tient à 8–20 m ; le proche demande une couche de surface | `M_AnastasisGround` (macro / méso / détail, pente, humidité, normale monde) ; arbres à 3 LOD notés dans `macro-forest-001` (canopée feuillu 9610 / 4804 / 450, conifère 12534 / 6267 / 558) ; ruines, rocs et lithos dont la recette dépasse ~1500 triangles |
| C — PROTOTYPE ONLY | lisible dans le monde, faux à 2 m | herbe (`M_AnastasisGrass`, deux faces, WPO, pas de texture, LOD écrits à 45 % puis 30 % des lames) ; roseaux ; props réfugiés ; abri (37 716 triangles, un seul LOD, Nanite off, `camp-shelter-009`) ; bâtiments village (albedo = sinus sur la couleur de sommet) |
| D — SHOULD EVENTUALLY BE REPLACED | la représentation elle-même est un placeholder | portraits PNG 512×1024 sur un canevas 128×256 cm, soit 4 px/cm (400 px/m) sur une carte plane, shading `MSM_DEFAULT_LIT` masqué ; `SM_Tree_Generic_01` décrit comme tronc + cône dans `ASTRAL_ENV_001.md` |

Rien de C ou D n'est supprimé.

Nanite, pour quand l'éditeur confirmera le flag réel (les recettes le posent à `False`) :

| | |
|---|---|
| NANITE GOOD CANDIDATE | roche opaque, mur, lithos, ruine, les meshes héros du labo (`enable_nanite` demandé, repli sans Nanite si la création échoue) |
| NANITE QUESTIONABLE | arbres : `MSM_TWO_SIDED_FOLIAGE` + WPO. Le convertir casse le vent ou le HISM forestier |
| NANITE BAD CANDIDATE | herbe, roseaux, PNG. Le terrain procédural n'est pas un static mesh Nanite ; lui donner des cartes Lumen est un autre chantier (`lumen-hit-lighting-001` STOP) |

## 7. Architecture matériaux

Un seul maître, `M_AAA_Lab_Surface`, opaque, `MSM_DEFAULT_LIT`, compatible avec le substrat déjà activé (même voie que les matériaux existants).

Entrées : position monde, couleur de sommet, position caméra.

Paramètres d'instance, groupe `AAA` :

- couleur : `BaseColor`, `TintB`, `DirtColor`, `MossColor`, `ColorVariation`
- surface : `Roughness`, `RoughnessVariation`, `Metallic`, `AoStrength`
- échelles : `MacroScale`, `MicroScale`, `DetailStrength`, `WoodAmount`
- récit : `Dirt`, `Wetness`, `MossAmount`
- distance : `FadeStart`, `FadeEnd` (cm)

La couleur de sommet porte le mélange local, pour éviter une frontière d'asset :

- R saleté
- G humidité (assombrit, baisse la rugosité vers 0,22, monte le spéculaire)
- B mousse, multipliée par `MossAmount`

Trois bruits incommensurables (macro, méso, micro), plus un grain de bois quand `WoodAmount` vaut 1. La normale de détail est analytique, en espace tangent, et son amplitude tombe entre `FadeStart` et `FadeEnd`. L'AO reste entre 1 et 0,84 : ancrage, pas un liseré noir.

Instances : sol, pierre, bois, mannequin gris (`DetailStrength` 0, pour que l'échelle ne mente pas avec un faux relief).

Le maître du jeu `M_AnastasisGround` reste la référence de production pour le terrain. Il a déjà la bonne famille (monde, trois échelles, pente, lustre humide). Le labo ne le remplace pas : il montre la même famille sur la pierre et le bois, là où le jeu n'a encore qu'un sinus.

Décal : `M_AAA_Lab_Stain`, domaine décal différé, mode stain, opacité en disque bruité, posé sur la flaque. S'il ne compile pas, le script le saute et le vertex paint reste la couche d'imperfection. Pas de RVT : l'activer sur le projet toucherait le sol.

## 8. Benchmark visuel

Trois caméras fixes, mêmes chiffres à chaque run. Hauteur des yeux pour A : 165 cm. Repère humain : `SM_AAA_Scale_180cm` (boîte 180 cm, pas un PNJ).

| | position (cm) | cible | FOV |
|---|---|---|---|
| A proche | (380, -620, 165) | (40, -80, 90) | 48 |
| B moyenne | (980, -1280, 250) | (40, 180, 110) | 55 |
| C large, encore dans les 30 m | (1750, -2100, 780) | (0, 280, 90) | 62 |

Le fond de la zone (Y positif) reçoit, à l'échelle native, un arbre understory, un rocher, un mur de ruine, un lithos, un bois flotté, trois touffes d'herbe. Le premier plan (Y négatif) est la pierre, le mur de planches, la poutre, les dalles. Même lumière, même sol. L'écart se voit dans le cadre, il n'est pas un autre éclairage.

Lumière locale au niveau : directionnelle 10 lux, soleil d'atmosphère, contact shadow length 0,2, skylight temps réel, `SkyAtmosphere` moteur, post-process non borné **dans ce niveau seul** (biais 1,2, exposition min et max à 0,5, bloom 0,35). Pas l'acteur d'atmosphère du jeu.

Captures du run 012 : `cam_a.png`, `cam_b.png`, `cam_c.png` dans `Saved/SliceEvidence/aaa-visual-lab-012/`. Les icônes de lumière et le cadre de sélection de l'éditeur sont encore dans l'image.

## 9. Benchmark performance

Non mesuré sur ce labo. Inventer un FPS serait faux.

Budgets connus d'ailleurs :

- map de jeu, viewport éditeur, RTX 3060 : environ 17–21 ms GPU selon la vue (`lumen-hit-lighting-001`)
- herbe monde : de l'ordre d'un million d'instances HISM en éditeur sur la slice (`GROUND_COVER_001`)

Le labo, lui, est petit : un plan de 30 m, une pierre 48×24, un mur de 11 planches, une poutre, six dalles. Son coût sera Lumen et l'ombre, pas le triangle. Nanite sur la pierre, le mur, la poutre et les dalles est un test de compatibilité, pas un besoin de budget.

Deux profils, à appliquer **dans la console de cet éditeur**, jamais dans `DefaultEngine.ini` :

| | TARGET QUALITY | TARGET PERFORMANCE |
|---|---|---|
| hérite | Lumen, VSM, SM6, hit lighting | les mêmes |
| écran | 100 | 80 (`r.ScreenPercentage`) |
| détail matériau | `DetailStrength` des instances | `FadeStart` 600, `FadeEnd` 1200 |
| Nanite labo | demandé sur l'opaque héros | repli off : la scène tient sans |
| contact | 0,1 | 0,1, on le garde |
| bloom | 0,35 | 0 |

## 10. Écarts

| | CURRENT LEVEL | TARGET LEVEL | MAIN GAP | REQUIRED WORK | COST |
|---|---|---|---|---|---|
| TERRAIN | mesh procédural, couleur de sommet, `M_AnastasisGround` déjà multi-échelle ; pas de cartes Lumen | relief lisible à 2 m, détail qui meurt avec la distance, contact au pied des objets | fréquence du mesh (tuile / tessellation) et cache Lumen, pas la logique du shader | cartes Lumen ou `UDynamicMeshComponent`, déjà identifié ; ne pas réécrire le shader de sol | élevé, chantier terrain |
| VEGETATION | lames opaques, deux faces, WPO, fondu distance ; arbres LOD moyens, Nanite off, vent en WPO | écorce volumique à 2 m, feuille qui a une épaisseur, SSS déjà là sur l'herbe | le proche montre le nombre de lames et la carte alpha, pas un manque de 4K | une espèce héros à part du HISM mondial ; garder les cartes pour > 15 m | moyen pour un arbre, élevé pour toute la forêt |
| ARCHITECTURE | ruines et bâtiments en géométrie script, matériau sinus sur vertex color, Nanite off, souvent 1 LOD | pierre et bois avec cassure de rugosité, saleté en pied, mousse locale | la forme est là ; la surface est une teinte | instances du maître labo, d'abord sur une copie | bas pour un mur, moyen pour le village |
| PROPS | accessoires et abri denses (abri : 37 716 tris) mais shader plat, 1 LOD | objet tenu en main crédible | texel et micro-normal, pas le triangle count de l'abri | matériau ; LOD seulement si l'objet revient souvent loin | bas |
| MATERIALS | sol déjà B ; le reste est un Custom à un sinus, ou une couleur de sommet | un maître, des instances, mélange par masque | duplication de logique, pas de chute de détail à distance | le maître du labo, puis migration asset par asset | bas par famille |
| SHADOWS | VSM projet ; contact non uniformément posé ; Lumen assombrit si la surface n'a pas de carte | contact au sol, feuille qui porte une ombre, pas d'objet flottant | cartes Lumen manquantes + biais de contact | `contact_shadow_length` local ; cartes Lumen côté terrain | contact : bas. Cartes : élevé |
| GEOMETRY | assez de triangles au loin sur les arbres notés ; trop lisse ou trop carte au près | facette là où l'œil touche, carte là où l'œil ne résout plus | complexité mal répartie | héros seulement : pierre, un arbre, un mur. Pas une subdivision globale | moyen |
| TEXTURES | presque pas de textures. Le flou proche des PNG vient du canevas 400 px/m sur une carte | 1024 px/m sous 2 m, 512 px/m en architecture, 256 px/m au sol + normale de détail | une 4K sur la carte PNG ne crée pas de volume | densité ci-dessous, réservée aux prochains assets peints | nul tant qu'on reste procédural |
| POSTPROCESS | exposition locale 0,8 / 0,8 dans l'ini ; pas de volume de lookdev | contraste lisible, bloom faible, AO < 0,2 d'écart | l'exposition locale peut aplatir un matériau déjà peu contrasté | volume du labo seulement | bas |
| PERFORMANCE | 17–21 ms GPU sur la map, une vue forestière à ~48 ips après le hit lighting | le labo doit rester sous le coût d'une vue de jeu | le monde, pas les 30 m | mesurer `cam_b` avant d'ajouter du luxe | une session |

## 11. Prototype qui peut rester

- le sol procédural et son matériau, à plus de 5 m
- l'herbe et les roseaux comme masse, au-delà du fondu déjà paramétré (~108 m pour l'herbe, et le proche du labo dira jusqu'où la touffe tient)
- les HISM forestiers pour la canopée lointaine
- les PNG comme silhouettes de foule tant que la caméra ne s'arrête pas sur un visage
- les props et l'abri comme lecture d'occupation à l'échelle du hameau

## 12. À remplacer pour une caméra AAA

- le portrait PNG, dès qu'un visage passe sous ~4 m
- `SM_Tree_Generic_01` s'il sert encore de sujet (tronc + cône)
- le shader sinus des bâtiments et des props, sur les meshes qu'on cadre
- toute carte d'herbe cadrée en gros plan : la remplacer localement, laisser le système mondial

## 13. Recommandations de pipeline

Densité, pour les **prochains** assets peints. Le procédural du labo n'a pas de texel : l'UV est posé à 1 unité = 1 m (`position_cm / 100`) pour le jour où une texture arrive.

| usage | distance | cible |
|---|---|---|
| prop tenu, visage, outil | < 2 m | 1024 px/m |
| mur, porte, roche cadrée | 2–8 m | 512 px/m |
| sol | < 5 m | 256 px/m plus une normale de détail à l'échelle du centimètre |
| falaise, canopée | > 8 m | 128–256 px/m |

Règles :

- un maître, des instances ; la logique ne se copie pas
- trois échelles de bruit, jamais une seule (le sol du jeu le fait déjà)
- la saleté raconte un fait : pied de mur, flaque, bout de poutre. Pas un overlay général
- Nanite sur l'opaque statique qui gagne au proche. Feuille et terrain : dossier à part
- la preuve est `cam_a` / `cam_b` / `cam_c`, pas une vue orbit libre

## 14. Ordre

1. Lancer le labo quand la RAM libre dépasse 4 Go. Lire `asset-audit.json` et les trois PNG avant toute autre décision visuelle.
2. Geler le renderer global.
3. Faire gagner `cam_a` : pierre, bois, sol, contact. C'est le maître du labo.
4. Un arbre héros, hors du HISM.
5. Remplacer le PNG seulement pour les plans serrés.
6. Cartes Lumen du terrain, avec le chantier terrain, pas dans ce dossier.
7. Seulement ensuite : Nanite sur les opaques dont l'audit dit B, et un RVT si un mélange sol/pierre doit traverser des meshes que le vertex paint ne joint pas.

```powershell
cd C:\dev\ANASTASIS_WORKTREES\aaa-visual-lab
tools\unreal\anastasis-unreal.ps1 build
tools\unreal\aaa-visual-lab.ps1 -OutDir C:\dev\ANASTASIS_WORKTREES\aaa-visual-lab\Saved\SliceEvidence\aaa-visual-lab-001
```

Le premier build du worktree est complet. Le `.ps1` s'arrête tout seul si la RAM libre est sous 4 Go.
