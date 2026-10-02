# HANDOFF: tsr-flicker-001

## MISSION

Savoir si la détection de géométrie fine du TSR (`r.TSR.ThinGeometryDetection`, 0 par défaut en 5.8.2,
RU-002-16) réduit le scintillement de l'herbe et des branches. Un scintillement est temporel : une
capture fixe ne le montre pas. La mission écrit donc d'abord l'outil qui le mesure en images
successives, puis fait l'A/B. Aucun réglage du jeu n'est changé.

## FILES_OWNED

- `tools/unreal/tsr-flicker-pie.ps1`, `tools/unreal/tsr-flicker-pie.py` (nouveaux)
- `tools/unreal/tsr-flicker-metrics.py` (nouveau, hors éditeur)
- `AGENTS.md` (deux lignes d'index)
- `.claude/skills/anastasis-realisme/registre.md` (RU-002-16 → `REJETÉ`, mesure)
- `.claude/skills/anastasis-realisme/fiches/post-traitement.md` (Vérifier, Ne pas faire, Ouvert)
- `docs/unreal/handoffs/tsr-flicker-001.md`

## COMMIT

voir `git log agent/tsr-flicker-001`

## MEC

- BUILD: `finish` (aucun fichier sous `Source/`, `Config/`, `Content/`)
- TESTS: idem
- Outil vérifié sur séquences synthétiques : dérive linéaire → flicker 0,00 ; clignotement ±10 →
  12,54, ±3 → 3,77 ; verdict `REVENDICABLE` sur le seul cas qui change.
- COMMANDS et valeurs (`flicker_detail` = moyenne de |dérivée seconde temporelle| sur le quart de
  l'image au plus fort gradient, en niveaux sur 255 ; 24 images par séquence, `Shot` en PIE, AA méthode 4
  = TSR lu dans le moteur) :
  - `tsr-flicker-pie.ps1 -Label ab-001` (vent actif) :
    | vue | d0 | d1 | d0b | effet | témoin | verdict |
    |---|---|---|---|---|---|---|
    | prairie 1,7 m | 5,36 | 4,34 | 4,83 | −0,76 | 0,53 | dans le bruit |
    | prairie ras du sol | 3,06 | 3,16 | 3,30 | −0,01 | 0,24 | dans le bruit |
    | lisière | 2,81 | 2,70 | 2,67 | −0,05 | 0,14 | dans le bruit |
  - `tsr-flicker-pie.ps1 -Label static-001 -States 'd0=slomo 0.0001;r.TSR.ThinGeometryDetection 0|d1=…1|d0b=…0'`
    (temps gelé : seul le tremblement sous-pixel du TSR change d'une image à l'autre) :
    | vue | d0 | d1 | d0b | effet | témoin | verdict |
    |---|---|---|---|---|---|---|
    | prairie 1,7 m | 6,77* | 2,49 | 2,59 | (−0,10 contre d0b) | — | dans le bruit |
    | prairie ras du sol | 1,65 | 1,60 | 1,65 | −0,05 | 0,00 | dans le bruit |
    | lisière | 1,75 | 1,69 | 1,75 | −0,06 | 0,00 | dans le bruit |
    \* première séquence du run, monde pas encore posé (intervalle 0,52 s, mouvement ×8) : écartée.
- Images regardées : `ab-001/prairie_eye_d0/f05.png` (vue du jeu, bandes noires du viewport PIE),
  `flicker_prairie_eye_d0.png` (l'écart est sur les épis que le vent déplace, pas sur des pixels isolés).
- Deux défauts de l'outil trouvés par ces runs et corrigés : un témoin nul (temps gelé, image
  déterministe) rendait revendicable un écart infime → seuil absolu de 0,25 niveau ; la première
  séquence d'un run était polluée → 8 s de stabilisation pour elle. Un passage `static-002` avec l'outil
  corrigé était prévu ; il n'a pas tourné (MAIN_LOCK d'un lot d'intégration), d'où les valeurs ci-dessus
  re-mesurées par `tsr-flicker-metrics.py` corrigé, captures inchangées.

## PROOFS

Preuves PIE que le lot rejoue pour cette mission, noms de `tools/unreal/proofs.txt` (EDITOR_QUEUE_001) :

PROOFS: (aucune)

## SCN

`Lvl_AnastasisSlice` en PIE, caméra de preuve non sauvée ; ciel épinglé à midi sec, simulation gelée
(`anastasis.Sim.TimeScale 0`), `r.MotionBlurQuality 0`, `t.MaxFPS 30`. Rien n'est sauvé.

## PLY

`PLAYER` non touché.

## INTEGRATION_RISK

- `tsr-flicker-pie.py` lit les `Shot` dans `Saved/Screenshots/` : un autre script qui y écrit au même
  moment dans le même worktree lui volerait un fichier. Un worktree, une capture à la fois.

## STOP

- `Shot` ne sert qu'une image toutes les ~0,2 s : la mesure n'est pas image par image. Avec le vent, le
  mouvement des épis domine ; le passage temps gelé isole l'anticrénelage, mais ne montre pas un
  scintillement qui naîtrait seulement du mouvement à 30 images/s.
- Coût GPU de la détection : non mesuré, puisqu'elle n'est pas activée.
- Trois vues, midi : pas de nuit, pas de contre-jour.
