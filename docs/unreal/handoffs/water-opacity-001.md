# HANDOFF: water-opacity-001

## MISSION

Faire exister le volume de Single Layer Water dans `M_AnastasisWater` : brancher le pin Opacity (couverture
d'écume, 0 ailleurs), mettre les coefficients en centimètres, Specular 0,25, et choisir une palette. Rien
d'autre : ni relief, ni rubans, ni vagues, ni plugin Water.

Cause : `WaterVisibility = 1 - Opacity` (`BasePassPixelShader.usf:1141`) et le volume n'est calculé que si
`WaterVisibility > 0` (`SingleLayerWaterShading.ush:74`). Opacity non branché vaut 1
(`MaterialAttributeDefinitionMap.cpp:401`). L'eau n'avait donc jamais de volume : couleur peinte, fond
invisible, absorption et diffusion sans effet.

Base : `main` 0fb80f93b. Branche `agent/water-opacity-001`.

## FILES_OWNED

- tools/unreal/water-look.py
- tools/unreal/riverbank-capture.py
- tools/unreal/proofs.txt (une ligne : `water-volume-capture`)
- Content/Anastasis/Materials/M_AnastasisWater.uasset
- .claude/skills/anastasis-realisme/fiches/eau.md, .claude/skills/anastasis-realisme/registre.md
- docs/unreal/RIVER_LOOK_001.md, docs/unreal/WATER_LOOK_001.md (errata)
- docs/unreal/handoffs/water-opacity-001.md

## COMMIT

PENDING (voir `git log` de la branche).

## MEC

- BUILD: PASS, `tools\unreal\anastasis-unreal.ps1 build` dans le worktree (222,9 s, 19 actions). Aucun fichier C++ touché.
- TESTS: UNKNOWN avant `finish`.
- `tools\unreal\water-look.ps1 -Rebuild` : `WATER_MATERIAL::PASS`, `opacity=True` au câblage, un seul noeud de sortie SLW, aucun échec de compilation.
- A/B `tools\unreal\riverbank-capture.ps1 -Label opacity-ab -States 'vol_old,vol_on,vol_ior,vol_old2'`, vues
  calme_plaine, eau_vive, ruisseau, rive_lac, berge_haute, ciel à 11 h, `RIVERBANK_CAPTURE_COMPLETE views=5 states=4`,
  trois sections d'eau reçoivent l'instance dynamique (`WATER_VOLUME_STATE ... sections=3`).
  Pixels > 16/255, `vol_old` contre `vol_on` (bruit : `vol_old` contre `vol_old2`) :

  | vue | vol_old / vol_on | bruit |
  |---|---|---|
  | calme_plaine | 14,13 % | 7,46 % |
  | eau_vive | 21,91 % | 2,02 % |
  | ruisseau | 9,98 % | 9,43 % (cachée par des roseaux, dans le bruit) |
  | rive_lac | 20,97 % | 7,27 % |
  | berge_haute | 34,97 % | 0,88 % |

  `vol_on` contre `vol_ior` (Specular 0,5 contre 0,25) : 1,0 à 7,9 % (reflets d'arbres du lac un peu plus faibles).
- Coût GPU (`gpu_ms_p50`, RTX 3060, un seul lancement) : eau_vive 15,6 → 15,5 ; calme_plaine 17,8 → 17,8 ;
  berge_haute 13,9 → 14,0 ; ruisseau 20,5 → 21,1 ; rive_lac 16,8 → **45,3** sur `vol_on`, non reproduit sur
  `vol_ior` (17,8, même matériau, Specular seul différent) : une prise bruitée par une autre charge de la machine.
  Aucun surcoût mesurable, mais un seul échantillon par vue.
- Palettes (`opacity-palettes`, 4 vues, 4 états) : valeurs par défaut d'origine contre trois candidates ;
  choix d'Alexandre le 2026-10-10 : **C « trouble »** (absorption 0,45 / 0,22 / 0,40, diffusion 0,045 / 0,045 / 0,025).
- COMMANDS:
  - `python -I -c "import ast..."` sur water-look.py et riverbank-capture.py : SYNTAX_OK
  - `tools\unreal\editor-batch.ps1 -Proofs water-volume-capture` : `PROOF::PASS water-volume-capture (271.0s)`,
    `EDITOR_BATCH::PASS 1/1`, 20 images dans `Saved/WaterVolumeEvidence/proof`, rejoué APRÈS régénération de l'asset
    avec la palette C par défaut (15,8 min d'attente à la porte mémoire derrière un lot d'intégration). Pixels > 16/255,
    `vol_old` contre `vol_ior` (= valeurs par défaut de l'asset), bruit `vol_old` / `vol_old2` entre parenthèses :
    calme_plaine 20,66 % (10,76), eau_vive 22,21 % (3,53), rive_lac 20,20 % (5,17), berge_haute 34,76 % (1,67).

## PROOFS

PROOFS: water-volume-capture

Registre : quatre états (`vol_old`, `vol_on`, `vol_ior`, `vol_old2`) sur cinq caméras. COMPLETE prouve que les
captures sont écrites et que le matériau est à jour (refus d'un matériau sans `VolumeOn` ni Opacity branché) ;
le verdict visuel est celui ci-dessous, pas celui du registre.

## SCN

Vu par l'agent sur les images d'A/B (`Saved/RiverbankEvidence/opacity-ab`, non commitées) : avec le volume
actif, le fond (sable, cailloux, roseaux immergés) se lit sous l'eau, la couleur passe du clair au bord au
sombre au large, la ligne de rive s'adoucit. Avec les valeurs d'origine l'eau devenait turquoise, d'où la
palette C (Alexandre, 2026-10-10). **Le verdict esthétique final reste celui d'Alexandre** sur le rendu de
la palette C dans le jeu : il n'a vu que les planches de comparaison.
Limites : un seul instant (11 h), eau de rivière vue de haut (berge_haute) encore floue par endroits, aucune
séquence animée, nuit et couchers non rephotographiés.

## PLY

UNKNOWN. Aucune preuve joueur.

## ECARTS

AUCUN — `Source/AnastasisSim` non touché ; matériau de rendu seulement.

## INTEGRATION_RISK

- `agent/water-continuity-001` (autre agent, non versée, `HANDOFF_READY: NO` depuis le 2026-10-02) modifie
  `tools/unreal/water-look.py` (normales de rivière, `WaterContinuity`), `riverbank-capture.py`, `proofs.txt`,
  `fiches/eau.md` et **le même `M_AnastasisWater.uasset`**. Les hunks de `water-look.py` sont séparés des
  miens, sauf deux copies **identiques** (liste de nœuds copiée avant suppression, contrôle de `save_asset`). Si les
  deux branches sont versées : fusionner les scripts, puis régénérer l'asset avec `water-look.ps1 -Rebuild` — un
  `.uasset` binaire ne se fusionne pas.
- `eau.md` : mes ajouts (EAU-07 à 09, ligne Optique) peuvent entrer en conflit textuel avec les 17 lignes de l'autre branche.
- Changement visible sur toute l'eau du jeu (lacs, rivières, mer, horizon) : rien n'est lié à une CVar ; `VolumeOn`
  à 0 par instance dynamique redonne l'ancien aspect si besoin d'un repli.

## STOP

- Ne revendique pas : un verdict esthétique définitif, la nuit, l'orage, l'animation, les caustiques, l'écume de
  contact, la réflexion des arbres (le terrain n'est pas dans le cache Lumen), ni une mesure de coût sur plus d'un échantillon.
- Pas d'intégration ni de push.
