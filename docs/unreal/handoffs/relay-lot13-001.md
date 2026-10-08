# HANDOFF: relay-lot13-001

## MISSION

Relais de canopy-rain-shelter-001 (agent hors ligne, branche d'origine en conflit avec `main`) :
ses commits rejoués par `git cherry-pick -x` sur `main` 625f5ad3, plus le classement de `RainCanopyCover`
(STATE_ORACLE_001) et la correction de visée de la preuve `canopy-rain-pie`, qui échouait au lot 13.

RELAIS: canopy-rain-shelter-001

pontic-water-micro-001 a été **retiré** de ce relais (historique refait depuis `main`) : il est versé
séparément. settlement-morphogenesis-001 reste hors relais (conflit sémantique avec nav-wiring-001,
voir l'historique de cette fiche avant reconstruction : 51d99830).

## FILES_OWNED

- `docs/unreal/handoffs/relay-lot13-001.md`
- `tools/unreal/canopy-rain-pie.py` (correction de visée, ci-dessous)
- `tools/migration/state-fields.json` (ligne `RainCanopyCover`)
- résolutions et renumérotation (n° 41) déjà portées par le commit rejoué de canopy :
  `AGENTS.md`, `Source/AnastasisSim/ECARTS.md`, `Source/AnastasisSim/Public/Village/AnastasisVillage.h`,
  `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp`,
  `Source/AnastasisSim/Private/Life/AnastasisWeatherBehavior.cpp`, `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp`
- tout le reste appartient à la fiche `canopy-rain-shelter-001.md`

## COMMIT

Sur `main` 625f5ad3 : canopy-rain-shelter-001 (feat + deux commits de fiche, `-x`), classement STATE_ORACLE_001,
correction de la preuve, cette fiche. Voir `git log main..agent/relay-lot13-001`.

## MEC

Échec du lot 13 (`_integration-2/Saved/EditorBatch/20261008-114201/editor-batch.log`) :
`CANOPY_RAIN_PIE FAIL tree=(34.566,1.410) radius_tiles=0.071 on=0.000 off=0.000 on_again=0.000 open=0.000`.

Diagnostic (lecture du log et du code, aucun changement C++ requis) :

- Les couronnes sont bien enregistrées : `ANASTASIS_CANOPY_RAIN rendered_crowns=5489 bins=22545` dans le monde PIE,
  et l'échantillon (34.566, 1.410), rayon 0.071 tuile, est **identique** au PASS local de l'auteur. Ni les cartes
  du chêne vert, ni ForestUse, ni le rayon ne sont en cause.
- La couverture est branchée au village par `UAnastasisSimulationSubsystem::BindRainCanopy()`, appelée depuis le
  `Tick` du sous-système (`AnastasisSimulationSubsystem.cpp`, `Tick` → `BindRainCanopy`). Avant ce premier tick, le
  village n'a pas de callback et `GetRainCanopyCover` vaut 0 partout (d'où `open=0` aussi).
- `canopy-rain-pie.py` attendait 3 s d'horloge murale **depuis la requête PIE**. Sur l'arbre actuel, le démarrage
  PIE bloque ~41 s dans la frame de requête (`StaticDuplicateObject took: 30.97s`, `temps total de démarrage 41,611 s`) :
  les 3 s étaient écoulées dès la première frame PIE, et la preuve a lu à la frame [2], avant le premier tick du
  sous-système (le `SETTLEMENT_SITE` / village de départ, faits au même tick, n'apparaissent qu'à la frame [3]).
  Course entre la preuve et l'hôte, pas régression du couplage.

Correction (visée de la preuve, justifiée par l'allongement du démarrage PIE sur `main`) : l'attente compte à partir
de la première frame PIE observée et exige 30 frames PIE et 3 s. Les critères de réussite sont inchangés
(`on > 0.5`, `off == 0`, `on_again == on`, sol ouvert à 0).

- BUILD: voir le rapport de `finish`
- PREUVE locale: voir PROOFS / SCN
- COMMANDS:
  - `node tools/migration/check-ecarts.mjs -base main -handoff docs/unreal/handoffs/relay-lot13-001.md`
  - `node tools/migration/check-state-fields.mjs -base main`
  - `python -m py_compile tools/unreal/canopy-rain-pie.py` : OK

## PROOFS

PROOFS: canopy-rain-pie

## SCN

Preuve rejouée une fois dans ce worktree, `MAIN_LOCK::libre` : voir le résultat consigné ci-dessous.

## PLY

Ceux de la fiche relayée ; aucun comportement joueur nouveau.

## ECARTS

- ouvert : n° 41 OUVERT — Interception partielle de la pluie par une couronne incarnée (EXTENSION, A_TRANCHER,
  canopy-rain-shelter-001), renuméroté par ce relais (le trente-huit est pris sur `main` par geopolitical-world).
- la correction de ce relais ne touche pas `Source/AnastasisSim/` : aucun écart nouveau ni modifié.

## INTEGRATION_RISK

- La couverture n'existe qu'après le premier tick du sous-système de simulation. Un `Anastasis.Sim.Advance`
  lancé avant ce tick (même frame que le démarrage PIE) passerait sans canopée ; aucun script actuel ne le fait.
- ForestUse (`anastasis.Dressing.ForestUse`) masque des instances d'arbres exploités mais `RainTreeCrowns` reste
  celui de l'incarnation : un arbre abattu et masqué abrite encore de la pluie. Décision de conception laissée
  à l'auteur / Alexandre (relire `ForestUse` dans `RainCanopyCoverAt`, ou accepter) ; non tranché ici.
- Fichiers chauds : `AnastasisVillage.h`, `AnastasisWorldEmbodiment.cpp`, `AnastasisSimulationSubsystem.cpp`, `ECARTS.md`.
- Verser ce relais à la place de la branche d'origine `agent/canopy-rain-shelter-001` (`RELAY_ADMITTED::`).

## STOP

Ne tranche pas l'écart n° 41 (A_TRANCHER). Ne porte rien de pontic-water-micro-001 ni de settlement-morphogenesis-001.
Pas d'intégration ni de push par cette session.
