# HANDOFF: valley-air-001

## MISSION
Isoler le voile lointain a 14 h sur base fe474f7, deux vues rive/hauteur. Au plus un correctif conserve apres A/B ; aucune exposition, terrain ou materiau modifie.

## FILES_OWNED
- tools/unreal/valley-air.py et .ps1 : diagnostic transitoire
- AGENTS.md : index
- .claude/skills/anastasis-realisme/fiches/atmosphere.md : resultat a ne pas retester sans fait nouveau
- Cette fiche

## COMMIT
Commit portant cette fiche ; diagnostic puis rebase sur main bc2b7b4, aucune modification runtime propre.

## MEC
Build PASS 140.19 s sur fe474f7. Diagnostic termine, aucune modification runtime pour le diagnostic.

## PROOFS
PROOFS: (aucune)

## SCN
DIAGNOSTIC CONFIRME dans les deux vues ; CORRECTIF REJECT. Aucun gain visuel livre. Critere predefini : plans lointains plus lisibles, profondeur conservee, effet superieur au temoin et cout mesure dans la meme session.

## PLY
UNKNOWN : captures editeur fixes uniquement.

## ECARTS
AUCUN : Source/AnastasisSim inchange.

## INTEGRATION_RISK
Domaine atmosphere uniquement ; aucun materiau ni asset de vegetation/sol.

## STOP
Une seule branche causale apres diagnostic ; un correctif prouve ou diagnostic d'exclusion, puis passation.

## DIAGNOSTIC OBSERVE
Commande : tools/unreal/valley-air.ps1 -Label valley-air-diagnostic
Preuves : Saved/SkyEvidence/valley-air-diagnostic (12 PNG, sky.json, capture.log, comparison.json, far-roi.json, review_*.png), toutes images vues.
Six etats : warmup, reference, no_global, no_aerial, no_local, reference2 ; deux vues rive et hauteur. Etat : jour1,14h,H.45,vent.3,couverture.25,pluie0.
Lecture editeur : fog density .010482149, aerial scale3.45,69brumes. no_global relit visibleFalse, no_aerial relit0, no_local compte0 ; temoins remettent les valeurs.
Pct pixels>16/255 vs reference (hauteur/rive) : no_global15.954/26.979 ; no_aerial6.317/6.343 ; no_local4.598/11.146 ; reference2 5.532/8.106.
Zone lointaine rive fixee AVANT candidat, rectangle x1180..1580,y285..410 : no_global80.598%, no_aerial.046%, no_local9.706%, temoin.236%. Ecart moyen respectif21.135/1.342/4.587/1.035 niveaux. Le retrait global distingue nettement arbres et plans ; retrait aerien ne repond pas au defaut cible.
GPU NON ATTRIBUABLE : lancement externe editeur canonique PID38132 pendant notre PID44096. Aucun processus d'autrui ferme. Notre sortie3 apres COMPLETE, crash fermeture connu ; pas de stabilite revendiquee.
DEC : tester seulement FogMaxOpacity .48 -> .30, garder densite, aerien, brumes, exposition. Une experience candidate avec repetitions, puis KEEP/REJECT. Aucun autre axe.

## CANDIDAT FINAL : REJECT
Commande : tools/unreal/valley-air.ps1 -Mode candidate -Label valley-air-candidate
Preuves : Saved/SkyEvidence/valley-air-candidate (10 PNG, sky.json, capture.log, comparison.json, review_*.png), toutes vues.
Etat identique au diagnostic ; FogMaxOpacity .48/.30/.48/.30 apres warmup. Valeurs relues, densite .010482149, aerial3.45 et69brumes identiques. Aucun asset sauve. Notre editeur PID13176 seul aux controles de debut et milieu ; sortie3 apres COMPLETE, aucune stabilite de fermeture revendiquee.

| Mesure | A/B1 | A/B2 | A/A2 |
|---|---:|---:|---:|
| Rive pixels>16 image entiere | 5.708% | 5.685% | 7.854% |
| Hauteur pixels>16 image entiere | 5.510% | 6.324% | 5.995% |
| Rive ROI lointain pixels>16 | .352% | .364% | .368% |
| Rive ROI lointain ecart moyen RGB | 1.294 | 1.285 | 1.331 |

GPU p50 ms, reference/candidat/reference2/candidat2 : hauteur23.951/24.042/24.040/24.131, rive26.782/26.812/26.749/26.827. Differences de quelques centiemes/dixiemes, pas de surcout significatif etabli. PNG1600x900, viewport editeur1280x720 ; aucune preuve packaged.

DEC : REJECT .30, maintien .48 sans changement de code ni asset. Le groupe brouillard global contribue fortement au voile, mais le plafond ne fournit pas de levier visuel discriminant ici. Ne pas attribuer ce diagnostic a sa densite ou a sa composante volumetrique sans un nouvel essai dedie. Perspective aerienne et brumes locales preservees.
STOP apres un candidat, comme convenu ; pas de troisieme cycle. Aucun gain artistique revendique ni correction runtime a integrer. Le prochain discriminant eventuel serait de separer les contributions exponentielle et volumetrique du brouillard global ; hors de cette mission.
Les images portent sur fe474f7, avant rebase settlement. Ne pas les presenter comme captures du nouveau main.
