# HANDOFF: asset-protocol-v1 — révision V2

## MESSAGE À L'INTÉGRATEUR

**Ne pas admettre l'ancien commit V1 `faaf242178e7b002f21c5e1151b8955ca50189f5`.**
Son marqueur `nounreal` a été rendu caduc par le commit d'arrêt `0ea1cad8f`.
La V1 n'avait créé aucun asset et son validateur n'avait jamais tourné dans
Unreal. La mission ne redevient candidate au lot qu'après un **nouveau**
`finish` sur le commit V2, avec les preuves indiquées ci-dessous.

## MISSION

Corriger le protocole V1 par un pilote de création réel : générer
`SM_Pontic_Horsetail_02`, le router dans le consommateur existant sous CVar
désactivée par défaut, contrôler le `.uasset` sauvé, puis capturer
ancien/nouveau/ancien aux mêmes placements. La V2 est détaillée dans
`docs/unreal/assets/ASSET_PROTOCOL_V2.md`.

## FILES_OWNED

- `docs/unreal/assets/ASSET_PROTOCOL_V1.md` (archivage)
- `docs/unreal/assets/ASSET_PROTOCOL_V2.md`
- `docs/unreal/assets/contracts/_TEMPLATE.json`, `_TEMPLATE_V2.json`,
  `pontic-micro-v1.json`, `pontic-horsetail-v2.json`
- `tools/unreal/asset-contract-validate.py`
- `tools/unreal/validate-asset-contract.ps1`
- `tools/unreal/create-pontic-horsetail-v2.py`, `.ps1`
- `tools/unreal/ground-cover-capture.py` (états V2 uniquement)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisPonticWaterMicro.cpp`
- `Content/Anastasis/PonticMicro/SM_Pontic_Horsetail_02.uasset`
- `tools/unreal/proofs.txt`, `AGENTS.md` (lignes V2)
- `docs/unreal/PRESENTATION_ASSET_BINDING.md` (correction worktree V1)
- cette fiche

## COMMIT

PENDING — le commit marqué par le dernier `finish` fait foi.

## MEC

- Syntaxe Python et JSON : à reporter avec commandes et résultats.
- Génération Unreal `-nullrhi` : `PONTIC_HORSETAIL_V2 PASS`,
  `SM_Pontic_Horsetail_02`, trois LOD **420/133/48 triangles**, bounds
  `[-23.0,-18.3,-1.0]..[21.6,21.4,61.1]` cm. Fichier sauvé de 38 860 octets,
  filtre Git LFS confirmé ; log `Saved/PonticHorsetailV2Evidence/create-pontic-horsetail-v2.log`.
- Contrat MEC en éditeur neuf (`asset-contract-horsetail-v2`) : en attente de
  son exécution ; les valeurs du générateur ne remplacent pas cette lecture.
- Build C++ et suite sans rendu : à reporter après `finish`.

## PROOFS

PROOFS: asset-contract-horsetail-v2, pontic-horsetail-v2-capture

## SCN

UNKNOWN jusqu'au rejeu de l'A/B/A, à l'inspection des images et aux mesures
comparant ancien/nouveau à ancien/ancien répété. Le marqueur instrumental
`PONTIC_HORSETAIL_ABA PASS` contrôle les inventaires ; il ne juge pas l'image.

## PLY

UNKNOWN. Aucun parcours humain à vitesse normale n'est revendiqué par cette
mission ; l'asset est visuel et le HISM n'a pas de collision.

## ECARTS

AUCUN — `Source/AnastasisSim/` inchangé.

## INTEGRATION_RISK

- **V1 retirée** : seul le nouveau SHA V2 marqué par `finish` est admissible.
- Le candidat est désactivé par défaut (`PonticHorsetailCandidate=0`). Une
  intégration technique n'est pas la décision artistique de l'activer.
- La preuve de capture exige la carte courante et un éditeur avec rendu ; la
  machine est partagée et la porte mémoire peut retarder le rejeu. Ne pas
  contourner cette porte ni déplacer `main` pendant le lot.
- Le contrôle `consumer_token` est statique. L'inventaire de la capture doit
  prouver l'emploi réel de `SM_Pontic_Horsetail_02` à placements constants.
- `ground-cover-capture.py`, `proofs.txt` et `AGENTS.md` sont des fichiers
  partagés ; conserver les états des autres missions en cas de rebase.

## STOP

Ne pas annoncer `SCN PASS`, `PLY PASS`, fidélité botanique ou amélioration GPU
tant que les images et chiffres correspondants ne sont pas inspectés. Si la
preuve locale ne tourne pas, le verdict reste `UNKNOWN` et la CVar reste à 0.
