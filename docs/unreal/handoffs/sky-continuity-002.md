# HANDOFF: sky-continuity-002

## MISSION

Remplacer le relais abrupt jour/nuit par un passage terre -> ciel -> lune, sans modifier
l'horloge de simulation. Mandat Alexandre : transition originale et suppression du flash.
Base : main 4fef72d526ee0be45a923c46c9e626dc3a6e7deb. Branche agent/sky-continuity-002.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisSkyPassage.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisSkyPassageTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldAtmosphere.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldAtmosphere.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisAtmosphereProfile.h
- tools/unreal/atmosphere-profile.py
- tools/unreal/sky-passage-pie.py
- tools/unreal/proofs.txt
- AGENTS.md (index)
- .claude/skills/anastasis-realisme/fiches/atmosphere.md
- .claude/skills/anastasis-realisme/registre.md
- docs/unreal/handoffs/sky-continuity-002.md

## COMMIT

Voir le commit portant cette fiche sur agent/sky-continuity-002.

## MEC

- BUILD: PASS, `tools/unreal/anastasis-unreal.ps1 build` sur sources stabilisees, 2026-10-02.
  Premiere tentative invalide (source en cours d'edition) ; seconde : Result Succeeded,
  54,48 s UBT, puis BUILD::PASS. Aucun PASS repris de la premiere tentative.
- Syntaxe Python des deux scripts : PASS (`ast.parse`, sans executer Unreal).
- Index tools/unreal : Missing={}, Stale={}. `git diff --check` : PASS.
- TESTS: UNKNOWN, a rejouer dans la suite du lot.
- Tests nouveaux : Anastasis.Sky.Passage.ExposureSafety et SurfaceRelay.
- ExposureSafety traverse quatre saisons, six vitesses dont retour arriere et Warp 1000,
  avec frame de 2 s ; le controle negatif exige que l'ancienne adaptation viole le contrat.
- Contrat : EV applique >= EV cible (hors epsilon d'ecriture 0,005), adaptation vers le noir
  bornee a 3 EV/s, dt plafonne a 0,1 s. Ce n'est PAS une borne de luminance de l'image.
- Relais : soleil direct smoothstep(0..4 deg), lune direct smoothstep(0..6 deg)^2
  pour la lune opposee actuelle ; les deux contributions directes sont nulles a l'horizon.
- DiffuseScale, SpecularScale, IndirectLightingIntensity attenuent les surfaces ; la lune
  attenue aussi sa diffusion dans le brouillard. Lux physiques et SkyAtmosphere conserves.
- Le brouillard exponentiel suit la cible lumineuse, pas le retard de sensibilite de l'oeil.
- Les valeurs adoptees des lumieres sont memorisees/restaurees : pas de multiplication cumulee.
- CVar anastasis.Sky.Passage 0 = temoin ; 1 = passage. Aucune ecriture simulation/terrain.

## PROOFS

PROOFS: sky-passage-pie, sky-clock-pie

## SCN

UNKNOWN. KEEP exige une sequence aube ET coucher A/B/A : moindre pic d'eblouissement
au-dela de la dispersion A/A, silhouettes lisibles, pas de trou noir au relais ni de saut
de reflet. Les valeurs 4/6 deg sont des choix de presentation, pas une loi physique.
Le script prend une vue fixe a 1,7 m, ciel/materiaux meteo epingles, meme camera pour les
trois etats. `Shot` n'observe pas toutes les images : un flash entre captures reste possible.
Une seconde vue sur la rive et une observation joueur restent necessaires au verdict artistique.

## PLY

UNKNOWN. Aucun claim "impressionnant", aucun claim de suppression totale du flash.

## ECARTS

AUCUN : aucun fichier Source/AnastasisSim modifie. Horloge, meteo et saisons de simulation conservees.

## INTEGRATION_RISK

- Fichiers atmosphere partages avec d'autres missions ; passage par integrate-batch uniquement.
- Ancienne courbe d'exposition calibree avant ce relais : reevaluation visuelle obligatoire.
- Aube : protection immediate de l'exposition plutot que sensibilite nocturne retardee ;
  un saut d'heure extreme peut encore faire une coupure sombre. Ce n'est pas un lissage de temps.
- Lumen, SkyLight et pre-exposition de frame precedente ont leurs propres historiques :
  les contrats de composant ne les certifient pas.
- Les sliders diffus/speculaire constituent une enveloppe artistique locale, sans pretendre
  remplacer la transmittance physique du moteur ni masquer un autre defaut de rendu.

## STOP

Pas d'integration ni de push dans cette mission. Ne pas annoncer de PASS visuel sur un build.
Canonique et changements concurrents exclus (download.png, .claude/settings.local.json).
