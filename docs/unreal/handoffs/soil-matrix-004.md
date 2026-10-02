# HANDOFF: soil-matrix-004

## MISSION
Matiere entre touffes : reference, echelle, essai local reversible.
Worktree C:/dev/ANASTASIS_WORKTREES/soil-matrix-004, branche agent/soil-matrix-004.
Base73248bc, MCP8873. Chantier autorise par Alexandre pendant attente editeur.

## FILES_OWNED
- tools/unreal/ground-material.py (candidat local default0)
- tools/soil-crusade/capture.py (valeurs A/B numeriques et cadrages sol)
- tools/soil-crusade/README.md (sources, observations, hypotheses, commandes)
- tools/unreal/proofs.txt (soil-matrix-normal-capture)
- docs/unreal/handoffs/soil-matrix-004.md

## COMMIT
HEAD de cette branche; checkpoint avant rendu, pas une livraison validee.

## MEC
Syntaxe Python des deux scripts PASS; git diff --check PASS.
Pas de C++ modifie, pas de build ni de shader compile, pas d'asset regenere.
PROOFS: soil-matrix-normal-capture

## SCN
UNKNOWN. References sources observees; ancien rendu suite-002-final reexamine.
Le candidat n'a PAS ete observe dans Unreal. Protocole et KEEP/REJECT dans README.
Les dimensions source Grass2m/Worked1.3m/Litter3m correspondent a la configuration;
aucune correction d'echelle arbitraire. Une photo contextuelle et deux cartes diffuse,
pas trois photos terrain ni une reference botanique/historique du Pont.

## PLY
UNKNOWN : aucun parcours joueur.

## ECARTS
AUCUN : Source/AnastasisSim intact.

## INTEGRATION_RISK
Candidat source DEFAULT OFF seulement; aucune regeneration d asset au lot.
La preuve declaree compare uniquement BumpStrength existant .16/0/.16 (9 images).
Son COMPLETE ne valide PAS SoilMatrixStructure : cet essai reste a compiler et voir
apres diagnostic normal, dans un creneau coordonne. Pas d activation ni generalisation.
Source autorite ground-material.py partagee : coordonner avec autres proprietaires.
Pas d'assets M/MI sauvegardes ici. Capture.py chevauche soil-contact-003 : conserver
son mode CONTACT, puis ajouts numeriques identiques et nouveaux cadrages de004.
Aucun remplacement de fichier en bloc lors assemblage.

## STOP
Travail hors editeur uniquement tant que la file est occupee; ne pas prolonger le
creneau soil-contact-003. Passation du diagnostic au lot via finish, sans editeur autonome.
Le portail sans changement binaire ne vaut ni compilation HLSL ni PASS visuel.
Ni photo reference ni syntaxe PASS ne prouvent le rendu final.
