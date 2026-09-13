# ANÁSTASIS Unreal — cartographie technique et santé

Document de réalité, pas d'architecture souhaitée. Produit par la mission Guardian
(`agent/unreal-guardian`). Commande unique :

```powershell
tools\unreal\anastasis-unreal.ps1 health
```

`health` n'invente aucun PASS. Une dimension sans preuve, ou dont la preuve porte
sur un fingerprint source différent, est `UNKNOWN` ou `STALE`. Exit 1 seulement
si le verdict est `RED`.

## Identité

| Champ | Valeur |
|---|---|
| Engine | Unreal Engine 5.8.2, CL 56702186 |
| Canonical root | `C:\dev\ANASTASIS_UNREAL` |
| `.uproject` | `Anastasis_UnrealV2.uproject` |
| Product | AnastasisUR |
| EngineAssociation | `"5.8"` (ne pas réécrire) |
| C++ modules | `AnastasisSim`, `Anastasis_UnrealV2` |
| Targets | `Anastasis_UnrealV2Editor` (Editor), `Anastasis_UnrealV2` (Game) |
| GameInstance | `/Script/Engine.GameInstance` (stock) |
| GameMode (ini) | `/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode` |
| GameMode (C++) | `AAnastasis_UnrealV2GameMode` (abstract) — spawn `AAnastasisWorldEmbodiment` en DEBUG |
| Subsystem | `UAnastasisWorldProbeSubsystem` (`UWorldSubsystem`) |
| PLAYER | NOT_IMPLEMENTED |
| Canonical / PIE map | `/Game/FirstPerson/Lvl_FirstPerson` |
| Observation map | `/Game/Anastasis/Maps/Lvl_AnastasisSlice` |
| Template maps | `Lvl_Horror`, `Lvl_Shooter` |

## Commandes

```text
status      identité + fingerprint Source/Config/.uproject
build       Anastasis_UnrealV2Editor Win64 Development
build-game  Anastasis_UnrealV2         Win64 Development
verify      build + Editor dédié + load map + PIE smoke
health      agrège les preuves ci-dessus ; n'en produit aucune
editor      lance l'éditeur (PAS une preuve)
report-tests.ps1     suite Anastasis, 3 catégories (PASS / KEF / FAIL)
```

Preuves : `Saved/CanonicalVerification/` (`latest.json`, `build.log`,
`build-game.log`, `built-source.sha256`, `built-game-source.sha256`,
`report-tests.log`, `health.txt`, `scheduled-verify.log`).

## Logs

`tools/unreal/known-log-patterns.txt` est la baseline. Les 20
`LogAutomationTest: Error: Condition failed` au boot FR sont du bruit moteur
(voir `docs/unreal/AUTOMATION_TRIAGE.md`). Deux warnings connus : EULA MCP,
`r.MotionVectorSimulation`. Toute autre Error/Warning est NEW.

## Tests

33 tests `Anastasis*` + Inspect au HEAD `f6fa234` :

- 29 PASS réels
- 4 KNOWN_EXPECTED_FAILURE (`Parite.Fbm`, `Parite.SemantiqueJs`, 2 Inspect)
- 0 FAIL

`KNOWN_EXPECTED_FAILURE` n'est pas un PASS. Registre :
`tools/unreal/known-expected-failures.txt`.

## Config drift connu (LOW, non corrigé)

`Config/DefaultEditor.ini` `SimpleMapName=/Game/FirstPerson/Maps/FirstPersonExampleMap`
pointe une map template absente. Le boot réel utilise `Lvl_FirstPerson`.
Corriger seulement si un boot casse ; ce n'est pas le cas aujourd'hui.

Cold worktree DDC: `verify` editor budget is 12 minutes. A 4-minute budget failed on
2026-09-13 still loading engine modules, with three foreign `UnrealEditor-Cmd`
sessions (other worktrees). Concurrent editors are logged (`VERIFY::NOTE`) and
left untouched.

## Nightly 2026-09-13 03:00

`Anastasis-ScheduledVerify` : `VERIFY_FAIL` + `TESTS_PASS` (27+4 / 0 FAIL),
aucun `editor-03*.log`. Cause : Task Scheduler démarre dans `System32` ;
`anastasis-unreal.ps1` refusait ce cwd avant même le build. Corrigé :
`Set-Location` vers la racine du projet (opérateur + wrapper). Le wrapper
conserve désormais la queue de sortie de `verify` (la cause interne n'était
pas dans `scheduled-verify.log`).

## Dette assumée

- PLAYER NOT_IMPLEMENTED
- GameInstance stock
- Game target absent de l'opérateur jusqu'à `build-game`
- Fenêtre `quiescence.json` : artefact de seal, hors ce chantier
- Suite Automation complète hors seal (culture FR)
