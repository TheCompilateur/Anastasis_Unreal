# HANDOFF: pontic-ground-assets-001

## MISSION

Rendre la pierre remaniee des tuiles `Ruin` distincte de la roche naturelle
dans le terrain procedural. La planche fournie par Alexandre sert de reference
visuelle pour les surfaces, pas de source PBR ni d'autorite historique.

## FILES_OWNED

- `tools/unreal/ground-textures.py`
- `tools/unreal/ground-material.py`
- `tools/unreal/ground-cover-capture.py`
- `tools/unreal/proofs.txt`
- `Content/Anastasis/Materials/GroundTextures/T_Ground_Ruin_AH.uasset`
- `Content/Anastasis/Materials/GroundTextures/T_Ground_Ruin_NR.uasset`
- `Content/Anastasis/Materials/M_AnastasisGround.uasset`
- `Content/Anastasis/Materials/MI_AnastasisGround.uasset`
- `.claude/skills/anastasis-realisme/fiches/sol.md`
- `AGENTS.md`

## COMMIT

PENDING

## MEC

- Build initial du worktree: `BUILD::PASS` (19 actions UBA, 264.05 s, compilation locale limitee a une action par la RAM disponible).
- Suite: UNKNOWN
- Source: [Poly Haven, Cobblestone Floor 13](https://polyhaven.com/a/cobblestone_floor_13), CC0, 2K, taille physique 2 m.
- Cartes lues: diffuse JPG, normale DirectX PNG, rugosite JPG, deplacement PNG ; AO neutre car la source ne livre pas d'AO exploitable.
- SHA256 source: diffuse `235e479175a847d13e3e6fca3acc0db3c4edd587b6ab00307c5f6c59d1800ce4` ; normale `957f4f36f30962992aca423456cdb2d37db63135aa8338c07a5c1bbe041f8dc5` ; rugosite `65586076f955668c5f1e37d7cc503af600e07fe0ca0ced8835b7d5397a586ba5` ; deplacement `60018285c1209cb53073296cb4aac30b66cb690c2acb35ef2e79a129bad32001`.
- Empaquetage: albedo detail neutre en moyenne, passe-haut periodique a 35 cm, hauteur 1-99 %, normale sans composante lente, BC7 prevu a l'import.
- `python tools/unreal/ground-textures.py Ruin`: PASS, 2048 x 2048, clipping albedo 0.155 %, rugosite source moyenne 0.879, AO neutre 1.0.
- Controle de couture du paquet AH: ecart RGB moyen bord a bord 7.89/255, voisin immediat 8.09/255. Cela controle le fichier, pas une repetition visible en scene.
- `python -m py_compile` des trois scripts et generation du HLSL: PASS ; 3 lectures triplanaires Ruin sur les axes potentiels.
- `tools/unreal/ground-material.ps1 -Rebuild -TimeoutSec 900`: `GROUND_MATERIAL::PASS` ; 10 textures disponibles, import `Ruin_AH` sRGB et `Ruin_NR` lineaire, `M_AnastasisGround` recompile et sauve, `MI_AnastasisGround` recharge.
- Statistiques Unreal: 1282 instructions pixel, 160 vertex, 12 samplers, contre 1175 pixel / 10 samplers mentionnes pour `soil-slope-002`. Cette comparaison de compilation ne mesure pas les ms GPU.
- Assets Unreal enregistres: `Ruin_AH` 8 310 001 octets, `Ruin_NR` 6 772 377 octets, maitre 139 990 octets, instance 10 086 octets.
- Premiere capture instrumentale `EDITOR_BATCH::PASS 1/1`, mais `hameau_eye` a montre seulement 0,10 % de pixels RGB > 16 entre avant/apres, contre 0,08 % entre avant/temoin. Cette vue regardait surtout au-dela de la bande photo de 4 m : effet visuel NON DEMONTRE. Une pose `hameau_ground` a hauteur humaine, dirigee vers le sol proche du centre de la ruine, a ete ajoutee pour discriminer ce point.
- Deuxieme capture instrumentale `EDITOR_BATCH::PASS 1/1` (`Saved/EditorBatch/20261009-010551/editor-batch.log`). Meme pose `hameau_ground`, avant/apres/temoin a 11 h : 47,93 % des pixels RGB changent de plus de 16/255 avant/apres, contre 0,16 % avant/temoin ; ecart moyen RGB 18,75/18,16/16,15 contre 1,14/1,13/1,14. La photo est effectivement active au sol proche.
- Prairie temoin : 1,63 % de pixels > 16 avant/apres contre 0,69 % avant/temoin, sous le repere de variance du comparateur (~3,6 %) ; inspection visuelle sans pave hors ruine.
- GPU p50 sur la pose `hameau_ground` : 11,246 ms avant, 11,193 ms apres, 10,949 ms temoin. Ces trois nombres n'etablissent aucun gain et ne quantifient pas solidement un cout ; la derive du temoin est du meme ordre.

## PROOFS

PROOFS: ruin-ground-capture

## SCN

PASS borne a `hameau_ground` : lecture directe des trois PNG et mesure A/B/A. La
surface auparavant rocheuse et fissuree montre des pierres irregulieres avec
joints terreux, a une echelle lisible au premier plan. Aucune repetition evidente
dans cette pose, pas de pave visible sur la prairie temoin. La vue `hameau_eye`
reste quasi inchangee car elle regarde au-dela de la bande photo du materiau.
Cela ne demontre pas une amelioration du paysage lointain ni de tous les sols.

KEEP : les pierres remaniees et leurs joints se lisent mieux au premier plan du
hameau, sans repetiton visible ni faux pave sur la prairie temoin, et si le cout
GPU reste dans la derive du temoin. Le cout GPU precis reste UNKNOWN.

## PLY

UNKNOWN. Pas de marche humaine normale verifiee.

## INTEGRATION_RISK

- `ground-material.py` et `M_AnastasisGround` sont des autorites partagees par d'autres missions visuelles ; rebase et regeneration si ces fichiers ont change sur `main`.
- Le masque `Ruin` utilise la signature existante `RockW=0.85`, `Worked=0.10` ; `Stone` garde `Worked=0`. Aucune nouvelle valeur n'est ecrite dans la simulation ni le maillage.
- Reachability: `AnastasisPlaces::Compose` choisit le hameau depuis `Components(ETileType::Ruin, true)` ; la camera `hameau_ground` se pose au coeur de son emprise et confirme les pixels du sol.
- L'effet est local aux ruines et s'eteint avec la distance photo du sol ; aucun pave n'est ajoute a la geometrie ou a la collision.
- Les deux textures ajoutees doivent etre suivies par Git LFS. Le materiau maitre doit compiler, etre recharge, puis compare en A/B/A.

## STOP

Ce scan ne prouve ni fidelite archeologique a 1204, ni gain GPU, ni effet joueur.
Le verdict de capture `COMPLETE` reste instrumental ; SCN exige inspection des images.
Le chemin de village reste une suite distincte : `AnastasisHumanGeography` calcule
deja `RoadWeight`, mais ce poids n'est pas encore un canal du materiau de sol.
Une nouvelle photo de chemin sans ce branchement ne serait pas visible dans le jeu.
