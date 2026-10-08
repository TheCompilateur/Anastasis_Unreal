# HANDOFF: nox-cubemap-003

## MISSION

Mesurer le ciel HDR emissif du montage NOX sur une capture cubique native UE 5.8.2, puis comparer sans lune / pleine lune / midi / sans lune repete. Base main du 2026-10-08 ; commits NOX precedents repris par contenu.

RELAIS: nox-001, nox-skylight-002

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisSkyRadianceProbe.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisSkyRadianceProbe.cpp
- tools/unreal/nox-cubemap-probe.py, tools/unreal/proofs.txt, AGENTS.md
- docs/unreal/NOX_001.md, docs/unreal/handoffs/nox-cubemap-003.md
- Les commits repris portent les fichiers de nox-001 et nox-skylight-002.

## COMMIT

Voir le commit de cette fiche sur agent/nox-cubemap-003.

## MEC

- BUILD: PASS apres correction du helper C++ ; premiere tentative FAIL (collision de nom Error), puis build incremental Result: Succeeded / BUILD::PASS (18,07 s).
- TESTS: QUEUED pour la suite complete du lot ; seule la preuve NOX nouvelle a ete executee ici.
- Python ast.parse : PASS.
- Test-AnastasisToolsIndex : Missing=[], Stale=[].
- git diff --check : PASS avant compilation.

## PROOFS

PROOFS: nox-sky-capture, nox-exposure-diagnostic, nox-skylight-probe, nox-cubemap-probe

## SCN

editor-batch.ps1 -Proofs nox-cubemap-probe : PROOF::PASS 1/1 (76,6 s), quatre etats dans Saved/NoxEvidence/cubemap-probe/sky.json. Chaque cube contient 98 304 texels, 0 invalide, 49 152 non nuls. Y moyenne sans lune 1,7689366e-6 ; pleine lune 0,0027058247 (x1 529,6) ; midi 711,04034 ; sans lune repete 1,7689362e-6 (ecart relatif 2,45e-7). Les quatre PNG ont ete generes ; sans lune, 99,98 % des pixels sont sous 4/255, visuellement REJECT ; pleine lune lisible partiellement. 0 % des pixels different de >16/255 entre sans lune et temoin ; 62,51 % entre sans lune et pleine lune. Capture emissive distincte, pas cubemap temps reel ni lux. Aucun verdict joueur ou cout GPU accepte.

## PLY

UNKNOWN : aucune camera joueur dans cette sonde.

## ECARTS

AUCUN — Source/AnastasisSim/ n'est pas modifie.

## INTEGRATION_RISK

- Le relais porte les deux missions NOX precedentes et toutes leurs preuves.
- cosmic-night-001 modifie aussi AnastasisWorldAtmosphere.cpp, non repris ici ; simuler son empilement avant admission.
- La nouvelle sonde ne sauvegarde aucun asset. Son appel natif lance une capture emissive et synchronise le rendu ; elle est reservee au diagnostic, jamais au tick de jeu.
- Les captures SDR sont des temoins, pas un verdict de jouabilite ni de cout GPU.

## STOP

Aucun profil nocturne selectionne ; pas d'activation par defaut tant qu'une source nocturne naturelle et les images joueur ne sont pas validees.
