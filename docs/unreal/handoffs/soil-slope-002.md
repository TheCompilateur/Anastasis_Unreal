# HANDOFF: soil-slope-002

## MISSION
Lire une matiere rocheuse sur les pentes a moyenne distance, sans modifier relief,
vegetation ou simulation. Base fe474f7c0dccb1ead8bf3be2b6d07c50a9cc9ffd.
Worktree C:/dev/ANASTASIS_WORKTREES/soil-slope-002, branche agent/soil-slope-002,
port MCP8731. Proprietaire : cette mission uniquement.

## FILES_OWNED
- tools/unreal/ground-material.py
- Content/Anastasis/Materials/M_AnastasisGround.uasset
- Content/Anastasis/Materials/MI_AnastasisGround.uasset
- tools/soil-crusade/capture.py
- tools/soil-crusade/README.md
- .claude/skills/anastasis-realisme/fiches/sol.md
- docs/unreal/handoffs/soil-slope-002.md

## COMMIT
HEAD de agent/soil-slope-002 (cette fiche est dans le commit).

## HYPOTHESIS
Les photos de sol s'effacent a20m : la couleur seule laisse les grandes pentes lisses.
Reutiliser la famille Rock existante a9m, projection triplanaire, modulation bornee
et faible normale; poids derive de pente20..41deg, reduit sous litiere et humidite.
Bande10..350m, pleine22..180m. Pas de nouveau bruit, texture, geometrie ou WPO.
SlopeSurface=0 restaure le materiau integre precedent; SoilHistory reste actif.
Regles SOL-01..05; PERF-01..05 : pas de promesse packaged a partir du viewport.

## KEEP_REJECT
KEEP si les pentes montrent une matiere coherente dans les deux cadrages cibles,
au-dela du temoin0/0, sans damier/peau de leopard ni deterioration du premier plan.
Delta GPU cible <=0.5ms contre temoins, indiquer toute derive ou depassement16.7ms.
REJECT si texture geante repetitive, taches noires, ou gain seulement numerique.

## MEC
BUILD PASS : anastasis-unreal.ps1 build,160.45s UBT,16actions.
Python syntaxe et diff-check passes. Shader compile et deux assets sauvegardes :
205 expressions,1175 instructions pixel,160 vertex,10 samplers (1066 pixel auparavant).
Parametre SlopeSurface default1 dans le master regenere; A/B relu sur deux MIDs.
Rechargement dans une nouvelle session a verifier par integrateur.
PROOFS: (aucune)

## SCN
KEEP borne sur paroi hors_vallee; validation artistique independante par integrateur.
PARTIAL sur objectif general : pente_face cadre surtout la rive, pas une seconde
paroi discriminante. Ne pas transformer les quatre cadrages en quatre gains.
12/12 images observees, CAPTURE::PASS sortie0 propre. Aucun crash dans ce run.
Commande (MAX1, MAIN.lock libre, creneau accorde par integrateur) :
ANASTASIS_SOIL_PARAMETER=SlopeSurface
ANASTASIS_SOIL_VIEWS=prairie_eye,pente_face,hors_vallee_eye,oblique
tools/soil-crusade/capture.ps1 -Label slope-v1 -Rebuild
Soil states before/after/control =0/1/0 de SlopeSurface; poses et MIDs conserves.
Preuves Saved/SoilEvidence/slope-v1 sous ce worktree : capture.log, cameras.json,
ground-cover.json et12PNG. COMPLETE11:48:27 UTC; fermeture propre11:48:53 UTC.
Boot debloque en arretant uniquement cmd192 ValidatePlatforms, parent6628 et chemin
verifies, signature PIEGES_UNREAL. Aucun autre editeur touche.

| Vue | Pixels >16 apres / temoin (%) | GPU avant / apres / temoin (ms) |
|---|---|---|
| Prairie | 0.255 / 1.423 | 14.944 / 15.155 / 15.258 |
| Oblique | 0.314 / 0.350 | 13.500 / 13.406 / 13.582 |
| Hors vallee | 1.382 / 1.980 | 15.600 / 15.489 / 15.576 |
| Pente face | 2.330 / 2.619 | 14.771 / 14.984 / 14.935 |

Le seuil global16/255 ne discrimine PAS ce gain de matiere de faible contraste :
le mouvement vegetal le domine. Ne pas revendiquer une victoire sur ce seul nombre.
Mesure supplementaire sur zones geometriques explicites de hors_vallee_eye, choisies
apres observation (analyse exploratoire, pas critere preenregistre) :

| Rectangle pixels (x0,y0,x1,y1) | MAE RGB apres / temoin (0..255) | Gradient horizontal avant / apres / temoin |
|---|---|---|
| Face rocheuse800,360,1200,580 | 4.267 / 0.589 | 0.908 / 2.801 / 0.903 |
| Face eclairee1210,340,1290,520 | 3.243 / 0.450 | 0.797 / 2.168 / 0.797 |
| Sol proche700,750,1150,860 | 2.703 / 3.164 | 9.286 / 9.322 / 9.299 |

MAE=mean(abs(B-A)) sur les canaux RGB. Gradient=mean(abs(diff(mean(RGB),axe x))).
Ces mesures localisent la matiere, elles ne prouvent pas son realisme geologique.
compare.py execute sur la paroi : ecart moyenRGB[2.04,2.01,1.97], luminanceRGB
avant[89.1,102.1,101.4], apres[89.0,102.0,101.3]; heatmap observee hors depot.
GPU apres13.406..15.489ms, delta contre avant -0.111..+0.213ms; derive temoin
jusqu'a+0.314ms. Pas de cout net resolu sous ce bruit. Mesure viewport editeur,
pas certification PERF-05 complete (banc vegetation six vues non rejoue).

La paroi gagne une texture rocheuse sans nouvelle tache noire visible; contours et
cassures de geometrie restent artificiels et inchanges. Prairie et oblique preservees.
KEEP confirme par integrateur sur triplet hors_vallee; perimetre scelle sans extension.

## PLY
UNKNOWN : viewport editeur, aucune preuve joueur.

## ECARTS
AUCUN : aucun Source/AnastasisSim modifie.

## INTEGRATION_RISK
Materiau partage : integrateur seul assemble apres finish. Aucune modification
arbre/atmosphere/naturaliste. Pas de changement de topographie ou de collision.
L'affleurement est une signature de matiere, pas une nouvelle geologie simulee.

## STOP
Une branche causale seulement : pentes a moyenne distance. Pas de correction du
crash de fermeture, du ciel, de l'eau ou des arbres dans cette mission.
