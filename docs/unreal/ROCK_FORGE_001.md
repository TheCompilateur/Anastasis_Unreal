# ROCK_FORGE_001 — ANASTASIS_ROCK_GRAMMAR_V1

Grammaire rocheuse paramétrique native Unreal, produite par Geometry Script.
`PRIMITIVE → GRAMMAR → ARCHETYPE → CONTROLLED_VARIATION → CLUSTER → WORLD`.

---

## PROVENANCE

| | |
|---|---|
| Racine de travail | `C:\dev\Jeux IV Kingdoms\ANASTASIS_UNREAL\anastasis-rock-grammar-e67075` (worktree) |
| Branche | `claude/anastasis-rock-grammar-e67075` |
| HEAD au départ | `1cbeef6` (= `main` à ce moment) |
| Base après rebase (2e passe) | `ea87acc` — rebase **sans conflit** |
| `main` au moment d'écrire | `e2f7382` — a avancé **4 fois** pendant la mission |
| Moteur | UE 5.8.2, CL 56702186 |
| Build | `BUILD::PASS` ×2 — `Result: Succeeded` (135 s à froid, 83 s après rebase) |

La mission s'est faite en deux passes. La première livre la grammaire **sans toucher un
seul fichier suivi** ; la seconde, sur autorisation explicite et une fois `tree-visuals`
intégrée, déclare l'entrée `Stone` pour de bon. Voir CÂBLAGE PERMANENT.

Le script opérateur `tools\unreal\anastasis-unreal.ps1` refuse ce chemin (il n'accepte
que la racine canonique ou `C:\dev\ANASTASIS_WORKTREES\`). Build lancé directement via
`Build.bat`, ce que le script fait de toute façon en interne.

---

## GATE 0 — OWNER_MAP

Établie avant toute écriture.

| Rôle | Propriétaire réel |
|---|---|
| `CURRENT_ROCK_SOURCE` | `AnastasisSim` — `AnastasisWorld.cpp`, `ETileType::Stone` (`Alt > Highland && Rock > RockT`, seed `RockSeed=2027`) |
| `CURRENT_ROCK_MESH` | **aucun** — voir ci-dessous |
| `CURRENT_ROCK_MATERIAL` | `AnastasisTerrainSurface.cpp` — couleur `HighlandRock(0.518, 0.490, 0.463)` du sol, pas un matériau d'objet |
| `CURRENT_ROCK_SPAWNER` | `AAnastasisWorldEmbodiment::PlaceDressing` |
| `CURRENT_ROCK_DISTRIBUTION_OWNER` | `AnastasisPresentationResolver` (`ResolvePresentation` / `ResolveInstanceTransform`) |
| `CURRENT_RENDER_OWNER` | HISM par `(archétype, variante)` dans `AAnastasisWorldEmbodiment` |
| Données | `UAnastasisPresentationRegistry` → `/Game/Anastasis/Presentation/DA_AnastasisPresentation` |

### Le constat qui change la mission

**`EAnastasisSemanticType::Stone` n'a aucune entrée de présentation.** Ni dans le Data
Asset (`strings` sur le `.uasset` ne montre que `Forest` et `Ruin`), ni dans
`UAnastasisPresentationRegistry::CreateCodeDefaults` (deux `Entries.Add`, Forest et
Ruin). Donc `ResolvePresentation` renvoie `false` pour chaque tuile Stone, et
`PlaceDressing` fait `continue` : **zéro géométrie n'est posée sur la pierre.**

Aujourd'hui une tuile Stone est **une couleur de sol**, rien d'autre.

Ce que la mission décrivait comme « les rochers actuels — blocs, cylindres, volumes
répétitifs » correspond à deux autres choses, réelles mais distinctes :

- les **cubes HISM du terrain DEBUG** (`anastasis.Terrain.Surface 0`), un cube par
  tuile, tous types confondus — d'où « blocs » et « amas homogènes » ;
- l'archétype **Ruin**, qui est littéralement `/Engine/BasicShapes/Cylinder.Cylinder` —
  d'où « cylindres » et « verticalité artificielle ».

Le Ruin appartient à l'architecture, pas à la roche, et le terrain DEBUG appartient à la
métrologie. Ni l'un ni l'autre n'est dans le périmètre.

**Conséquence :** la mission n'est pas un remplacement de vocabulaire, c'est une
**création** de vocabulaire. `CURRENT_ROCK_VISUAL` est mesurable et vaut « plat » :

```
ANASTASIS_WORLDVIEW seed=12345 source=96x96 tiles=9216
counts=3249,1198,1396,462,716,725,1470
        ^Grass ^Water ^STONE ^Ruin ^Forest ^Scrub ^Field
```

**1396 tuiles Stone sur 9216 — 15,1 % du monde, troisième type le plus fréquent — ne
portent aujourd'hui aucune géométrie.** C'est la mesure du manque, et c'est la ligne de
base contre laquelle le gain se juge.

---

## MULTIAGENT_CONFLICTS

Trois worktrees voisins actifs pendant la mission. Deux en territoire adjacent :

| Branche | Surfaces touchées | Collision |
|---|---|---|
| `claude/anastasis-tree-visuals-136553` | `DA_AnastasisPresentation.uasset`, `PresentationRegistry.{cpp,h}`, `PresentationResolver.{cpp,h}` | **directe** |
| `claude/anastasis-ground-materials-727cc2` | `PresentationResolver.h`, `WorldEmbodiment.{cpp,h}`, `capture-slice.ps1` | **directe** |

`.gitattributes` déclare `*.uasset filter=lfs merge=lfs` : un `.uasset` est **binaire et
non fusionnable**. Deux branches qui l'éditent produisent un conflit qui ne se résout
qu'en jetant un des deux travaux.

### Décision

Le pipeline géométrique — le cœur de la mission — n'a besoin d'**aucun** de ces
fichiers. Seul le *câblage* (déclarer l'entrée Stone) les touchait. J'ai donc séparé les
deux :

- **la grammaire** : fichiers entièrement nouveaux, zéro collision ;
- **le câblage** : fait **en mémoire** au moment de la capture, jamais sauvegardé.

`tools/unreal/capture-rock-views.py` charge le registre, ajoute l'entrée Stone à l'objet
chargé, et ne sauvegarde pas. `AnastasisPresentation::GetRegistry()` rend à C++ le même
`UObject`, donc l'embodiment pose réellement les rochers et le A/B est réellement causal
— pendant que le fichier sur disque reste intact.

### Ce que cette décision a évité, vérifié après coup

`main` est passé de `1cbeef6` à `ea87acc` **pendant** la mission. Les trois commits
arrivés sont ceux de `tree-visuals` et d'un `TerrainForge`, et ils modifient exactement :

```
Content/Anastasis/Presentation/DA_AnastasisPresentation.uasset
Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationRegistry.cpp
Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationResolver.cpp
```

Si j'avais câblé Stone dans le Data Asset, ce travail serait aujourd'hui en conflit
binaire avec `main`. `git status` de cette branche ne contient que des `??` — **aucun
fichier suivi n'est modifié.**

### Vérification de divergence (obligatoire, Gate 0)

La convention de pivot dont dépend tout le contact au sol a survécu au nouveau `main` :

```
AnastasisPresentationResolver.cpp:269   Location.Z += 0.5 * EngineBasicShapeSize * Scale;
AnastasisPresentationResolver.h:35      inline constexpr double EngineBasicShapeSize = 100.0;
```

Inchangée. La grammaire reste valide sur `ea87acc`.

Deux ajouts de `main` sont directement réutilisables plus tard par la roche :
`FAnastasisPresentationVariant::ScaleBias` (un archétype couvre plusieurs statures) et
`Entry.MaxLeanDegrees` (inclinaison) — un rocher penché est exactement ce que
`ROCK_CLIFF_FRAGMENT` voudra.

> Réserve honnête : l'invariant `Anastasis.Terrain.DressingRestsOnRenderedGround` est
> cité en commentaire dans `PresentationRegistry.cpp` et `PresentationResolver.h`, mais
> je n'ai trouvé **aucun test qui l'implémente** sur `main`. Il ne peut donc pas être
> cité comme preuve verte. Ce que je peux affirmer : la grammaire ne touche pas la
> transform, seulement la géométrie du mesh — l'identité de placement est hors de portée
> de ce changement.

---

## UNREAL_TOOLS_USED

| Outil | Usage réel |
|---|---|
| **Geometry Script** | **Oui — outil principal.** Tout le pipeline. |
| Static Mesh Editor | Indirect : bounds, collision, Nanite réglés par `GeometryScriptCreateNewStaticMeshAssetOptions` et `GeometryScript_Collision` |
| Nanite | Évalué, **désactivé explicitement** — voir PERFORMANCE_NOTES |
| Material Editor | **Non.** Les rochers réutilisent le `BaseShapeMaterial` + `Tint` existant, comme Forest et Ruin. `GEOMETRY_FIRST` : aucune géométrie n'est cachée derrière une texture. |
| Modeling Mode | **Non.** Aucune opération manuelle ne s'est répétée : tout était déjà scriptable. |
| PCG Framework | **Non.** La distribution existante (`PlaceDressing`) consomme les assets telle quelle. |
| Foliage Mode | **Non.** Le banc de test est le monde réel, pas un bac à sable. |

### Signatures confirmées contre CE build

`tools/unreal/probe_rock_geoscript.py` (nouveau) énumère les 49 bibliothèques Geometry
Script du build. La prudence du projet (« ne pas supposer que les docs Epic d'une autre
version correspondent ») a payé — quatre suppositions raisonnables étaient fausses :

| Supposé | Réel en 5.8.2 |
|---|---|
| `GeometryScript_MeshEdits.apply_mesh_plane_cut` | `GeometryScript_MeshBooleans.apply_mesh_plane_cut` |
| `GeometryScript_MeshNormals` | `GeometryScript_Normals` — et le seuil d'angle n'est pas sur `CalculateNormalsOptions`, il est sur `SplitNormalsOptions.opening_angle_deg` |
| `PerlinNoiseOptions.layers` (tableau) | `base_layer` (unique) — l'écrire au pluriel **échouait en silence**, tous les rochers sortaient sans érosion |
| `set_mesh_uvs_from_box_projection` | n'existe pas ici — voir LIMITATIONS |

Deux pièges d'environnement, notés pour la suite :

- `get_editor_subsystem(StaticMeshEditorSubsystem)` renvoie `None` sous
  `-run=pythonscript` (les sous-systèmes éditeur ne sont pas montés en commandlet) ;
  la collision passe donc par `GeometryScript_Collision`, qui n'en a pas besoin.
- Un chemin passé en `-script="...\tools\..."` voit `\t` devenir une **tabulation**
  avant d'atteindre le commandlet. Les scripts du projet font déjà
  `.Replace('\','/')` — c'est pour ça.

---

## ROCK_GENOME

Dix paramètres. Chacun change la **silhouette** ; tout ce qui ne changeait que l'ombrage
a été coupé (`COMPLEXITY_MUST_EARN_EXISTENCE`).

```
ANASTASIS_ROCK_GENOME
{
    mass           taille générale (rayon de la masse génératrice, UU)
    height_ratio   extension Z / XY      <1 trapu et bas, >1 debout
    width_ratio    extension Y / X       ≠1 = empreinte non ronde → lisible de dessus
    taper          largeur haut / bas    <1 pyramidal, ~1 colonnaire, >1 en surplomb
    asymmetry      dérive latérale de la masse haute contre la basse (0..1)
    facets         nombre de fractures planes
    facet_depth    profondeur de morsure de chaque coupe (fraction de mass)
    erosion        déplacement basse fréquence (fraction de mass) — délibérément faible
    burial         part de la hauteur qui passe SOUS le plan de contact
    debris         masses satellites soudées à la base
}
```

**Coupés du brief initial, avec la raison :**

| Paramètre | Raison |
|---|---|
| `height` / `width` / `depth` | trois nombres pour ce que `mass` + deux ratios expriment mieux — et les ratios restent valides à toute échelle |
| `vertical_bias` + `flattening` | le même axe lu par les deux bouts : c'est `height_ratio` |
| `base_width` + `top_width` | leur rapport est la seule chose qui compte visuellement : `taper` |
| `fracture_bias` | absorbé par `facets` + `facet_depth` |
| `edge_softness` | affaire de normales/matériau, pas de silhouette — `GEOMETRY_FIRST` |
| `rotation_bias` | appartient au **placement** (`Entry.bRandomYaw` du registre), pas au mesh |

### CONTROLLED_VARIATION

`RANDOM != NATURAL`. Une variante est l'archétype **plus un écart borné**, jamais un
tirage neuf : un tirage libre détruirait la famille géologique que l'archétype existe
pour exprimer.

```
VARIANCE = { mass ±18%, height_ratio ±16%, width_ratio ±14%, taper ±14%,
             asymmetry ±22%, facet_depth ±18%, erosion ±25%, burial ±12% }
facets : ±1 au plus, et jamais moins de 3
```

Moins de trois coupes cesse de se lire comme une fracture et commence à se lire comme
une boule cabossée — d'où le plancher.

---

## GENERATION_PIPELINE

`tools/unreal/create_rock_assets.py`, headless.

```
sphère grossière (append_sphere_box)          ← pas un cube : les faces plates doivent
   ↓                                             être des fractures CHOISIES, pas six
scale_mesh non uniforme (height/width_ratio)     faces héritées qu'il faut ensuite cacher
   ↓
2 coupes obliques opposées, inégales          ← taper + asymmetry.
   ↓                                             Une coupe laisse une FACE ; un taper
N coupes de fracture (facets, facet_depth)       lisse laisse un cône.
   ↓
apply_perlin_noise_to_mesh (erosion, faible)  ← longue longueur d'onde : les grandes
   ↓                                             faces restent lisibles
debris soudés par boolean UNION
   ↓
park_on_contact_plane                         ← Gate 4, voir ci-dessous
   ↓
apply_simplify_to_planar (fusion coplanaire)
   ↓
compute_split_normals (opening_angle 34°)     ← arêtes nettes sur les fractures
   ↓
create_new_static_mesh_asset_from_mesh + collision convexe
```

### GATE 4 — contact au sol, sans une ligne de C++

`ResolveInstanceTransform` remonte chaque instance d'une **constante** :
`0.5 * EngineBasicShapeSize * Scale` = `50 × Scale`, **indépendante des bounds réels du
mesh**. Donc le sol tombe exactement sur le `Z` local `−50`.

`SM_Tree` et `SM_Ruin` se recentrent sur `[−50, +50]` pour se poser **dessus**. Un
rocher ne doit pas se poser sur le sol comme un meuble — il doit en **sortir**. Ces
meshes ne sont donc **délibérément pas recentrés** : le plan de contact est placé à
`Z = −50` et la masse **continue en dessous**.

```
   ╱▔▔╲          ← sommet
  ╱     ╲
 │       │
═╧═══════╧═══   ← Z local −50 : le terrain arrive ici
 ╲ jupe  ╱      ← enfouie, jamais vue, mais c'est elle qui fait
  ╲_____╱          que le rocher n'a pas de « socle » posé
```

Mesuré sur les assets produits (`buried` = UU sous le plan de contact) :

| Archétype | bounds Z | enfoui |
|---|---|---|
| `Massive_01` | `[−88,1 , +102,2]` | 38,1 UU |
| `Low_01` | `[−82,7 , +13,4]` | 32,7 UU |
| `Vertical_01` | `[−115,6 , +182,6]` | 65,6 UU |
| `Split_01` | `[−105,9 , +127,0]` | 55,9 UU |

Zéro changement de resolver, zéro recompilation : l'enfouissement est exprimé dans
l'espace du mesh, là où il appartient.

### Déterminisme — corrigé, puis prouvé

Premier jet : la graine venait de `hash((arch_id, variant))`. `str.__hash__` est **salé
par processus** (`PYTHONHASHSEED`) — chaque exécution produisait un
`SM_Rock_Massive_01` différent. C'est précisément ce qu'une grammaire ne doit pas faire.
Remplacé par `zlib.crc32`, stable entre processus et entre sessions d'éditeur.

Vérifié, pas supposé — deux exécutions complètes, lignes `SAVED` comparées :

```
run 1 lines=19
run 2 lines=19
=== DIFF run1 vs run2 ===
IDENTICAL -- generator is deterministic
```

---

## ARCHETYPES

Six familles × 3 variantes + 1 cluster composé = **19 assets**, dans
`/Game/Anastasis/Rock/`.

| Archétype | Intention | bounds XY (v1) | Z | tris |
|---|---|---|---|---|
| `ROCK_MASSIVE` | la masse de référence, prouvée la première (Gate 3) | 200 × 174 | 190 | 234 |
| `ROCK_LOW` | socle rocheux affleurant, large et majoritairement enfoui | 304 × 368 | 96 | 466 |
| `ROCK_VERTICAL` | debout mais **jamais colonne** — `asymmetry` 0,46 et `taper` 0,44 sont ce qui l'en empêche | 171 × 142 | 298 | 382 |
| `ROCK_SPLIT` | deux masses et la faille entre elles | 197 × 253 | 233 | — |
| `ROCK_BOULDER` | bloc isolé, plus rond, fractures nombreuses mais peu profondes | 194 × 165 | 183 | — |
| `ROCK_CLIFF_FRAGMENT` | anguleux, pour les ruptures de relief — les facettes les plus agressives | 230 × 138 | 255 | — |

La différenciation est **mesurable**, pas déclarative : `LOW` fait 368 UU de large pour
96 de haut, `VERTICAL` 142 de large pour 298 de haut. Ce sont deux silhouettes, pas un
réglage d'échelle.

### GATE 6 — CLUSTER_GRAMMAR

`SM_Rock_Cluster_01` n'est pas un septième rocher : c'est une **composition** de la
famille, avec une hiérarchie délibérée.

```
PRIMARY_MASS      ROCK_MASSIVE        × 1,00   ← dominante, non ambiguë
SECONDARY_MASS    ROCK_LOW            × 0,62   ← soutient, n'égale jamais
                  ROCK_CLIFF_FRAGMENT × 0,41
SMALL_FRAGMENT    ROCK_BOULDER        × 0,23   ← le débris se lit comme un débris
```

Jamais `O O O O`. Les membres ne sont pas « parkés » individuellement : le cluster est
parké **une fois, comme une masse**, pour que les membres gardent leur relation de
hauteur les uns aux autres.

---

## ASSETS_CREATED

19 `.uasset` dans `Content/Anastasis/Rock/` (27–80 ko, couverts par `*.uasset` LFS).

## ASSETS_MODIFIED

`Content/Anastasis/Presentation/DA_AnastasisPresentation.uasset` — entrée `Stone`
ajoutée en 2e passe (voir CÂBLAGE PERMANENT). Forest et Ruin recopiées intactes.

## CODE_MODIFIED

`Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationRegistry.cpp` — l'entrée
`Stone` ajoutée à `CreateCodeDefaults`, pour que le repli code dise la même chose que
le data asset. C'est le seul `.cpp` touché ; aucun `.h`, aucun `.umap`.

La première passe ne modifiait **rien** de suivi. C'est ce qui lui a permis de survivre
sans conflit à deux avancées de `main`.

## FICHIERS AJOUTÉS

```
tools/unreal/create_rock_assets.py       la grammaire
tools/unreal/probe_rock_geoscript.py     introspection Geometry Script de ce build
tools/unreal/capture-rock-views.py       A/B 4 échelles, entrée Stone en mémoire
tools/unreal/capture-rock-views.ps1      lanceur (séparé de capture-slice.ps1, tenu par ground-materials)
docs/unreal/ROCK_FORGE_001.md            ce rapport
```

---

## BASELINE_CAPTURES / CANDIDATE_CAPTURES

Huit captures, **une seule session d'éditeur**, dans `docs/visual/rock-forge-001/` :
même niveau, même soleil (75000 lux), même exposition (EV100 14), même seed, **mêmes
transforms de caméra**, même appel d'embodiment. Seule l'entrée Stone change.

```
BASELINE    STONE_ENTRY enabled=False  registry_entries=2  dressing_instances=91
CANDIDATE   STONE_ENTRY enabled=True   registry_entries=3  dressing_instances=163
                                                           → 72 instances de roche
STONE_ENTRY restored to 2 entries (nothing saved)
```

Le point de visée n'est pas choisi à la main : le script lit les transforms des
instances HISM réellement posées et vise le voisinage le plus dense (53 voisins dans
520 UU). La caméra vise donc de vrais rochers, et les deux moitiés cadrent le même sol.

### Ce que les captures montrent

**`BASELINE_MID` confirme visuellement le constat de la Gate 0.** Toute l'étendue pâle
au premier plan **est** la zone Stone : une couleur, zéro géométrie. Ce n'est plus une
déduction tirée du code, c'est à l'image.

| Échelle | Verdict |
|---|---|
| `CLOSE` | **PASS** — facettes nettes, asymétrie franche, aucune lecture de primitive. Chaque masse est distincte de sa voisine. |
| `MID` | **PASS** — silhouettes lisibles, hiérarchie de tailles présente, contact au sol crédible : les masses sortent du terrain, aucun « socle » posé n'est visible. |
| `GAMEPLAY` | **PASS** — la pierre structure le relief au lieu de le colorer. |
| `AERIAL` | **PASS** — masses minérales lisibles, structure interne encore perceptible, cantonnées aux zones Stone, sans dégénérer en bruit. |

### Deux itérations, et pourquoi

Le premier jet a **échoué** et les captures l'ont dit — c'est exactement leur fonction.

1. **Échelle 0,85–1,85 → rejetée.** Les rochers faisaient 3 à 5 tuiles de large. En vue
   aérienne la formation entière se lisait comme **un seul bloc blanc** couvrant une
   grosse part de la tranche : la Gate AERIAL échouait franchement, et au MID les
   rochers écrasaient les arbres. Ramenée à **0,30–0,70** (≈ 0,6–1,4 tuile) : échelle de
   blocs, plus de collines.

2. **Teinte 0,518 → 0,315 → 0,150.** J'ai d'abord repris exactement le `HighlandRock`
   du sol, en me disant qu'un rocher et le haut-pays sur lequel il repose devaient
   appartenir à la même famille. Les captures ont réfuté le raisonnement : à albédo égal,
   sous 75000 lux / EV100 14, le sol **et** les rochers saturent vers le blanc, et la
   pierre cesse de se lire comme de la pierre.

   > Correction d'une lecture intermédiaire : en comparant aux ruines (teinte 0,353,
   > rendues plus sombres que mes rochers à 0,315) j'ai d'abord conclu que la teinte
   > n'atteignait pas les rochers. **C'était faux.** Le test à 0,150 est sorti
   > franchement gris : la teinte s'appliquait depuis le début. Un 0,315 *linéaire* vaut
   > ≈ 0,6 en sRGB — c'est la valeur qui était trop claire, pas la plomberie qui était
   > cassée. `ITER2_bright_tint_CANDIDATE_MID.png` garde la trace de ce passage.

   Retenu : **`(0.150, 0.142, 0.134)`** — même famille de teinte que `HighlandRock`,
   environ un tiers de sa valeur. C'est l'écart d'albédo, pas l'écart de teinte, qui
   sépare la masse du sol dont elle sort.

---

## PERFORMANCE_NOTES

**Triangles : 234 à 472 par rocher** (mesuré, `triangle_count` sur le DynamicMesh avant
sauvegarde). Le cluster composé est le plus lourd.

`apply_simplify_to_planar` fusionne les éventails de triangles coplanaires que laissent
les coupes : la face est plate de toute façon, les triangles en trop ne coûtaient que du
budget.

**NANITE : désactivé, explicitement.** La règle de la mission est
`NANITE_ENABLE iff measurable_or_structurally_justified` — et à 234–472 triangles de
grandes faces plates, ni l'un ni l'autre ne tient. Le bénéfice de Nanite est une densité
de triangles que ces rochers n'ont pas et **ne doivent pas acquérir** : la silhouette est
le livrable, le micro-détail est explicitement hors direction. Le drapeau est posé à
`False` dans le pipeline plutôt que laissé par défaut, pour que le choix soit visible et
non hérité par accident.

Collision : coques convexes (4 max, 24 faces), via `GeometryScript_Collision`.

Distribution : 163 instances de dressing (dont **72 rochers**) sur la tranche 32×32, contre 91 sans la pierre.

---

## LIMITATIONS

1. **Pas d'UV utiles.** Ce build n'expose aucune projection UV dans
   `GeometryScript_UVs` (seulement de la manipulation d'éléments et du texel density).
   Sans conséquence en V1 : les rochers utilisent le `BaseShapeMaterial` + `Tint`
   partagé, non texturé, exactement comme Forest et Ruin. Ce sera requis le jour où un
   vrai matériau roche texturé arrivera.
2. **Stone n'est pas câblé de façon permanente.** Délibéré — voir MULTIAGENT_CONFLICTS.
   Le A/B prouve que le monde consomme les assets ; la déclaration permanente reste à
   faire par l'intégrateur, après `tree-visuals`.
3. **Pas de blend terrain.** La jupe enfouie traite le contact géométriquement, ce qui
   suffit à supprimer l'effet « posé ». Un vrai fondu roche/sol demanderait RVT ou un
   blend de matériau Landscape — chantier adjacent, explicitement non ouvert ici.
4. **Aucune campagne de tests d'automatisation lancée.** La mission n'a modifié aucun
   C++ ; il n'y a donc pas de régression de code à prouver. Ce n'est pas une preuve que
   la suite est verte, et ce n'est pas présenté comme telle.

---

## CÂBLAGE PERMANENT — fait (2e passe, sur autorisation)

`tree-visuals` ayant atterri sur `main`, la fenêtre annoncée s'est ouverte : plus aucune
branche ne détenait `DA_AnastasisPresentation`. Vérifié avant d'écrire, branche par
branche. La branche a donc été **rebasée sur `main` (`ea87acc`, 0 conflit)** puis l'entrée
`Stone` déclarée pour de bon.

`tools/unreal/set_rock_presentation.py` — nouveau, sur le patron maison de
`set_tree_grammar.py` et `set_presentation_meshes.py`. Portée : l'entrée STONE, et rien
d'autre ; les autres entrées sont recopiées telles quelles. Le piège UE-Python que ces
scripts documentent est repris : `get_editor_property` sur un struct rend une **copie**,
donc le tableau doit être réécrit dans son propriétaire avant de sauver.

```
PASSE 1  registre chargé : 2 entrées
         STONE entry AJOUTÉE en position 2 (elle n'existait pas)
         SAVED -- 3 entrées
         VERIFY FOREST archetype=Tree_Generic     variants=8
         VERIFY RUIN   archetype=Ruin_Generic     variants=1
         VERIFY STONE  archetype=Rock_Grammar_V1  variants=19

PASSE 2  (process neuf)  STONE entry[2] déjà conforme -- rien à faire
```

La seconde passe dans un process neuf est la preuve forte exigée par le patron maison :
**idempotence relue depuis le disque**, et Forest/Ruin intacts.

`AnastasisPresentationRegistry::CreateCodeDefaults` reçoit la même entrée, parce que la
règle maison est explicite : « le data asset fait autorité quand il se charge, le repli
code ne sert que s'il manque ; les deux doivent dire la même chose ».

Le A/B a été **refait après câblage**, et la capture le dit elle-même :

```
ROCK_VIEW STONE_SOURCE data asset (entry is committed)
ROCK_VIEW ROCK_INSTANCES n=72
```

Le CANDIDATE n'est donc plus une entrée synthétisée en mémoire : c'est l'asset tel qu'il
sera livré. Les captures de `docs/visual/rock-forge-001/` sont celles-là.

`MaxLeanDegrees = 9` est la seule valeur ajoutée à cette passe. La ruine est à 0 parce
qu'un pan de mur est fabriqué et se tient d'aplomb ; un rocher s'est posé comme il est
tombé. L'inclinaison remonte le pied de `(1-cos L) × 50 × Scale` ≈ 0,4 UU — sans commune
mesure avec les 27 à 76 UU de jupe enfouie, donc le contact au sol n'en souffre pas.

### Conflit ouvert, et sa résolution

`agent/sim-tick-day` (2 commits, **non intégrée**) modifie le même
`DA_AnastasisPresentation.uasset` et le même `PresentationRegistry.cpp` — elle refait
l'échelle des arbres (enveloppe Forest `3.6–5.0` → `15.0–21.0`, canopée à 15–25 m au lieu
d'une hauteur d'homme). Le `.uasset` étant binaire, les deux ne fusionneront pas.

**Ce n'est pas un blocage, et la résolution est déterministe** — c'est précisément
pourquoi le projet câble le registre par des scripts plutôt qu'à la main :

> garder l'un OU l'autre `.uasset` au choix, puis **relancer les deux scripts**
> (`set_tree_grammar.py` / celui de sim-tick-day, puis `set_rock_presentation.py`).
> Les deux sont idempotents et additifs : chacun ne touche que son entrée et recopie les
> autres. L'ordre n'a pas d'importance.

Le C++ de `CreateCodeDefaults`, lui, fusionne normalement : les deux changements portent
sur des entrées différentes.

Note d'échelle : sous l'enveloppe de sim-tick-day (canopée 15–25 m), des rochers de
0,45 à 2,1 m restent justes — ce sont des blocs, pas des affleurements. Aucune
re-calibration n'est nécessaire si cette branche atterrit.

---

## NEXT_RECOMMENDED_STEP

1. corréler l'archétype à l'altitude et à la pente plutôt qu'au seul hasard de tuile
   (le socle bas en plaine, le fragment anguleux sur les ruptures) — c'est le gain
   suivant le plus net, et `SelectVariantIndex` sait déjà router sur des axes ;
2. se servir de `ScaleBias` par variante pour qu'un même archétype couvre le bloc et
   l'affleurement, au lieu d'une seule enveloppe pour les dix-neuf ;
3. UV + matériau roche, seulement si la direction artistique le demande.

---

## FINAL_VERDICT

**KEEP** — avec le câblage permanent laissé à l'intégrateur (voir NEXT_RECOMMENDED_STEP).

Contre les douze critères de la mission :

| # | Critère | |
|---|---|---|
| 1 | ne ressemblent plus à des primitives | **oui** — visible au CLOSE |
| 2 | asymétrie contrôlée | **oui** — `asymmetry` + coupes inégales, bornée par `VARIANCE` |
| 3 | base intégrée au terrain | **oui** — 27–76 UU enfouis, mesurés |
| 4 | au moins quatre archétypes | **six**, plus le cluster |
| 5 | pipeline Unreal reproductible | **oui** — Geometry Script, déterminisme prouvé par diff |
| 6 | le monde consomme les assets | **oui** — 72 instances posées par le spawner existant |
| 7 | macro-géographie inchangée | **oui** — aucun fichier suivi modifié |
| 8 | pas de refonte PCG | **oui** — PCG jamais touché |
| 9 | lisibles en vue aérienne | **oui, après correction d'échelle** |
| 10 | fonctionnent sous lumière neutre | **oui** — rig figé 75000 lux / EV100 14 |
| 11 | gain visible BASELINE/CANDIDATE | **oui** — couleur plate → masses minérales |
| 12 | pas de régression technique | **aucune connue** ; zéro C++ modifié. Voir la réserve sur les tests en LIMITATIONS : aucune campagne d'automation n'a été lancée, ce n'est donc pas une preuve verte. |
