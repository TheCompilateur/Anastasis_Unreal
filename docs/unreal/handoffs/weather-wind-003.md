# HANDOFF: weather-wind-003

## MISSION
Tour 3 : meme vent meteorologique pour pluie, vegetation, eau et nuages.
Continuation de weather-transition-002 (1b59581) avec documentation main12d8f6b reprise.
Aucun test/capture demande ; build seul.

## FILES_OWNED
- Source/Anastasis_UnrealV2/WorldView/AnastasisSkyClock.h et .cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldAtmosphere.cpp
- tools/unreal/weather-contract.py : nettoyage WindHeading -1 au lieu du cap fixe
- .claude/skills/anastasis-realisme/fiches/atmosphere.md
- Cette fiche

## COMMIT
Commit portant cette fiche ; SHA et build transmis separement.

## MEC
FSkyWeather.WindHeading lit Weather.WindDir et suit le meme fondu que SkyWind, par arc
angulaire le plus court. Evaluate expose SkyWindHeading. Aucun nouveau tirage ou Tick.
VisualWindHeadingRadians est commun a MPC WeatherWind, Layout_WindControls des nuages et
UpdateRain. La pluie lit aussi SkyWind (override Sky.Wind compris), sous Coupling1.
WindHeading negatif suit la simulation (nouveau defaut -1) ; >=0 impose un cap en degres.
Sky.Weather0 conserve le vent fixe .3 et cap26.565 sans forçage de cap.
Coupling0 restaure les reponses anciennes, y compris vent brut des gouttes.
Les materiaux gardent leurs amplitudes/frequences propres : arbre != herbe != surface d'eau.

## PROOFS
PROOFS: (aucune)
Aucun test ni capture ; changement du nettoyage de weather-contract seulement, pas des assertions.
Le marqueur queued ne vaut pas autorisation de lancer une suite.

## SCN
NON OBSERVE. Cohesion directionnelle codee, mouvement visible non valide. Les rafales fines des
materiaux ne deviennent pas une simulation de turbulences ; pluie/nuages suivent le vent moyen.

## PLY
UNKNOWN.

## ECARTS
AUCUN : Source/AnastasisSim inchange.

## INTEGRATION_RISK
La branche contient le tour 2 : l'integrer avant tour3 ou dedupliquer son commit ; ne pas le compter
deux fois. Tour1 sec reste separe/non inclus. Direction quotidienne lisse autour de minuit uniquement.
Cap fixe obsolete dans anciennes fiches historiques ; remise a -1 pour suivre le vent courant.
Aucune nouvelle preuve artistique/performance. Pas de compilation revendiquee avant resultat finish.

## STOP
Tour3 seulement, sans editeur, test, capture ou integration autonome.
Retour cible : revert du commit tour3 en conservant tour2. WindHeading26.565 repingle l'ancien cap
pour collection/nuages, mais n'annule pas a lui seul le partage du vent avec les gouttes.
