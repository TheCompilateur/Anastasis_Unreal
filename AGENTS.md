# ANÁSTASIS Unreal — règles agent

Projet **Unreal Engine 5.8.2** (CL 56702186). Pas une app web, pas Unity, pas Godot.

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
