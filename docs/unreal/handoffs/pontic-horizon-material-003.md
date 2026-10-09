# HANDOFF: pontic-horizon-material-003 (fiche reconstruite par relay-pontic-003)

L'agent d'origine n'a pas ecrit de fiche de passation. Celle-ci est reconstruite par le relais
relay-pontic-003 (2026-10-09) depuis son unique commit `841e2cf0c`
(`docs: save Pontic horizon material experiment state`), qui ajoute seulement
`PONTIC_HORIZON_MATERIAL_003_NOTES.md` a la racine du depot.

## MISSION

Expliquer puis corriger la paroi bleu-blanc des versants lointains vue depuis le bassin (vue S045 de
`capture-horizon.ps1 -Mode skyline`), observee par pontic-horizon-realism-002 : A/B/A de
`anastasis.Terrain.HorizonFarMaterial` 1/0/1 a meme carte, graine, vue et heure ; si le materiau lointain est en
cause, corriger l'autorite `tools/unreal/far-terrain-material.py`, sinon isoler la perspective aerienne.

## FILES_OWNED

- `PONTIC_HORIZON_MATERIAL_003_NOTES.md` (etat et procedure de reprise)
- cette fiche

## COMMIT

`841e2cf0c` sur `agent/pontic-horizon-material-003`, rejoue par `cherry-pick -x` dans relay-pontic-003.

## MEC

UNKNOWN. Aucun changement de rendu, de carte, de materiau ni de code de jeu ; aucun build ni capture dans la
branche d'origine. La variation `aerial_scale=0.5` de la mission precedente (ciel noir, GPU invalide) est rejetee.

## PROOFS

PROOFS: (aucune)

## SCN

UNKNOWN.

## PLY

UNKNOWN.

## ECARTS

AUCUN — `Source/AnastasisSim/` inchange.

## INTEGRATION_RISK

- Documentation seule. La suite de l'experience recoupe pontic-mountain-presence-003 (meme autorite
  `far-terrain-material.py`, meme vue S045) : ne pas reprendre les deux en parallele.

## STOP

Aucune amelioration du monde jouable revendiquee ; observation d'une ancienne branche, pas preuve courante.
