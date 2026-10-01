---
name: anastasis-mission
description: Cycle complet d'une mission ANÁSTASIS Unreal, du worktree au push — create, build, travail, fiche de passation, finish, rebase quand main bouge, integrate, push, prune. À charger dès qu'on va modifier un fichier du projet, et avant toute intégration ou tout push.
---

# Mission ANÁSTASIS : du worktree au push

Règles de fond : `AGENTS.md` (racine). Ce skill donne l'ordre et les décisions, pas les règles.
Devant un échec sans rapport évident avec ton changement : `docs/unreal/PIEGES_UNREAL.md` d'abord.

## 1. Ouvrir

```powershell
cd C:\dev\ANASTASIS_UNREAL
tools\unreal\agent-worktree.ps1 create -Mission <mission>     # minuscules, chiffres, . _ -
cd C:\dev\ANASTASIS_WORKTREES\<mission>
tools\unreal\anastasis-unreal.ps1 build                         # premier build complet : 3 à 11 min
```

- Rien n'est écrit dans `C:\dev\ANASTASIS_UNREAL`. Jamais.
- `create` affiche parfois une erreur git sous PowerShell 5.1 alors qu'il a réussi : vérifier
  `git worktree list` avant de relancer.
- Une nouvelle session Claude Code ouverte dans le worktree parle à l'éditeur de CE worktree (port MCP
  local). Voir le skill `anastasis-editeur-mcp`.

## 2. Travailler

- Unreal se lance par `Start-AnastasisEditor`, jamais `Start-Process` (Alexandre travaille sur la même
  machine ; `finish` refuse sinon).
- Un nouveau script dans `tools/unreal/` → une ligne dans l'index d'`AGENTS.md` (`finish` refuse sinon).
- Commits : message dans un fichier, `git commit -F <fichier>` (les `"` cassent `-m` sous PS 5.1).

## 3. Passer la main

1. Fiche `docs/unreal/handoffs/<mission>.md` depuis `_TEMPLATE.md` : MISSION, FILES_OWNED, COMMIT, MEC
   (commandes et **valeurs** obtenues), SCN, PLY, INTEGRATION_RISK, STOP (ce que tu ne revendiques pas).
2. Tout commiter.
3. `tools\unreal\agent-worktree.ps1 finish -Mission <mission>` → attendre `HANDOFF_READY::YES`.
   Rapporter PASS / KNOWN_EXPECTED_FAILURE / FAIL séparément, jamais un total « vert ».
   Une branche qui ne touche ni `Source/`, ni `Config/`, ni `Content/`, ni `Plugins/`, ni le `.uproject`
   passe sans build ni tests (`BUILD::SKIP TESTS::SKIP`) : le dire tel quel, ce n'est pas un PASS.
   `finish` marque le commit prouvé : un commit ajouté ensuite exige un nouveau `finish`.
4. Plusieurs agents en parallèle : **ne pas intégrer soi-même**. S'arrêter à `HANDOFF_READY::YES` et
   donner le nom de la mission à l'intégrateur (section 4).

Si `finish` échoue :

| Ce que tu vois | Quoi faire |
|---|---|
| `MISSING` / `STALE` | mettre l'index d'`AGENTS.md` à jour |
| `Unreal lance sans Start-AnastasisEditor` | convertir la ligne citée |
| `TESTS::FAIL lanceur bloque`, ou `RUN_INCOMPLET` | lire le log avant tout : `PIEGES_UNREAL.md` (éditeur fermé, VRAM, machine saturée) |
| un test marqué sort `Fail` | lire la sortie du test lui-même ; ne jamais toucher au marqueur ni au registre |
| machine saturée (RAM libre < 4 Go, éditeur > 5 min au boot) | attendre une fenêtre calme, relancer ; ne pas tuer l'éditeur d'un autre |
| `editeur Unreal encore ouvert sur ce worktree` | c'est le tien : `quit_editor()` par MCP ou `Stop-Process -Id <pid>`, puis relancer `finish` |
| `EDITOR_GATE::WAIT` / `EDITOR_GATE::TIMEOUT` | porte mémoire (`AGENTS.md`) : trop d'éditeurs ou de RAM prise ; relancer plus tard, jamais `ANASTASIS_EDITOR_GATE=0` sans mandat |

## 4. Verser (rôle intégrateur, sur demande d'Alexandre)

**File groupée, d'abord.** Une seule session intègre ; elle verse toutes les missions prêtes d'un coup :

```powershell
cd C:\dev\ANASTASIS_UNREAL
tools\unreal\agent-worktree.ps1 integrate-batch -Missions mission-a,mission-b,mission-c
```

- Admises : les missions dont `finish` a passé sur leur commit actuel. Les autres sont listées
  (`BATCH_REJECTED::`), avec la raison ; une mission en conflit est écartée, les autres passent.
- Un seul portail (build + suite seulement si le lot touche Unreal), dans `ANASTASIS_WORKTREES\_integration`,
  dont les binaires survivent d'un lot à l'autre : build incrémental.
- `main a bouge pendant le lot` : relancer la même commande.
- Une mission écartée pour conflit : `git rebase main` dans son worktree, `finish`, puis lot suivant.

**Une mission seule** (la boucle d'avant, toujours valable) :

`main` bouge plusieurs fois par heure. La boucle est normale :

```powershell
cd C:\dev\ANASTASIS_WORKTREES\<mission>
git log --oneline HEAD..main          # que m'apporte main ? un nouveau tools/unreal/*.ps1 ? du C++ ?
git rebase main
tools\unreal\agent-worktree.ps1 finish -Mission <mission>      # la preuve se refait, elle ne se recopie pas
tools\unreal\agent-worktree.ps1 integrate -Mission <mission>
```

- `integrate` refuse un non-fast-forward (`main` a encore bougé) : recommencer la boucle.
- Il déplace `main` seule ; si le canonique est extrait ailleurs, sa copie de travail reste intacte.
- Enchaîner `finish` et `integrate` dans la même commande réduit la fenêtre pendant laquelle `main` peut bouger.

## 5. Pousser (sur demande d'Alexandre, à chaque fois)

```powershell
cd C:\dev\ANASTASIS_UNREAL
git fetch origin; git log --oneline origin/main..main   # dire ce qui part, y compris les commits des autres
git push origin main
git ls-remote origin refs/heads/main                    # relire le tip distant
```

- Le pre-push compile le canonique. S'il échoue sur `Build.bat is already running` (un autre agent
  compile) : `anastasis-unreal.ps1 build` ; `BUILD::PASS` → repousser.
- « Everything up-to-date » : un autre agent a déjà poussé. Vérifier que tes commits sont dans `origin/main`.

## 6. Élaguer

```powershell
tools\unreal\agent-worktree.ps1 prune -Mission <mission>
```

Supprime worktree, branche et enregistrement MCP local. Refuse si un commit manque à `main`. Pas
`git branch -d` : il compare à la branche extraite du canonique, pas à `main`.
