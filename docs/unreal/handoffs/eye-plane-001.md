# HANDOFF: eye-plane-001

## MISSION

Un seul plan net à 1,7 m, puis le grain qui se paie. `anastasis.Depth.EyePlane`
(défaut 1) garde les 7 premiers mètres hors du brouillard volumétrique et fait
tomber la couleur vers l'air jusqu'à 120 m. Le sol, la couronne, l'écorce et la
pierre perdent leur détail fin avant cette distance. Le roseau qui coupe le cadre
reste net.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldAtmosphere.h` / `.cpp` — `ApplyEyePlane`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisAtmosphereTests.cpp` — le test réalisme force le plan à 0, puis vérifie le plan à 1
- `tools/unreal/eye-plane-capture.ps1` / `.py` — même cadrage, CVar 0 puis 1
- `tools/unreal/poly-realism.ps1` — régénère le sol puis les trois matériaux primitifs
- `tools/unreal/ground-material.py` — fondu du grain, lustre du trait d'eau
- `tools/unreal/create_tree_asset.py` — couronne, écorce, pierre ; `ANASTASIS_TREE_MATERIALS_ONLY=1` ne régénère pas les meshes
- `Content/Anastasis/Materials/M_AnastasisGround.uasset`, `MI_AnastasisGround.uasset`
- `Content/Anastasis/Materials/M_AnastasisVegetation.uasset`, `M_AnastasisBark.uasset`, `M_AnastasisRock.uasset`
- `AGENTS.md` — index
- cette fiche

## COMMIT

BRANCH_HEAD

## MEC

- BUILD: `BUILD::PASS` (`tools\unreal\anastasis-unreal.ps1 build`, worktree `eye-plane-001`)
- TESTS: `Anastasis.Atmosphere.Realism.Reversible` Result={Success}. La suite complète est celle de `finish`.
- PLAN NET, même cadrage, seed 12345, azimut 120, œil z=1894, coupe à 211 cm, regard 12983 cm. `compare.py` `eye_off` → `eye_on` (`Saved/EyePlaneEvidence/eye-plane-003/`) :
  - 27,08 % des pixels > 16/255 (bruit de capture ~3,6 %)
  - herbe sous les pieds : saturation 0,486 → 0,488, écart RGB 0,1
  - eau : +12,9 / +9,0 / +6,5 RGB, saturation 0,078 → 0,066
  - rive : +12,0 / +10,5 / +9,0 RGB, saturation 0,078 → 0,066
  - ciel : saturation 0,190 → 0,105
  - log : `on=1 start_cm=700 vol_start_cm=700 vol_dist_cm=12000 max_opacity=0.48 aerial=3.45 extinction=3.50`
- GRAIN, même cadrage, plan net déjà allumé. `eye-plane-003/eye_on.png` → `poly-realism/eye_on.png` :
  - 19,26 % des pixels > 16/255, luminosité inchangée (133,7 → 133,6)
  - écart local du sol : pieds 35,0 → 31,4, haut de talus 31,6 → 28,3
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\eye-plane-capture.ps1 -Label eye-plane-003`
  - `tools\unreal\poly-realism.ps1`
  - `tools\unreal\eye-plane-capture.ps1 -Label poly-realism` avec `ANASTASIS_EYE_STATES` réduit à l'état `on`

Valeurs. Plan net : départ volumétrique 700 cm, fondu 1800 cm, distance 12000 cm, extinction ×3,5, perspective aérienne ×1,15 (3,00 → 3,45), opacité max 0,48. Une extinction ×8 à 160 m a aplati le ciel (saturation 0,190 → 0,040) : refusée. Sol : `DetailFade` 250–1600 cm, `TexFade` 400–2000 cm, `DampRoughness` 0,22, `DampSpecular` 0,55.

## SCN

La carte, la graine et le réseau ne changent pas. Le cadrage est choisi par le script : plus bas quart du relief, regard > 40 m, un obstacle entre 80 et 260 cm.

## PLY

UNKNOWN — PLAYER reste NOT_IMPLEMENTED. Aucun personnage, aucun contrôle.

## INTEGRATION_RISK

- `AnastasisWorldAtmosphere` est aussi touché par surround-look : le plan net s'applique après le réalisme, il ne remplace pas l'échelle aérienne du profil quand la CVar est à 0.
- Les `.uasset` de sol, feuillage, écorce et pierre sont la sortie des scripts. Un conflit binaire se tranche en régénérant, pas à la main.
- Le trait d'eau est faible dans ce cadrage (+1 niveau). Les oliviers à 130 m ne montrent pas la cassure de couronne : ils sont déjà dans la brume.

## STOP

- Ne revendique pas un ciel intact : il pâlit (saturation 0,190 → 0,105).
- Ne revendique pas que l'écorce et la pierre se lisent dans la prise à 130 m.
- Ne revendique pas la suite `Anastasis` entière : un seul test nommé, le reste est `finish`.
- Pas de joueur. Pas de poussée distante.
