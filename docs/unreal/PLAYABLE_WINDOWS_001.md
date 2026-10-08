# PLAYABLE_WINDOWS_001 — exécutable Windows local

## Décision

Après intégration, l'intégrateur produit un paquet **Win64 Development** depuis la racine
canonique. `build-game` compile le target Game mais ne cuit pas les assets ; il ne suffit pas
à créer un jeu autonome. `package-playable.ps1` compile d'abord Editor et Game avec l'opérateur
du projet, puis pilote `RunUAT BuildCookRun` (`-skipbuild`, cook de `GameDefaultMap`, stage, pak,
archive). Il vérifie la présence de l'exécutable et des données
cuites avant de publier la version.

```powershell
cd C:\dev\ANASTASIS_UNREAL
tools\unreal\package-playable.ps1
tools\unreal\play-packaged.ps1
```

Le paquet se trouve dans
`C:\dev\ANASTASIS_RELEASES\Playable\releases\<commit>\Windows\` ; le chemin exact
est affiché par `GAME_EXE::`. `latest.json` désigne le dernier paquet complet. Un second
appel au même commit réutilise ce paquet. Une nouvelle intégration crée un autre dossier,
puis déplace le pointeur seulement après un empaquetage réussi. Les anciennes versions
restent disponibles ; le script ne les supprime pas.

Après une interruption entre la fin d'UAT et la publication, relancer la même commande :
elle détecte un `BuildCookRun.log` terminé avec succès dans `staging/`, vérifie le lanceur
`Windows\Anastasis_UnrealV2.exe`, le binaire interne et les données cuites, puis publie
ce paquet sans reconstruire.

## Portes d'entrée

- `main` doit être extrait dans la racine canonique, sans fichier suivi modifié.
  Les fichiers non suivis hors `Content/` ne sont pas inclus dans le paquet.
- Le verrou `MAIN.lock` doit être libre. Le script attend sa libération, puis prend un
  ticket dans la file mémoire Unreal. Il attend un créneau sans autre processus Unreal
  (45 minutes par défaut), puis garde ce créneau pendant `BuildCookRun`, sans interrompre
  les sessions existantes. `-WaitMinutes 0` échoue aussitôt si la machine est occupée.
- La version du moteur doit être UE 5.8.2, CL 56702186 ; la carte cuite vient de
  `GameDefaultMap` dans `Config/DefaultEngine.ini`.
- Les deux compilations passent par `anastasis-unreal.ps1`, qui transmet `-WaitMutex`
  à UnrealBuildTool. UAT ne compile pas de nouveau : son `-ubtargs` ne couvrait pas le
  target Editor et produisait `ConflictingInstance` malgré l'option.
- Si `main` bouge pendant la cuisson, le paquet reste dans `staging/` et `latest.json`
  conserve l'ancienne version.

La commande de mise à jour est indépendante des tests de gameplay. Un paquet créé prouve
la compilation et la cuisson de la carte ; il ne prouve ni la jouabilité, ni la qualité
de la boucle, ni les touches physiques. Les commandes `DebugExecBindings` documentées
pour PIE ne définissent pas à elles seules une interface de jeu distribuable.
