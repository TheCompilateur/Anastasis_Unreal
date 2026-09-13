# WORLD_SLICE_006 :: SEALED

Scellé le 2026-09-13 à 01:29 (UTC-4) par la session intégratrice
`multi-agent-control-001-f9`, sous gel confirmé des trois autres sessions actives
(`anastasis-unreal-45`, `anastasis-unreal-83`, `anastasis-unreal-dc`).

## CANONICAL_COMMIT

```
2c0b330d907fd764056c9ec00b9716efeb5e82e9
```

C'est l'état **vérifié**. Ce document est committé par-dessus : il est
documentaire et ne modifie ni code, ni configuration, ni asset.

Fenêtre de vérification : ouverture 01:24:58, fermeture 01:29:09.
Empreinte de l'arbre (hors `.git`, `Saved`, `Intermediate`, `Binaries`,
`DerivedDataCache`) identique à l'ouverture et à la fermeture :

```
2D983C8BF9266FB279205F36233592D7E85827132924B730D28ECDA7131ECC33
MUTATION_PENDANT_VERIFY :: AUCUNE
PREFLIGHT::PASS   POSTFLIGHT::PASS
```

## Ce qui est scellé

La première tranche visuelle lisible d'ANÁSTASIS dans Unreal : le crop canonique
32×32 du monde 96×96 devient une surface continue dont la couleur de sommet porte
toute la sémantique du sol, surmontée d'une nappe d'eau plate au niveau de la mer.

Chaîne de provenance, sans aucune donnée inventée :

```
AnastasisSim -> monde canonique 96x96 -> WorldView -> crop 32x32
             -> AnastasisTerrainSurface -> ProceduralMeshComponent
```

| Lecture | Source dans le snapshot |
|---|---|
| terre / eau | `FVisualTile::Type` |
| profondeur | `FVisualTile::Shade` (`Lerp(0.6, -1.0, Depth)` côté sim) |
| rive | `FVisualTile::Shore` (0 sur l'eau, décroît sur ~5 tuiles) |
| altitude | `FVisualTile::Alt`, normalisée sur le crop |
| niveau de l'eau | `AnastasisWorld::SeaLevel`, constante du monde |

Invariants figés, identiques avant et après la consolidation, l'arrivée de
WorldProbe et l'activation de la collision :

```
TERRAIN_CONTRACT  vertices=1024 triangles=1922 max_error=0.000000000 boundary_edges=124
TERRAIN_SEMANTICS water_tiles=151 land_tiles=873 shore_tiles=158 water_quads=187
```

## TEST_TRUTH

```
PASS                   : 27
KNOWN_EXPECTED_FAILURE : 4
FAIL                   : 0
TOTAL                  : 31
```

Les 13 tests du périmètre sont verts : `Terrain.Contract`, `Terrain.Fallback`,
`Terrain.Semantics`, `Visual.ModeDefault`, les huit `WorldView.*`,
`WorldVisual.SemanticSliceScan`.

Les quatre `KNOWN_EXPECTED_FAILURE` sont inscrits à
`tools/unreal/known-expected-failures.txt` et **ne sont jamais comptés comme
PASS**. Voir `tools\unreal\report-tests.ps1`.

`BUILD::PASS` · `VERIFY::PASS` (éditeur dédié + PIE) · UE 5.8.2 CL 56702186.

## VISUAL_EVIDENCE

`docs/visual/slice-006/`, versionné, octets réels résolus par LFS :

| Fichier | Octets | SHA-256 (16) |
|---|---:|---|
| `A_legacy_debug.png` | 1 446 464 | `A36CB35FF97D8647` |
| `B_slice_surface.png` | 766 473 | `8A3E10E385ED301A` |

Comparaison contrôlée : même carte, seed `12345`, caméra `(-1800,-1800,3500)`
pitch `-32.8` yaw `45`, soleil 75 000 lux, exposition figée EV100 = 14,
`viewmode lit`. Seule varie la CVar `anastasis.Terrain.Surface`.

Les deux emprises diffèrent volontairement : le chemin DEBUG incarne le monde
**96×96** entier, la surface ne traite que le crop canonique **32×32**.

## SIMULATION_MUTATION

**Aucune.** Ni `c452b5c` ni `971e2ec` ne touchent `Source/AnastasisSim/`. Les
seuls commits qui y touchent sont la ligne de base `566f65c` et `b31b4c3`, ce
dernier n'ajoutant que des annotations `AddExpectedError` dans un fichier de
test — aucun changement de comportement de simulation.

## KNOWN_DEBT

1. **Revendication périmée dans l'historique.** Le message de `971e2ec` affirme
   « Collision and navigation are disabled on the surface ». Depuis `94a3be6`
   c'est faux à moitié : la surface est en `QueryAndPhysics` (elle est
   marchable), la navigation reste désactivée. L'historique git conserve la
   formulation d'origine ; **ce document fait foi**.
2. Quatre divergences connues : 2 dans `AnastasisSim` (1 ULP sur `fbm`,
   sémantique `ToUint32(1e21)`), 2 dans le marshaling du décorateur
   `@toolset_registry.tool_call`.
3. **Garde-fou `verify` instable.** `anastasis-unreal.ps1` ligne 70 vérifie
   l'origine des modules en *échantillonnant* `$proc.Modules` toutes les 2 s ;
   sous pression mémoire l'échantillon peut être vide et produire un
   `VERIFY::FAIL module origin` alors que le log prouve le chargement depuis la
   racine canonique. Le log (`InternalLoadLibrary`) est la source déterministe
   et devrait remplacer l'échantillonnage.
4. Formatage en ligne de l'entrée plugin dans `Anastasis_UnrealV2.uproject`
   (JSON valide, cosmétique).
5. Deux squelettes de dossiers vides subsistent sous l'ancien chemin OneDrive.

## KNOWN_VISUAL_LIMITS

- Un seul degré de liberté sur la transition sol→eau (la bande `Shore`). Le
  gradient complet décrit dans `docs/visual/reference/` n'est pas fait.
- Nappe d'eau plate, sans vague, sans transparence, sans réfraction.
- Aucune texture : la couleur de sommet est la seule porteuse de sens.
- La surface refuse tout crop autre que le 32×32 canonique et retombe alors sur
  le terrain DEBUG.

## NEXT_PHASE_AUTHORIZED

**Aucune.** Ce sceau clôt WORLD_SLICE_006 et n'autorise aucune suite. Forêt, PCG,
bâtiments, PNJ, navigation et `PLAYER` restent hors mandat. `PLAYER` demeure
`NOT_IMPLEMENTED`.

Toute suite passe par un worktree dédié (`AGENTS.md`, protocole multi-agent) :

```powershell
tools\unreal\agent-worktree.ps1 create -Mission <mission>
```
