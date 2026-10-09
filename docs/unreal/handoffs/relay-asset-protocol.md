# HANDOFF: relay-asset-protocol

## MISSION

Relais d'integration (mandat de l'integrateur, autorite d'Alexandre, 2026-10-09) : verser asset-protocol-v1,
dont l'agent n'est plus actif et dont la passation portait sur un ancien commit (V1, retire par l'agent lui-meme).
Les dix commits de `agent/asset-protocol-v1` (cf18e62b0 .. cfd2197a6) sont rejoues sur main par `cherry-pick -x`,
la branche d'origine n'est pas touchee.

Ce que la mission apporte (fiche d'origine `docs/unreal/handoffs/asset-protocol-v1.md`, revision V2) :
protocole d'asset V2 (`docs/unreal/assets/ASSET_PROTOCOL_V2.md`, V1 archivee), contrat JSON
`pontic-horsetail-v2.json`, validateur en lecture seule `asset-contract-validate.py` + lanceur sans rendu
`validate-asset-contract.ps1`, generateur `create-pontic-horsetail-v2.ps1/.py`, le candidat LFS
`Content/Anastasis/PonticMicro/SM_Pontic_Horsetail_02.uasset` (3 LOD 420/133/48 triangles), son routage dans
`AnastasisPonticWaterMicro.cpp` sous la CVar `anastasis.Dressing.PonticHorsetailCandidate` (**0 par defaut** :
rien ne change en jeu), et les etats `horsetail_old,horsetail_new,horsetail_old2` de `ground-cover-capture.py`.

RELAIS: asset-protocol-v1

## FILES_OWNED

Ceux de asset-protocol-v1 (voir sa fiche), plus cette fiche.

## COMMIT

PENDING — le commit marque par `finish` fait foi.

## MEC

- Conflits de cherry-pick : `AGENTS.md` seul, deux fois (2b686cc24, b90b5c28f), resolus a la main sans union :
  la ligne `capture-ground-cover.ps1` de main (etats `path_*` / `gravel_*` de pontic-ground-context-002) est
  gardee telle quelle, la ligne `create-pontic-horsetail-v2` de la mission (forme finale, `-nullrhi`) est ajoutee
  avant elle, la ligne `pontic-horsetail-v2-capture` et la ligne `asset-contract-validate.py` (V2) viennent de la
  mission. `ground-cover-capture.py` et `proofs.txt` fusionnes sans conflit (etats horsetail_* a cote des etats
  de main).
- Asset `SM_Pontic_Horsetail_02.uasset` : pointeur LFS du commit d'origine 622a49f9f, objet present localement
  (38 860 octets), non regenere.
- Mesures de l'agent d'origine (non refaites ici) : generation `PONTIC_HORSETAIL_V2 PASS`, contrat
  `ASSET_CONTRACT PASS id=asset-protocol-v2-horsetail assets=1 stage=scene` dans un editeur neuf.
- Build et suite sans rendu : sortie de `finish` sur ce relais.

## PROOFS

PROOFS: asset-contract-horsetail-v2, pontic-horsetail-v2-capture

## SCN

UNKNOWN : l'A/B/A ancien/nouveau/ancien n'a jamais ete rejoue (l'agent d'origine attendait la porte memoire).
`PONTIC_HORSETAIL_ABA PASS` ne controle que l'inventaire spatial, pas l'image.

## PLY

UNKNOWN. Asset visuel, HISM sans collision.

## ECARTS

AUCUN — `Source/AnastasisSim/` inchange.

## INTEGRATION_RISK

- La V2 remplace au registre la preuve V1 `asset-contract-pontic-micro` par `asset-contract-horsetail-v2` et
  `pontic-horsetail-v2-capture` ; aucune fiche de main ne declare l'ancienne.
- La CVar reste a 0 : verser n'est pas la decision artistique d'activer le candidat.
- `pontic-horsetail-v2-capture` exige un editeur avec rendu ; preuve jamais jouee jusqu'ici.
- Fichiers partages : `AGENTS.md`, `proofs.txt`, `ground-cover-capture.py` (etats micro_* / path_* / gravel_*
  d'autres missions conserves).

## STOP

Pas de SCN PASS ni de PLY PASS, pas de fidelite botanique ni de gain GPU revendiques ; aucune preuve PIE jouee par
ce relais.
