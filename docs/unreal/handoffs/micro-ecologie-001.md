# HANDOFF: micro-ecologie-001

## MISSION

Le pré à hauteur d'œil se lisait comme des rubans sur une nappe. Cette passe amincit
l'herbe et pose un grain de sol d'environ 25 cm entre les touffes. Le C++ de
MICRO_ECOLOGY_001 est déjà sur `main` (89ba58c, 53b2343). Ici : le regard.

## FILES_OWNED

- `tools/unreal/create-ground-cover.py`
- `tools/unreal/ground-material.py`
- `tools/unreal/ground-cover-capture.py`
- `AGENTS.md` (ligne `capture-ground-cover`, états `eco,noeco`)
- `Content/Anastasis/GroundCover/SM_Grass_*.uasset`
- `Content/Anastasis/Materials/M_AnastasisGrass.uasset`
- `Content/Anastasis/Materials/M_AnastasisGround.uasset`
- `Content/Anastasis/Materials/MI_AnastasisGround.uasset`
- `docs/visual/micro-ecologie-001/`
- `docs/unreal/handoffs/micro-ecologie-001.md`

## COMMIT

ce commit

## MEC

- BUILD: le portail `finish` rejoue le build. Pas de C++ dans cette passe.
- TESTS: pas de nouveau test. La suite du portail classe PASS / KNOWN_EXPECTED_FAILURE / FAIL.
- COMMANDS:
  - `create-ground-cover.ps1` → `GROUND_COVER_ASSETS::PASS`
    MeadowTall [3058, 1232, 67], MeadowShort [2640, 1017, 75], Sedge [1560, 528, 34],
    HeathTussock [1216, 362, 68], Heather [1810, 759, 81], bornes Z min = -6 cm
  - `ground-material.ps1 -Rebuild` → `GROUND_MATERIAL::PASS` pixel_instructions=999
    DetailTiling 1/16, DetailContrast 0.40, TexAO 0.85, TexRoughness 0.28, TexNormal 1.15,
    GapSoil (0.16, 0.12, 0.07) amount 0.70
  - `capture-ground-cover.ps1 -Label ground-soil -States eco`
    prairie_low gpu 12.9 ms ; contre `prairie_low_apres` : 8,2 % des pixels > 16, luminosité inchangée
    premier plan (y 520–780) : moyenne 137,127,70 → 139,128,73 ; 17,5 % des pixels > 16 ; déjà jaune (warm 100 %)

## SCN

Cadre `prairie_low`, 60 cm. L'herbe du premier plan est plus fine, pieds plus sombres.
Le sol entre les touffes est moucheté, pas creusé : la terre du creux, sous le soleil,
vaut le jaune déjà là. Images : `docs/visual/micro-ecologie-001/prairie_low_apres.png`
et `prairie_low_soil.png` (œil : `prairie_eye_apres.png`, `prairie_eye_soil.png`).

L'A/B `eco` / `noeco` (label micro-eco-ab, avant cette passe d'herbe) : la couche
20 000 instances ne se voit pas aux caméras de validation. La berge visible est
riverbank, la lisière est la forêt P2, le maquis est l'understory.

## PLY

NOT_IMPLEMENTED (hors mission).

## INTEGRATION_RISK

- `ground-material.py` et `M_AnastasisGround` sont l'autorité du sol. `main` depuis
  bdc7a41 n'y a pas touché (seul `AGENTS.md`, ligne human-occupation, autre paragraphe).
  Le script et l'asset décrivent le même GapSoil capturé. Le prochain
  `ground-material.ps1 -Rebuild` réécrit l'asset : ne pas le retoucher à la main.
- Un disque sombre sous chaque touffe a été généré puis retiré avant l'asset versé.
  Sur `M_AnastasisGrass` il se lisait comme un trou hexagonal (`prairie_low_gap.png`).
- Un creux plus sombre (terre 0.045, queue basse du grain) a été écrit puis retiré
  du script : la porte mémoire a expiré avant le rebuild, l'image ne l'a pas vu.
- `AnastasisWorldEmbodiment` n'est pas modifié ici. Riverbank et la forêt P2 restent.

## STOP

Ne revendique pas : le contact vraiment sombre sous le pied, les cartes de la lande
dans le visage, le flare du tronc, le replacement du budget 20 000 devant la caméra,
le micro-relief, les PNJ, le ciel, Nanite.
