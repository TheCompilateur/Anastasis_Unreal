# HANDOFF: atmosphere-crusade-001

## MISSION
Coupler la presentation existante au meme etat meteorologique. Aucun changement propre a AnastasisSim, terrain, hydrologie, meshes ou placements.

## FILES_OWNED
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldAtmosphere.cpp et .h
- tools/unreal/create_tree_asset.py : import, materiaux seulement, garde __main__ ; geometrie reservee a visual-crusade-001
- tools/unreal/create-ground-cover.py et water-look.py : materiaux et garde __main__
- tools/unreal/weather_materials.py, weather-materials.py/.ps1, weather-contract.py, weather-reference.py/.ps1, weather-visibility.py
- tools/unreal/capture-sky.py : evite de recharger la carte deja ouverte
- tools/unreal/proofs.txt, AGENTS.md : registre/index
- .claude/skills/anastasis-realisme/fiches/atmosphere.md, eau.md, vegetation.md
- Content/Anastasis/Materials/MPC_AnastasisWeather.uasset ; M_AnastasisVegetation, Grass, Water, Bark, Rock.uasset
- Cette fiche

## COMMIT
Commit portant cette fiche, puis rebase sur agent/natural-history-001 (43412cca4da42009ed5426c073b8967b266408dc) a la demande de l'integrateur. Aucun deplacement de main ici.

## MEC
- BUILD PASS avant rebase : dernier build C++ UE 5.8.2 en 18.29 s. finish reconstruit la branche combinee ; suite et preuves du lot restent QUEUED.
- WEATHER_MATERIALS PASS : cinq materiaux et collection generes/sauvegardes dans Unreal, aucun mesh regenere.
- WEATHER_CONTRACT PASS : cinq graphes raccordes, valeurs MPC vent 0/1, cap 90, humidite .45/.9, couverture .25/.9 ; valeurs du MID nuage et restauration parent effectivement relues.
- Collection par monde, ecrite par le Tick existant de l'atmosphere ; aucun Tick par instance.
- Le simulateur ne fournit aucune direction : WindHeading est une direction de presentation explicite, 26.565 degres.
- Nuages : Layout_WindControls inspecte dans l'editeur (RGB axes monde signes, A multiplicateur) ; direction/intensite communes.
- Brumes locales : Wetness, dilution par vent et dissipation solaire deja presentes, conservees. Aucun nouveau volume.
- Mie x lerp(.85,1.15,humidite). AirVisibility multiplie la densite existante par .4+.6*humidite^2 ; aucune modification d'exposition ni du profil authored.
- Herbe/couronnes : fronts communs en coordonnees monde, herbe plus rapide, branches limitees, troncs immobiles.
- Eau : courant original conserve, petites perturbations alignees au vent, ripples des lacs attenues au calme.
- Bois/roche : albedo -12% max, roughness x.86, plancher .48. Humidification visuelle, pas accumulation physique. Sol non modifie.

## PROOFS
PROOFS: weather-contract, sky-clock-pie
Le lot ne rejoue pas les 25 captures. weather-reference reste disponible au registre pour observation explicite.

## SCN
PARTIAL. KEEP du couplage et KEEP borne de la correction AirVisibility a 14 h. Matin humide REJECT comme vitrine artistique ; couvert prepare mais pas valide comme objectif final.
Captures sur base 506f6db49aea640d4f0a8f96373b2e84d8853590 + changements de cette mission, AVANT rebase : elles ne prouvent pas la scene combinee avec les autres agents.

Preuves locales, sous C:/dev/ANASTASIS_WORKTREES/atmosphere-crusade-001/Saved/SkyEvidence/ :
- weather-reference-v2/ : 25 PNG, sky.json, capture.log, metrics.json, comparison.json, review_*.png. A/B couplage, temoin repete, matin, couvert ; cinq poses. Toutes vues.
- weather-visibility/ : huit PNG, memes fichiers de mesures, deux planches review_*.png. Deux poses (valley_long, riviere_eye), a 14 h et 7 h, seule AirVisibility change dans chaque paire. Toutes vues.
- Les deux campagnes atteignent SKY_CAPTURE_COMPLETE ; crash de fermeture apres COMPLETE, derniere sortie 3. Ce n'est pas une fermeture propre ni un PASS de suite.
- Premiere tentative weather-reference/ : contrat passe mais erreur Python de nettoyage avant captures ; corrigee, ne constitue pas une preuve SCN.

A 14 h, le fond forestier de la rive se separe mieux et le voile diminue modestement. Contraste local du tiers median : rive 14.1 -> 15.2 ; vallee 4.4 -> 4.7. Le fond reste laiteux, les ombres forestieres sombres. A 7 h, la vallee reste sombre/brumeuse (contraste 1.6 -> 1.7) : aucun gain artistique substantiel. Les poches locales vues d'en haut restent visibles. Aucun drapeau automatique sur les huit images ne leve ces limites visuelles.

GPU p50, ms, mesures stat unit en editeur (PNG 1600x900 ; pas une preuve packaged 1080p) :
| Vue | Couplage avant / apres / temoin | AirVisibility avant / apres |
|---|---|---|
| ov_sw 14 h | 17.452 / 19.518 / 19.413 | - |
| valley_long 14 h | 19.490 / 19.582 / 19.541 | 17.740 / 19.136 |
| ridge_long 14 h | 22.127 / 22.266 / 22.268 | - |
| riviere_eye 14 h | 23.184 / 23.379 / 23.405 | 23.616 / 23.724 |
| sousbois_eye 14 h | 25.347 / 25.642 / 25.705 | - |
| valley_long 7 h | - | 19.046 / 19.233 |
| riviere_eye 7 h | - | 23.317 / 23.712 |
Les premieres vues montrent +2.066 puis +1.396 ms : echauffement ou autre cause non tranchee. Ne pas affirmer cout nul/globalement constant. Les autres vues varient de quelques dixiemes de ms.

## PLY
UNKNOWN. Aucun parcours joueur ni preuve temporelle de vent en deplacement. Contrat numerique et captures fixes ne prouvent pas la sensation finale en jeu.

## ECARTS
AUCUN pour cette mission : Source/AnastasisSim non modifie.

## INTEGRATION_RISK
- Rebase demande sur natural-history-001 ; conserver ses ajouts de registre et fiche vegetation. Preuves combinees au lot.
- create_tree_asset.py partage : fonctions materiaux ici, geometrie a visual-crusade-001 ; garde __main__ identique.
- Sol propriete de soil-crusade-001 ; WeatherAir.g communique, aucun ecrasement.
- Collection et cinq consommateurs doivent etre verses ensemble. Autorite : weather-materials.ps1.
- Comparaisons anterieures au rebase et aux nouveaux arbres ; aucune revendication artistique sur la scene finale integree.

## CONDITIONS REPRODUCTIBLES
- Reference : Sky.Day 1, Sky.Hour 14, Sky.Humidity .45, Sky.Wind .3, Sky.Cover .25.
- Matin : heure 7, humidite .85, vent .12, couverture .4.
- Couvert : heure 14, humidite .72, vent .5, couverture .9.
- weather-reference.ps1 -Label <nom> : 25 captures.
- weather-reference.ps1 -Mode visibility -Label <nom> : huit captures, heure identique dans chaque paire.

## STOP
Pas de reconstruction, pas de simulateur climatique, pas d'integration ni push autonomes. Pas de troisieme correctif artistique.
Retour authored : anastasis.Atmosphere.Coupling 0. Retour de la seule correction de densite : anastasis.Atmosphere.AirVisibility 0. Sky.Hour/Humidity/Wind/Cover a -1 suivent de nouveau le simulateur.
