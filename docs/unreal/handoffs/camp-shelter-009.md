# HANDOFF: camp-shelter-009

## MISSION
Produire un abri de toile rapiecee d'apres les planches pontiques.
Branche agent/camp-shelter-009, base 9175a37.
Worktree : C:/Users/alex_/.codex/worktrees/asset-map-002/ANASTASIS_UNREAL.
Aucune modification du canonique.

## FILES_OWNED
- tools/unreal/create-camp-shelter.py
- tools/unreal/capture-camp-shelter.py
- tools/unreal/capture-camp-shelter.ps1
- AGENTS.md : trois entrees dans l'index.
- docs/unreal/handoffs/camp-shelter-009.md
- Deux packages Content/Anastasis/CampShelter009/ suivis en Git LFS sur mandat d integration.

## COMMIT
BRANCH_HEAD. Mandat utilisateur : integre. Branche agent/camp-shelter-009-integration.
Integration du lot seul dans main ; pas de placement automatique dans la carte.

## ASSET
SM_Shelter_PatchedCanvas_01 et M_Shelter_LinenTimber.
Toit d'environ 4,32 x 3,44 m, hauteur totale 2,85 m.
Empreinte avec piquets/haubans : environ 5,90 x 4,45 m.
Six poteaux, faitiere, deux longerons, quatre haubans et piquets.
Toit affaisse, panneaux cousus, trois reprises de toit avec points visibles,
rideau arriere repare et pan lateral partiel. Facade et cote droit ouverts.
Geometrie statique, toile en coque mince fermee (0,36 cm), double face reelle.
Un mesh, un materiau, aucune texture externe.

Reference principale :
docs/visual/reference/pontique-props-materiel-refugies.png.
Reference de composition :
docs/visual/reference/pontique-camp-installation-rive.png.
Interpretation de silhouettes et materiaux ; pas de fidelite photorealiste revendiquee.

## MEC
Geometrie pure : 21 888 sommets, 37 716 triangles.
Invariants herites de Mesh.finish : valeurs finies, pas de triangle degenere,
normales et winding gauche Unreal concordants, pivot au sol centre XY.
Build Editor Win64 Development via Build.bat dans le worktree :
Result: Succeeded, 9 actions, 108,62 s. Aucun changement C++ propre a la mission.
Parse Python PASS. Index tools Missing=0 / Stale=0.
Find-RawEditorLaunch : aucune occurrence ; lancement par Start-AnastasisEditor.
Pas de suite Automation revendiquee pour ce lot de geometrie.

## SCN
Trois captures Unreal v2 (face, dos, interieur), toutes inspectees, dans
C:/Users/alex_/.codex/visualizations/2026/09/29/01a0eab3-eaab-7610-8f6d-bb6dd660a518/camp-shelter-009-render-c/.
Les quatre accessoires RefugeeProps008 sont recharges comme contexte,
sans les modifier. Aucun niveau sauvegarde.

Run A interrompu par nous pendant l'attente du verrou Build.bat pour
ValidatePlatforms ; aucun asset ou screenshot produit, INCOMPLETE.
Run B lance avec UE_SKIP_UBT_SDK_SETUP=1, restaure ensuite dans le parent.
Option moteur confirmee dans TargetPlatformManagerModule.cpp lignes 54, 223-227.
Elle evite seulement la verification SDK au demarrage de cette capture ; aucun
build/packaging de plateforme n'est revendique. L'editeur a ete compile avant.

Run B termine (3 vues), sauvegarde du mesh et du materiau confirmee.
Un longeron traverse le toit affaisse : v1 archivee, rejet local du raccord.
v2 abaisse seulement les deux longerons suivant la hauteur de la toile.
Le nombre de triangles reste identique. ANASTASIS_SHELTER_REBUILD=1 ne peut
reconstruire que le mesh nomme et marque v1/v2 de ce lot ; le materiau est reutilise.
L'interieur conserve un jour en haut du rideau, sans etancheite revendiquee.

Run C COMPLETE shots=3 assets=1, sortie editeur propre.
Vues 1600x900 : plus de longeron traversant le toit sur les deux vues exterieures.
Mesh v2 et materiau sauvegardes ; pas de relecture dans un nouvel editeur apres
la sauvegarde finale revendiquee. Livraison camp-shelter-009.zip, SHA256 dans
camp-shelter-009-validation.json.

## PLY
NOT_IMPLEMENTED. Abri visuel : aucune simulation de toile, logement ou PNJ.

## INTEGRATION_RISK
Deux nouveaux packages seulement, ne remplace aucun asset existant.
Recette reutilise les helpers de create-refugee-props.py ; ne lance pas sa creation.
LOD0 uniquement, Nanite desactive, budget GPU UNKNOWN.
Pas de collision ajoutee : ne pas presenter cet abri comme un batiment navigable.
UV0 de projection simple, pas de lightmap unwrap valide.
Les materiaux proceduraux sont une premiere passe.
EcotoneReview004 local preexistant exclu et preserve.

## REPRODUCTION
tools/unreal/capture-camp-shelter.ps1 -OutDir <nouveau-dossier-absolu>
La recette cree les packages absents ou verifie version et triangles des existants.
Aucun ecrasement automatique. Une retouche du mesh exige une version explicite.

## STOP
Un abri et trois vues pour jugement artistique. Pas de variante supplementaire,
pas de placement dans la map, pas de passe de perfectionnement des autres assets.
