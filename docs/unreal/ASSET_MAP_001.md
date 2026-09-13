# ASSET_MAP_001 — cartographie des assets ANÁSTASIS

Mission `ANASTASIS_ASSET_AGENT_001`. Worktree isolé `C:\dev\ANASTASIS_WORKTREES\asset-agent-001`,
branche `agent/asset-agent-001`, base `main` à `f6fa234a`.

But : relier canon visuel (`docs/visual/reference/`), état réel du code (`Source/`), et contenu
présent (`Content/`) pour chaque catégorie d'asset, et choisir une tranche verticale défendable.

| Catégorie | Référence canon | Asset existant ? | Placeholder existant ? | Système code consommateur | Format Unreal attendu | Priorité |
|---|---|---|---|---|---|---|
| **Arbres** | `pontique-etat-zero-3-stratification-forestiere`, 6 planches taxonomiques Downloads (espèces, silhouettes, âges) | Non | **Oui — `/Engine/BasicShapes/Cone.Cone`**, tinté vert via MID | `AnastasisPresentationResolver::Resolve(Forest)` → `AAnastasisWorldEmbodiment::DressingMeshes[Tree]` (HISM) | `UStaticMesh`, 1 slot matériau, pivot compatible convention `EngineBasicShapeSize=100uu` | **HAUTE — cible de cette mission** |
| **Buissons** | Implicite dans État Zéro 3/5 ("arbustes 1-4m") | Non | Non | Aucun — pas de type de tuile dédié ; scatter secondaire possible sur `Forest`/`Scrub` sans toucher `AnastasisSim` | `UStaticMesh` (future 2e couche HISM) | Moyenne (après arbre) |
| **Fougères** | État Zéro 3 ("fougères 0,3-1m") | Non | Non | Aucun — même remarque que buissons | `UStaticMesh` | Basse |
| **Herbes** | `pontique-etat-zero-2-anatomie-sol` (palette sol), État Zéro 3 | Non | Non — `Grass` (`ETileType::Grass`) reçoit une couleur de sommet, zéro instance | `AnastasisTerrainSurface` (couleur seule) ; aucun dressing HISM pour `Grass` aujourd'hui | `UStaticMesh` (touffes) ou matériau proc. | Moyenne |
| **Bois mort** | État Zéro 3 ("bois mort, toutes les strates") | Non | Non | **Faisable sans toucher `AnastasisSim`** : variante de résolution sur `Forest` (ex. 10% des tuiles Forest → archétype `DeadTree` au lieu de `Tree`, décidé par le même hash déterministe déjà utilisé pour le jitter) | `UStaticMesh` | Moyenne — bon candidat de 3e tranche |
| **Rochers** | `pontique-etat-zero-2-anatomie-sol` | Non | Non — `Stone` (`ETileType::Stone`) reçoit une couleur de sommet, zéro instance | Aucun dressing HISM pour `Stone` aujourd'hui, mais le type de tuile existe déjà (symétrique à `Ruin`) | `UStaticMesh` | **Haute — 2e candidat naturel après l'arbre** (même maturité que Ruin avant cette mission) |
| **Sols** | `pontique-etat-zero-2-anatomie-sol`, `hydrologie-systeme-eau` | Oui, minimal — `M_AnastasisSlice.uasset` (1 matériau, dégradé `Shore` à 1 seul axe) | — (pas un placeholder au sens mesh) | `AnastasisTerrainSurface::TileColor` | Matériau (blend multi-couche), pas un mesh | Haute, piste séparée (matériaux, pas assets discrets) |
| **Rives** | `pontique-etat-zero-2/4`, IV Kingdoms water fix-pack (hérité) | Oui, minimal — même matériau, bande `Shore` ~5 tuiles | — | `AnastasisTerrainSurface::Build` (`FVisualTile::Shore`) | Matériau | Haute, piste séparée |
| **Eau** | `pontique-hydrologie-systeme-eau`, `pontique-etat-zero-4` | Oui, minimal — plan plat au niveau de la mer, couleur de sommet | — | `AnastasisTerrainSurface` (section eau plate) | Matériau eau dédié (pas de plugin Water actif) | Moyenne |
| **Ruines** | Aucune planche dédiée explicite ; `Ruin` apparaît comme stand-in honnête dans `production-frame-test-integration` | Non | **Oui — `/Engine/BasicShapes/Cylinder.Cylinder`**, tinté gris via MID | `AnastasisPresentationResolver::Resolve(Ruin)` → `DressingMeshes[Ruin]` (HISM) | `UStaticMesh` | **Haute — même maturité que l'arbre avant cette mission, candidat symétrique** |
| **Props** | `pontique-props-materiel-refugies` | Non | Non | Aucun — aucune sémantique de prop dans `AnastasisWorld::FTile` | `UStaticMesh` | Basse, bloqué sans décision côté sim |
| **Architecture** | `pontique-grammaire-architecturale-batiment`, Tier0/1/2, héritage IV Kingdoms | Non | Non | **Aucun** — confirmé par lecture directe de `AnastasisWorld.h` : 7 `ETileType` seulement (`Grass,Water,Stone,Ruin,Forest,Scrub,Field`), aucune sémantique bâtiment/village | `UStaticMesh` / Blueprint modulaire | Basse pour cette mission — bloqué par `REACHABILITY_GATE` (décision côté `AnastasisSim` hors mandat) |
| **Chemins** | `pontique-lisiere-defrichement-pcg` (sentiers), État Zéro 5 | Non | Non | **Aucune donnée source** — confirmé `NOT_REACHABLE` dans `docs/unreal/VISUAL_PIPELINE_AUDIT.md` (zéro graphe de chemins dans `AnastasisSim`) | — | Bloqué, hors mandat |
| **Matériaux** | `pontique-usure-materiaux-vieillissement`, `pontique-etat-zero-2` | `M_AnastasisSlice.uasset` seul | — | `AnastasisTerrainSurface` | `UMaterial`/`UMaterialInstance` | Haute, piste séparée (système, pas un asset unique) |

## Lecture de la carte

Deux catégories sont *aujourd'hui* au même niveau de maturité technique : **Arbres** (Forest) et
**Ruines** (Ruin) — toutes deux ont déjà un type de tuile réel, un résolveur de présentation
branché, et un placeholder géométrique (Cône / Cylindre) prouvé par capture runtime. **Rochers**
(Stone) est structurellement identique (type de tuile réel, zéro dressing) mais n'a pas encore de
résolveur — un pas de moins que Ruin.

Sols/Rives/Eau/Matériaux sont une piste **différente en nature** : ce ne sont pas des meshes
discrets à instancer, mais des couches de matériau sur la surface continue déjà scellée
(`AnastasisTerrainSurface`) — les traiter dans cette mission mélangerait deux chantiers distincts.

Architecture/Chemins/Props sont **bloqués** : aucune donnée de simulation n'existe pour les
piloter, et en créer une sort du mandat de cette mission (présentation uniquement, pas de
changement à `AnastasisSim`).

## Choix de la tranche verticale — Phase 2

**Arbres / Forest.** C'est la frontière la mieux préparée : un type de tuile réel (`Forest`, 716
tuiles/9216 dans le monde canonique), un résolveur de présentation déjà branché et testé
(`AnastasisPresentationResolver::Resolve`, `Anastasis.Presentation.Reachability/Determinism`
verts), un placement déterministe déjà prouvé par capture PIE réelle
(`dressing_instance_count` vérifié), et un seul point d'intégration à modifier
(`AnastasisPresentation::TreeMeshPath`). Ruin serait un candidat tout aussi valable et
symétrique — recommandé comme prochain asset (voir rapport final).
