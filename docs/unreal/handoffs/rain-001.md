# HANDOFF: rain-001

## MISSION

La pluie de la simulation tombe en stries autour de la camera : `AnastasisRain` (intensite, vent),
`M_AnastasisRain` + `SM_AnastasisRainStreak` (recette `rain-material.ps1`), 8000 stries placees en HLSL,
branchees dans `AnastasisWorldAtmosphere`. CVars `anastasis.Weather.Rain` (1 par defaut) et
`anastasis.Sky.Rain` (-1 = suit la meteo simulee, sinon intensite epinglee).

**Reprise par l'integrateur le 2026-10-02** : l'agent a commite son travail a 00:51 avec la mention
« (en cours) » et « Reste : mesure PERF-05, fiche, finish », puis s'est arrete (aucune session active a
01:48). Sur consigne d'Alexandre (« tout ce qui a ete fait aujourd'hui doit etre commit et integre »),
l'integrateur ecrit cette fiche et passe le portail normal (build, puis suite complete au lot). Aucune
ligne de code n'a ete ecrite par l'integrateur.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisRain.h/.cpp`, `AnastasisRainTests.cpp` (nouveaux)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldAtmosphere.h/.cpp`
- `Content/Anastasis/Weather/M_AnastasisRain`, `SM_AnastasisRainStreak` (generes par `rain-material.ps1`)
- `tools/unreal/rain-material.ps1/.py` (nouveaux), `tools/unreal/vegetation-cost-capture.ps1` (`-PreCmds`)
- `AGENTS.md` (index)

## COMMIT

`f2301ff` (agent d'origine), puis cette fiche.

## MEC

- BUILD: au `finish` de la reprise.
- TESTS: `Anastasis.Atmosphere.Rain.*` ajoutes par l'agent ; suite complete au lot d'integration.
- Mesure de l'agent d'origine (`capture-sky.ps1 -Label rain-ab3`) : +0,23 ms GPU a intensite 0,4,
  +0,32 ms a 1.
- COMMANDS:
  - `tools\unreal\rain-material.ps1`
  - `tools\unreal\agent-worktree.ps1 finish -Mission rain-001`

## PROOFS

PROOFS: (aucune)

## SCN

Captures A/B de l'agent d'origine dans son worktree (`Saved/SkyEvidence/rain-ab*`), non versees.

## PLY

Sans objet.

## ECARTS

Sans objet : ne touche pas `Source/AnastasisSim`.

## INTEGRATION_RISK

- Actif par defaut : la pluie apparait des que la meteo simulee pleut. Retour arriere :
  `anastasis.Weather.Rain 0`.
- Mesure PERF-05 (`vegetation-cost-capture`) non faite par l'agent.
- `AnastasisWorldAtmosphere.cpp` est un fichier chaud (eye-plane-001, fog-fsss-001).

## STOP

Ne revendique ni la mesure PERF-05, ni l'achevement de la mission (marquee « en cours » par son agent).
