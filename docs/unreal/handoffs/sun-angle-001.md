# HANDOFF: sun-angle-001

## MISSION

Trancher l'entrée ouverte de `fiches/eclairage.md` : la taille apparente du soleil (`LightSourceAngle`),
jamais posée par le projet, jamais mesurée en A/B. Verdict : la valeur du moteur (0,5357°) est le disque
solaire réel et donne la pénombre la plus juste ; rien à changer dans le jeu. Documentation, outil d'A/B,
piège consigné.

## FILES_OWNED

- `tools/unreal/sun-angle.py` (nouveau : pose l'angle par le setter, relit la valeur)
- `AGENTS.md` (une ligne d'index)
- `.claude/skills/anastasis-realisme/fiches/eclairage.md` (ECL-06 réécrite, état, Vérifier, Ne pas faire,
  entrée Ouvert retirée)
- `docs/unreal/PIEGES_UNREAL.md` (piège de la commande `set`)
- `docs/unreal/handoffs/sun-angle-001.md`

## COMMIT

voir `git log agent/sun-angle-001`

## MEC

- BUILD: `finish` (aucun fichier sous `Source/`, `Config/`, `Content/`)
- TESTS: idem
- Moteur lu : `UDirectionalLightComponent::LightSourceAngle = 0.5357f` (« Angle of earth's sun »),
  `LightSourceSoftAngle = 0` ; VSM SMRT directionnel : `RayCountDirectional` 7, `SamplesPerRayDirectional` 8.
  Projet : aucune écriture de `LightSourceAngle` (`Source/`, `tools/unreal/`, `Config/`).
- `capture-sky.ps1 -Label sun-angle` (commande `set DirectionalLightComponent LightSourceAngle`) :
  **invalide**. Contrôle `sun-angle-check` à 10° : ombres inchangées. La commande ne touche pas les
  lumières posées ; piège consigné dans `PIEGES_UNREAL.md`.
- `capture-sky.ps1 -Label sun-angle-check2` (setter Python, `SUN_ANGLE_SET … angle=10.0000` au log) :
  à 10°, les ombres de feuillage deviennent une tache floue ; le réglage agit.
- `capture-sky.ps1 -Label sun-angle-2` : midi sec et 16 h humide × 0,5357° / 0° / 1° / 0,5357° (témoin),
  quatre vues, 15 lignes `SUN_ANGLE_SET`. Pixels > 16/255 contre 0,5357° :
  | vue | témoin | 0° | 1° |
  |---|---|---|---|
  | crête, midi | 4,71 % | 9,87 % | 9,49 % |
  | contre-jour, midi | 3,60 % | 5,03 % | 6,18 % |
  | crête, 16 h | 12,11 % | 13,30 % | 12,98 % |
  | contre-jour, 16 h | 8,96 % | 15,66 % | 14,32 % |
  | oblique et vallée | ≤ 0,69 % | ≤ 1,41 % | ≤ 0,93 % |
  Les vues en forêt portent le bruit du vent ; l'écart ne se juge qu'à l'œil sur les bords d'ombre.
  GPU p50 contre 0,5357° : 0° +0,23 ms, 1° +0,42 ms, témoin +0,16 ms (médianes sur 8 vues) : rien de
  mesurable.
- Image regardée : `sun-angle-2/crop_a0_a05_a1.png` (ombres de feuillage, crête, 16 h). 0° : bords
  tranchants, polygonaux ; 0,5357° : pénombre douce ; 1° : flou au-delà du réel.

## PROOFS

Preuves PIE que le lot rejoue pour cette mission, noms de `tools/unreal/proofs.txt` (EDITOR_QUEUE_001) :

PROOFS: (aucune)

## SCN

`Lvl_AnastasisSlice` dans un éditeur dédié (`capture-sky`), angle posé par état. Rien n'est sauvé.

## PLY

`PLAYER` non touché.

## INTEGRATION_RISK

- Aucun : documentation et un script Python appelé à la main.

## STOP

- La lune prend le même angle dans le test ; son réglage propre n'a pas été jugé (nuit non capturée).
- `LightSourceSoftAngle` (bord doux supplémentaire) non essayé : 0 par défaut, et le disque réel suffit.
