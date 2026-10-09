# HANDOFF: sprites-vegetation-gpt-001

## MISSION

Treize sujets de vegetation tires d'une planche generee par GPT (marronnier, bouleau, chene, deux pins, cypres, genevrier,
noisetier, saule, arbuste a baies, rhododendron, fougere, prairie fleurie), MODELISES en vraie geometrie 3D dans Unreal, puis
poses logiquement dans le monde.

**Modelisation (GPT_FLORA_002).** Aucune texture, aucune image collee : la planche ne sert que de reference (silhouette,
proportions, couleurs mesurees). `gpt_flora_model.py` construit chaque sujet : fut mesure sur le sprite, branches ramifiees par
colonisation d'espace dans le volume que dessine la silhouette (la couronne est de revolution : la face cachee est une
hypothese), section decroissante (loi du tuyau), et au bout des rameaux de VRAIS maillages : feuilles pliees (2 a 8
triangles), feuilles palmees, pinceaux d'aiguilles, fleurs, baies, chandelles du marronnier, grappes du rhododendron,
fougere fronde par fronde, prairie brin par brin. Materiaux du projet : `M_AnastasisVegetation` (slot 0), `M_AnastasisBark`
(slot 1). Trois LOD. Un premier essai en cartes de feuillage texturees par les pixels des sprites a ete ecarte (mandat
d'Alexandre : modeliser, pas photocopier).

**Placement.**
- Arbres : cinq essences nouvelles (`DeciduousOak`, `Birch`, `ScotsPine`, `Willow`, `HorseChestnut`) dans `SelectTreeSpecies`
  (altitude, pente, riviere) ; `SM_Gpt_PinSombre` et `SM_Gpt_Cypres` sont une forme de plus de `BlackPine` et `Cypress`.
- Sous-bois : six sortes nouvelles dans `AnastasisUnderstory` (`Juniper`, `Hazel`, `BerryShrub`, `Rhododendron`, `Fern`,
  `Meadow`), posees seulement dans une cellule que le maquis, les ronces et les rochers laissent vide (leurs tirages sont
  inchanges, test a l'appui). Habitats : fougere = ombre humide sous les couronnes ; rhododendron = versants frais et humides,
  ubac, sous couvert ; noisetier = lisieres des couronnes ; arbuste a baies = haies (lisieres, berges) ; genevrier = pentes
  seches et pierreuses au soleil, terrain ouvert ; prairie fleurie = pres ouverts et frais, par plaques, au plat.

## FILES_OWNED

- `SourceArt/Vegetation/gpt/` (planche source, 13 PNG detoures, `gpt-flora.json`)
- `tools/unreal/gpt-flora-cut.py`, `gpt-flora-fit.py`, `gpt-flora-preview.py`, `gpt_flora_model.py`
- `tools/unreal/create-gpt-flora.ps1`, `create-gpt-flora.py`
- `tools/unreal/set_tree_grammar.py` (GPT_SPECIES), `tools/unreal/capture-tree-lineup.py` (`-Set gpt`, `-Set gpt_low`)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationRegistry.{h,cpp}`, `AnastasisPresentationResolver.cpp`,
  `AnastasisPresentationResolverTests.cpp`, `AnastasisWorldEmbodiment.cpp`, `AnastasisUnderstory.{h,cpp}`,
  `AnastasisUnderstoryTests.cpp`
- `Content/Anastasis/Vegetation/Gpt/`, `Content/Anastasis/Presentation/DA_AnastasisPresentation.uasset`
- `docs/visual/sprites-vegetation-gpt-001/`, `AGENTS.md` (index)

## COMMIT

PENDING

## MEC

- BUILD: PASS (`anastasis-unreal.ps1 build`, worktree)
- TESTS: suite sans rendu rejouee par `finish` (verdict au message de passation). Tests touches :
  `Anastasis.Presentation.TreeSpecies`, `Anastasis.Presentation.TreeZoning` (cinq essences), `Anastasis.Understory.GptFlora`
  (nouveau : habitats, plan d'avant inchange a densite 0), `Anastasis.Understory.EdgesRiversAndSpecies` (l'assertion d'ombre ne
  compte plus que le maquis d'origine : fougere et rhododendron aiment l'ombre).
- MESURES :
  - `GPT_FLORA_ASSETS::PASS`, triangles LOD0 / LOD1 / LOD2 : marronnier 7002/3441/254, bouleau 6617/3169/254,
    chene 9075/4953/254, pin sombre 6942/2528/254, pin sylvestre 5958/2262/254, cypres 3005/1371/320, genevrier 7654/2697/254,
    noisetier 4317/2340/254, saule 9993/4979/254, arbuste a baies 5323/2622/205, rhododendron 4694/2223/254,
    fougere 3978/730/58, prairie 2273/961/58 ; bornes Z = [-50, +50] pour les treize
  - `set_tree_grammar` : `VERIFY variants=36 attendu=36 ... especes=13 attendu=13 -> OK`, `RESULT::PASS`
  - recouvrement de la silhouette de face avec le sprite (rendu logiciel, IoU) : marronnier 0,74, cypres 0,71, noisetier 0,62,
    chene 0,60, bouleau 0,60, arbuste 0,53, prairie 0,49, pin sombre 0,49, rhododendron 0,44, saule 0,40, pin sylvestre 0,37,
    genevrier 0,36, fougere 0,36 -- le chiffre dit la silhouette, pas la beaute
  - monde reel (`capture-slice.ps1 -Mode 2`, journal `ANASTASIS_UNDERSTORY_GPT`) : genevrier 2877, noisetier 1566, arbuste a
    baies 2367, rhododendron 613, fougere 3657, prairie 1577 (placed=38687 au total, missing_meshes=0) ; arbres
    (`ANASTASIS_TREE_TAXA_GPT`) : chene caducifolie 539, bouleau 423, pin sylvestre 775, saule 8, marronnier 4 sur 5 489
  - 1er `finish` (ancienne methode) : `Anastasis.Terrain.HumanGeography.CollisionAndDressing` en echec (plafond de 30 m, pin
    sylvestre a 34 m) ; fourchette ramenee a 18-24 m
- COMMANDS :
  - `python tools/unreal/gpt-flora-cut.py`, puis `gpt-flora-fit.py`, puis `gpt-flora-preview.py` (hors editeur)
  - `tools\unreal\create-gpt-flora.ps1` (maillages et registre)
  - `tools\unreal\capture-tree-lineup.ps1 -Out <png> -Set gpt` et `-Set gpt_low`

## PROOFS

PROOFS: (aucune)

## SCN

Images dans `docs/visual/sprites-vegetation-gpt-001/` : `engine-arbres.png` et `engine-petits-sujets.png` (planches a hauteur reelle
sous la lumiere du banc d'Unreal), `monde-oblique.png` (le monde reel), `preview-planche-1.png` / `-2.png` (rendu logiciel :
sprite, face, trois-quarts, profil), `decoupe-controle.png` (decoupe de la planche). Verdict visuel : Alexandre.

Defauts vus, non corriges :
- feuillage encore plus clairseme que les sprites (couronnes aeriennes, surtout noisetier, genevrier, pins) ;
- fut du chene en cone, tres massif ; saule : fut epais au milieu des brins ;
- fougere : une coupe basse, pas le vase touffu du dessin ; prairie : petite et clairsemee ;
- fleurs du rhododendron : saumon / orange au lieu de rose (le materiau rechauffe la lumiere transmise, un rose sans bleu
  vire a l'orange) ; chandelles du marronnier jaune-orange ;
- cadrage « lisiere a 1,7 m » non refait avec la nouvelle methode (editeur bloque a la capture) : les plantes ne sont pas
  verifiees a hauteur d'oeil dans le monde.

## PLY

UNKNOWN

## ECARTS

AUCUN — aucun fichier de `Source/AnastasisSim/` touche.

## INTEGRATION_RISK

- `DA_AnastasisPresentation.uasset` (binaire LFS) est reecrit : toute autre mission qui le modifie conflit. `set_tree_grammar.py`
  est idempotent : sur conflit, garder la version de `main` et relancer `create-gpt-flora.ps1`.
- La distribution reelle des essences change (cinq essences de plus dans `SelectTreeSpecies`) et le sous-bois gagne six sortes
  (la cellule est consommee seulement si le maquis, les ronces, les rochers n'en veulent pas) : ~38 700 instances de sous-bois
  au total sur la carte de reference.
- Triangles : LOD0 de 2,3 k (prairie) a 10 k (saule) ; les arbres de la foret passent surtout en LOD1 / LOD2. Cout GPU non mesure.

## STOP

- Pas de verdict de beaute : celui d'Alexandre, a l'image.
- La face cachee des arbres est une hypothese (couronne de revolution), pas une mesure.
- Anachronismes pour 1204 pontique, a trancher par Alexandre : marronnier d'Inde (absent du Pont a cette date) et saule pleureur
  (forme ornementale tardive). Poids faible dans le zonage ; a retirer si besoin.
- Aucune mesure de cout GPU ni de memoire sur la machine reelle.
