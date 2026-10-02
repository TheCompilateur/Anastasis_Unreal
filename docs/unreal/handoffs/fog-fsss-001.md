# HANDOFF: fog-fsss-001

## MISSION

Essayer le Fog Screen Space Scattering d'UE 5.8 (expérimental, RU-002-06) sur le brouillard de hauteur :
le brancher dans la couche réalisme derrière un interrupteur, le mesurer en A/B (image, couleur, ms), et
décider s'il entre. Verdict : **il n'entre pas**. L'interrupteur reste, coupé par défaut.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldAtmosphere.cpp` : CVars
  `anastasis.Atmosphere.FogScattering` (0 par défaut) et `anastasis.Atmosphere.FogScattering.SceneColor`
  (−1 = le profil, surcharge pour l'A/B) ; propriétés FSSS du composant de brouillard écrites dans
  `ApplyRealism` (pas de setter en 5.8 : écriture puis `MarkRenderStateDirty`, seulement sur changement) ;
  champ `fog_scattering=` dans la ligne `ANASTASIS_ATMOSPHERE`.
- `Source/Anastasis_UnrealV2/WorldView/AnastasisAtmosphereProfile.h` : `bFogScreenSpaceScattering`,
  `FogScatteringSpreadScale` (0,1), `FogScatteringSceneColorScale` (1) — défauts du moteur.
- `Source/Anastasis_UnrealV2/WorldView/AnastasisAtmosphereTests.cpp` : test
  `Anastasis.Atmosphere.Realism.FogScattering` ; `FScopedRealismCVar` prend un nom de CVar.
- `tools/unreal/capture-sky.py`, `capture-sky.ps1` : GPU p50 de `stat unit` par image dans `sky.json`,
  lignes `SKY_SHOT_OK` à l'écran.
- `AGENTS.md` (ligne de `capture-sky`), `.claude/skills/anastasis-realisme/fiches/atmosphere.md`,
  `registre.md` (RU-002-06 → `REJETÉ`).
- `docs/unreal/handoffs/fog-fsss-001.md`

## COMMIT

voir `git log agent/fog-fsss-001`

## MEC

- BUILD / TESTS : `finish` (build + `report-tests`), voir sa sortie.
- Moteur lu : `bEnableFSSS` (false par défaut) et `r.Fog.ScreenSpaceScattering` (1) ; sans le premier,
  rien ne se passe.
- `capture-sky.ps1 -Label fsss-ab` (64 images ; 8 situations h06 à h20 × sec / humide / saturé × off / on,
  binaire de `main` 9ef7bec) : écart d'image 0,1 à 18,8 % de pixels > 16/255, crêtes lointaines fondues,
  hautes lumières adoucies (`white` 0,009 → 0,001 au coucher), « mur » en contre-jour réduit
  (0,191 → 0,141 le matin saturé) ; aucune alerte nouvelle d'`atmosphere-metrics.py` (les 3 `HAZE`
  existent sans). Mais toutes les vues plus sombres : −1,5 à −4,5 de luminance. GPU +0,34 ms (médiane
  sur 32 paires, −0,21 à +0,66), machine chargée par deux autres éditeurs.
- `capture-sky.ps1 -Label fsss-temoin` (off / on / off) : la dérive entre les deux « off » est de 0,3 au
  plus ; l'assombrissement vient bien de l'effet. Vues en forêt : le vent fait déjà 8 à 15 % de pixels
  d'écart entre deux « off » ; seules l'oblique et la vallée jugent l'image.
- `capture-sky.ps1 -Label fsss-scenecolor` (h09 saturé, h16 et h18 humides × off / SceneColor 1 / 0,5 / 0
  / off, binaire de `main` 22db051 + cette branche) : luminance contre « off » : dosage 1 −0,4 à −5,6 ;
  dosages 0,5 et 0 entre +0,2 et −0,7 (−1,9 dans une vallée où le témoin dérive de −2,7). Écart d'image
  à 0 et 0,5 : celui du témoin (vallée h16 : 1,27 % contre 1,02 % ; oblique : 0,8 % contre 0,1–0,25 %).
  GPU : +0,22 / +0,28 / +0,35 ms (médianes). Images regardées : `valley_long_h16_humid_sc0` ≈ off (la crête
  lointaine reste une bande nette), `_sc1` : ciel gris plus sombre, crête dissoute.
- Conclusion : dans le brouillard du projet (densité 0,012), l'effet visible est la couleur de scène
  injectée, et elle assombrit sous une exposition fixe (ECL-01 interdit de compenser). Sans elle, rien de
  visible, mais toujours ~0,3 ms. `anastasis.Atmosphere.FogScattering` reste à 0.

## PROOFS

Preuves PIE que le lot rejoue pour cette mission, noms de `tools/unreal/proofs.txt` (EDITOR_QUEUE_001) :

PROOFS: (aucune)

## SCN

`Lvl_AnastasisSlice` dans un éditeur dédié (`capture-sky`), atmosphère réappliquée par état. Rien n'est sauvé.

## PLY

`PLAYER` non touché.

## INTEGRATION_RISK

- `AnastasisWorldAtmosphere.cpp` est chaud : `eye-plane-001` y a ajouté `anastasis.Depth.EyePlane` et le
  champ `eye_plane=` du résumé pendant cette mission ; conflit résolu au rebase en gardant les deux. Un
  test qui découpe la ligne `ANASTASIS_ATMOSPHERE` par position verrait un champ de plus
  (`fog_scattering=` avant `forward_light=`).
- Effet coupé par défaut : aucune image du jeu ne change.

## STOP

- Pas mesuré par brouillard dense (orage, brume épaisse), là où la diffusion multiple existe vraiment ;
  c'est la condition pour rouvrir RU-002-06.
- Les deux premières séries ont été prises avant `surround-look-001` / `eye-plane-001` : l'assombrissement
  en forêt y était plus fort (−3 à −4) que dans la dernière (−0,5 à −0,8). Le verdict tient sur les trois.
