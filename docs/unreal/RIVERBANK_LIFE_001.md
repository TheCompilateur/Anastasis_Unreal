# RIVERBANK_LIFE_001 — l'eau à hauteur d'homme, et des rives vivantes

Suite de WATER_LOOK_001. Deux étapes demandées par Alexandre (2026-10-01) :

0. regarder l'eau **à 1,7 m** (jusqu'ici elle n'avait été jugée que de 100 m et plus) et mesurer
   ce qu'elle coûte ;
1. donner aux rives des bandes qui disent le courant : vase et roseaux en eau calme, gravier et
   galets en eau vive.

Bascule : `anastasis.Dressing.Riverbank` (1 par défaut, 0 = la prairie touche l'eau), appliquée
à l'incarnation. La correction de la rive des lacs est sous `anastasis.Terrain.WaterLook`.

## Étape 0 — audit à hauteur d'homme

Outil : `tools/unreal/riverbank-capture.ps1` (+ `.py`). Les caméras sont tirées du réseau
réellement rendu (export `anastasis.Drainage.Dump`) : plaine calme, eau vive, confluence,
berge vue de 40 m, ruisseau, rive de lac, mare. Œil à 1,7 m sur la berge, regard vers l'aval.

| Constat (vues `audit/*_water.png`) | Suite |
|---|---|
| **Rive de lac coupée à angle droit**, contre un palier plat 30 cm au-dessus de l'eau | corrigé ici (ci-dessous) |
| Rivières rapides : vagues en bandes régulières, taches claires (berge reflétée) | domaine de `river-look-001` (autre agent, réécrit `M_AnastasisWater`) — non touché ici |
| Berges nues, sol sableux jusqu'à l'eau | étape 1 |
| Coins de ruban visibles dans les coudes serrés | non traité |

**Coût de Single Layer Water : non conclu par la frame.** Deux captures du même état (`water`,
`water2`) donnaient des GPU p50 qui diffèrent jusqu'à ×4 (rive de lac 21 / 49 ms) : la machine est
partagée avec les éditeurs des autres agents, et le premier état paie la compilation des shaders.
D'où `-Profile` : un `ProfileGPU` par vue, la durée de la passe d'eau DANS la frame. Résultat :
voir « Coût » plus bas.

### Rive de lac : la cause, la correction

Même mal que la rivière en escalier de WATER_LOOK_001. L'arrondi des lacs floute le masque puis
le seuille à 0,5 — en cellules de 5 m —, et la grève partait ensuite de `niveau + 30 cm + 1,1 m
par cellule` : un palier, un bord qui suit la grille. Le masque flouté, lui, est continu ; il était
jeté. Désormais (WaterLook) on en garde la **distance signée à la rive**, `(0,5 − M) / |∇M|`, et
un talus droit de pente 0,15 la traverse : le fond remonte jusqu'à la ligne d'eau, la grève en
part. La ligne d'eau passe entre les sommets, interpolée : planche
`docs/visual/riverbank-life-001/rive_lac_ligne_d_eau.png`.

## Étape 1 — rives vivantes (`AnastasisRiverbank`)

Module pur, déterministe, sans UObject : `Source/Anastasis_UnrealV2/WorldView/AnastasisRiverbank.{h,cpp}`.

- **Champ de vitesse** : la vitesse de chaque point de rivière (Manning, déjà calculée par le
  drainage et lue par aucune végétation jusqu'ici) est estampée sur la grille du réseau, pleine à
  5 m du bord mouillé, nulle à 15 m. Ailleurs — lacs, mares, mers — l'eau est calme.
- **Calme / vif** : fondu entre 1,3 et 2,3 m/s. Seuils **relatifs au réseau** : ses vitesses sont
  hautes (seed 12345 : médiane 1,7 m/s, la grande rivière de plaine 1,1–1,5). À 0,45 / 1,1 (v1),
  tout sortait « vif » : des galets partout, des roseaux seulement aux lacs.
- **Roseaux** (`SM_Ecotone_Reed_01`, existant) : un massif par maille de 4,5 m au plus, à la ligne
  d'eau calme (sol de −30 à +25 cm de l'eau), pente < 0,35, dans les taches d'un bruit de 22 m —
  une roselière est faite de taches, pas d'un liseré. 6 à 13 tiges par massif.
- **Galets** (`SM_Rock_Low_01..03`) : par groupes de 3 à 7, de 18 à 55 cm, enfoncés d'un tiers, à
  la ligne d'eau vive. **Blocs** (`SM_Rock_Boulder`) : rares, dans le courant vif. Matériau de forme
  teinté pierre mouillée.
- **Bandes de sol** (couleur de sommet, avant la section de sol) : vase sombre et lustre humide
  (UV1.y) en eau calme, gravier gris et poids de roche (UV0.x) en eau vive.
- Incarnation : un HISM par famille, variante et tuile de 160 m, sans collision, réutilisés par nom
  (jamais recréés). Coupés pendant l'automatisation (`anastasis.Riverbank.InAutomation`).

Essayé et retiré : `SM_Ecotone_ShoreTuft` derrière les roseaux (v2) — à 1,7 m, des bulbes verts.
Les laîches de GROUND_COVER_001 tiennent déjà cette bande.

### Mesures (seed 12345, Human_Geography_V2)

`ANASTASIS_RIVERBANK` : 16 153 roseaux en 1 271 massifs, 35 240 galets, 431 blocs ; 51 824
instances, 248 HISM ; plan 97–131 ms, incarnation 190–240 ms. Peinture : 2 720 sommets de vase,
1 344 de gravier.

Écart d'image `banks` / `nobanks` (`compare.py`, part des pixels à plus de 16/255), comparé à
l'écart entre deux captures du MÊME état (`water` / `water2`), qui n'est pas de 3,6 % ici mais de
6 à 16 % : l'eau est animée.

| Vue | rives / sans rives | même état, deux captures | Lecture |
|---|---|---|---|
| mare | 30,7 % | 13,4 % | roselière nette |
| rive de lac | 15,3 % | 6,4 % | massifs à la ligne d'eau |
| confluence | 20,2 % | 16,0 % | galets côté vif, effet modeste |
| plaine calme | 17,0 % | 12,7 % | quelques massifs, effet modeste |
| eau vive | 13,0 % | 13,1 % | galets visibles à l'œil, **pas mesurables** : trop peu de pixels |
| ligne d'eau du lac (avant / après correction) | 34,2 % | 6,4 % | la rive n'est plus une grille |

Tests : `Anastasis.Terrain.Riverbank.CalmAndFast` (chenal synthétique : roseaux côté calme,
pierres côté vif, aucune du mauvais côté, chaque pied dans sa bande, déterminisme, refus des
entrées invalides), `.PaintBanks` (vase côté calme, gravier côté vif, fond immergé et terre
lointaine intacts, alpha gardé), `.CanonicalWorld` (monde réel : contrôles du réseau à 0 avec les
rives de lac continues, champ de vitesse, chaque instance posée sur le sol rendu).

## Coût

`riverbank-capture.ps1 -Label cost -States "water,flat,water2" -Profile` → `profile.txt`.

**Une seule mesure valide**, plaine calme à 1,7 m, état `water` (frame 942) :

| Passe | GPU |
|---|---|
| `SingleLayerWater` (inclusive) | 0,940 ms |
| `SingleLayerWaterDepthPrepass` | 0,128 ms |
| **eau, total** | **≈ 1,07 ms**, soit ~9 % d'une frame GPU de 11,66 ms |

Les 20 autres `ProfileGPU` ont profilé des frames vides (racine 0,04 ms, aucun rendu de viewport à
cet instant) : inexploitables, non rapportés. Le coût de l'eau n'est donc établi qu'en un point de
vue ; l'outil doit déclencher le profil dans une frame où le viewport rend (piste : après la prise
de vue, pas avant). La durée de frame de l'éditeur, elle, ne mesure pas l'eau sur cette machine
partagée (écart ×4 entre deux captures du même état).

## Ce que la mission ne revendique pas

- Les **bandes de sol** (vase, gravier) se voient à peine sous `M_AnastasisGround` à ces caméras :
  ce sont les roseaux et les pierres qui font la rive.
- Les vagues de rivière : laissées à `river-look-001`.
- Les coins de ruban dans les coudes serrés ; le sol « dune » des berges hautes (matériau de sol).
- Le rendu de `SM_Ecotone_Reed` lui-même (tiges à boules) : maillage existant, non retouché.
