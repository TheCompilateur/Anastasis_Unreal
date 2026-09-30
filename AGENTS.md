# ANÁSTASIS Unreal — règles agent

Projet **Unreal Engine 5.8.2** (CL 56702186). Pas une app web, pas Unity, pas Godot.

## Ne développe pas dans la racine canonique — règle n°0

**DO NOT IMPLEMENT FEATURES DIRECTLY IN THE CANONICAL ROOT.**

`C:\dev\ANASTASIS_UNREAL` est un **poste d'intégration**, pas un plan de travail.
Plusieurs agents travaillent en parallèle sur ANÁSTASIS : si deux d'entre eux écrivent
dans cette racine pendant qu'un troisième build, teste ou scelle, la preuve produite ne
porte plus sur ce qui est réellement sur le disque. C'est arrivé, et ça a invalidé un
`VERIFY::PASS`.

Dans la racine canonique, un agent peut :

| Action | Autorisé |
|---|---|
| lire, chercher, inspecter l'historique | ✅ |
| `build`, `verify`, lancer l'éditeur, PIE | ✅ |
| intégrer une branche d'agent, **s'il tient le rôle d'intégrateur** | ✅ |
| créer, éditer ou supprimer des fichiers de développement | ❌ |

Ton développement vit dans **ton** worktree :

```powershell
tools\unreal\agent-worktree.ps1 create -Mission <ma-mission>
cd C:\dev\ANASTASIS_WORKTREES\<ma-mission>
```

Si ton répertoire de travail est `C:\dev\ANASTASIS_UNREAL` et que tu t'apprêtes à écrire
un fichier de feature, **arrête-toi et crée ton worktree d'abord**.

## Protocole multi-agent

```
CANONICAL_MAIN   C:\dev\ANASTASIS_UNREAL        intégration seulement
AGENT_WORK       agent/<mission> + worktree dédié
COMMIT           unité de passation
INTEGRATOR       seul écrivain pendant l'intégration
VERIFY / SEAL    exigent une racine canonique quiescente
```

Conventions, sans exception :

- branche : `agent/<mission>`
- worktree : `C:\dev\ANASTASIS_WORKTREES\<mission>`
- mission en minuscules, chiffres, `.` `_` `-`

Cycle de vie, un outil unique : `tools\unreal\agent-worktree.ps1`

| Commande | Rôle |
|---|---|
| `create -Mission <m>` | branche + worktree depuis `main` |
| `status` | tous les worktrees : modifications, avance/retard sur `main`, branches non intégrées |
| `finish -Mission <m>` | portail de fin : build + `report-tests`, refuse de passer la main si du travail n'est pas commité |
| `integrate -Mission <m>` | rôle intégrateur : refuse si le canonique est sale, puis avance rapide de `main` |
| `preflight` | avant un `verify`/seal : dit ce qui bloque et **ouvre une fenêtre** d'observation |
| `postflight` | après : échoue si source, config ou `HEAD` ont bougé pendant la fenêtre |

Un `verify` ou un seal dont le `postflight` échoue ne prouve rien. Ne jamais le rapporter
comme une réussite.

Chaque worktree a ses propres `Binaries/` et `Intermediate/` : le premier build y est
complet, c'est normal et c'est le prix de l'isolation.

`core.hooksPath` pointe vers `tools/git-hooks`, qui est **suivi**. git-lfs y installe donc
ses propres `post-checkout`, `post-commit`, `post-merge` : ils sont versionnes, ce qui les
empeche de reapparaitre en non-suivis dans chaque nouveau worktree, et garantit que LFS
fonctionne apres un clone. Ne pas les supprimer. A configurer une fois par clone :

```powershell
git config core.hooksPath tools/git-hooks
```

## Racine canonique — règle n°1

```
C:\dev\ANASTASIS_UNREAL
```

**C'est la seule racine de développement autorisée.** Toute autre copie est un artefact mort.

Ne jamais ouvrir, lire, builder ni modifier les copies sous
`C:\Users\alex_\OneDrive\Documents\Unreal Projects\` :

| Copie | Statut |
|---|---|
| `Anastasis_UnrealV2 5.8` | ex-canonique — `READ_ONLY_BACKUP_CANDIDATE` |
| `Anastasis_UnrealV2` | prédécesseur |
| `Anastasis_UnrealV2 5.8 - 2` | doublon exact du prédécesseur |
| `Anastasis_Unreal`, `Anastasis_Unreal 5.8` | legacy UE 5.7, module `AnastasisCore` |
| `Final_AnastasisUR` | sonde template, jamais le jeu |

**État au 2026-09-12 : ces six copies ont été supprimées** (29,67 Go), après vérification fichier par
fichier qu'elles ne contenaient rien d'absent du canonique. Le seul contenu unique, le module
`AnastasisCore` du legacy 5.7, est archivé dans `archive/legacy-AnastasisCore/`. Il ne subsiste que deux
squelettes de dossiers vides, sans `.uproject`. Le tableau reste ici comme interdit permanent : ne pas
les recréer, ne pas les restaurer.

Si un chemin de travail contient `OneDrive`, **s'arrêter et le signaler** : la session est dans la mauvaise racine.

## Pourquoi les copies se multiplient (cause racine corrigée le 2026-09-12)

`EngineAssociation` dans le `.uproject` doit rester exactement `"5.8"`.

Historique : les copies OneDrive avaient une association non résoluble — `Anastasis_UnrealV2` porte
le GUID `{2C7D17E4-42D1-2935-EB7E-098AE6DEDC25}`, qui était orphelin
(`HKCU\Software\Epic Games\Unreal Engine\Builds` vide). Unreal ne pouvait pas la résoudre, affichait
donc la boîte de conversion **à chaque ouverture**, et l'option « Open a copy » dupliquait tout le
dossier en `<Projet> <Version>`, puis ` - 2`, ` - 3`… C'est ce mécanisme précis qui a produit les 4
copies mortes listées ci-dessus.

**Correction du 2026-09-12 (vérifiée) :** une version antérieure de ce document affirmait que ce GUID
avait été enregistré dans `HKCU\Software\Epic Games\Unreal Engine\Builds`. **C'est faux.** Cette clé
existe mais ne contient aucune valeur — vérifier avec :
```powershell
(Get-Item 'HKCU:\Software\Epic Games\Unreal Engine\Builds').GetValueNames()
```
Le point est désormais sans objet : les copies porteuses de ce GUID ont été supprimées. Ne pas
réenregistrer ce GUID — cela ne servirait qu'à faire revivre une copie morte.

**Fix distinct, toujours en attente (HKLM, élévation admin requise) :** `HKLM\SOFTWARE\EpicGames\Unreal Engine\`
a des clés `4.0` et `5.7` mais pas `5.8` — l'association `"5.8"` du projet **canonique** lui-même n'est
résoluble aujourd'hui que parce que `tools/unreal/anastasis-unreal.ps1` code en dur le chemin du moteur.
Si ce `.uproject` est un jour ouvert autrement (double-clic Explorateur), le même type de boîte apparaîtra
sur le canonique. Fix, à lancer en PowerShell admin :
```powershell
New-Item -Path 'HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.8' -Force | Out-Null
New-ItemProperty -Path 'HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.8' -Name 'InstalledDirectory' -Value 'C:\Program Files\Epic Games\UE_5.8' -PropertyType String -Force | Out-Null
```

- Ne jamais proposer « Open a copy ». Si la boîte apparaît malgré tout : « Convert in place » ou « Skip conversion ».
- Ne jamais réécrire `EngineAssociation`.
- Une copie recompile toujours de zéro : le cache UBT (`Intermediate/Build/**/Makefile.bin`) et les
  `.target` gravent le chemin absolu du projet. Chemin différent = cache invalide. Le rebuild ne produit
  aucun code nouveau ; il refait le même travail. Ce n'est jamais une raison de dupliquer.

## Périmètre

- C++ dans `Source/AnastasisSim/` (contrat portable, parité JS) et `Source/Anastasis_UnrealV2/` (gameplay, WorldView).
- Python éditeur dans `Content/Python/anastasis_toolset/`.
- Racine = dossier du `.uproject`, **jamais** `Intermediate/Build/...`.
- Le simulateur JS (`C:\dev\Jeux IV Kingdoms`) est une **référence de parité**, pas du code à porter.
  `src/render3d/` est obsolète — le rendu se réécrit dans Unreal.

## Interdit

Ne pas lire, modifier ni générer dans `Intermediate/`, `Saved/` (logs en lecture seule), `DerivedDataCache/`, `Binaries/`.

Ne pas éditer `.uasset` / `.umap` comme du texte — ils sont binaires et suivis en Git LFS.
Les inspecter via un éditeur vivant (Unreal MCP, `AnastasisInspectTools`).

Ne jamais commiter d'artefact généré. `.gitignore` les couvre : le vérifier avec `git check-ignore -v` en cas de doute.

## Opérateur

```powershell
tools\unreal\anastasis-unreal.ps1 status   # racine, empreinte source, git
tools\unreal\anastasis-unreal.ps1 build    # build incrémental Editor Win64 Development
tools\unreal\anastasis-unreal.ps1 verify   # build + éditeur dédié + smoke PIE
tools\unreal\anastasis-unreal.ps1 editor   # lance l'éditeur (pas une vérification)
```

Le script refuse de tourner hors de la racine canonique et valide l'identité moteur (5.8.2 / CL 56702186).

## Tests

```powershell
UnrealEditor-Cmd.exe "<uproject>" -unattended -nopause -nosplash -NoLiveCoding `
  -abslog="<log>" -ExecCmds="Automation RunTests Anastasis;Quit" -testexit="Automation Test Queue Empty"
```

**Divergences connues, marquées explicitement dans le framework de tests — ne pas les attribuer à un changement en cours, ne pas les « corriger » sans mandat :**

- `Anastasis.Sim.Parite.Fbm` — divergence ULP contre la référence JS, isolée via `AddExpectedError` dans `AnastasisParityTests.cpp`. Rapportée `Success`.
- `Anastasis.Sim.Parite.SemantiqueJs` — `ToUint32(1e21)`, idem.
- `AI.Toolsets.AnastasisInspect…test_list_selected_actors_returns_list` — l'API renvoie `Array`, pas `list` (le décorateur `@toolset_registry.tool_call` marshalle le retour). Marqué `@unittest.expectedFailure` dans `test_inspect.py`. Rapportée `Success`.
- `AI.Toolsets.AnastasisInspect…test_list_level_actors_raises_on_non_positive_max_count` — `ValueError` converti en script error par le même décorateur, sauf sous `toolset_registry.tool_raising_exceptions()`. Idem, marqué `@unittest.expectedFailure`. Rapportée `Success`.

### KNOWN_EXPECTED_FAILURE n'est pas PASS

Un test marqué rapporte `Success`. **Ne jamais agréger ces `Success` avec les vrais `PASS`** : une suite
« 29/29 verte » est un mensonge si quatre de ces tests ne vérifient pas ce qu'ils annoncent.

Tout rapport de tests de ce projet doit distinguer trois catégories :

```
PASS                    le test vérifie ce qu'il annonce, et il passe
KNOWN_EXPECTED_FAILURE  divergence connue, marquée, inscrite au registre
FAIL                    tout le reste
```

Le registre fait autorité : `tools/unreal/known-expected-failures.txt`. Le rapporteur croise les
résultats avec lui et refuse de les confondre :

```powershell
tools\unreal\report-tests.ps1
```

N'ajouter une entrée au registre que sur mandat explicite, jamais pour faire taire une régression
nouvelle. Si un test marqué redevient `Fail`, c'est soit une vraie régression, soit le marqueur retiré —
dans les deux cas, investiguer avant de toucher au marqueur. Voir `docs/unreal/AUTOMATION_TRIAGE.md` : une campagne de tests complète reste hors du seal.
Le bruit `Condition failed` au démarrage est sensible à la culture de l'éditeur, pas un échec ANÁSTASIS.

## Preuve

Un changement C++ n'est pas fini tant que `build` n'est pas `BUILD::PASS` et que les tests du chantier
concerné ne sont pas verts. Citer les noms de tests et les valeurs, pas « ça marche ».

## État

`PLAYER` reste **NOT_IMPLEMENTED**. Ne pas le commencer sans mandat explicite d'Alexandre.

Voir `ANASTASIS_CANONICAL_PROJECT.md` et `docs/unreal/UNREAL_CANONICAL_STATE.md`.

## Index de `tools/unreal/`

Index du contenu de ce worktree. Les recettes de creation ecrivent dans Content ; les captures utilisent leur chemin de sortie documente.

| Script | Role |
|---|---|
| `anastasis-unreal.ps1` | `status` / `build` / `build-game` / `verify` / `health` / `editor` |
| `agent-worktree.ps1` | cycle de vie multi-agent : `create` / `status` / `finish` / `integrate` / `preflight` / `postflight` / `mcp` |
| `report-tests.ps1` | suite `Anastasis`, classée PASS / KNOWN_EXPECTED_FAILURE / FAIL, refuse un run tronqué |
| `project-health.ps1` | rapport de santé des preuves (appelé par `health`) ; absent ou périmé ≠ PASS |
| `automation-log.ps1` | lecture de log d'automation partagée par les deux précédents, pas un point d'entrée |
| `scheduled-verify.ps1` | run nocturne (Planificateur de tâches) : `verify` puis `report-tests` |
| `smoke-pie.py` | smoke PIE lancé par `verify` |
| `known-expected-failures.txt` | registre KNOWN_EXPECTED_FAILURE — sur mandat seulement |
| `known-log-patterns.txt` | baseline des Error/Warning connus du log éditeur — idem |
| `capture-slice.ps1` + `observe-slice.py` | capture viewport de `Lvl_AnastasisSlice` ; `-PreCmds` pour un A/B sur une seule CVar. `-RebuildMaterial 1` réécrit le matériau |
| `probe-demo.ps1` + `probe-demo.py` | preuve PIE : snapshot monde + capture par bookmark (défauts connus : `ATMOSPHERE_002.md`) |
| `asset_agent_probe.py` | preuve PIE partagée par les missions d'asset, un bookmark par run |
| `shore-capture.ps1` + `shore-capture.py` | A/B visuel du bord d'eau, cadrage sur une rive |
| `capture-terrain-forge.ps1` + `terrain-forge-capture.py` | captures avant/après du relief → `Saved/TerrainForgeEvidence/` |
| `capture-terrain-relief.ps1` + `terrain-relief-capture.py` | avant/après d'une étape de la forge de relief (`-Step 1/2/3/scale`), dressing masqué → `Saved/TerrainReliefEvidence/` |
| `capture-reed-form.ps1` + `capture-reed-form.py` | comparaison des formes de roseaux dans une scene temporaire |
| `capture-tree-lineup.ps1` + `capture-tree-lineup.py` | planche de stature de la grammaire d'arbres |
| `astral-observe.py` | A/B lumière du jour fixe, Ecology seule variable |
| `measure-tree-cost.ps1` + `measure-tree-cost.py` | triangles, LOD, instances HISM réellement soumis |
| `inspect_presentation_registry.py` | dump de `DA_AnastasisPresentation` |
| `introspect_geoscript.py` | docstrings des fonctions GeometryScript utilisées |
| `observe-slice.py` | `M_AnastasisSlice`, `Lvl_AnastasisSlice` |
| `ground-material.ps1` + `.py` | `M_AnastasisGround`, `MI_AnastasisGround` |
| `shore-water.ps1` + `.py` | `M_AnastasisShoreWater` |
| `presentation-registry.py` | `DA_AnastasisPresentation` |
| `atmosphere-profile.py` | `DA_AnastasisAtmosphere` |
| `create_tree_asset.py` | `SM_Tree_*`, `M_AnastasisVegetation` — régénérés à **chaque** run |
| `create-reed-form.py` | recette isolee de roseaux courbes ; cree les assets de la variante |
| `create_ruin_asset.py` | `SM_Ruin_Generic_01` |
| `set_presentation_meshes.py` | câble un mesh par archétype dans `DA_AnastasisPresentation` |
| `set_tree_grammar.py` | entrée FOREST du registre (variantes d'arbres) |
| `set_ruin_variant.py` | entrée Ruin — généralisé depuis par `set_presentation_meshes.py` |
| `capture-refugee-props.ps1` | Lance la creation/relecture des quatre props et neuf captures dans un editeur dedie ; sortie -OutDir. |
| `capture-refugee-props.py` | Scene studio temporaire : planche + deux angles par prop, sans sauvegarde de niveau. |
| `capture-shore-composition.py` | Compare les placements de roseaux, reutilise capture-reed-form.ps1 ; sortie ANASTASIS_REED_FORM_OUT. |
| `capture-shore-contact.py` | Compare ShoreProfile 0/1 avec placements fixes ; pilote par capture-reed-form.ps1. |
| `capture-shore-material.py` | Compare les matieres sol/eau et diagnostique la nappe masquee ; pilote par capture-reed-form.ps1. |
| `create-refugee-props.py` | Cree quatre StaticMesh et leur materiau dans RefugeeProps008 ; geometrie pure testable, refuse la derive de recette. |
