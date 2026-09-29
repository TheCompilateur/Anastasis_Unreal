# WORLD DRESSING 01 — trois souvenirs spatiaux

## MISSION
Composer trois lieux ponctuels sur Human Geography V2 (graine 12345, echelle 5,
emprise 1,9 km), en reutilisant les assets du projet. Aucun changement de topographie, PNJ, generation forestiere ou village.
Correction C++ minimale de persistance requise par la sauvegarde de la map. Branche agent/world-dressing-spatial-01,
base afaf2b6. Worktree gere par Codex (chemin non reconnu par le finish du projet ;
build direct et report-tests executes separement, aucun claim HANDOFF_READY) :
C:/Users/alex_/.codex/worktrees/world-dressing-01/ANASTASIS_UNREAL.

## FILES_OWNED
- Content/Anastasis/Maps/Lvl_AnastasisSlice.umap : 127 objets visuels et trois cameras editor-only.
- Content/Anastasis/WorldDressing01/MI_WeatheredStone.uasset : instance de M_FlatCol existant.
- tools/unreal/world-dressing-01.py et .ps1 : recette editor, capture et relecture.
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.h : trois references de composants temporaires marquees Transient.
- AGENTS.md : une ligne d'index d'outils.
- docs/unreal/handoffs/world-dressing-spatial-01.md : cette fiche.

## COMMIT
BRANCH_HEAD

## DIRECTION ET ASSETS
Inventaire reel via Asset Registry et bounds/materials charges dans l'editeur :
29 StaticMesh, 26 Blueprint, 20 Material, 14 MaterialInstanceConstant dans /Game.
Huit silhouettes d'arbres, leur alias generique, un roseau, une ruine ; le reste
est principalement du prototype et de l'armement. Aucun pack de rochers/bois mort
ni PCG utile decouvert. Pas de nouveau mesh.

Utilises : 3 Conifer Emergent, 3 Conifer Subcanopy, 2 Conifer Understory,
2 Broadleaf Emergent, 5 Broadleaf Canopy, 5 Broadleaf Understory,
101 SM_Ecotone_Reed_01, 6 SM_Ruin_Generic_01. Materiaux vegetation/ecorce existants.

- Les Trois Veilleurs (tuiles environ 40.5,35) : trois coniferes de 22/26/29 m,
  silhouettes asymetriques sur une epaule du passage ; couloir central laisse libre.
- Le Coude des Roseaux (50,56.5) : feuillu de repere, jeunes arbres et cinq ilots
  de roseaux discontinus sur la berge. Rejets des positions trop immergees.
- La Memoire de Pierre (33.6,66.2) : une limite effondree, fragments de hauteurs
  inegales, grand feuillu et reprises vegetales ; aucune enceinte/batiment neuf.

Les grands fonds agricoles restent sans placement de cette mission. Rochers,
bois mort, vrais troncs couches, textures de vestiges et raccord avec la nouvelle
macro-foret sont reserves a la suite. Le rendu garde les limites stylisees des meshes.

## MEC
Build Editor isole : Result Succeeded, UE 5.8.2 CL 56702186 (34 actions).
Le premier rechargement a reproduit un crash dans EmbodyCrop/CreateMeshSection :
les references de composants RF_Transient etaient persistantes et rechargeaient null.
Correction : UPROPERTY(Transient) sur ExperimentalSurface, TerrainMeshes, DressingMeshes.
Build final apres correction : Result Succeeded, 5 actions, 86 secondes, exit 0 (build-reload-fix.log).
Tests Anastasis.Terrain : 22 PASS, 0 KNOWN_EXPECTED_FAILURE, 0 FAIL ; 22 annonces/22 termines.
report-tests.ps1 -Filter Anastasis.Terrain : TESTS::PASS, TEST COMPLETE. EXIT CODE: 0.
Preuve : terrain-tests-final.log. Noms exacts :
- Anastasis.Terrain.Contract
- Anastasis.Terrain.DressingOnGround
- Anastasis.Terrain.Fallback
- Anastasis.Terrain.Forge.Banks
- Anastasis.Terrain.Forge.CarriesMorphology
- Anastasis.Terrain.Forge.ChunkSeam
- Anastasis.Terrain.Forge.Contract
- Anastasis.Terrain.Forge.NoCliffs
- Anastasis.Terrain.Forge.NoSpikes
- Anastasis.Terrain.Forge.NoStaircase
- Anastasis.Terrain.Forge.SampleHeight
- Anastasis.Terrain.HumanGeography.BasinsAndConservation
- Anastasis.Terrain.HumanGeography.CollisionAndDressing
- Anastasis.Terrain.HumanGeography.RiverAndOutlet
- Anastasis.Terrain.HumanGeography.ScaleAndSampler
- Anastasis.Terrain.HydrologyGradient
- Anastasis.Terrain.MorphologyChannels
- Anastasis.Terrain.SampleHeight
- Anastasis.Terrain.Semantics
- Anastasis.Terrain.Shoreline
- Anastasis.Terrain.SlopeShade
- Anastasis.Terrain.WorldExtent
Index des outils : Missing=[] ; Stale=[]. Syntaxe Python valide.
127 poses et meshes controles ; 0 racine hors des seuils d'eau (roseaux tolerent 8cm).
Assise echantillonnee sur huit points ; objets sans collision ni contribution navigation.
Empreinte exacte des sommets terrain ET eau identique a l'inspection initiale :
4ef01e5e54840d43908db16b9c9914394ea024ebb5e357703e5490e0bfa0e104.

## SCN
Premiere preview inspectee au sol et en hauteur pour chacun des trois lieux.
Iteration retenue : troncs affines, trois veilleurs mieux separes, contact au sol
calcule sur la largeur du pied, canopees intermediaires remplacees.
Save a ecrit la map et le materiau ; sa premiere capture a expire pendant les shaders.
Le premier Verify a plante sur la reference de surface ; ce run est FAIL, pas une preuve visuelle.
RELOAD_VALIDATION: PASS. Nouveau processus, 127 poses/meshes retrouves, empreinte terrain/eau identique.
Sept captures finales reload_*.png : trois lieux au sol et en hauteur, puis vue d ensemble ; toutes inspectees.
Le premier run combine a termine ses 22 tests sans echec mais sans le marqueur TEST COMPLETE ;
le rapporteur l a correctement refuse. Appel corrige avec Automation RunTests ...;Quit.

Preuves brutes externes, non commitees :
C:/Users/alex_/.codex/visualizations/2026/09/29/01a0ef53-d859-7fa3-ac79-36f073eb69a3/
asset_inventory.json, terrain_0.json, terrain_1.json, placements.json,
placement_report.json, reload_report.json, preview_*.png, reload_*.png et logs.

## PLY
UNKNOWN : verification de scene editor, pas de validation joueur ni de performance GPU.
Il s'agit de presentation ; pas de collision/navigation ajoutee par les accessoires.

## INTEGRATION_RISK
Pas de fusion automatique. La racine canonique et les travaux concurrents restent hors
propriete. La nouvelle macro-foret en cours dans une autre branche n'est pas integree.
La .umap est binaire : si elle a evolue, rejouer la recette dans un worktree d'integration
sur la bonne geometrie, sans remplacer aveuglement le fichier. La recette refuse une
emprise/altitude differente de la V2 inspectee. Pour d'autres graines/echelles, recomposer.
Les assets de base restent inchanges. Les objets sont classes WORLD_DRESSING_01,
avec tags WorldDressing01 ; seuls ces objets sont remplaces par la recette.

## NEXT / UTILISATION
Ouvrir le .uproject du worktree avec UE 5.8.2, puis Lvl_AnastasisSlice.
Les cameras WD01_VIEW_* dans WORLD_DRESSING_01/Views reperent les trois lieux.
Recette : tools/unreal/world-dressing-01.ps1 -Out <dossier-preuves> -Mode Preview.
Save ecrit les assets ; Verify relit les poses du meme dossier de preuves dans un
nouveau processus ; -TerrainTests lance ensuite les tests existants du terrain. Le canonique est refuse par l'outil d'edition.
STOP apres livraison de cette passe ; prochaine decision = validation artistique
et assemblage explicite avec le boisement.
