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
- Portage dans `Source/AnastasisSim/` : ce qui n'est pas fidèle à la référence JS se déclare **dans le
  même commit** : fiche dans `Source/AnastasisSim/ECARTS.md` (prochain numéro libre, `A_FERMER` ou
  `A_TRANCHER`, jamais `ASSUME`), marque `ecart n°N` dans le code. Ce que tu portes enfin : fiche
  `FERME`, marques retirées. `docs/migration/PROTOCOLE_ECARTS.md`.

## 3. Passer la main

**Une file, un éditeur (EDITOR_QUEUE_001, `AGENTS.md`).** Un seul éditeur utile à la fois sur cette
machine : on ne démarre plus d'éditeur pour se prouver. La suite et les preuves PIE tournent au lot.

1. Fiche `docs/unreal/handoffs/<mission>.md` depuis `_TEMPLATE.md` : MISSION, FILES_OWNED, COMMIT, MEC
   (commandes et **valeurs** obtenues), SCN, PLY, ECARTS (si `Source/AnastasisSim/` est touché),
   INTEGRATION_RISK, STOP (ce que tu ne revendiques pas),
   et **`PROOFS:`** — les preuves PIE du registre `tools/unreal/proofs.txt` que le lot doit rejouer pour
   toi, ou `PROOFS: (aucune)`. Une preuve nouvelle : l'inscrire au registre (une ligne), la mettre au
   point par `tools\unreal\editor-batch.ps1 -Proofs <nom>` (un éditeur, ta preuve, il se ferme).
2. Tout commiter.
3. `tools\unreal\agent-worktree.ps1 finish -Mission <mission>` → `HANDOFF_READY::YES (queued)` : build
   passé, **pas d'éditeur**, la suite et tes preuves attendent le lot. Le dire tel quel : `queued` n'est
   pas un PASS. Une branche sans `Source/`, `Config/`, `Content/`, `Plugins/` ni `.uproject` sort
   `(nounreal)`, sans build. `finish -Prove` (suite dans ton propre éditeur) : seulement si Alexandre
   attend un verdict tout de suite ; rapporter alors PASS / KNOWN_EXPECTED_FAILURE / FAIL séparément.
   `finish` marque le commit : un commit ajouté ensuite exige un nouveau `finish`.
4. **Ne pas intégrer soi-même.** S'arrêter à `HANDOFF_READY::YES` : l'intégrateur voit ta mission dans
   `status` (`PRETES_POUR_LE_LOT::`). Le verdict du lot (`BATCH_INTEGRATED::` ou `BATCH_PROOF_FAIL::<preuve>
   (mission <toi>)`) est ta vraie preuve.

Si `finish` échoue :

| Ce que tu vois | Quoi faire |
|---|---|
| `MISSING` / `STALE` | mettre l'index d'`AGENTS.md` à jour |
| `ECARTS::FAIL` | lire les lignes `FAIL` : section `## ECARTS` de la fiche, fiche du registre, marque `ecart n°N`, tirage hors `sim.rng` ; `node tools/migration/check-ecarts.mjs -base main -handoff <fiche>` pour rejouer en quelques secondes |
| `Unreal lance sans Start-AnastasisEditor` | convertir la ligne citée |
| `TESTS::FAIL lanceur bloque`, ou `RUN_INCOMPLET` | lire le log avant tout : `PIEGES_UNREAL.md` (éditeur fermé, VRAM, machine saturée) |
| un test marqué sort `Fail` | lire la sortie du test lui-même ; ne jamais toucher au marqueur ni au registre |
| machine saturée (RAM libre < 4 Go, éditeur > 5 min au boot) | attendre une fenêtre calme, relancer ; ne pas tuer l'éditeur d'un autre |
| `editeur Unreal encore ouvert sur ce worktree` | c'est le tien : `quit_editor()` par MCP ou `Stop-Process -Id <pid>`, puis relancer `finish` |
| `EDITOR_GATE::WAIT` / `EDITOR_GATE::TIMEOUT` | porte mémoire (`AGENTS.md`) : trop d'éditeurs ou de RAM prise ; relancer plus tard, jamais `ANASTASIS_EDITOR_GATE=0` sans mandat |

## 4. Verser (rôle intégrateur, sur demande d'Alexandre)

**File groupée, la voie normale.** Une seule session intègre ; elle verse toutes les missions prêtes d'un coup :

```powershell
cd C:\dev\ANASTASIS_UNREAL
tools\unreal\agent-worktree.ps1 status            # PRETES_POUR_LE_LOT:: et la commande toute faite ; MAIN_LOCK
tools\unreal\agent-worktree.ps1 integrate-batch -Missions mission-a,mission-b,mission-c
```

- Le lot prend le **verrou de `main`** : pendant qu'il tourne, personne ne la déplace (`integrate` d'un
  autre refuse : `MAIN_LOCK::TENU`). Le verrou est rendu à la fin, même en échec.
- Admises : les missions dont `finish` a passé sur leur commit actuel (`proved`, `queued` ou `nounreal`).
  Les autres sont listées (`BATCH_REJECTED::`) ; une mission en conflit est écartée, les autres passent.
- Un seul portail dans `ANASTASIS_WORKTREES\_integration` (binaires conservés, build incrémental) : build +
  suite si le lot touche Unreal, puis **toutes les preuves PIE déclarées (`PROOFS:`) dans un seul
  éditeur** (`editor-batch.ps1`).
- `BATCH_PROOF_FAIL::<preuve> (mission <m>)` : `main` intact ; relancer le lot sans `<m>`, et le dire à son agent.
- Une mission écartée pour conflit : `git rebase main` dans son worktree, `finish`, puis lot suivant.
- Si `main` a été réécrit par quelqu'un (une base qui n'est plus ancêtre de `main`) : `git range-diff`,
  puis `git rebase --onto main <ancienne base>` dans le worktree, `finish`.

**Une mission seule, déjà prouvée** (`proved` ou `nounreal` ; `queued` est refusé et renvoyé au lot) :

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
