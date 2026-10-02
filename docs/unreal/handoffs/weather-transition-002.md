# HANDOFF: weather-transition-002

## MISSION
Tour 2 demande par Alexandre : transitions de pluie accordees a celles du ciel.
Base 4fef72d. Instruction persistante : aucun test ni capture ; compilation seule.

## FILES_OWNED
- Source/Anastasis_UnrealV2/WorldView/AnastasisSkyClock.h et .cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldAtmosphere.cpp : UpdateRain uniquement
- Cette fiche

## COMMIT
Commit portant cette fiche ; SHA et resultat de compilation transmis a l'integrateur.

## MEC
FSkyWeather transporte Rain, echantillonnee avec couverture/humidite/vent et interpolee
par le meme Alpha de SkyClockLerpWeather. Evaluate expose SkyRain ; UpdateRain utilise
une copie locale de Weather avec cette intensite lissee. Weather.Rain simule reste intact.
Utilise WeatherBlendHours existant (4 heures autour de minuit dans le profil courant).
Hors fenetre : pluie brute. BlendHours<=0 : ancien comportement. Premier jour : pas de jour
precedent invente. Aucune horloge, memoire temporelle ou Tick supplementaire.
Sky.Rain forcee reste prioritaire et immediate, y compris 0 ; seuil visuel .02 conserve.
La direction et l'intensite du vent des gouttes restent celles d'avant : tour 3 distinct.

## PROOFS
PROOFS: (aucune)
Aucun test/capture sur instruction utilisateur. queued ne vaut pas autorisation de tests.

## SCN
NON OBSERVE : continuite mathematique a minuit visee, pas de validation visuelle.
Ne lisse pas un saut arbitraire de temps, une commande forcee ou le seuil visuel .02.

## PLY
UNKNOWN.

## ECARTS
AUCUN : Source/AnastasisSim inchange ; evolution de presentation Unreal uniquement.

## INTEGRATION_RISK
Independant du tour 1 weather-dry-001 : ce lissage ne supprime pas le plancher saisonnier
si le tour 1 n'est pas integre. Les deux changements doivent etre distingues.
Les PNJ lisent toujours la pluie brute : decalage de presentation possible autour de minuit,
comme pour les nuages/humidite deja lisses. Aucune regression testee ni parite revendiquee.
Compilation seule par finish ; ne pas annoncer PASS avant retour du compilateur.

## STOP
Tour 2 seulement. Aucun test, capture, editeur ou integration autonome.
Retour cible : UpdateRain repasse Sky.Weather a VisualFor (supprimer sa copie VisualWeather).
