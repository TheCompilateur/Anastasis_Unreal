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
| `create-ground-cover.ps1` + `create-ground-cover.py` | **ecrit** dans `Content/` : les trois touffes `SM_Grass_MeadowTall/MeadowShort/Sedge_01` (`/Game/Anastasis/GroundCover`) et `M_AnastasisGrass`, regeneres a chaque run ; huit autres familles (lande, sous-bois) et trois de fleurs sauvages (`SM_Grass_FlowerWarm/Cool/White_01`, WILDFLOWERS_001) ; `-Only flowers` ne regenere que les fleurs et garde le materiau et les autres touffes tels quels ; editeur dedie qui se ferme |
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
| `status` | tous les worktrees : modifications, avance/retard sur `main`, branches non intégrées, **verrou de `main`**, **missions prêtes pour le lot** et la commande `integrate-batch` à lancer. Le lot est **simulé d'avance** (`git merge-tree`, sans éditeur, ~1 s par commit) : `PRETES_POUR_LE_LOT::` ne garde que ce qui s'empile proprement sur `main` ; `LOT_SUIVANT::` = propre seule mais en conflit avec une mission du lot ; `A_REBASER::` = en conflit avec `main`, fichiers nommés (l'agent rebase puis `finish`) ; `DEJA_DANS_MAIN::` = versée par contenu (`prune`) |
| `finish -Mission <m>` | portail de fin : index `tools/unreal/` à jour, aucun Unreal lancé hors `Start-AnastasisEditor`, aucun éditeur encore ouvert sur le worktree, preuves déclarées (`PROOFS:` de la fiche) présentes au registre, **build seul** si la branche touche `Source/`, `Config/`, `Content/`, `Plugins/` ou le `.uproject` — **pas d'éditeur : la suite et les preuves PIE attendent le lot** (`TESTS::QUEUED`) ; `-Prove` lance la suite ici (ancien finish), `-Full` force build + suite même sans changement Unreal ; refuse de passer la main si du travail n'est pas commité ; marque le commit (`ANASTASIS_WORKTREES\.handoff\<m>.txt` : `<sha> proved|queued|nounreal`) |
| `mcp -Mission <m>` | (ré)enregistre le port MCP d'un worktree existant côté Claude Code |
| `integrate -Mission <m>` | rôle intégrateur, **une mission déjà prouvée** (`proved` / `nounreal`) : prend le verrou de `main`, avance rapide de **`main`** (jamais de la branche extraite du canonique), après avoir rejoué index et lancements Unreal sur l'arbre versé ; canonique hors `main` → copie de travail intacte. Refuse une mission `queued` (elle passe par le lot) et refuse pendant un lot (`MAIN_LOCK::TENU`). Si `main` a avancé : rejoue la branche sur `main` et la verse si la règle de retest le permet (point 6 ci-dessous), sinon `RETEST::REQUIS` |
| `integrate-batch -Missions a,b,c` | rôle intégrateur, **file groupée — la voie normale** : prend le verrou de `main` (personne ne la déplace pendant le lot), admet les missions dont `finish` a passé sur le commit actuel, les empile sur `main` dans le worktree d'intégration persistant `ANASTASIS_WORKTREES\_integration` (`ANASTASIS_INTEGRATION_DIR` le déplace si ce chemin devient inscriptible) (une mission en conflit est écartée, les autres passent), **un seul** portail (index, lancements, build + suite si le lot touche Unreal), puis **toutes les preuves PIE déclarées par le lot dans un seul éditeur** (`editor-batch.ps1` ; une preuve en échec désigne sa mission, `main` intact), puis avance rapide de `main`. Une mission qui porte les commits d'une autre non versée est refusée, sauf **relais déclaré** : sa fiche écrit `RELAIS: a, b` et son `PROOFS:` reprend toutes les preuves de ces fiches (`RELAY_ADMITTED::`) |

Sur mandat explicite de l'intégrateur, `integrate-batch -TestFilter <filtre>` remplace la suite complète par une sélection Unreal de **1 à 30 cas**. Le portail affiche `TEST_SCOPE::TARGETED` et `TESTS::TARGETED_PASS` ; ce verdict ne couvre que les cas sélectionnés. Build et preuves PIE déclarées restent obligatoires. Sans `-TestFilter`, la suite complète demeure la règle.
| `prune -Mission <m>` | après versement : worktree, branche et enregistrement MCP local supprimés ; refuse si un commit manque à `main` (par contenu : une copie versée par lot compte ; une copie au diff retouché par l'union de `proofs.txt` aussi, si rejouer la branche sur `main` ne change rien : `PRUNE::PAR_CONTENU`) ou si le worktree n'est pas propre |
| `preflight` | avant un `verify`/seal : dit ce qui bloque et **ouvre une fenêtre** d'observation |
| `postflight` | après : échoue si source, config ou `HEAD` ont bougé pendant la fenêtre |

Un `verify` ou un seal dont le `postflight` échoue ne prouve rien. Ne jamais le rapporter
comme une réussite.

### Une file, un éditeur (EDITOR_QUEUE_001) — pour tous les agents, Cursor comme Claude

Validé par Alexandre le 2026-10-01. La machine a 16 Go, un éditeur en prend 8 à 13 : **un seul
éditeur utile à la fois**. Quand chaque agent démarrait le sien pour se prouver, chacun attendait la
porte mémoire 8 à 14 min, puis 12 min de suite — et la preuve était perdue dès qu'un autre agent
avançait `main` pendant ces 25 min. Désormais :

1. **Ne démarre pas d'éditeur pour te prouver.** Travaille, build, commit, déclare dans ta fiche ce
   qu'il faudra rejouer — `PROOFS: <noms de tools/unreal/proofs.txt>` (ou `PROOFS: (aucune)`) — puis
   `finish`. Il compile et s'arrête là : `HANDOFF_READY::YES (queued)`.
2. **Une preuve PIE nouvelle s'inscrit au registre** `tools/unreal/proofs.txt` (nom, script, motif de
   réussite, motif d'échec, délai, variables). Pour la mettre au point, `editor-batch.ps1 -Proofs <nom>`
   dans ton worktree : un éditeur, ta preuve, puis il se ferme.
3. **L'intégrateur verse par lots** : `agent-worktree.ps1 status` donne `PRETES_POUR_LE_LOT::` et la
   commande ; `integrate-batch` prend le verrou de `main`, empile, compile une fois, lance la suite une
   fois, puis rejoue **toutes** les preuves du lot dans **un** éditeur. Mission en échec : désignée,
   écartée, le lot se relance sans elle.
4. **Personne d'autre ne déplace `main`.** Pendant un lot, `integrate` refuse (`MAIN_LOCK::TENU`). Ne
   jamais contourner par un `git merge` / `git fetch . x:main` à la main.
   **Un seul intégrateur (Alexandre, 2026-10-01)** : une seule session, désignée par Alexandre, lance
   `integrate` et `integrate-batch` ; aujourd'hui c'est une session Claude Code ouverte sur la racine canonique.
   **Tout autre agent — Codex, Cursor, Claude dans un worktree — s'arrête à `HANDOFF_READY::YES`**, même si on
   lui dit « intègre » ou « mets-le dans le jeu » : il le dit, et c'est l'intégrateur qui verse. Une mission prête
   se voit d'elle-même dans `status` (`PRETES_POUR_LE_LOT::`) ; ce qui doit être su avant le versement (ordre,
   dépendance entre branches, conflit connu, preuve fragile) s'écrit dans `INTEGRATION_RISK` de la fiche.
   Deux intégrateurs se volent le verrou et la base : leurs lots se refusent l'un l'autre (`main a bougé`)
   et chacun perd ses 20 minutes de portail.
5. **Regarder dans l'éditeur** (MCP, capture, réglage à l'œil) reste permis, un à la fois : `anastasis-unreal.ps1 editor`,
   puis fermer. Pas pendant qu'un lot tourne (`status` → `MAIN_LOCK`).
6. **Règle de retest (RETEST_RULE_001, adoptée par Alexandre le 2026-10-01)** : si les arbres git de `Source/`,
   `Config/`, `Content/`, `Plugins/` et du `.uproject` du commit marqué par `finish` sont identiques à ceux de
   l'arbre rebasé (`finish` après `git rebase main`) ou empilé (`integrate`, `integrate-batch`), ni build ni suite :
   `RETEST::SKIP (arbres Unreal identiques à <sha>)`, la preuve du commit marqué est **reprise**, ce n'est pas un
   nouveau PASS. Un seul fichier Unreal différent : portail complet (`RETEST::REQUIS` dans `integrate`). Seule
   une preuve `proved` couvre la suite ; `queued` ne couvre que le build. `integrate` rejoue désormais sur `main`
   une branche qui n'est plus en avance rapide, au lieu de la refuser, quand la règle le permet ; `-Full` la désactive.
7. **`finish -Prove`** (suite dans ton propre éditeur) est l'exception : quand Alexandre attend un verdict
   tout de suite, ou pour une mission que le lot ne peut pas juger.

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

**La racine canonique est toujours à jour, binaires compris (règle absolue d'Alexandre, 2026-10-07,
CANONICAL_FRESH_001).** Les lots compilent dans le worktree d'intégration ; sans recompilation du canonique,
l'éditeur d'Alexandre a chargé des DLL du 2 octobre alors que `main` était du 7. Donc :
- `integrate` et `integrate-batch` recompilent le canonique après chaque avance de `main` qui le rend
  périmé (`CANONICAL_BUILD::PASS`). Éditeur ouvert sur le canonique : `CANONICAL_BUILD::DIFFERE` avec son pid —
  le dire à Alexandre, recompiler dès qu'il l'a fermé. `CANONICAL_BUILD::FAIL` se traite avant tout autre lot.
- `status` affiche `CANONICAL_BINAIRES::A_JOUR` ou `PERIMES` ; `PERIMES` se corrige tout de suite par
  `tools\unreal\anastasis-unreal.ps1 build` dans la racine, éditeur fermé.

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

## Portage : un écart se déclare le jour où il entre

La migration absorbe d'abord la masse du simulateur JS, la stabilisation viendra ensuite. Elle ne
tient que si chaque divergence du harnais se range tout de suite : bug de portage, écart provisoire,
ou évolution voulue. Donc, dans `Source/AnastasisSim/` :

- tout comportement qui n'est pas la copie fidèle de la référence (branche sautée, repli inventé,
  flux aléatoire propre, extension) a une fiche dans `Source/AnastasisSim/ECARTS.md` **dans le
  commit qui l'introduit**, et la marque `ecart n°N` à l'endroit du code ;
- la passation a une section `## ECARTS` : numéros ouverts, modifiés, fermés, ou `AUCUN — <preuve>` ;
- seul Alexandre passe un écart à `ASSUME` (évolution définitive) ; un agent écrit `A_TRANCHER`.

`finish` le contrôle (`node tools/migration/check-ecarts.mjs`). Protocole complet :
`docs/migration/PROTOCOLE_ECARTS.md`. Avant d'enquêter sur une divergence du harnais :
`node tools/migration/check-ecarts.mjs -section <section>`.

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

## Procédures (skills)

Les procédures répétées sont écrites une fois, dans `.claude/skills/<nom>/SKILL.md`. Claude Code les charge
quand la tâche s'y prête ; tout autre agent (Codex…) les lit comme des documents ordinaires. Elles donnent
l'ordre et les décisions ; les règles restent ici, les pièges dans `docs/unreal/PIEGES_UNREAL.md`.

| Skill | Quand |
|---|---|
| `anastasis-mission` | avant de modifier un fichier du projet ; avant toute intégration ou tout push |
| `anastasis-editeur-mcp` | avant tout appel `mcp__unreal__*` : bon port, bon éditeur, bon groupe d'outils |
| `anastasis-capture` | quand le verdict est une image : A/B à une seule variable, regarder, mesurer (`compare.py`) |
| `anastasis-realisme` | avant toute mission visuelle (« plus réaliste », réglage de rendu, matériau, décor) et avant d'appliquer une recommandation tirée d'une recherche ou d'un tutoriel : comment Unreal fait, ce que le projet a décidé, comment le prouver |
| `anastasis-historicite` | avant toute revendication geographique ou historique sur la vallee pontique fictive post-1204 : modele causal, sources, hypotheses et brief verifiable |

Une procédure qui change (nouveau portail, nouveau script) se corrige dans son skill, dans le même commit.

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

### Porte mémoire — un éditeur ne démarre que si la machine peut le porter

La machine a 16 Go. Un éditeur en prend 8 à 13 Go. Le 2026-09-30, Windows a levé douze alertes « mémoire
virtuelle insuffisante » en une soirée, chaque fois avec deux ou trois éditeurs d'agents plus une
compilation : tout gelait, puis un éditeur mourait, et son agent cherchait une régression.

`Start-AnastasisEditor` attend donc, sous un verrou global à la machine, que les trois conditions tiennent :

| Condition | Défaut | Variable |
|---|---|---|
| éditeurs Unreal ouverts (`UnrealEditor`, `UnrealEditor-Cmd`, **celui d'Alexandre compris**) | < 2 | `ANASTASIS_EDITOR_MAX` |
| RAM disponible, dès qu'un éditeur est déjà ouvert | ≥ 3 Go | `ANASTASIS_EDITOR_MIN_RAM_GB` |
| marge avant la limite de mémoire engagée, idem | ≥ 8 Go | `ANASTASIS_EDITOR_MIN_COMMIT_GB` |

Le premier éditeur passe toujours : après un redémarrage, les sessions Claude et Cursor laissent à elles
seules moins d'1 Go de RAM disponible ; des seuils mémoire sur le premier éditeur bloqueraient tout le monde.

Pendant l'attente, une ligne `EDITOR_GATE::WAIT` par minute dit ce qui bloque. Au bout de 45 min
(`ANASTASIS_EDITOR_WAIT_MIN`) : `EDITOR_GATE::TIMEOUT`, levé comme une erreur. **C'est une machine saturée,
pas une régression** : relancer plus tard. Ne jamais fermer l'éditeur d'un autre pour passer.
`ANASTASIS_EDITOR_GATE=0` supprime la porte — sur demande d'Alexandre seulement.

Un éditeur interactif (`anastasis-unreal.ps1 editor`) ne se ferme pas seul : hors écran, il garde sa mémoire
après la mission et bloque la porte des autres. `finish` refuse donc la passation tant qu'un éditeur est
ouvert sur le worktree (`Find-WorktreeEditor`), et donne son pid.

## Index de `tools/unreal/`

Chaque `.ps1` lance l'éditeur de **son** worktree (racine déduite de son chemin), discrètement (voir
ci-dessus), et pilote le `.py` associé. Un `.py` sans `.ps1` se lance dans un éditeur ouvert (`py <chemin>` en console) : son en-tête
dit comment. Sorties dans `Saved/SliceEvidence/` sauf mention contraire.

Opérateur et portails :

| Script | Rôle |
|---|---|
| `sky-passage-pie.py` | relais crepusculaire : aube/coucher continus en PIE, A/B/A `anastasis.Sky.Passage`, exposition et contributions directes echantillonnees, images `Shot` et JSON dans `Saved/SkyPassageEvidence/` ; PASS instrumental, verdict visuel separe ; registre `sky-passage-pie` |
| `anastasis-unreal.ps1` | `status` / `build` / `build-game` / `verify` / `health` / `editor` |
| `agent-worktree.ps1` | cycle de vie multi-agent : `create` / `status` / `finish` / `integrate` / `integrate-batch` / `prune` / `preflight` / `postflight` / `mcp` |
| `test-agent-worktree.ps1` | banc d'essai de `finish` (saut sans changement Unreal, preuves déclarées), `integrate`, `integrate-batch`, verrou de `main`, règle de retest (arbres identiques / un `Source/` changé) et `prune` sur un dépôt jetable ; à relancer après toute modification de `agent-worktree.ps1` |
| `mcp-port.ps1` | port MCP d'une racine, à dot-sourcer |
| `tools-index.ps1` | contrôle cet index contre le dossier, à dot-sourcer : `finish` bloque, `health` passe YELLOW |
| `editor-launch.ps1` | `Start-AnastasisEditor` : lancement d'Unreal sans focus, avec gardien, derrière la porte mémoire, à dot-sourcer |
| `editor-window-guard.ps1` | gardien lancé par `Start-AnastasisEditor` : fenêtres hors écran, focus rendu |
| `report-tests.ps1` | suite `Anastasis`, classée PASS / KNOWN_EXPECTED_FAILURE / FAIL, refuse un run tronqué |
| `editor-batch.ps1` + `editor-batch.py` | plusieurs preuves PIE du registre dans **un seul** éditeur (EDITOR_QUEUE_001) : `-Proofs a,b` ; les preuves PIE et la première capture `*-capture` partagent un éditeur, chaque capture de plus a le sien (EDITOR_BATCH_SPLIT_001 : la 2e capture d'un même éditeur plante dans PythonScriptPlugin) ; le `quit_editor()` de chaque script passe au suivant, PIE arrêté et rythme (`TimeScale`, `Speed`, `Warp`) reposé entre deux ; verdict `PROOF::PASS/FAIL` par preuve → `Saved/EditorBatch/<horodatage>/` ; appelé par `integrate-batch` |
| `proofs.txt` | registre des preuves PIE rejouables en lot : nom, script, motif de réussite, motif d'échec, délai, variables ; une fiche les déclare par `PROOFS:` |
| `weather-materials.ps1` + `weather-materials.py` | regenere uniquement les materiaux existants de vegetation, herbe, eau, bois et roche avec la collection meteo commune ; ecrit Content/ |
| `weather-contract.py` | preuve editeur numerique des consommateurs de la collection meteo et des valeurs ecrites par Apply ; aucun asset sauve |
| `valley-air.ps1` + `valley-air.py` | diagnostic transitoire du voile a 14 h : deux poses, retrait independant fog global/perspective aerienne/brumes, temoin repete ; aucun asset sauve |
| `weather-visibility.py` | A/B de AirVisibility a 14 h et 7 h, memes poses vallee/rive, sans changer exposition ni brume locale ; via weather-reference.ps1 -Mode visibility |
| `weather-reference.ps1` + `weather-reference.py` | capture-sky aux memes cameras : 14 h avant/apres, matin humide, couvert ; completion technique seulement |
| `weather_materials.py` | aide partagee des autorites de materiaux : collection MPC_AnastasisWeather, vent monde, perturbation eau, humidite bornee |
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
| `npc-life-pie.py` | preuve PIE du village initial sans scénario : foyer, travail alimentaire partagé, besoins, sommeil, matériaux portés du monde au chantier sec puis maison attribuée à un bâtisseur |
| `material-courier-pie.py` | preuve PIE d'un chantier explicitement ouvert a sec : un porteur preleve bois/pierre du monde, les livre, et la maison se termine sans stock injecte |
| `first-building-pie.py` | preuve PIE du premier bâtiment : pilote `Anastasis.Village.*` en console (puits, habitants, retraits), lecture par les lignes `ANASTASIS_VILLAGE` du log |
| `house-rest-pie.py` | preuve PIE de la maison : `Anastasis.Village.FirstHouse`, une nuit de sommeil, retrait d'un dormeur puis de la maison occupée |
| `abandon-pie.py` | preuve PIE du vieillissement des maisons vides (ABANDON_001) : `FirstHouse 4`, `RemoveNpc` de tous, `Anastasis.Sim.Advance` 3 / 5 / 12 / 30 jours (3, 8, 20, 50 jours vides) ; lit le parametre `Neglect` du materiau dynamique pose sur le mesh et le compare a `NeglectForDays` ; temoin faux `anastasis.Village.Metabolism 2` (village qui ne vieillit pas) ; verdict `ABANDON_PIE PASS/FAIL` -> `Saved/AbandonEvidence/pie/abandon.json` ; au registre (`abandon-pie`) ; exige `M_VillageBuilding_Aged` (`create-building-aging.ps1`) ; aucun asset sauve |
| `metabolism-pie.py` | preuve PIE de la metabolisation des maisons (ICEBERG_001) : `Anastasis.Village.FirstHouse 4`, plein jour (foyer eteint), nuit avec dormeur (foyer allume), `RemoveNpc` de tous (le batiment reste, le foyer s'eteint), temoin faux `anastasis.Village.Metabolism 2` (la meme maison vide rallumee), retour a `1` ; lit le vrai composant de lumiere de l'acteur ; verdict `METABOLISM_PIE PASS/FAIL` → `Saved/MetabolismEvidence/pie/metabolism.json` ; au registre (`metabolism-pie`) ; aucun asset sauve |
| `granary-eat-pie.py` | preuve PIE du grenier (Noûs) : `Anastasis.Village.FirstGranary`, repas confirmés, stock qui baisse, démolition avec réservations en cours |
| `village-weather-pie.py` | preuve PIE de la météo des habitants : `Anastasis.Village.FirstFarmer 1`, orage par `Anastasis.Village.ForceWeather 0.9`, le fermier entre à l'abri puis en ressort, `ForceWeather off` ; temps accéléré (`anastasis.Sim.Warp 10`), verdict `VILLAGE_WEATHER PASS/FAIL` ; lecture par les lignes `VILLAGE_WEATHER_` et `ANASTASIS_VILLAGE` du log ; au registre (`village-weather-pie`) |
| `geo-remote-crisis-pie.py` | preuve PIE du monde exterieur (geopolitical-world-001, ecart n°38) : `Anastasis.Geo.Load` (scenario `Content/Anastasis/Scenario/geo-pontos-1204.json`), crise injectee a Konya (`Anastasis.Geo.Inject`), `Anastasis.Sim.Advance 1d` jour par jour ; PASS = rien au jour de l'injection, puis la nouvelle, puis la penurie au village, puis un groupe d'arrivants devenu habitants, trace jusqu'a la cause, `Anastasis.Geo.Save`/`Restore` identiques a chaud ; lecture par `get_geo_status` / `get_geo_trace` → `Saved/GeoEvidence/pie/geo-remote-crisis.json` ; au registre (`geo-remote-crisis-pie`) ; aucun asset sauve |
| `player-pie.ps1` + `player-pie.py` | preuve PIE du joueur minimal : `Anastasis.Player.Arrive`, attente (`idle`), marche par `Anastasis.Player.Move`, pawn pose sur le corps, `Anastasis.Player.Goal build` refuse (`hors-table`) puis `drink` (il va boire au puits), `Anastasis.Sim.Advance 7d` puis `1d` (presence, jours oisifs, personne ne le voit, reputation), `Release` ; lecture par `get_player_status` → `Saved/PlayerEvidence/pie/` ; aucun asset sauve |
| `gather-deliver-pie.ps1` + `gather-deliver-pie.py` | preuve PIE du fermier au grenier : `Anastasis.Village.FirstFarmer`, recolte, retour, livraisons, conservation a chaque echantillon, sac du retour livre en entier (au plus un sac plein : le fermier peut rentrer plus tot, reconsider-001), une capture par etape → `Saved/SliceEvidence/gather-deliver/` ; aucun asset sauve ; au registre (`gather-deliver-pie`) |
| `player-pie.ps1` + `player-pie.py` | preuve PIE du joueur minimal : `Anastasis.Player.Arrive`, attente (`idle`), marche par `Anastasis.Player.Move`, pawn pose sur le corps, `Anastasis.Sim.Advance 7d` puis `1d` (presence, jours oisifs, personne ne le voit, reputation), `Release` ; lecture par `get_player_status` → `Saved/PlayerEvidence/pie/` ; aucun asset sauve |
| `gather-deliver-pie.ps1` + `gather-deliver-pie.py` | preuve PIE du fermier au grenier : `Anastasis.Village.FirstFarmer`, recolte, retour, livraisons, conservation a chaque echantillon, une capture par etape → `Saved/SliceEvidence/gather-deliver/` ; aucun asset sauve |
| `build-site-pie.ps1` + `build-site-pie.py` | preuve PIE du chantier : `Anastasis.Village.FirstSite`, devis livre, batisseurs ; une capture a l'ouverture, aux fondations, aux murs, au toit et a l'achevement, devis et pieces controles a chaque echantillon → `Saved/SliceEvidence/build-site/` ; aucun asset sauve |
| `food-supply-pie.py` | preuve PIE du circuit vivrier fini : prise, depot, repas, epuisement et conservation ; sortie via ANASTASIS_FOOD_OUT ; aucun asset sauvegarde |
| `hydro-network-capture.ps1` + `hydro-network-capture.py` | A/B du reseau de drainage (`anastasis.Terrain.Drainage 0/1`) : vues zenithale, oblique et gros plans, export de la grille relief + nappe et du reseau JSON, `-Debug 1..4` pour les lignes largeur / profondeur / vitesse / ordre → `Saved/HydroNetworkEvidence/<Label>/` |
| `riverbank-capture.ps1` + `riverbank-capture.py` | l'eau et ses rives a 1,7 m, cameras tirees du reseau rendu (plaine calme, eau vive, confluence, ruisseau, rive de lac, mare, berge vue de 40 m) ; etats `water` / `flat` (cout de Single Layer Water), `banks` / `nobanks` (`anastasis.Dressing.Riverbank`) ; GPU p50 par vue, `-Profile` pour un ProfileGPU par vue → `Saved/RiverbankEvidence/<Label>/` |
| `contact-realism-capture.ps1` + `contact-realism-capture.py` | A/B de la peau de contact (`AAA_CONTACT_REALISM_001`, `anastasis.Contact.Realism` 1 / 0 / 1) : le monde est incarne, la couche rebatie, son plan ecrit (`plan.json`), puis la case de 40 m qui porte eau + arbres (+ rocher, roseaux) est elue ; vues `humain_arbre`, `rive_proche`, `rive_pierre`, `rocher`, `humain_20m`, `paysage_80m` aux memes cameras, ciel epingle (jour 1, 16 h 30) ; frame p50/p95 et GPU p50 par vue, `-Profile` pour un ProfileGPU par vue → `Saved/ContactRealismEvidence/<Label>/` ; aucun asset sauve |
| `shore-capture.ps1` + `shore-capture.py` | A/B visuel du bord d'eau, cadrage sur une rive |
| `capture-terrain-forge.ps1` + `terrain-forge-capture.py` | captures avant/après du relief → `Saved/TerrainForgeEvidence/` |
| `capture-terrain-relief.ps1` + `terrain-relief-capture.py` | avant/après d'une étape de la forge de relief (`-Step 1/2/3/scale`), dressing masqué → `Saved/TerrainReliefEvidence/` |
| `capture-horizon.ps1` + `capture-horizon.py` | A/B de l'horizon (`anastasis.Terrain.Horizon` 0 puis 1), cinq vues calées sur la carte, part de pixels « vide » par image ; `-PreCmds` pour l'étape brume ; `-Mode skyline` (CONTINENTAL_001) : huit vues à hauteur d'œil depuis le bassin, tous les 45°, pour juger la ligne de crête du continent → `Saved/HorizonEvidence/<Label>/` |
| `capture-sky.ps1` + `capture-sky.py` | ciel de `Lvl_AnastasisSlice` par états de CVars, mêmes caméras (oblique, fond de vallée, crête, contre-jour) : réalisme (`anastasis.Atmosphere.Realism`), heure et jour du ciel (`anastasis.Sky.Hour` / `Sky.Day`), météo (`anastasis.Sky.Weather`), humidité du ciel (`anastasis.Sky.Humidity`), diffusion du brouillard (`anastasis.Atmosphere.FogScattering`) ; GPU p50 de `stat unit` par image dans `sky.json` ; `-Preset cycle` = 06 09 12 16 18 20 00 03 × sec / humide / saturé → `Saved/SkyEvidence/<Label>/` |
| `eye-plane-capture.ps1` + `eye-plane-capture.py` | plan net à 1,7 m sur `Lvl_AnastasisSlice` : un premier plan qui coupe, un seul sujet au milieu, le lointain qui perd le contraste (`anastasis.Depth.EyePlane` 0 puis 1, même cadrage, heure 11) → `Saved/EyePlaneEvidence/<Label>/` |
| `atmosphere-metrics.py` | **hors éditeur** (Python système, Pillow + numpy) : mesures par capture de ciel (médiane, surexposition, mur de brouillard, voile, profondeur par tiers, drapeaux `WALL` / `HAZE` / `CLIPPED` / `BLACK`) et planches heure × humidité par vue, dans le dossier lu (`ATMOSPHERE_COHERENCE_001`) |
| `world-theatre-read.py` | releve du monde **execute** en PIE (WORLD_THEATRE_001) : `anastasis.Theatre.Read` rasterise sol et eau rendus (carte forgee + anneau, grilles 20 m / 200 m) et liste les objets poses par famille -> `Saved/WorldTheatreEvidence/read/` ; instrument, au registre (`world-theatre-read`) ; aucun asset sauve |
| `world-theatre-analyze.py` | **hors editeur** (Python systeme, numpy + Pillow) : lecture perceptuelle du releve -- derives (pente, courbure, TPI, cuvette), signatures anti-generatives mesurees, vistas canoniques (`docs/unreal/world-theatre-001/vistas.json`) evaluees et rendues en logiciel ; `--compose` plan de masses (avant / apres logiciel), `--emit-plan` ecrit `WorldTheatre/AnastasisWorldTheatrePlan.inl` |
| `world-theatre-capture.py` | vistas canoniques en PIE, `anastasis.Theatre` 0 / 1 / 0 (temoin) aux memes cameras, ciel epingle (11 h), simulation gelee, `Shot` -> `Saved/WorldTheatreEvidence/capture/` ; au registre (`world-theatre-capture`) ; COMPLETE = images ecrites, verdict visuel separe ; aucun asset sauve |
| `sun-angle.py` | pose `LightSourceAngle` (taille apparente de la source) sur toutes les lumières directionnelles du monde de l'éditeur et relit la valeur (`SUN_ANGLE_SET`) ; s'appelle dans un état de `capture-sky.ps1` : `py <chemin>/sun-angle.py 1.0` (`SUN_ANGLE_001`) |
| `tsr-flicker-pie.ps1` + `tsr-flicker-pie.py` | scintillement de l'herbe et des branches (`TSR_FLICKER_001`) : PIE, caméra fixe, vent actif, ciel épinglé à midi, simulation gelée ; N images successives par `Shot` (vue affichée, historique TSR compris) en prairie à 1,7 m, prairie au ras du sol, lisière ; états `d0` / `d1` / `d0b` de `r.TSR.ThinGeometryDetection` (d0b = témoin), `-States` pour un autre A/B → `Saved/TsrFlickerEvidence/<Label>/` ; aucun asset sauvé |
| `tsr-flicker-metrics.py` | **hors éditeur** (Python système, Pillow + numpy) : mesure des séquences de `tsr-flicker-pie.py` -- mouvement, scintillement (dérivée seconde temporelle, où le balancement régulier du vent s'annule), part de pixels qui clignotent, sur l'image et sur la géométrie fine ; carte du scintillement par séquence ; verdict par vue contre le témoin `d0b` → `metrics.json` |
| `anthropic-paths-capture.py` | Traces PNJ reelles, memoire figee, vues humaines off/on/off et mesures GPU ; registre anthropic-paths-capture ; aucun asset sauve |
| `anthropic-memory-pie.py` | preuve PIE experimentale des traces de deplacement PNJ : herbe modifiee puis couche effacee a off ; aucune preuve artistique, aucun asset sauve ; registre `anthropic-memory-pie` |
| `river-use-pie.py` | scenario PIE sans puits : PNJ autonome assoiffe, berge choisie sur section 2, trajectoire/cible/consommation/soif et maillages bruts exportes dans `Saved/RiverUseEvidence/` ; PASS local strict, UNKNOWN bloque ; registre `river-use-pie` |
| `lived-paths-capture.py` | trajet observe : cadrage sur herbe reellement modifiee, A/B/A Display sans effacer la memoire, retrait des habitants et recuperation apres 16 jours ; registre `lived-paths-capture`, sorties `Saved/LivedPathsEvidence`, verdict artistique separe |
| `lived-paths-capture.py` | trajet observe : cadrage sur herbe reellement modifiee, A/B/A `anastasis.Anthropic.Draw` sans effacer la memoire, retrait des habitants et recuperation apres 16 jours ; registre `lived-paths-capture`, sorties `Saved/LivedPathsEvidence`, verdict artistique separe |
| `geography-concordance-pie.py` | preuve de lecture eau simulee/rendue : partition des centres de tuiles, sections lac/riviere, JSON + SVG dans `Saved/GeographyConcordanceEvidence/` ; `INSTRUMENT_PASS` ne prouve ni concordance du monde ni trajet PNJ ; registre `geography-concordance-pie` |
| `terrain-access-pie.py` | diagnostic des trois chemins maison/eau/champ/grenier et du mouvement PNJ sur le relief rendu ; INSTRUMENT_PASS ne prouve pas leur coherence ; registre terrain-access-pie |
| `settlement-site-pie.py` | trois tests cibles, comparaison ancien site / meilleur site, implantation PIE reelle et trois captures (sol, oblique, ancien terrain) ; sortie `Saved/SettlementSiteEvidence/` ; registre `settlement-site-pie` |
| `village-fabric-pie.py` | preuve PIE du tissu du village (VILLAGE_FABRIC_001) : `Anastasis.Village.Hamlet 6 0` (a defaut `FirstWell 0` + `FirstHouse 0`), lit `AnastasisVillageFabricLibrary.get_village_fabric_status` ; seuils tous relies a la placette, aucune ruelle sur un corps, A/B `anastasis.Village.FabricClear 0/1` (herbe rendue puis meme defrichement), `Anastasis.Village.FabricRebuild` et `Fabric 0/1` a signature identique ; verdict `VILLAGE_FABRIC PASS/FAIL` -> `Saved/VillageFabricEvidence/pie/fabric.json` ; au registre (`village-fabric-pie`) ; aucun asset sauve |
| `village-fabric-capture.py` | captures du tissu du village (VILLAGE_FABRIC_001) : meme hameau que `village-fabric-pie.py`, ciel epingle 16 h 30, debug coupe, cameras calees sur le tissu (oblique sur la placette, ruelle la plus longue a 1,7 m, bord de placette a 1,7 m), puis oblique et ruelle sous `anastasis.Village.Fabric 0` (temoin A/B) ; `Shot` -> `Saved/VillageFabricEvidence/capture/` ; `VILLAGE_FABRIC_CAPTURE COMPLETE` = images ecrites, pas un verdict visuel ; au registre (`village-fabric-capture`) ; aucun asset sauve |
| `sky-clock-pie.py` | preuve PIE de l'horloge du ciel : PIE sur `Lvl_AnastasisSlice` plus d'un jour de simulation, lignes `ANASTASIS_SKY` à chaque phase du village et échantillons `SKY_PIE_SAMPLE` (temps et phase de la simulation, toutes les 5 s simulées) ; temps accéléré : un jour et quart simulé à `Warp 4` (~30 s), verdict `SKY_PIE PASS/FAIL` (durée et six phases vues) ; `ANASTASIS_SKY_PIE_SIM_SECONDS` / `_WARP` / `_SECONDS` (plafond réel) ; au registre (`sky-clock-pie`) |
| `soundscape-pie.py` | preuve PIE du son causal : coups de chantier observes, silence des gestes a simulation figee, extinction des voix a off, budgets voix/PCM ; registre `soundscape-pie` ; ne prouve ni l'audibilite ni la qualite artistique, aucun asset sauve |
| `sky-clock-pie.py` | preuve PIE de l'horloge du ciel : PIE sur `Lvl_AnastasisSlice` plus d'un jour de simulation, lignes `ANASTASIS_SKY` à chaque phase du village et échantillons `SKY_PIE_SAMPLE` (temps et phase de la simulation, toutes les 5 s simulées) ; temps accéléré : un jour et quart simulé à `Warp 4` (~30 s), verdict `SKY_PIE PASS/FAIL` (durée et six phases vues) ; `ANASTASIS_SKY_PIE_SIM_SECONDS` / `_WARP` / `_SECONDS` (plafond réel) ; au registre (`sky-clock-pie`) |
| `capture-places.ps1` + `places-capture.py` | lieux composés (`AnastasisPlaces`) : vue lointaine et vues à 1,7 m par lieu, lieux actifs puis coupés aux mêmes caméras (`-OnOnly`, `-All`) → `Saved/PlacesEvidence/<Label>/` |
| `places-lifecycle.py` | preuve en éditeur de la réutilisation des HISM des lieux composés : quatre incarnations actif/actif/coupé/retour, mêmes composants et nombre d'instances → `Saved/PlacesLifecycleEvidence/` ; registre `places-lifecycle` |
| `capture-human-geography.py` | comparaison du relief corrige et de Human_Geography_V2 : export des maillages et vues a 170 cm ; sortie via ANASTASIS_HUMAN_EVIDENCE ; ferme l'editeur dedie |
| `capture-macro-forest.py` | A/B forestier sur Human_Geography_V2, ouverture par defaut, empreintes des instances et du terrain ; sortie via ANASTASIS_FOREST_OUT ; aucun asset sauve |
| `capture-forest-walk.py` | Vues forestieres fixes, tailles des LOD et empreinte des HISM ; ANASTASIS_FOREST_OUT requis, aucun asset sauve, ferme l'editeur dedie |
| `capture-reed-form.ps1` + `capture-reed-form.py` | comparaison des formes de roseaux dans une scene temporaire |
| `capture-shore-reeds.ps1` + `capture-shore-reeds.py` | comparaison de silhouettes et proportions de roseaux sur la rive |
| `capture-tree-lineup.ps1` + `capture-tree-lineup.py` | planche de stature de la grammaire d'arbres ; `-Set species [-Shape 01..03]` : planche des sept essences a leur hauteur mediane |
| `astral-observe.py` | A/B lumière du jour fixe, Ecology seule variable |
| `measure-tree-cost.ps1` + `measure-tree-cost.py` | triangles, LOD, instances HISM réellement soumis |
| `vegetation-cost-capture.ps1` + `vegetation-cost-capture.py` | coût GPU de la végétation strate par strate (`FOREST_COST_001`) : monde incarné une fois, arbres / sous-bois / herbe / rives masqués à l'exécution (ni dessin ni ombre), aux mêmes caméras (intérieur de forêt, lisière, prairie, vallée B, oblique, aérien) ; états `all,notrees,nounder,nograss,bare,all2` (all2 = témoin), GPU p50 de `stat unit` par vue, inventaire des instances par strate → `Saved/VegetationCostEvidence/<Label>/` ; rien de sauvé |
| `inspect_presentation_registry.py` | dump de `DA_AnastasisPresentation` |
| `introspect_geoscript.py` | docstrings des fonctions GeometryScript utilisées |

Sources d'autorité d'assets — **écrivent** dans `Content/`. Sauf mention, ils créent l'asset s'il
manque puis se contentent de le vérifier ; `*_REBUILD=1` le régénère et écrase toute retouche manuelle :

| Script | Asset |
|---|---|
| `observe-slice.py` | `M_AnastasisSlice`, `Lvl_AnastasisSlice` |
| `ground-material.ps1` + `.py` | `M_AnastasisGround`, `MI_AnastasisGround`, les huit `T_Ground_*` de `Materials/GroundTextures` (import si absentes ; `-ReimportTextures` les reimporte) |
| `ground-textures.py` | **Python systeme, hors Unreal**, a lancer avant `ground-material.ps1` : telecharge les quatre textures CC0 Poly Haven du sol et les empaquette (detail neutre en moyenne) dans `Saved/GroundTextures/packed` ; n'ecrit pas dans `Content/` |
| `shore-water.ps1` + `.py` | `M_AnastasisShoreWater` |
| `water-look.ps1` + `.py` | `M_AnastasisWater` (Single Layer Water, WATER_LOOK_001) |
| `rain-material.ps1` + `.py` | `M_AnastasisRain` et `SM_AnastasisRainStreak` (`/Game/Anastasis/Weather`, RAIN_001) : stries de pluie placées en HLSL autour de la caméra, translucide éclairé ; `-Rebuild` régénère |
| `presentation-registry.py` | `DA_AnastasisPresentation` |
| `atmosphere-profile.py` | `DA_AnastasisAtmosphere` |
| `create_tree_asset.py` | `SM_Tree_*` (grille pontique + 7 essences x 3 formes : pin d'Alep, cypres, chene vert, olivier, platane, pin noir, sapin de Cephalonie), `SM_Shrub_*` (lentisque, chene kermes, genet, ronce x 3), `M_AnastasisVegetation`, `M_AnastasisBark`, `M_AnastasisRock` — régénérés à **chaque** run ; `ANASTASIS_TREE_MATERIALS_ONLY=1` ne réécrit que les trois matériaux |
| `far-terrain-material.ps1` + `far-terrain-material.py` | **ecrit** `/Game/Anastasis/Materials/M_AnastasisFarTerrain` : le materiau des montagnes de l'anneau d'horizon au-dela de 8 km (la couleur de sommet est l'albedo, variation de valeur macro) ; M_AnastasisGround ne laissait lire ni foret, ni roche, ni neige a cette distance (CONTINENTAL_001) ; editeur dedie qui se ferme |
| `create-hero-trees.ps1` + `create-hero-trees.py` | **ecrit** `/Game/Anastasis/Vegetation/Hero/` : quatre heros a taille reelle (pin d'Alep, cypres, chene vert, olivier) et `SM_CanopyShell` ; ne touche pas `SM_Tree_*` ni les materiaux |
| `poly-realism.ps1` | enchaîne `ground-material.ps1 -Rebuild` puis les trois matériaux d'arbres et de pierre (`ANASTASIS_TREE_MATERIALS_ONLY=1`) : grain du sol éteint vers 40 m, trait d'eau, couronne, écorce et roche |
| `create-reed-form.py` | recette isolee de roseaux courbes ; cree les assets de la variante |
| `world-dressing-01.ps1` + `.py` | trois lieux composes sur Human Geography V2 ; Preview sans sauvegarde, Save ecrit les acteurs WD01 de Lvl_AnastasisSlice et MI_WeatheredStone, Verify relit la map ; preuves via -Out |
| `human-occupation-001.ps1` + `.py` | micro-implantation dans le bassin habitable ; écrit `Lvl_HumanOccupation`, `M_HO_Tread` et `M_HO_Building`, ecarte l'herbe sur les tags HO01, ne sauvegarde pas `Lvl_AnastasisSlice` ; preuves via -Out |
| `create-building-aging.ps1` + `.py` | `M_VillageBuilding_Aged` (`/Game/Anastasis/VillageBuildings`, ABANDON_001) : le grain de `M_VillageBuilding_Surface` plus le parametre `Neglect` (ternit, grise le bois, mousse sur les faces tournees vers le ciel) ; applique par l'acteur maison seulement quand la simulation dit la maison vide ; cree s'il manque, `-Rebuild` regenere ; ne touche ni mesh ni autre materiau |
| `aaa-contact-realism.ps1` + `.py` | **ecrit** `/Game/Anastasis/AAAContactRealism/` : `M_ACR_Decal` (maitre de decalque DBuffer couleur + normale + rugosite, tout en HLSL, sans texture) et dix `MI_ACR_<famille>` (WetBand, Mud, StoneWet, ReedBed, Litter, ContactDark, RockDirt, Deposit, Depression, Streak) ; instances reecrites a chaque lancement, `-Rebuild` regenere aussi le maitre |
| `create_ruin_asset.py` | `SM_Ruin_Generic_01` |
| `set_presentation_meshes.py` | câble un mesh par archétype dans `DA_AnastasisPresentation` |
| `set_tree_grammar.py` | entrée FOREST du registre (variantes d'arbres, essences et hauteurs reelles) ; apres `create_tree_asset.py` |
| `set_ruin_variant.py` | entrée Ruin — généralisé depuis par `set_presentation_meshes.py` |

Retoucher un de ces assets à la main dans l'éditeur ne survit pas au prochain rebuild : la valeur
se change dans le script.

### Refugee props 008

| Script | Role |
|---|---|
| `create-refugee-props.py` | Cree quatre accessoires de refugies et leur materiau dans RefugeeProps008. |
| `create-village-buildings.ps1` + `.py` | Forge le puits, la maison et le grenier dans VillageBuildings, meme matiere que les props. `-Rebuild` regenere. |
| `capture-refugee-props.ps1` + `capture-refugee-props.py` | Capture en studio les quatre objets, sans sauvegarder de niveau. |

### Architecture du village (ARCHITECTURE_SCALE_001)

Convention d'echelle et regles chiffrees : `docs/unreal/ARCHITECTURE_SCALE_001.md`. Une tuile de simulation fait 20 m ;
une maisonnee remplit sa parcelle (corps + assise terrassee). Catalogue fonctionnel : `Village/AnastasisArchitecture.*`.

| Script | Role |
|---|---|
| `create-village-architecture.ps1` + `create-village-architecture.py` | **ecrit** `/Game/Anastasis/VillageArchitecture` : sept maisonnees a l'echelle humaine (`SM_Arch_*` + `_Footing` : maison pauvre, moyenne, ferme, grenier, puits, atelier, chapelle), le kit de 23 pieces (`Kit/SM_Kit_*`) et `M_AnastasisArchitecture` (classes de materiau par l'alpha du sommet, usure physique, `Neglect`) ; collision complexe (on entre) ; meshes reecrits a chaque run, `-Rebuild` regenere aussi le materiau ; sans editeur, `python create-village-architecture.py` valide l'echelle (ARCH-01..10) et ecrit `docs/unreal/architecture/architecture-kit-001.json` |
| `architecture-preview.py` | **hors editeur** (Python systeme, Pillow + numpy) : rend chaque maisonnee en z-buffer logiciel avec des silhouettes de 170 cm (trois-quarts a hauteur d'oeil, oblique, arriere, porte, coupe) en secondes -> `Saved/ArchitecturePreview/` ; juge proportions et silhouettes, pas la matiere |
| `architecture-pie.ps1` + `architecture-pie.py` | preuve PIE : `Anastasis.Village.Hamlet` AVANT (`anastasis.Village.Architecture 0`, anciens meshes) puis APRES, memes cameras ; chaque batiment porte son archetype et son assise, terrasse < 480 cm, deux typologies au moins ; mannequins au corps des habitants devant, dans la porte, a l'interieur ; 13 prises -> `Saved/ArchitectureEvidence/pie/` ; au registre (`architecture-pie`) ; rien n'est sauve |

### AAA visual lab

| Script | Role |
|---|---|
| `aaa-visual-lab.ps1` + `.py` | LookDev isole `/Game/Anastasis/LookDev/AAA_Lab` : materiau maitre, zone 30 m, trois cameras, audit meshes en lecture seule. Ne sauve pas la map du jeu ni les materiaux existants. |

### Camp shelter 009

| Script | Role |
|---|---|
| `create-camp-shelter.py` | Abri de toile statique, recette CampShelter009. |
| `capture-camp-shelter.ps1` + `capture-camp-shelter.py` | Trois vues de l'abri avec les accessoires existants, aucun niveau sauve. |

### Strate herbacee (GROUND_COVER_001)

| Script | Role |
|---|---|
| `create-ground-cover.ps1` + `create-ground-cover.py` | **ecrit** dans `Content/` : les trois touffes `SM_Grass_MeadowTall/MeadowShort/Sedge_01` (`/Game/Anastasis/GroundCover`) et `M_AnastasisGrass`, regeneres a chaque run ; editeur dedie qui se ferme |
| `capture-ground-cover.ps1` + `ground-cover-capture.py` | A/B de l'herbe aux memes cameras, `-States on,off,notint,noshadow,on2` (`notint` = memes touffes, sol non teinte) : prairie, riviere, lisiere, vallee B, oblique, aerien, hameau, hors vallee, lande ; frame p50/p95 et GPU par vue ; `-States on,on_notex,bare,bare_notex` fait l'A/B des textures photo du sol (GROUND_TEXTURE_001) ; `-States eco,noeco` laisse l'herbe et compare la micro-ecologie seule (MICRO_ECOLOGY_001) → `Saved/GroundCoverEvidence/<Label>/`  Naturalisation : etats `reference,natural,reference2`, `ANASTASIS_GROUND_VIEWS` limite aux vues choisies ; controle des hauteurs echantillonnees et du retour au temoin. Ecotone : ecotone_reference,ecotone,ecotone_reference2 ; transect proche ecotone_open/edge/inside/oblique, reference figee sur un arbre reel. Woodland : woodland_reference,woodland,woodland_reference2 ; meme transect plus woodland_gap, controle integral des positions MicroEco hors foret. |

### Population visuelle (VILLAGER_PNG_001)

Autorite : les planches `SourceArt/Characters/Sheets/serie-*.png` et `SourceArt/Characters/villager-population.json`
(qui est ou sur quelle planche, poses assises). Les habitants simules portent un portrait ; la simulation n'en sait rien.
Au lancement, `anastasis.Village.StartVillagers` (12) habitants autour du premier puits ; le premier scenario
`Anastasis.Village.First*` / `FoodSupply` remplace ce village. Voir `docs/unreal/VILLAGER_PNG_001.md`.

**Rythme** : `anastasis.Sim.TimeScale` (0.0375 par defaut) ralentit le temps simule -- jour ~40 min, habitants
~3 m/s. Une preuve PIE qui attend sur le temps simule pose `anastasis.Sim.TimeScale 1` avant le PIE.
`anastasis.Sim.Speed 0` ne gele rien (`PumpFrame` lit < 1 comme 1) : geler par `TimeScale 0`.
Pour aller plus vite sans toucher a ce rythme : section « Temps accelere » ci-dessous.

| Script | Role |
|---|---|
| `villager-png.py` | **hors editeur** (Python systeme, Pillow + numpy) : `sheets` (decoupe des planches → `Raw/` + `villager-extract.json`, statures mesurees), `prep` (→ `SourceArt/Characters/PNG/<Categorie>/`, canevas 512x1024 = 128x256 cm, pieds alignes), `board` (planches → `docs/visual/villager-png-001/`), `check` (ressemblance silhouette / visage par paire), `colours` (teintes du corps 3D mesurees sur chaque PNG : vetement, peau, tete → `SourceArt/Characters/villager-colours.json` + planche `docs/visual/villager-body-3d-001/`) |
| `import-villagers.ps1` + `import-villagers.py` | **ecrit** dans `Content/` : textures `/Game/Anastasis/Characters/PNG/<Categorie>/CHR_*` (BC7, sRGB, Character, Clamp, hors streaming, couverture alpha), `M_AnastasisVillager`, et `DA_AnastasisPresentation.Villagers` (avec les teintes `Body*` de `villager-colours.json`) ; relit, verifie, refuse un materiau qui ne compile pas ; editeur dedie qui se ferme |
| `villager-lineup.ps1` + `villager-lineup.py` | planche dans Unreal (vrais acteurs `AnastasisVillagerVisual`) : population entiere puis par groupe, sur un arc, temoin 180 cm → `Saved/VillagerEvidence/<Label>/` ; niveau jamais sauve |
| `villager-pie.ps1` + `villager-pie.py` | preuve PIE : village du lancement sans commande, puis `FirstWell 12` (le remplace), `RemoveNpc`, `FirstFarmer 1` ; une carte par habitant, portraits distincts et du METIER simule de chacun → `Saved/VillagerEvidence/pie/` |

### Corps 3D des habitants (VILLAGER_BODY_3D_001)

De pres, chaque habitant est un mannequin anime (Manny / Quinn, `BS_Idle_Walk_Run`) habille par son materiau ;
au-dela de `anastasis.Village.BodyDistance` (80 m), sa carte portrait. `anastasis.Village.Bodies` 0 = cartes
seules, 1 = corps de pres (defaut), 2 = corps partout. Voir `docs/unreal/VILLAGER_BODY_3D_001.md`.

| Script | Role |
|---|---|
| `create-villager-body.ps1` + `create-villager-body.py` | **ecrit** dans `Content/` : `M_AnastasisVillagerBody` (tenue peinte sur la pose de liaison), regenere a chaque run ; mesure d'abord les os de Manny et Quinn et en tire les seuils (cou, ceinture, manches, chevilles) ; editeur dedie qui se ferme |
| `villager-body-pie.ps1` + `villager-body-pie.py` | preuve PIE au rythme du jeu, sans gel : corps monte pour chaque habitant, pieds d'un marcheur qui balaient > 20 cm, cap qui suit la marche, main et tete qui bougent a l'arret, bascule corps / carte a la distance ; prises de profil en rafale, face, groupe, lointain → `Saved/VillagerEvidence/body/` |

## Temps accelere (TIME_WARP_001)

Par-dessus `TimeScale` et `Speed`, pour le joueur comme pour les agents. Detail : `docs/unreal/TIME_WARP_001.md`.

| Pour | Comment |
|---|---|
| attendre la nuit, un jour, une semaine dans une preuve | `Anastasis.Sim.Advance @22` / `6h` / `3d` / `45` : la simulation saute dans la frame, ligne `ANASTASIS_SIM advance` au log |
| regarder le village vivre plus vite | `anastasis.Sim.Warp 8` (0 = pause, jusqu'a 1000) ; en PIE **`8` accelere, `9` ralentit** (rangee des chiffres ou pave numerique ; aussi pave `+` / `-`), `Pause` |
| un editeur batch deja accelere | `-dpcvars=anastasis.Sim.Warp=64` ; `anastasis.Sim.WarpBudgetMs 0` si le debit prime sur les images |
| lire l'etat | `Anastasis.Sim.TimeStatus`, ou `AnastasisSimulationDebugLibrary.get_time_warp_status` (JSON) |

Le pas ne depasse jamais 10 x FixedDt (la reference) : c'est le nombre de pas par frame qui monte. `Warp 1` reprend
exactement le chemin `PumpFrame`.

**Regle (Alexandre, 2026-10-01), pour tous les agents, Cursor comme Claude : une preuve n'attend pas le temps
simule, elle l'avance.** Toute preuve qui attend une heure, la nuit ou des jours de simulation le fait par
`Anastasis.Sim.Advance` (saut) ou `anastasis.Sim.Warp` (acceleration, delais gardes en temps simule) — jamais par
`TimeScale 1` + attente murale. Seules exceptions : une preuve qui mesure le pas fin de 1/60 s, ou une comparaison
bit a bit avec la reference JS (`PumpFrame`). Une preuve qui accelere remet `anastasis.Sim.Warp 1` en partant ;
au lot, `editor-batch` repose de toute facon le rythme entre deux preuves. Exemples : `village-weather-pie.py`
(Warp 10), `sky-clock-pie.py` (Warp 4, un jour et quart en ~30 s), `player-pie.py` (`Advance 7d`).

Sans joueur incarne, accelerer ne coute rien. Avec un joueur (`Anastasis.Player.Arrive`), le temps accelere est du
temps ou il ne fait rien aux yeux du village : sa presence s'efface, ses jours oisifs comptent dans sa reputation.
Une preuve qui accelere ne doit donc pas incarner de joueur, sauf si c'est ce qu'elle mesure.

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

`PLAYER` est **minimal** (player-minimal-001, mandat d'Alexandre du 2026-10-01, `docs/unreal/PLAYER_MINIMAL_001.md`) :
le joueur est un habitant de la simulation (`FVillage::PlayerPersonId`, comme la reference), qui attend sans
commande, marche a la main, et que le village voit ou oublie selon l'usage du temps accelere. Depuis
player-goals-001 (mandat d'Alexandre, 2026-10-01), il **choisit son but** dans la table de Nous (touches 1 a 5,
`Anastasis.Player.Goal`), sous les memes verrous, avec un refus motive. La parole dirigee et le mode visuel
`PLAYER` du GameMode restent **NOT_IMPLEMENTED** : ne pas les etendre sans mandat explicite d'Alexandre.

Voir `ANASTASIS_CANONICAL_PROJECT.md` et `docs/unreal/UNREAL_CANONICAL_STATE.md`.
