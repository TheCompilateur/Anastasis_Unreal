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
| `create -Mission <m>` | branche + worktree depuis `main`, port MCP du worktree enregistré |
| `status` | tous les worktrees : modifications, avance/retard sur `main`, branches non intégrées |
| `finish -Mission <m>` | portail de fin : index `tools/unreal/` à jour, aucun Unreal lancé hors `Start-AnastasisEditor`, build + `report-tests`, refuse de passer la main si du travail n'est pas commité |
| `mcp -Mission <m>` | (ré)enregistre le port MCP d'un worktree existant côté Claude Code |
| `integrate -Mission <m>` | rôle intégrateur : avance rapide de **`main`** (jamais de la branche extraite du canonique), après avoir rejoué index et lancements Unreal sur l'arbre versé ; canonique hors `main` → copie de travail intacte |
| `prune -Mission <m>` | après versement : worktree, branche et enregistrement MCP local supprimés ; refuse si un commit manque à `main` ou si le worktree n'est pas propre |
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

Le script refuse de tourner hors de la racine canonique ou d'un worktree sous `C:\dev\ANASTASIS_WORKTREES`,
et valide l'identité moteur (5.8.2 / CL 56702186).

## Éditeur vivant : MCP Unreal

Le plugin `ModelContextProtocol` (expérimental, UE 5.8) démarre avec l'éditeur un serveur MCP sur
`http://localhost:8000/mcp` (`Config/DefaultEditorPerProjectUserSettings.ini`). `.mcp.json` le déclare
pour Claude Code ; la première session demande d'approuver le serveur `unreal`.

La recherche d'outils est active : le serveur n'expose que 3 méta-outils. `list_toolsets` liste les
groupes (`EditorToolset`, `StateTreeToolset`, `AutomationTestToolset`, `AnastasisInspectTools`…) et on
ne charge que celui dont on a besoin. `AnastasisInspectTools` (`Content/Python/anastasis_toolset/`)
est en lecture seule : snapshot de session, acteurs, tuile, établissement, `verify_world_contract`.

**Un port par racine.** Sans argument, tout éditeur vise 8000 et le premier levé le garde : un agent
croirait inspecter son éditeur et parlerait à celui d'un autre. `mcp-port.ps1` donne donc à chaque
racine son port — 8000 pour le canonique, un port stable dans 8100–8899 dérivé du nom de mission pour
un worktree :

- `anastasis-unreal.ps1 editor` lance l'éditeur sur le port de sa racine ; `status` et `editor` affichent `MCP_URL::`.
- `agent-worktree.ps1 create` enregistre ce port côté Claude Code en portée locale, qui prime sur `.mcp.json`.
  Pour un worktree créé avant : `agent-worktree.ps1 mcp -Mission <m>`, puis nouvelle session.
- Les éditeurs lancés par les autres scripts (`verify`, `report-tests`, captures) gardent 8000.

**Le contrôle reste obligatoire.** Deux missions peuvent tomber sur le même port, et un éditeur batch
peut tenir 8000. Avant toute inspection, appeler `get_session_snapshot` et comparer `project_dir` à ta
racine. S'il ne correspond pas, tu inspectes l'éditeur d'un autre agent : ne rien en conclure sur ton code.

Serveur injoignable = aucun éditeur ouvert, ou éditeur qui n'a pas démarré son serveur. Ce n'est jamais
une raison de tuer l'éditeur d'un autre agent.

Ce qui transite par ce plugin est « Licensed Technology » au sens de l'EULA Unreal (section 6(e)) :
l'avertissement de démarrage est attendu (`known-log-patterns.txt`).

## Éditeurs discrets — ne jamais lancer Unreal avec `Start-Process`

Alexandre travaille sur la même machine. Le 2026-09-29, les éditeurs des agents s'ouvraient au premier
plan devant lui ; il les fermait, et tuait sans le savoir la preuve en cours d'un agent (six fermetures,
dont deux `report-tests`). Signature dans le log : `Window '… - Unreal Editor' being destroyed` puis
`Cmd: QUIT_EDITOR`. Ce n'est ni un crash ni un test : relancer, ne pas chercher de régression.

Tout lancement d'Unreal passe donc par `Start-AnastasisEditor` (`editor-launch.ps1`), jamais par
`Start-Process` — `agent-worktree.ps1 finish` refuse la passation sinon (`Find-RawEditorLaunch`) :

- la première fenêtre s'affiche **sans prendre le focus** ;
- un gardien caché (`editor-window-guard.ps1`) envoie chaque fenêtre du processus hors de l'écran, au fond
  de la pile, y compris celles ouvertes plus tard (PIE, Journal des messages), et **rend le focus** à la
  fenêtre de l'utilisateur si l'éditeur le prend ;
- pas de minimisation : Slate ne dessine plus une fenêtre minimisée, et les captures en dépendent. Les
  captures gardent `bThrottleCPUWhenNotForeground=False`.

L'objet rendu est un `System.Diagnostics.Process` : `Wait-Process`, `HasExited`, `ExitCode`, `Modules`
fonctionnent comme avant. `ANASTASIS_EDITOR_VISIBLE=1` rétablit la fenêtre normale (pour regarder un
éditeur travailler, ou débloquer une boîte de dialogue). `ANASTASIS_EDITOR_GUARD_LOG=<fichier>` journalise
le gardien.

## Index de `tools/unreal/`

Chaque `.ps1` lance l'éditeur de **son** worktree (racine déduite de son chemin), discrètement (voir
ci-dessus), et pilote le `.py` associé. Un `.py` sans `.ps1` se lance dans un éditeur ouvert (`py <chemin>` en console) : son en-tête
dit comment. Sorties dans `Saved/SliceEvidence/` sauf mention contraire.

Opérateur et portails :

| Script | Rôle |
|---|---|
| `anastasis-unreal.ps1` | `status` / `build` / `build-game` / `verify` / `health` / `editor` |
| `agent-worktree.ps1` | cycle de vie multi-agent : `create` / `status` / `finish` / `integrate` / `prune` / `preflight` / `postflight` / `mcp` |
| `test-agent-worktree.ps1` | banc d'essai de `integrate` / `prune` sur un dépôt jetable (13 contrôles) ; à relancer après toute modification de `agent-worktree.ps1` |
| `mcp-port.ps1` | port MCP d'une racine, à dot-sourcer |
| `tools-index.ps1` | contrôle cet index contre le dossier, à dot-sourcer : `finish` bloque, `health` passe YELLOW |
| `editor-launch.ps1` | `Start-AnastasisEditor` : lancement d'Unreal sans focus, avec gardien, à dot-sourcer |
| `editor-window-guard.ps1` | gardien lancé par `Start-AnastasisEditor` : fenêtres hors écran, focus rendu |
| `report-tests.ps1` | suite `Anastasis`, classée PASS / KNOWN_EXPECTED_FAILURE / FAIL, refuse un run tronqué |
| `project-health.ps1` | rapport de santé des preuves (appelé par `health`) ; absent ou périmé ≠ PASS |
| `automation-log.ps1` | lecture de log d'automation partagée par les deux précédents, pas un point d'entrée |
| `scheduled-verify.ps1` | run nocturne (Planificateur de tâches) : `verify` puis `report-tests` |
| `smoke-pie.py` | smoke PIE lancé par `verify` |
| `known-expected-failures.txt` | registre KNOWN_EXPECTED_FAILURE — sur mandat seulement |
| `known-log-patterns.txt` | baseline des Error/Warning connus du log éditeur — idem |

Preuves visuelles et mesures (aucune n'écrit dans `Content/`, sauf mention) :

| Script | Rôle |
|---|---|
| `capture-slice.ps1` + `observe-slice.py` | capture viewport de `Lvl_AnastasisSlice` ; `-PreCmds` pour un A/B sur une seule CVar. `-RebuildMaterial 1` réécrit le matériau |
| `probe-demo.ps1` + `probe-demo.py` | preuve PIE : snapshot monde + capture par bookmark (défauts connus : `ATMOSPHERE_002.md`) |
| `asset_agent_probe.py` | preuve PIE partagée par les missions d'asset, un bookmark par run |
| `first-building-pie.py` | preuve PIE du premier bâtiment : pilote `Anastasis.Village.*` en console (puits, habitants, retraits), lecture par les lignes `ANASTASIS_VILLAGE` du log |
| `house-rest-pie.py` | preuve PIE de la maison : `Anastasis.Village.FirstHouse`, une nuit de sommeil, retrait d'un dormeur puis de la maison occupée |
| `granary-eat-pie.py` | preuve PIE du grenier (Noûs) : `Anastasis.Village.FirstGranary`, repas confirmés, stock qui baisse, démolition avec réservations en cours |
| `gather-deliver-pie.ps1` + `gather-deliver-pie.py` | preuve PIE du fermier au grenier : `Anastasis.Village.FirstFarmer`, recolte, retour, livraisons, conservation a chaque echantillon, une capture par etape → `Saved/SliceEvidence/gather-deliver/` ; aucun asset sauve |
| `food-supply-pie.py` | preuve PIE du circuit vivrier fini : prise, depot, repas, epuisement et conservation ; sortie via ANASTASIS_FOOD_OUT ; aucun asset sauvegarde |
| `hydro-network-capture.ps1` + `hydro-network-capture.py` | A/B du reseau de drainage (`anastasis.Terrain.Drainage 0/1`) : vues zenithale, oblique et gros plans, export de la grille relief + nappe et du reseau JSON, `-Debug 1..4` pour les lignes largeur / profondeur / vitesse / ordre → `Saved/HydroNetworkEvidence/<Label>/` |
| `shore-capture.ps1` + `shore-capture.py` | A/B visuel du bord d'eau, cadrage sur une rive |
| `capture-terrain-forge.ps1` + `terrain-forge-capture.py` | captures avant/après du relief → `Saved/TerrainForgeEvidence/` |
| `capture-terrain-relief.ps1` + `terrain-relief-capture.py` | avant/après d'une étape de la forge de relief (`-Step 1/2/3/scale`), dressing masqué → `Saved/TerrainReliefEvidence/` |
| `capture-horizon.ps1` + `capture-horizon.py` | A/B de l'horizon (`anastasis.Terrain.Horizon` 0 puis 1), cinq vues calées sur la carte, part de pixels « vide » par image ; `-PreCmds` pour l'étape brume → `Saved/HorizonEvidence/<Label>/` |
| `capture-sky.ps1` + `capture-sky.py` | ciel de `Lvl_AnastasisSlice` par états de CVars, mêmes caméras (oblique, fond de vallée, crête, contre-jour) : réalisme (`anastasis.Atmosphere.Realism`), heure et jour du ciel (`anastasis.Sky.Hour` / `Sky.Day`), météo (`anastasis.Sky.Weather`) → `Saved/SkyEvidence/<Label>/` |
| `sky-clock-pie.py` | preuve PIE de l'horloge du ciel : PIE sur `Lvl_AnastasisSlice` plus d'un jour de simulation, lignes `ANASTASIS_SKY` à chaque phase du village et échantillons `SKY_PIE_SAMPLE` (temps et phase de la simulation) ; `ANASTASIS_SKY_PIE_SECONDS` |
| `capture-places.ps1` + `places-capture.py` | lieux composés (`AnastasisPlaces`) : vue lointaine et vues à 1,7 m par lieu, lieux actifs puis coupés aux mêmes caméras (`-OnOnly`, `-All`) → `Saved/PlacesEvidence/<Label>/` |
| `capture-human-geography.py` | comparaison du relief corrige et de Human_Geography_V2 : export des maillages et vues a 170 cm ; sortie via ANASTASIS_HUMAN_EVIDENCE ; ferme l'editeur dedie |
| `capture-macro-forest.py` | A/B forestier sur Human_Geography_V2, ouverture par defaut, empreintes des instances et du terrain ; sortie via ANASTASIS_FOREST_OUT ; aucun asset sauve |
| `capture-forest-walk.py` | Vues forestieres fixes, tailles des LOD et empreinte des HISM ; ANASTASIS_FOREST_OUT requis, aucun asset sauve, ferme l'editeur dedie |
| `capture-reed-form.ps1` + `capture-reed-form.py` | comparaison des formes de roseaux dans une scene temporaire |
| `capture-shore-reeds.ps1` + `capture-shore-reeds.py` | comparaison de silhouettes et proportions de roseaux sur la rive |
| `capture-tree-lineup.ps1` + `capture-tree-lineup.py` | planche de stature de la grammaire d'arbres |
| `astral-observe.py` | A/B lumière du jour fixe, Ecology seule variable |
| `measure-tree-cost.ps1` + `measure-tree-cost.py` | triangles, LOD, instances HISM réellement soumis |
| `inspect_presentation_registry.py` | dump de `DA_AnastasisPresentation` |
| `introspect_geoscript.py` | docstrings des fonctions GeometryScript utilisées |

Sources d'autorité d'assets — **écrivent** dans `Content/`. Sauf mention, ils créent l'asset s'il
manque puis se contentent de le vérifier ; `*_REBUILD=1` le régénère et écrase toute retouche manuelle :

| Script | Asset |
|---|---|
| `observe-slice.py` | `M_AnastasisSlice`, `Lvl_AnastasisSlice` |
| `ground-material.ps1` + `.py` | `M_AnastasisGround`, `MI_AnastasisGround` |
| `shore-water.ps1` + `.py` | `M_AnastasisShoreWater` |
| `water-look.ps1` + `.py` | `M_AnastasisWater` (Single Layer Water, WATER_LOOK_001) |
| `presentation-registry.py` | `DA_AnastasisPresentation` |
| `atmosphere-profile.py` | `DA_AnastasisAtmosphere` |
| `create_tree_asset.py` | `SM_Tree_*`, `M_AnastasisVegetation` — régénérés à **chaque** run |
| `create-reed-form.py` | recette isolee de roseaux courbes ; cree les assets de la variante |
| `world-dressing-01.ps1` + `.py` | trois lieux composes sur Human Geography V2 ; Preview sans sauvegarde, Save ecrit les acteurs WD01 de Lvl_AnastasisSlice et MI_WeatheredStone, Verify relit la map ; preuves via -Out |
| `create_ruin_asset.py` | `SM_Ruin_Generic_01` |
| `set_presentation_meshes.py` | câble un mesh par archétype dans `DA_AnastasisPresentation` |
| `set_tree_grammar.py` | entrée FOREST du registre (variantes d'arbres) |
| `set_ruin_variant.py` | entrée Ruin — généralisé depuis par `set_presentation_meshes.py` |

Retoucher un de ces assets à la main dans l'éditeur ne survit pas au prochain rebuild : la valeur
se change dans le script.

### Refugee props 008

| Script | Role |
|---|---|
| `create-refugee-props.py` | Cree quatre accessoires de refugies et leur materiau dans RefugeeProps008. |
| `create-village-buildings.ps1` + `.py` | Forge le puits, la maison et le grenier dans VillageBuildings, meme matiere que les props. `-Rebuild` regenere. |
| `capture-refugee-props.ps1` + `capture-refugee-props.py` | Capture en studio les quatre objets, sans sauvegarder de niveau. |

### Camp shelter 009

| Script | Role |
|---|---|
| `create-camp-shelter.py` | Abri de toile statique, recette CampShelter009. |
| `capture-camp-shelter.ps1` + `capture-camp-shelter.py` | Trois vues de l'abri avec les accessoires existants, aucun niveau sauve. |

### Strate herbacee (GROUND_COVER_001)

| Script | Role |
|---|---|
| `create-ground-cover.ps1` + `create-ground-cover.py` | **ecrit** dans `Content/` : les trois touffes `SM_Grass_MeadowTall/MeadowShort/Sedge_01` (`/Game/Anastasis/GroundCover`) et `M_AnastasisGrass`, regeneres a chaque run ; editeur dedie qui se ferme |
| `capture-ground-cover.ps1` + `ground-cover-capture.py` | A/B de l'herbe aux memes cameras, `-States on,off,notint,noshadow,on2` (`notint` = memes touffes, sol non teinte) : prairie, riviere, lisiere, vallee B, oblique, aerien, hameau, hors vallee, lande ; frame p50/p95 et GPU par vue → `Saved/GroundCoverEvidence/<Label>/` |

### Population visuelle (VILLAGER_PNG_001)

Autorite : les planches `SourceArt/Characters/Sheets/serie-*.png` et `SourceArt/Characters/villager-population.json`
(qui est ou sur quelle planche, poses assises). Les habitants simules portent un portrait ; la simulation n'en sait rien.
Au lancement, `anastasis.Village.StartVillagers` (12) habitants autour du premier puits ; le premier scenario
`Anastasis.Village.First*` / `FoodSupply` remplace ce village. Voir `docs/unreal/VILLAGER_PNG_001.md`.

| Script | Role |
|---|---|
| `villager-png.py` | **hors editeur** (Python systeme, Pillow + numpy) : `sheets` (decoupe des planches → `Raw/` + `villager-extract.json`, statures mesurees), `prep` (→ `SourceArt/Characters/PNG/<Categorie>/`, canevas 512x1024 = 128x256 cm, pieds alignes), `board` (planches → `docs/visual/villager-png-001/`), `check` (ressemblance silhouette / visage par paire) |
| `import-villagers.ps1` + `import-villagers.py` | **ecrit** dans `Content/` : textures `/Game/Anastasis/Characters/PNG/<Categorie>/CHR_*` (BC7, sRGB, Character, Clamp, hors streaming, couverture alpha), `M_AnastasisVillager`, et `DA_AnastasisPresentation.Villagers` ; relit, verifie, refuse un materiau qui ne compile pas ; editeur dedie qui se ferme |
| `villager-lineup.ps1` + `villager-lineup.py` | planche dans Unreal (vrais acteurs `AnastasisVillagerVisual`) : population entiere puis par groupe, sur un arc, temoin 180 cm → `Saved/VillagerEvidence/<Label>/` ; niveau jamais sauve |
| `villager-pie.ps1` + `villager-pie.py` | preuve PIE : village du lancement sans commande, puis `FirstWell 12` (le remplace), `RemoveNpc`, `FirstFarmer 1` ; une carte par habitant, portraits distincts et du METIER simule de chacun → `Saved/VillagerEvidence/pie/` |

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

Un run qui échoue sans rapport évident avec ton changement : lire `docs/unreal/PIEGES_UNREAL.md` **avant**
de chercher une régression. Éditeur fermé de l'extérieur, mémoire vidéo saturée, capture sans focus,
build vert en non-unity : chaque piège y est rangé par sa signature dans le log.

## État

`PLAYER` reste **NOT_IMPLEMENTED**. Ne pas le commencer sans mandat explicite d'Alexandre.

Voir `ANASTASIS_CANONICAL_PROJECT.md` et `docs/unreal/UNREAL_CANONICAL_STATE.md`.
