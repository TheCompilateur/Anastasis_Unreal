# HANDOFF: playable-windows-001

## MISSION

Produire un vrai exécutable Windows jouable hors interface d'édition et fournir à
l'intégrateur une seule commande de mise à jour après chaque lot intégré.

## FILES_OWNED

- `tools/unreal/package-playable.ps1`
- `tools/unreal/play-packaged.ps1`
- `docs/unreal/PLAYABLE_WINDOWS_001.md`
- `AGENTS.md` (index des deux scripts)
- cette fiche

## COMMIT

PENDING

## MEC

- Analyse PowerShell : 0 erreur de syntaxe pour les deux scripts.
- `Test-AnastasisToolsIndex` : `MISSING=`, `STALE=` (vides).
- `git diff --check` : code 0.
- Empaquetage réel du `main` canonique `640fa3e81a522997912ef45941f6f3410051b99e` :
  `build-game` `Result: Succeeded` ; UAT `BUILD SUCCESSFUL`, `ExitCode=0` ;
  `PACKAGE::PASS` après reprise du staging complet. `latest.json` pointe vers ce commit.
- Artefacts contrôlés : `Windows/Anastasis_UnrealV2.exe` (lanceur), binaire interne
  `Windows/Anastasis_UnrealV2/Binaries/Win64/Anastasis_UnrealV2.exe`, `.pak` et `.ucas`.
- Lancement demandé via `play-packaged.ps1` : `GAME::START_REQUESTED` pour le commit
  `640fa3e` ; le binaire interne est observé comme processus `Anastasis_UnrealV2`
  (pid 35724) avec fenêtre `AnastasisUR (64-bit Development PCD3D_SM6)`.
- Première tentative `BuildCookRun` sur `640fa3e` : `ExitCode=10`, log
  `Result: Failed (ConflictingInstance)` ; un autre UBT tenait le mutex. Correction :
  `-ubtargs=-WaitMutex` tentée ; UAT l'a appliquée au Game mais pas à l'Editor.
- Deuxième tentative : même `ConflictingInstance` sur le target Editor. Correction :
  `anastasis-unreal.ps1 build` puis `build-game` (tous deux `-WaitMutex`) avant
  `BuildCookRun -skipbuild`. Aucun paquet publié à ce stade.
- Troisième tentative : Game build `Result: Succeeded`, UAT `BUILD SUCCESSFUL` et
  `ExitCode=0`. Le garde final a trouvé deux exécutable nommés pareil (lanceur racine
  et binaire interne) et refusé le paquet. Correction : chemin explicite du lanceur,
  plus reprise du `staging/` réussi sans nouvelle cuisson : `PACKAGE::PASS`.
- Le paquet est produit depuis `main` canonique, avec garde sur le verrou d'intégration,
  les processus Unreal et le HEAD. Le pointeur `latest.json` n'est publié qu'après UAT.

## PROOFS

PROOFS: (aucune)

## SCN

UNKNOWN : aucun scénario de jeu exécuté par cette mission.

## PLY

UNKNOWN : l'exécutable est ouvert pour Alexandre ; aucune observation de ses actions,
de la lisibilité ou du plaisir de jeu n'a été faite.

## INTEGRATION_RISK

- Le script empaquette depuis `C:\dev\ANASTASIS_UNREAL` et écrit les releases sous
  `C:\dev\ANASTASIS_RELEASES\Playable` ; il ne modifie aucun fichier suivi canonique.
- Attendre la fin du lot et la recompilation canonique avant la première mise à jour.
- La première cuisson Win64 peut être longue et gourmande en RAM. Aucun éditeur d'agent
  ne doit être fermé pour la faire passer. Le script passe par la file mémoire existante
  et réserve le créneau pendant UAT ; les éditeurs suivants attendent leur tour.

## STOP

Pas de claim de gameplay, de Shipping, de distribution publique ou de performance.
