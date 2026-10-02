# HANDOFF: time-keys-001

## MISSION

Demande d'Alexandre (2026-10-01) : en PIE, la touche **8** accélère le temps et la touche **9** le ralentit.

## FILES_OWNED

- `Config/DefaultInput.ini` : `DebugExecBindings` `Eight` / `NumPadEight` → `Anastasis.Sim.Faster`, `Nine` /
  `NumPadNine` → `Anastasis.Sim.Slower` ; pavé `+` / `-` et `Pause` conservés.
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp` : textes d'aide de `anastasis.Sim.Warp`,
  `Anastasis.Sim.Faster` et `Anastasis.Sim.Slower` (chaînes seulement).
- `AGENTS.md` (« Temps accéléré »), `docs/unreal/TIME_WARP_001.md` (table des touches).

## COMMIT

PENDING

## PROOFS

PROOFS: (aucune)

## MEC

- BUILD: PENDING (`finish` : build seul).
- Conflits de touches : les entrées du jeu (`Content/Input/IMC_Default.uasset`, `IMC_MouseLook`, variantes
  Horror/Shooter) ne contiennent ni `Eight` ni `Nine`. Recherche dans les binaires, qui sont de vrais assets
  (en-tête Unreal, `SpaceBar` y est lisible), pas des pointeurs LFS.
- AZERTY : Unreal lit la touche par sa position (code virtuel Windows `0x38` / `0x39`), donc la touche
  « 8 » de la rangée marche sans Maj.

## SCN

Sans objet.

## PLY

Non vérifié à la main en PIE : aucune preuve automatique ne sait presser une touche. À vérifier par Alexandre :
en PIE, appuyer sur 8 doit afficher `TEMPS ×2` puis `×4`, et 9 doit revenir en arrière.

## INTEGRATION_RISK

- Les liaisons de debug ne sont lues qu'au démarrage de l'éditeur : il faut le relancer après versement.
- La référence JS réserve les touches 1 à 5 au choix de but du joueur (non porté). 8 et 9 ne les touchent pas.

## STOP

- Pas de liaison en build Shipping (`DebugExecBindings`). Quand l'interface du joueur existera, la vitesse
  passera par elle.
