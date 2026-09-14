# Contrat de passation agent

Ce document definit la sortie minimale d'un agent Unreal ANASTASIS.

But: l'integrateur doit pouvoir savoir vite ce qui a ete fait, ce qui est
prouve, ce qui ne l'est pas, et ce qui peut casser pendant l'assemblage.

## Regle

Une mission agent n'est pas prete a integrer tant que son worktree ne contient
pas une fiche de passation:

```text
docs/unreal/handoffs/<mission>.md
```

Le nom `<mission>` est le meme que celui passe a:

```powershell
tools\unreal\agent-worktree.ps1 create -Mission <mission>
```

Le portail:

```powershell
tools\unreal\agent-worktree.ps1 finish -Mission <mission>
```

refuse `HANDOFF_READY::YES` si cette fiche manque.

## Ce qu'une fiche doit contenir

La fiche n'est pas un rapport narratif. C'est une carte de preuve.

```text
MISSION
FILES_OWNED
COMMIT
MEC
SCN
PLY
INTEGRATION_RISK
```

Definitions:

| Champ | Sens |
|---|---|
| `MISSION` | Objectif borne de l'agent, en une phrase. |
| `FILES_OWNED` | Fichiers modifies volontairement par cette mission. |
| `COMMIT` | Hash de passation. Utiliser `PENDING` avant commit final, ou `BRANCH_HEAD` si la fiche est elle-meme dans ce commit. |
| `MEC` | Build, tests, scripts, donnees ou preuves mecaniques. |
| `SCN` | Preuve en scene/runtime/editor, ou `UNKNOWN` / `NOT_ATTEMPTED`. |
| `PLY` | Preuve joueur, controle humain, VR ou experience, ou `UNKNOWN`. |
| `INTEGRATION_RISK` | Chevauchements, fichiers chauds, hypothese fragile, conflit probable. |

## Invariants

```text
MEC != SCN != PLY
KNOWN_EXPECTED_FAILURE != PASS
rapport != preuve brute
local_fix != system_effect
agent branch ready != canonical integrated
```

Un agent declare: "mon lot est pret a etre integre".

Seule une passe d'integration declare l'etat de `main`.

## Template

Copier:

```text
docs/unreal/handoffs/_TEMPLATE.md
```

vers:

```text
docs/unreal/handoffs/<mission>.md
```

et remplir les champs avant `finish`.
