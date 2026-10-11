# HANDOFF: pontic-trees-geometry-001

## MISSION

Mandat d'Alexandre (2026-10-11) : les cinq essences des etages humides pontiques -- hetre d'Orient, epicea d'Orient,
charme, cerisier sauvage, sorbier des oiseleurs -- refaites en **vraie geometrie modelisee**, dans la chaine des treize
sujets de sprites-vegetation-gpt-001 (`gpt-flora-fit.py` -> `gpt_flora_model.py` -> `create-gpt-flora.py`), sans carte
de feuillage ni atlas (methode retiree de main). Elles remplacent les cartes texturees des branches
`agent/pontic-realism-001` / `agent/pontic-plants-3d-001` / `agent/pontic-fields-001` (non modifiees), dont on reprend
les references (sprites de la planche 02), les hauteurs et le zonage (ou elles poussent), pas la chaine de cartes.

**Modele.** Spec mesuree sur les sprites de la planche 02 (`SourceArt/Vegetation/gpt/PNG-pontic/`, decoupes sur
pontic-realism-001 par `gpt-flora-cut.py --set pontic` ; `planche-02.png` jointe pour la provenance) : tranches de
silhouette, fut, teintes ; plus grande tache seulement (oiseaux et petales isoles ecartes) ; pour le cerisier et le
sorbier, la teinte de l'accent (fleurs blanches, baies) est mesuree a part du vert. Puis, dans `gpt_flora_model.py` :
- hetre : colonisation d'espace, feuille elliptique entiere (`oblong`), ecorce lisse gris argent, fut plafonne (le pied
  du sprite porte un jeune hetre qui elargissait le releve) ;
- charme : couronne ovoide serree, feuille ovale (`diamond`), ecorce grise, grappes de bractees pales pendantes ;
- cerisier : branches ascendantes, feuille oblongue, ombelles de fleurs blanches a cinq petales au bout des rameaux
  (la planche le montre en pleine floraison ; part mesuree 29 %), ecorce brun-rouge a lenticelles ;
- sorbier : feuille composee **pennee** (rachis, trois paires de folioles, foliole terminale), corymbes de baies
  orange-rouge (couleur mesuree) qui pendent ;
- epicea : constructeur dedie -- fleche droite jusqu'a la cime, verticilles etages dont le rayon suit le cone mesure
  tranche par tranche, branches qui retombent puis se relevent, pinceaux d'aiguilles pendants ; LOD2 = trois cones.
Trois LOD, `M_AnastasisVegetation` (slot 0) + `M_AnastasisBark` (slot 1), aucune texture. Assets
`/Game/Anastasis/Vegetation/Gpt/SM_Gpt_{HetreOriental,EpiceaOrient,CharmeOriental,CerisierSauvage,SorbierOiseleurs}`.

**Les treize ne bougent pas.** Les cles nouvelles du modele ne servent qu'aux cinq ; spec des treize identique a
l'octet (`gpt-flora.json`, 13 premieres entrees == HEAD), empreinte SHA-1 des maillages construits (sommets, couleurs,
triangles, trois LOD) identique a main pour les treize ; `preview-planche-1.png` et `-2.png` regeneres a l'octet pres.

**Placement (repris de pontic-realism-001, C++ porte sur main).** `EAnastasisTreeSpecies` gagne `OrientalBeech`,
`OrientalSpruce`, `Hornbeam`, `WildCherry`, `Rowan` ; `SpeciesSuitability` les zone (charme et cerisier en bas, hetre a
mi-versant humide, epicea et sorbier en haut ; cerisier = arbre d'orée/isole) et replie pin d'Alep, olivier, chene vert,
cypres et sapin sur les pentes seches et ensoleillees, sous `anastasis.Dressing.PonticStages` (1 par defaut ; 0 = palette
d'avant, les cinq ne pesent rien). Registre : cinq lignes de plus dans `GptSpeciesDefaults` (C++) et `GPT_SPECIES`
(`set_tree_grammar.py`), hauteurs 19-24 / 19-24 / 11-18 / 9-14 / 6-11 m. Correction au passage :
`SpeciesCounts[16]` masque `& 15` -> `[32]` / `& 31` (Rowan = 17 serait compte comme pin d'Alep), et une ligne de log
`ANASTASIS_TREE_TAXA_PONTIC`.

## FILES_OWNED

- `tools/unreal/gpt_flora_model.py`, `gpt-flora-fit.py`, `gpt-flora-preview.py`, `set_tree_grammar.py`,
  `ground-cover-capture.py` (etats `pontic_reference,pontic,pontic_reference2`), `proofs.txt` (`pontic-stages-capture`)
- `SourceArt/Vegetation/gpt/PNG-pontic/` (5 PNG), `SourceArt/Vegetation/gpt/planche-02.png`, `gpt-flora.json`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationRegistry.{h,cpp}`, `AnastasisPresentationResolver.{h,cpp}`,
  `AnastasisPresentationResolverTests.cpp` (`Anastasis.Presentation.PonticStages`), `AnastasisWorldEmbodiment.cpp`
- `Content/Anastasis/Vegetation/Gpt/` (dix-huit `SM_Gpt_*` regeneres), `Content/Anastasis/Presentation/DA_AnastasisPresentation.uasset`
- `docs/visual/sprites-vegetation-gpt-001/` (preview des cinq, planche 3, alignement, iou/triangles),
  `docs/historicity/briefs/pontic-trees-geometry-001.json`, `AGENTS.md` (index), cette fiche

## COMMIT

PENDING

## MEC

- BUILD: `BUILD::PASS` (worktree, UE 5.8.3).
- TESTS: suite sans rendu rejouee par `finish` (verdict ci-dessous). Test ajoute : `Anastasis.Presentation.PonticStages`.
- Hors editeur : `python tools/unreal/gpt-flora-fit.py` (deterministe) puis `python tools/unreal/gpt-flora-preview.py`.
  IoU silhouette de face / triangles LOD0/1/2 : hetre 0,535 / 8480-3723-280, epicea 0,460 / 11169-6035-59,
  charme 0,578 / 7771-3378-280, cerisier 0,375 / 10656-4734-224, sorbier 0,362 / 13571-4512-280 (les treize : 0,36 a 0,74,
  2,5 k a 10 k). Un controle de silhouette, pas un verdict de beaute.
- Treize inchanges : empreinte des maillages construits identique a main (`TREIZE_IDENTIQUES`).
- Editeur : `tools\unreal\create-gpt-flora.ps1` (dix-huit regeneres) : `GPT_FLORA_ASSETS::PASS`, triangles LOD0/1/2
  hetre 8478/3721/254, epicea 11169/6035/59, charme 7771/3378/254, cerisier 10656/4734/205, sorbier 13571/4512/254 ;
  les treize exactement aux valeurs de sprites-vegetation-gpt-001 (marronnier 7002/3441/254 ... prairie 2273/961/58 ; les
  erreurs `AppendBuffersToMesh` de la prairie existent deja sur main). Les treize `.uasset` regeneres sont identiques en
  forme : leurs binaires de main sont gardes (pas de churn LFS). `set_tree_grammar` : `VERIFY variants=41 attendu=41 ...
  especes=18 attendu=18 -> OK`, `RESULT::PASS`.
- `python tools/historicity/check-brief.py docs/historicity/briefs/pontic-trees-geometry-001.json` : `BRIEF::PASS traceability only`.

## PROOFS

PROOFS: pontic-stages-capture

## SCN

Rendu logiciel (pas Unreal : ni Lumen, ni transmission) regarde : `docs/visual/sprites-vegetation-gpt-001/preview-planche-3.png`
(les cinq : sprite | face | trois-quarts | profil), `preview-alignement.png` (les dix-huit de face a leur hauteur adulte),
`preview-<espece>.png`. Lecture : hetre et charme lisent « feuillu humide » plein, plus proches du sprite que les
premieres GPT ; l'epicea est un cone dense et sombre, plus regulier que le sprite ; le cerisier lit « en fleur » mais
moins nuageux que le dessin ; le sorbier reste plus aere que le sprite, baies lisibles. Verdict de beaute : Alexandre.

Preuve mise au point : `tools\unreal\editor-batch.ps1 -Proofs pontic-stages-capture` -> `PROOF::PASS (223 s)`,
`PONTIC_STAGES_CAPTURE PASS trees=5522 pontic_trees=1919 meshes=5 views=5` (`Saved/PonticStagesEvidence/`). Inventaire :
5 522 arbres dans les trois etats, reference == reference2, a 1 hetre 864, epicea 605, charme 251, cerisier 112, sorbier 87 ;
chene vert 1 420 -> 566, pin d'Alep 279 -> 96, olivier 166 -> 83, sapin 371 -> 91. Premier essai en echec
(`tree total changed`) : le controle comptait aussi les sujets `SM_Gpt_` du sous-bois, qui suivent les couronnes ;
corrige (arbres seulement), puis PASS. Images regardees (lisiere a 1,7 m et oblique, reference / pontic) : couronnes
feuillues plus pleines et plus vertes en lisiere, cerisiers blancs lisibles, epiceas sombres sur les hauts ; relief,
herbe et montagnes inchanges. GPU p50 (ms, ref / pontic / ref2) : prairie 51,8 / 76,2 / 64,7, riviere 64,4 / 92,7 / 86,3,
lisiere 55,7 / 49,5 / 66,3, oblique 72,8 / 58,9 / 87,0, aerien 56,8 / 63,0 / 95,4 -- le temoin derive de 13 a 39 ms
(machine partagee) : aucun verdict de cout possible sur ce run.

## PLY

UNKNOWN : aucun parcours joueur.

## ECARTS

AUCUN — aucun fichier de `Source/AnastasisSim/` touche.

## INTEGRATION_RISK

- `DA_AnastasisPresentation.uasset` (LFS binaire) reecrit : sur conflit, garder la version de main et relancer
  `create-gpt-flora.ps1` (qui rejoue `set_tree_grammar.py`), jamais fusionner.
- **Ne pas verser `agent/pontic-realism-001` apres cette mission** : meme enum, meme CVar, memes tests, mais des
  `SM_Pontic_*` en cartes ; cette mission la remplace pour les arbres. `pontic-plants-3d-001` et `pontic-fields-001`
  portent les commits de pontic-realism-001 : a rebaser sans eux (`rebase --onto`) avant tout lot.
- `anastasis.Dressing.PonticStages` vaut 1 : la composition de la foret change des le versement (valide par Alexandre
  pour pontic-realism-001 le 2026-10-09 ; ici les arbres sont modelises). 0 rend la palette d'avant.
- `ground-cover-capture.py`, `proofs.txt`, `AGENTS.md` sont chauds.

## RESTE NON REPRIS

- pontic-realism-001 : `SM_Pontic_*` en cartes, `T_PonticFlora_Atlas`, `M_AnastasisPonticFoliage`, `gpt_flora_sets.py`
  (jeux de sprites) et `gpt-flora-cut.py --set pontic` (decoupe de la planche 02 : seuls ses cinq PNG d'arbres sont
  repris) ; les autres sujets de la planche (chene « pontique », pin noir, noisetier, rhododendron, fougere aigle, genet,
  rochers, tronc tombe) ; la fiche du registre de realisme RU-003.
- pontic-fields-001 / pontic-plants-3d-001 : champs (ble amidonnier, orge, epeautre, cultures, sillons ; scripts
  `create-field-crops*`, `field_crops_geometry.py`, `create-field-furrows*`) -- pas des arbres, hors mandat.

## STOP

- Pas de reconstitution botanique de 1204 : le zonage est un choix de presentation (brief, `hypothese_de_conception`).
- La face cachee des arbres est une hypothese (couronne de revolution).
- Le cerisier est toujours en fleur (la planche le montre ainsi ; le jeu n'a pas de saisons de vegetation).
- Aucune mesure de cout GPU sur la machine reelle hors la preuve.
