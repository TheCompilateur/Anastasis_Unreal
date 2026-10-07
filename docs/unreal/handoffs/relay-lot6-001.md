# HANDOFF: relay-lot6-001

## MISSION

Relais d'integration (session co-integratrice, mandat d'Alexandre du 2026-10-07) : rejouer sur main, dans
l'ordre fixe par le tri, quatre missions dont l'agent est hors ligne et qui se heurtaient a main, en gardant
chaque apport. Les branches d'origine ne sont pas touchees ; leurs commits sont cites par `cherry-pick -x`.

1. weather-wind-003 -- forme deja resolue d39763d5 (de weather-drying-004), Source/tools identiques a 9bad4056.
2. tree-leafcards-001 -- 744edcc0, 6dd81697.
3. riparian-transition-004 -- 0b2bed56, 8c4183ba (apres leafcards : meme script de capture).
4. soil-water-budget-001 -- ecf2c8da (ecart n° 37).

## FILES_OWNED

Ceux des quatre missions, plus les resolutions :
- `tools/unreal/ground-cover-capture.py` : etats fleurs (main) + cartes (leafcards) + riparian, chaines
  woodland -> riparian -> ecotone -> natural ; `if natural_run or flower_run or cards_run`.
- `AnastasisGroundCover.h`, `AnastasisWorldEmbodiment.cpp`, `AnastasisGroundCoverTests.cpp` : `FlowerShare`
  (wildflowers-001, main) ET `bRiparianTransition` ; test Wildflowers ET test RiparianTransition.
- `AGENTS.md` (ligne capture-ground-cover : Riparian ajoute avant Ecotone), `.claude/skills/anastasis-capture/SKILL.md`
  (sections Woodland et Riparian), `tools/unreal/proofs.txt` (tree-cards-capture, riparian-transition-capture).
- `Source/AnastasisSim/ECARTS.md` : fiche n° 37 placee apres n° 33.

## COMMIT

PENDING

## MEC

- BUILD: voir finish
- TESTS: au lot (`Anastasis.GroundCover.Wildflowers`, `Anastasis.GroundCover.RiparianTransition`, suite complete)
- COMMANDS:
  - `git cherry-pick -x d39763d5 744edcc0 6dd81697 0b2bed56 8c4183ba ecf2c8da` avec resolutions ci-dessus

## PROOFS

PROOFS: tree-cards-capture, riparian-transition-capture

## SCN

Captures des deux preuves au lot (completion technique ; le verdict artistique reste a faire par Alexandre).

## PLY

Sans objet.

## ECARTS

- n° 37 OUVERT (soil-water-budget-001, EXTENSION, A_TRANCHER) : reserve d'eau des champs, desactivee par
  defaut (`anastasis.Village.SoilWaterBudget=0`), marques `ecart n°37` dans AnastasisVillage.cpp/.h.
- Aucun autre ecart ouvert, modifie ou ferme.

## INTEGRATION_RISK

- `anastasis.Dressing.TreeCards=1` par defaut : change l'aspect de tous les chenes verts (leafcards-001).
- `anastasis.Dressing.RiparianTransition=1` par defaut (riparian-transition-004).
- Le cap de vent par defaut suit la simulation (weather-wind-003).
- weather-drying-004 contient une copie de weather-wind-003 : au rebase, ce commit disparaitra de sa branche.
- Resolution manuelle du script de capture : woodland et riparian ne s'executent pas ensemble (etats exclusifs).

## STOP

- Ne verse ni weather-dry-001 (parite meteo JS au bit pres probablement cassee, mandat requis), ni
  anthropic-wood-001 (second systeme de transport concurrent du porteur de main), ni player-food-loop-001.
- Aucun verdict artistique.
