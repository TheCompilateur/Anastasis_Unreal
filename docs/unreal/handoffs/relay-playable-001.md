# HANDOFF: relay-playable-001

## MISSION

Relais d'integration (mandat de l'integrateur, autorite d'Alexandre, 2026-10-08) : rejouer sur `main`
(da0c3094) playable-windows-001, en conflit avec `main` sur
`Source/Anastasis_UnrealV2/Village/AnastasisVillagePresentation.cpp` depuis le versement de familles-feu-001.
La branche et le worktree d'origine ne sont pas touches ; ses trois commits sont repris par `cherry-pick -x`
(5e469d11, a4761330, be2d7e9b). Mission d'origine : un executable Windows jouable produit depuis `main`
canonique (`package-playable.ps1`, `play-packaged.ps1`) et des PNJ toujours dessines en corps 3D en jeu,
sans carte PNG (fiche `docs/unreal/handoffs/playable-windows-001.md`, `docs/unreal/PLAYABLE_WINDOWS_001.md`).

RELAIS: playable-windows-001

## FILES_OWNED

Ceux de playable-windows-001, plus :
- `.gitignore` : `/Build/Windows/FileOpenOrder/` (voir plus bas) ;
- cette fiche.

## Conflit resolu

`AnastasisVillagePresentation.cpp`, `SyncVillagers`, choix de l'apparence d'un habitant qui n'a pas encore d'acteur.

- **Cote `main` (familles-feu-001, 93dd4235)** : un habitant dont la simulation connait le sexe et l'age
  (`CategoryFor(Npc.Gender, Npc.Age)`) recoit une apparence de sa categorie (`PersonPool`), en evitant un
  `LookId` deja porte par un autre habitant ; a defaut, le tirage d'avant (`PickLook(*Pool, Npc.Id)`).
  Ensuite, l'ancien code chargeait portrait PNG + materiau carte et abandonnait l'habitant s'il en manquait un.
- **Cote playable-windows-001 (a4761330)** : plus aucun portrait ni materiau carte charge en jeu ; un
  `LookIndex` absent avertit et passe ; le corps 3D (mesh, locomotion, materiau) est charge avant le spawn,
  une piece manquante est une erreur sans repli PNG ; `SetLook(LookId, nullptr, nullptr)` ne garde que l'identite.
- **Resolution** : la selection par categorie de `main` est gardee mot pour mot (elle produit `LookIndex`),
  puis le test `if (LookIndex == INDEX_NONE)` de playable-windows et tout son chemin corps 3D. Seules les deux
  lignes de chargement du portrait/materiau carte, que playable-windows supprimait, disparaissent.
  Les deux comportements tiennent : le style (donc les teintes et le sexe du corps, `BodyLookFor(Look)`) suit
  la categorie de la personne simulee, et rien n'est dessine en carte. Le « sans visage en double » de main
  reste valide : il lit `GetLookId()`, et playable-windows pose `LookId` avant le retour anticipe de `SetLook`.
- Aucun autre fichier en conflit. Verifie : `villager-pie.py` (main) lit `look`, `hidden`, `cards`
  via `get_villager_cards`, tous fournis inchanges ; `Anastasis.Familles.Portraits` ne teste que `CategoryFor`.
- Zero marqueur de conflit (`grep -c '^<<<<<<<\|^=======\|^>>>>>>>'` = 0) avant `git add`.

## .gitignore

`package-playable.ps1` lance UAT `BuildCookRun -cook` depuis la racine canonique ; la cuisson y laisse
`Build/Windows/FileOpenOrder/{CookerOpenOrder,EditorOpenOrder}.log` (observe dans `C:\dev\ANASTASIS_UNREAL`,
non suivi). Ajout de `/Build/Windows/FileOpenOrder/` (regle etroite : `Build/` peut porter un jour des icones
ou une config de paquet suivies). Verifie : `git check-ignore -v Build/Windows/FileOpenOrder/CookerOpenOrder.log`
-> `.gitignore:18:/Build/Windows/FileOpenOrder/`. Le packaging n'a pas ete lance par ce relais.

## COMMIT

PENDING

## MEC

- Cherry-pick : 3/3 commits repris (`git cherry main agent/playable-windows-001` : 3 `+`, aucun deja dans main).
- BUILD / TESTS : voir `finish` (build + suite sans rendu).
- Les mesures de la fiche d'origine (paquet `640fa3e`, `PACKAGE::PASS`, `BUILD::PASS` du correctif) portent
  sur son ancienne base, pas sur cet arbre.

## PROOFS

PROOFS: (aucune)

## SCN

UNKNOWN : aucun scenario de jeu execute par ce relais.

## PLY

UNKNOWN : aucun paquet produit ni joue sur cet arbre.

## ECARTS

AUCUN -- `git diff --stat main -- Source/AnastasisSim` est vide : le relais ne touche pas le portage.

## INTEGRATION_RISK

- Reprend tous les risques de playable-windows-001 (packaging depuis le canonique, cuisson longue, verifier
  `body=3d` dans le journal du nouveau paquet).
- Comportement change pour les preuves existantes : en jeu, plus aucune carte PNG ; un corps 3D manquant =
  habitant non dessine (erreur au log). `villager-body-pie.py` attend des corps a toute distance
  (`anastasis.Village.Bodies` 2 par defaut, `BodyDistance` supprime). `villager-pie` (registre) lit les `LookId`,
  inchanges ; il n'est pas declare ici, la fiche d'origine n'en declarait aucune.
- Relancer le packaging une fois le lot verse laissera `Build/Windows/FileOpenOrder/` dans la racine :
  desormais ignore.

## STOP

Pas de claim de gameplay, de Shipping, de distribution ou de performance. Pas de nouveau paquet. Le mannequin
3D reste un etat transitoire.
