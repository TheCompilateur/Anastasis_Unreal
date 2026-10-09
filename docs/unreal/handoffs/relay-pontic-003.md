# HANDOFF: relay-pontic-003

## MISSION

Relais d'integration (mandat de l'integrateur, autorite d'Alexandre, 2026-10-09) : verser quatre missions pontiques
dont les agents ne sont plus actifs, par `cherry-pick -x` de leurs commits absents de main (branches d'origine
intactes) :

- **pontic-slope-ecology-001** (`91c5c725d`) : la penalite de Wetness du placement forestier macro se relache sur
  les versants rendus de 8 a 24 degres (`anastasis.Dressing.PonticSlopeEcology`, **1 par defaut**, 0 = ancien
  tirage), test `Anastasis.Ecology.PonticSlopeEcology`, capture A/B/A `tools/pontic-slope-ecology/capture.*`.
- **pontic-common-ground-003** (`01d99ab4a`, seul commit absent de main : son premier commit `6bc8c3c29` est deja
  dans main par contenu, `cbd6f125c` de pontic-ground-context-002) : couverture photo de prairie et de litiere sur
  la jonction 20 m - 105 m (fin de la photo proche, fondu des HISM d'herbe) dans `M_AnastasisGround`
  (`TexDistanceStrength`, 1 par defaut), deux textures `T_Ground_{MeadowDistance,ForestDistance}_AH`, etats
  `distance_*` de `ground-cover-capture.py`.
- **pontic-mountain-presence-003** (`af3b84176`) : candidat `MountainRockDetail` dans l'autorite
  `far-terrain-material.py` (strates et fissures des versants lointains ; 0 = graphe precedent a l'identique) et
  outil `mountain-rock-capture.ps1/.py`. **Script seul** : `M_AnastasisFarTerrain.uasset` n'est pas regenere.
- **pontic-horizon-material-003** (`841e2cf0c`) : notes de reprise seulement
  (`PONTIC_HORIZON_MATERIAL_003_NOTES.md`) ; fiche absente, reconstruite ici dans
  `docs/unreal/handoffs/pontic-horizon-material-003.md`.

RELAIS: pontic-slope-ecology-001, pontic-common-ground-003, pontic-mountain-presence-003, pontic-horizon-material-003

## FILES_OWNED

Ceux des quatre fiches d'origine, plus les assets regeneres pour pontic-common-ground-003 (voir MEC) et cette fiche.

## COMMIT

PENDING — le commit marque par `finish` fait foi.

## MEC

- Cherry-picks : slope-ecology et horizon-material sans conflit. mountain-presence : conflit `AGENTS.md` seul
  (ligne `far-terrain-material` voisine des lignes `gpt-flora-*` reecrites par sprites-vegetation-gpt-001) ;
  resolu a la main, sans union : lignes `gpt-flora-fit.py`, `gpt-flora-preview.py`, `gpt_flora_model.py` de main
  gardees, ligne `far-terrain-material` de la mission (mention `MountainRockDetail`), ligne `mountain-rock-capture`
  ajoutee. common-ground : sans conflit une fois son commit parent reconnu dans main (les conflits annonces sur
  `ground-cover-capture.py`, `ground-material.py`, `ground-textures.py` venaient du rejeu de `6bc8c3c29`, deja
  verse) ; ses etats `distance_*` s'ajoutent aux `ruin_*` / `path_*` / `gravel_*` de main, son facteur de sol
  exclut routes, gravier et roche (`(1-Road)(1-Gravel)`), il ne retire rien a pontic-ground-context-002.
- Syntaxe Python des six scripts touches : `ast.parse` OK.
- Assets de pontic-common-ground-003 (sa fiche : « les `.uasset` [...] seront produits dans un commit suivant avant
  `finish` » ; ils manquaient) : textures emballees reprises telles quelles du worktree d'origine
  (`Saved/GroundTextures/packed/T_Ground_{MeadowDistance,ForestDistance}_AH.png` + `manifest.json`, CC0 Poly Haven
  `leafy_grass` et `forest_leaves_02`, detail_mean 0,4 ; aucun telechargement refait), puis
  `tools/unreal/ground-material.ps1 -Rebuild -TimeoutSec 2400` dans ce worktree : `GROUND_MATERIAL::PASS`,
  `TEXTURES_READY count=16` (deux importees), `MASTER_SAVED expressions=236`, aucun `Failed to compile` apres
  `RECOMPILE_BEGIN`, stats master/instance `pixel_instructions=1532 samplers=17 pixel_texture_samples=4`.
  Fichiers ecrits : `M_AnastasisGround`, `MI_AnastasisGround`, `T_Ground_MeadowDistance_AH`,
  `T_Ground_ForestDistance_AH` (LFS), rien d'autre dans `Content/`.
- Build : premier `BUILD::FAIL` en unity, `AnastasisPonticSlopeEcologyTests.cpp` C2872 « FPlan symbole ambigu »
  (sur main, `Module.Anastasis_UnrealV2.4.cpp` reunit ce test et un fichier en `using namespace AnastasisWorldView`).
  Corrige dans le commit `fix(relay)` : noms du test qualifies `AnastasisEcologicalDressing::`, rien d'autre.
  `BUILD::PASS` ensuite.
- Build et suite sans rendu : sortie de `finish` sur ce relais.

## PROOFS

Union exacte des preuves des fiches d'origine (4, au plafond PROOFS_CAP_001) :

PROOFS: pontic-slope-ecology-capture, pontic-common-ground-capture, pontic-path-capture, pontic-gravel-capture

## SCN

UNKNOWN pour les quatre missions : aucune image du candidat n'a jamais ete regardee. Les preuves au lot sont
instrumentales (`PONTIC_SLOPE_CAPTURE PASS`, `GROUND_CAPTURE_COMPLETE`) ; le verdict KEEP/REJECT est dans les images.

## PLY

UNKNOWN.

## ECARTS

AUCUN — `Source/AnastasisSim/` inchange (le C++ touche est `Source/Anastasis_UnrealV2/WorldView/`).

## INTEGRATION_RISK

- **Deux changements visibles actifs par defaut, sans verdict SCN** : `anastasis.Dressing.PonticSlopeEcology 1`
  (plus d'arbres possibles sur les versants humides, cout GPU possible) et `TexDistanceStrength 1` dans
  `M_AnastasisGround` (tout le sol entre 12 et 350 m). Les fiches d'origine demandaient un examen A/B/A et une
  decision KEEP/REJECT avant admission : regarder les images de `pontic-slope-ecology-capture` et
  `pontic-common-ground-capture` du lot avant de garder `main`. Repli sans revert : la CVar a 0, le scalaire a 0
  dans `ground-material.py`.
- pontic-mountain-presence-003 n'est qu'une preparation (sa fiche : « ne doit pas etre integre seul ») : le script
  porte `MountainRockDetail` **1 par defaut** mais l'asset en jeu reste l'ancien. Le prochain
  `far-terrain-material.ps1` activera le candidat jamais regarde ; le lancer avec la valeur a 0 tant qu'aucun
  A/B/A (`mountain-rock-capture.ps1`) n'a tranche. Aucune preuve declaree.
- Le materiau de sol passe a 17 textures (`samplers=17`) ; il compile sur ce poste (D3D12, sampler Wrap partage
  d'Unreal), aucun autre RHI n'a ete essaye.
- `M_AnastasisGround.uasset` / `MI_AnastasisGround.uasset` sont binaires (LFS) : toute autre mission qui les
  regenere entre en conflit au lot ; regenerer alors depuis le script fusionne, apres ce relais.
- `pontic-path-capture` et `pontic-gravel-capture` (de la fiche common-ground, deja jouees pour
  pontic-ground-context-002) sont rejouees parce que le materiau de sol change ; aucune preuve d'origine n'est
  laissee de cote.
- `PONTIC_HORIZON_MATERIAL_003_NOTES.md` est verse a la racine du depot tel que l'agent l'a ecrit.

## STOP

Ni photorealisme, ni fidelite botanique de 1204, ni gain visuel revendiques. Aucune preuve PIE jouee par ce relais.
