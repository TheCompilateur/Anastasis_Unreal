# HANDOFF: nox-skylight-002

## MISSION

Départager l'origine du noir numérique de `nox-001` : absence de radiance de ciel, capture Sky Light nulle, ou contribution trop faible pour l'exposition. Une seule variable par A/B sur le code NOX existant. Base `main` `f64aeac8`, commit de `nox-001` repris par contenu.

RELAIS: nox-001

## FILES_OWNED

- `tools/unreal/nox-skylight-probe.py`, `tools/unreal/proofs.txt`, `AGENTS.md`
- `docs/unreal/NOX_001.md`, `docs/unreal/handoffs/nox-skylight-002.md`
- Les fichiers de `nox-001` sont portés par le commit repris `7fb0aaf5` ; voir sa fiche pour leur liste.

## COMMIT

Voir le commit portant cette fiche sur `agent/nox-skylight-002`.

## MEC

`tools/unreal/anastasis-unreal.ps1 build` : `Result: Succeeded`, `BUILD::PASS`, 329,46 s, binaire de ce worktree. `ast.parse` du script PASS ; `Test-AnastasisToolsIndex` Missing={} Stale={} ; `git diff --check` PASS. La suite complète de lot reste en attente ; aucun PASS global revendiqué.

## PROOFS

PROOFS: nox-sky-capture, nox-exposure-diagnostic, nox-skylight-probe

## SCN

`editor-batch.ps1 -Proofs nox-skylight-probe` : `PROOF::PASS` (125,8 s), `EDITOR_BATCH::PASS 1/1` ; cinq états × deux poses dans `Saved/NoxEvidence/skylight-probe`, PNG SDR, export HDR et `sky.json`. Sur la crête, au point central de SceneColor HDR : natif `2,20585×10⁻⁷`, Sky Light 0 : `0`, ×1000 : `2,50603×10⁻⁴`, natif répété `2,20585×10⁻⁷`. La pleine lune revient à `4,36093×10⁻⁴` avec une autre exposition. SDR : 0,119/255 natif, 0,095 sans Sky Light, 107,16 à ×1000 ; le témoin natif vaut 0,119. Les PNG ont été inspectés : ×1000 rend la forêt artificiellement claire (**REJECT artistique**). `compare.py` : 0 % de pixels >16/255 natif/répété, 0 % natif/éteint, 92,12 % natif/×1000. Le signal est réel dans ce montage mais insuffisant pour une nuit lisible. Ces valeurs HDR peuvent être pré-exposées et ne sont ni des lux ni une lecture du cubemap interne du Sky Light. `docs/unreal/NOX_001.md` porte le protocole et l'inférence.

## PLY

UNKNOWN : aucune caméra joueur ni transition intérieure dans cette mission.

## ECARTS

AUCUN — `Source/AnastasisSim/` n'est pas modifié ; seul le commit antérieur de `nox-001` est relayé.

## INTEGRATION_RISK

- Ce relais porte `nox-001` ; les deux preuves de cette fiche sont reprises intégralement dans `PROOFS:` et seront rejouées au lot.
- `cosmic-night-001` modifie aussi `AnastasisWorldAtmosphere.cpp`. Aucun code de cette branche n'est repris ; empilement à simuler après que sa mission soit prête.
- Ne pas fusionner ou supprimer le travail d'un autre agent. Les captures de cette mission n'écrivent pas dans `Content/`.
- Un éditeur d'intégration et un autre agent ont tourné pendant la fenêtre de travail ; les p50 GPU des captures ne constituent pas une comparaison de performance acceptée.

## STOP

Ce diagnostic établit une contribution Sky Light non nulle sur la crête, pas sa radiance physique, une nuit jouable ou un profil de production. Ne pas assimiler la réussite instrumentale `SKY_CAPTURE_COMPLETE` à une preuve visuelle ou joueur. La prochaine branche doit lire directement le ciel HDR capturé et calibrer une source diffuse naturelle avant tout réglage joueur.
