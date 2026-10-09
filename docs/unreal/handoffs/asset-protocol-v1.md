# HANDOFF: asset-protocol-v1

## MISSION

Installer le protocole asset V1 : contrat JSON réutilisable, validateur Unreal
Python en lecture seule et rejeu de la capture existante PonticMicro. Pilote
sur les quatre meshes déjà consommés ; aucune régénération d'asset ni changement
de placement.

## FILES_OWNED

- `docs/unreal/assets/ASSET_PROTOCOL_V1.md`
- `docs/unreal/assets/contracts/_TEMPLATE.json`
- `docs/unreal/assets/contracts/pontic-micro-v1.json`
- `tools/unreal/asset-contract-validate.py`
- `tools/unreal/proofs.txt` (une ligne de preuve ajoutée)
- `docs/unreal/PRESENTATION_ASSET_BINDING.md` (instruction worktree corrigée)
- `AGENTS.md` (index du nouvel outil)
- cette fiche

## COMMIT

PENDING — SHA du commit et marqueur `finish` font foi.

## MEC

- Contrat JSON et syntaxe Python : PASS par `python -m py_compile` et `json.loads`.
- Références déclarées : quatre chaînes d'asset présentes dans les fichiers
  consommateurs déclarés, et tous les fichiers référence/générateur présents.
  Cela prouve une référence source, pas son exécution.
- Index `tools/unreal/` : `MISSING=` et `STALE=` vides.
- `editor-batch.ps1 -Proofs asset-contract-pontic-micro,pontic-water-micro-capture -DryRun` : PASS, deux jobs dans un éditeur prévu.
- Build du worktree : `BUILD::PASS` après 19 actions et 268,40 s
  (`tools/unreal/anastasis-unreal.ps1 build`). Une première tentative avait
  attendu le verrou UBT partagé ; la seconde a compilé sur source figée.
- Validation réelle des quatre `.uasset` par le nouveau script : UNKNOWN jusqu'au
  rejeu dans un éditeur de ce worktree ou du lot. Le lancement local de
  `editor-batch.ps1` a attendu cinq minutes à `EDITOR_GATE::WAIT` sous le seuil
  de 3 Go de RAM disponible, puis seule notre attente a été annulée.

## PROOFS

PROOFS: asset-contract-pontic-micro, pontic-water-micro-capture

## SCN

UNKNOWN pour cette mission : aucune nouvelle capture PonticMicro ni inspection
des images n'a été obtenue après la création du protocole. Le PASS de capture
historique ne se transpose pas à ce commit.

## PLY

UNKNOWN. Aucun parcours humain à vitesse normale.

## ECARTS

AUCUN — `Source/AnastasisSim/` inchangé.

## INTEGRATION_RISK

- Le validateur lit seulement les assets et les références source ; il ne sauve
  aucun `.uasset`. Les nombres de triangles viennent de la fiche PonticMicro
  antérieure : une différence sur l'arbre actuel doit être analysée, pas
  corrigée en desserrant silencieusement le contrat.
- `tools/unreal/proofs.txt` est partagé. Une seule ligne nouvelle, dans son
  état final, est ajoutée pour éviter les doubles entrées à l'intégration.
- L'éditeur de la preuve doit charger le projet du lot, puis exécuter le
  validateur avant `pontic-water-micro-capture`. Le registre ordonne les captures
  après les autres preuves.
- Le validateur n'a pas été exécuté dans Unreal avant le handoff : il peut
  révéler une API ou des bounds différents et doit échouer explicitement dans
  ce cas. Écarter la mission du lot si `ASSET_CONTRACT FAIL` apparaît.

## STOP

Ce travail ne crée pas un nouvel asset, ne change pas les placements, ne prouve
pas la qualité des formes, la collision, le coût GPU, l'interaction de la
grenouille, la fidélité historique ou le verdict joueur. Une preuve
instrumentale `PASS` future ne suffira pas seule à prononcer SCN ou PLY.
