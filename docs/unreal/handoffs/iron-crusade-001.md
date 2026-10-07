# HANDOFF: iron-crusade-001

## MISSION

Audit d'architecture d'ANÁSTASIS Unreal (mandat « IRON_CRUSADE_001 » d'Alexandre, 2026-10-07).
L'étude porte sur `main` = `e681e629`, puis est vérifiée sur `d059a812`. Trois livrables :
- un diagnostic borné ;
- un plan de reconstruction ;
- une première expérience décisive : l'empreinte `FVillage::Digest()` n'est pas un oracle
  d'égalité d'état.

## FILES_OWNED

- `docs/unreal/iron-crusade-001/IRON_CRUSADE_DIAGNOSIS.md`
- `docs/unreal/iron-crusade-001/NECRON_RECONSTRUCTION_BLUEPRINT.md`
- `docs/unreal/iron-crusade-001/PERTURABO_FIRST_BREACH.md`
- `Source/AnastasisSim/Private/Tests/AnastasisIronDigestBlindnessTests.cpp`
  (`Anastasis.Iron.Empreinte.AveugleAuxEcritures`) : le test d'expérience.

## COMMIT

Commité le 2026-10-07 avec l'accord d'Alexandre, sur `main` = `f64aeac8`.

## MEC

- BUILD: PASS (`tools\unreal\anastasis-unreal.ps1 build`, worktree, sur `e681e629`)
- TESTS: `tools\unreal\report-tests.ps1 -Filter Anastasis.Iron` → PASS 1 / KNOWN_EXPECTED_FAILURE 0 /
  FAIL 0, run complet (sur `e681e629`). Résultats `IRON_DIGEST` :

  | Écriture | Empreinte égale juste après ? | Premier pas où les empreintes divergent |
  |---|---|---|
  | aucune (témoin) | oui | aucun en 2 jours |
  | `Speed` ×1,25 | oui | 470 (7,83 s) |
  | `AiThinkAt` +3 s | oui | aucun |
  | `Reputation` +0,2 | oui | aucun |
  | `sim.rng ^ 1` | oui | 539 (8,98 s) |
- COMMANDS:
  - `node tools/migration/check-ecarts.mjs` → `ECARTS::PASS fiches=30 ouvertes=30`
  - `git cherry main agent/<m>` sur chaque branche non intégrée → 16 entièrement versées, 12 partielles,
    27 à verser (diagnostic F6)

## PROOFS

PROOFS: (aucune)

## SCN

N/A

## PLY

N/A

## ECARTS

AUCUN — la mission n'ajoute qu'un test sous `Private/Tests/`. Elle ne touche aucun code de
simulation.

## INTEGRATION_RISK

- Aucun : de la documentation et un test autonome. Le test n'assert que l'**aveuglement** de
  `Digest()`, qui est la propriété voulue de la projection de parité, figée. Les divergences
  mesurées sont rapportées en `AddInfo`, pas assertées. Si une mission élargissait un jour `Digest()`,
  ce test le signalerait : c'est voulu.
- Indépendant de `state-oracle-001`, qui réalise le chantier C1. Les deux peuvent passer dans le même
  lot, dans n'importe quel ordre.

## STOP

- Le diagnostic ne revendique aucun pourcentage de dette et aucun score.
- Les constats F2 à F5 reposent sur la lecture du code ; seul F1 est démontré par expérience.
- Ni profilage (Unreal Insights) ni build packagé.
- Les recommandations C2 à C6 attendent chacune une décision d'Alexandre.
