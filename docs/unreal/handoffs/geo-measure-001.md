# HANDOFF: geo-measure-001

## MISSION

Chantier C2 d'IRON_CRUSADE_001 (« Go » d'Alexandre, 2026-10-07) : chiffrer l'écart entre la
géographie de la simulation et celle du rendu avant de décider quoi que ce soit, puis démontrer ou
réfuter la menace F3 (un réglage du rendu déplace le village de départ). Aucune modification du jeu.

## FILES_OWNED

- `docs/unreal/GEO_MEASURE_001.md` : les quatre mesures et ce qu'elles décident
- `tools/unreal/settlement-sensitivity-pie.py` (nouveau) : A/B du site d'ouverture sous deux CVars de
  rendu, avec témoin
- `tools/unreal/proofs.txt` : ligne `settlement-sensitivity-pie`
- `AGENTS.md` : ligne d'index de `settlement-sensitivity-pie.py`

## COMMIT

Commité le 2026-10-07 sur `main` = `f64aeac8`.

## MEC

- BUILD: PASS (`tools\unreal\anastasis-unreal.ps1 build`, worktree, 142,7 s)
- TESTS: aucune automation ajoutée ni modifiée (`tools/` et `docs/` seulement).
- COMMANDS:
  - `tools\unreal\editor-batch.ps1 -Proofs geography-concordance-pie,terrain-access-pie,river-use-pie,settlement-sensitivity-pie`
    → un seul éditeur, `Saved/EditorBatch/20261007-181144/` :

    | Preuve | Verdict |
    |---|---|
    | `geography-concordance-pie` | `PROOF::PASS` (57 s) — 833 cases en désaccord sur 9 216 (569 simulation seule, 264 rendu seul) |
    | `terrain-access-pie` | `PROOF::PASS` (429 s) — 3 chemins en ANOMALY ; 2 568 segments sur 5 944 mesurables en anomalie, dont 126 en pente gravie > 18° et 41 dans ou au ras de l'eau rendue |
    | `river-use-pie` | `PROOF::FAIL route_crosses_steep_ground` — c'est la mesure : cible « rive » de la sim à ~99 m de la berge rendue, rien bu |
    | `settlement-sensitivity-pie` | `PROOF::PASS` (155 s) — `ref` (74 ; 36) = `ref2` (74 ; 36) ; `Drainage 0` → (50 ; 40) MOVED ; `HumanGeography 0` → (43 ; 23) MOVED |

## PROOFS

PROOFS: settlement-sensitivity-pie

`river-use-pie` n'est volontairement PAS déclarée : son FAIL est le constat mesuré. La déclarer ferait
échouer le lot pour une raison connue et documentée.

## SCN

N/A — mesure seulement.

## PLY

N/A

## ECARTS

Non concerné : aucun C++ de `Source/AnastasisSim/`.

## INTEGRATION_RISK

- `settlement-sensitivity-pie` enchaîne quatre PIE ; délai déclaré 1 100 s (mesuré : 155 s).
  - Le témoin `ref2` doit retrouver le site de `ref`. Si une mission rendait l'incarnation non
    déterministe, la preuve échouerait. C'est voulu.
  - Une mission qui **corrige** F3 ferait passer `drainage0` et `humangeo0` en STABLE. La preuve
    resterait PASS : MOVED / STABLE est une mesure, pas le critère.
- Indépendant de `state-oracle-001` et d'`iron-crusade-001`.

## STOP

- Ne décide pas entre l'autorité de la simulation et celle du rendu : `GEO_MEASURE_001.md` §5 penche pour
  la simulation et liste les décisions, qui reviennent à Alexandre.
- Une seule graine (12345), une seule carte, deux CVars entières : l'effet d'une retouche fine du relief
  n'est pas mesuré.
- Les sorties brutes restent dans `Saved/` du worktree, non versées.
