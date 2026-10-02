# HANDOFF: soil-crusade-001

## MISSION
Surface terrestre lisible par sa matiere : pente, terre mince, fines humides,
matrice minerale/organique proche. Pilote vallee A puis couverture par instance.
Base main: 506f6db49aea640d4f0a8f96373b2e84d8853590.
Worktree: C:/dev/ANASTASIS_WORKTREES/soil-crusade-001, agent/soil-crusade-001, MCP 8186.

## FILES_OWNED
- .claude/skills/anastasis-realisme/fiches/sol.md
- tools/unreal/ground-material.py
- Content/Anastasis/Materials/M_AnastasisGround.uasset
- Content/Anastasis/Materials/MI_AnastasisGround.uasset
- tools/soil-crusade/capture.py
- tools/soil-crusade/capture.ps1
- tools/soil-crusade/rebuild-capture.py
- tools/soil-crusade/deploy-capture.py
- tools/soil-crusade/README.md
- docs/unreal/handoffs/soil-crusade-001.md

## COMMIT
HEAD de agent/soil-crusade-001 : le commit de passation contient cette fiche.

## MEC
BUILD: PASS, tools/unreal/anastasis-unreal.ps1 build, 123.87 s UBT.
Python AST: PASS. Suite au lot; aucun changement C++.
Materiau compile et sauvegarde dans Unreal : 198 expressions, 1066 instructions
pixel, 160 vertex, 10 samplers, 4 echantillons texture pixel (statistiques moteur).
Source et deux assets binaires livres ensemble. Aucun changement simulation,
maillage, collision, vegetation ou carte.

## PROOFS
PROOFS: (aucune)
Banc visuel propre, sans preuve PIE enregistree :
`tools/soil-crusade/capture.ps1 -Label pilot-v2 -Rebuild`.
`tools/soil-crusade/capture.ps1 -Label deployed-v3 -Deploy`.
Preuves brutes locales : Saved/SoilEvidence/pilot-v2 et Saved/SoilEvidence/deployed-v3
sous le worktree indique. Chaque dossier contient capture.log, cameras.json,
ground-cover.json et les PNG avant/apres/temoin (SoilHistory 0/1/0).

## SCN
V1 REJECT : effet faible, 15 images completes puis crash de fermeture exit3.
Lanceur corrige : COMPLETE suivi d'un crash n'est plus rapporte PASS.
V2 KEEP local / artistique PARTIAL : 18 images observees, six vues, sortie0 propre.
Poses, geometrie et MIDs conserves entre les etats; seul SoilHistory change.
Pente mesuree par differences finies de traces du sol : 1-Nz=0.112549 (~27 degres).

| Vue | Pixels modifies >16/255 apres / temoin (%) | GPU avant / apres / temoin (ms) |
|---|---|---|
| Prairie | 17.276 / 4.096 | 13.022 / 13.372 / 13.504 |
| Riviere | 24.837 / 8.360 | 14.207 / 14.575 / 14.677 |
| Lisiere | 9.075 / 2.349 | 12.163 / 12.265 / 12.425 |
| Sous-bois | 25.995 / 8.231 | 15.641 / 15.973 / 15.695 |
| Pente | 26.480 / 2.465 | 12.468 / 12.791 / 12.637 |
| Oblique | 1.298 / 0.868 | 11.186 / 11.379 / 11.538 |

GPU apres <16.7 ms sur ces vues; delta +0.102..0.368 ms, temoin derive jusqu'a
+0.482 ms. Pas de cout net precis inferable sous cette derive. Captures1600x900,
mesure stat-unit avant HighResShot : pas un benchmark packaged ou PERF-05 complet.

Deploiement hors pilote : six images observees, deux vues, KEEP matiere / PARTIAL.
Parent et parametres recharges avant ecriture : SOIL_RELOADED
parent=/Game/Anastasis/Materials/M_AnastasisGround.M_AnastasisGround,
history=1.0 radius=38000.0. Puis rayon global relu10000000 et sauvegarde MI
confirmee par SOIL_SCOPE_SAVED. Master diagnostic380m, instance100km couvrant
le monde; SoilHistory=0 restaure le comportement anterieur.

| Vue hors pilote | Pixels modifies apres / temoin (%) | GPU avant / apres / temoin (ms) |
|---|---|---|
| Vallee B | 16.104 / 13.970 | 14.682 / 14.197 / 14.251 |
| Hors vallee | 12.726 / 2.803 | 12.905 / 12.968 / 13.165 |

La derive vegetale est forte en vallee B; le signal est plus discriminant hors vallee.
Les grandes faces escarpees restent lisses : la matiere proche n'efface pas ce defaut.
COMPLETE06:30:38 UTC puis crash de fermeture06:30:45, exit3 : images completes,
EDITOR_EXIT::FAIL, pas CAPTURE::PASS. Le pilote V2 avait termine proprement.
Deux essais precedents se sont arretes avant capture sur un controle booleen errone :
UE5.8 MaterialEditingLibrary.cpp:1485-1493 retourne false inconditionnellement dans
SetMaterialInstanceScalarParameterValue. Correction : valider la valeur relue.
Le dernier boot a requis l'arret du seul enfant cmd25704 ValidatePlatforms bloque,
parent editeur25932 et worktree verifies (signature PIEGES_UNREAL).
Aucun nouveau lancement apres ce controle; le prochain reload du rayon global est
la responsabilite du lot integre. La sauvegarde retournee vraie est attestee ici.

## PLY
UNKNOWN. Aucune preuve joueur ou jeu package.

## ECARTS
AUCUN — aucun fichier Source/AnastasisSim modifie.

## INTEGRATION_RISK
- La racine canonique n'est pas modifiee. Aucun merge/push autonome.
- Aucun asset Naturaliste/arbre/meteo ni script de capture partage ecrase.
- Main a avance pendant le pilote : le lot doit rejouer ses portails et inspecter
  le resultat combine. Ces captures ne prouvent pas ce futur arbre integre.
- UV0.y vient des tuiles Forest, pas des couronnes reelles. Proximite humide ne
  prouve ni plaine inondable historique ni transport sedimentaire.
- Grain Worked parfois trop regulier, pouvant evoquer une terre labouree.
- Pas de cailloux physiques supplementaires; micro-ecologie existante conservee.
- Exclusions canoniques preexistantes : .claude/settings.local.json, download.png.

## STOP
KEEP de matiere, pas victoire totale de geologie. Pas de nouveaux chenaux,
pas de simulation sedimentaire, pas de preuve PLY. Integrateur seul responsable
du lot avec arbres/Naturaliste/meteo et de son jugement visuel final.
