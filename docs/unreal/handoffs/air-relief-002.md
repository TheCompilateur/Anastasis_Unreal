# HANDOFF: air-relief-002

## MISSION
Modification directe de l'atmosphere pour laisser davantage de contraste au relief,
a l'eau et aux forets. Mandat utilisateur : « pas de test, je veux que tu ameliore et renforce la map ».
Base main 4fef72d. Aucun test, capture ou editeur demande pour cette mission.

## FILES_OWNED
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldAtmosphere.h
- .claude/skills/anastasis-realisme/fiches/atmosphere.md
- Cette fiche

## COMMIT
Commit portant cette fiche ; SHA exact transmis avec resultat du build a l'integrateur.

## MEC
EyePlaneExtinctionGain : 3.5 -> 1.5 (amplification reduite de 57.1%, pas une mesure de pixels).
Consommateur : AAnastasisWorldAtmosphere::ApplyEyePlane, dans AnastasisWorldAtmosphere.cpp :
Extinction = Profile.VolumetricFogExtinctionScale * EyePlaneExtinctionGain lorsque
anastasis.Depth.EyePlane est actif ; puis SetVolumetricFogExtinctionScale(Extinction).
Le multiplicateur existe deja, aucun nouveau Tick, volume ou passe GPU ajoute.
La compilation sera effectuee par finish ; ce document ne revendique pas son resultat a l'avance.

## PROOFS
PROOFS: (aucune)
Tests et captures non executes a la demande explicite de l'utilisateur. Le marqueur queued
de finish ne vaut pas autorisation de lancer une suite. L'integrateur doit respecter cette
restriction sans contourner ses portails ni transformer un build en preuve artistique.

## SCN
NON OBSERVE. Effet vise : volume moins voilant, plans du territoire plus lisibles.
Aucune amelioration artistique ni absence de regression matin/nuit revendiquee.
valley-air-001 testait FogMaxOpacity .48 -> .30, sans resultat : cette propriete concerne
le brouillard exponentiel, pas le volume. Ce nouvel ajustement porte sur l'extinction
volumetrique ; il ne ressuscite pas le candidat rejete.

## PLY
UNKNOWN ; aucun parcours joueur.

## ECARTS
AUCUN : Source/AnastasisSim inchange.

## INTEGRATION_RISK
Changement actif aux heures/meteo utilisant EyePlane, pas seulement a 14 h.
La modulation meteo de densite demeure, ainsi que Mie, l'exposition, l'aerien,
les 7 m de depart et 120 m de portee du volume, les brumes locales et le plafond .48.
Pas d'assets, terrain, eau, placements ou vegetation modifies.

## STOP
Build seul puis passation. Pas de test, pas de capture, pas d'integration autonome.
Retour arriere cible : EyePlaneExtinctionGain = 3.5f dans le header, puis recompilation.
