# HANDOFF: canopy-shell-fix-001

## MISSION

Corriger l'enveloppe de canopee de `hero-canopy-001` (`SM_CanopyShell`), vue en PIE par
Alexandre comme des boules vertes geantes, sans tronc, flottant au-dessus de la foret.

Cause (premier jet, jamais regarde : `SCN: UNKNOWN` dans `hero-canopy-001.md`) :

1. echelle uniforme : rayon du massif = trunk le plus lointain x 1,35 (12 m minimum), donc
   un dome de 30 m et plus de haut au-dessus d'arbres de 6 a 16 m ;
2. base posee a `GroundZ + 0,22 x rayon` : 7 a 9 m au-dessus du sol pour un grand massif ;
3. sol echantillonne au centre seul ;
4. donnee d'instance 0 (secheresse) laissee a 0 : vert le plus vif du materiau, plus clair
   que tout arbre dessous ;
5. plafond de 240 enveloppes atteint, attribuees dans l'ordre des cles de grille : une bande
   arbitraire de la carte ;
6. les arbres de production ne sont jamais coupes : l'enveloppe s'ajoute a eux, elle ne
   remplace rien.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisHeroCanopy.h`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisHeroCanopy.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisHeroCanopyTests.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp` (bloc enveloppe, CVar)
- `docs/unreal/handoffs/canopy-shell-fix-001.md`

## COMMIT

BRANCH_HEAD

## MEC

- BUILD: PASS (`tools\unreal\anastasis-unreal.ps1 build`).
- TESTS: PASS 1, KNOWN_EXPECTED_FAILURE 0, FAIL 0 (`tools\unreal\report-tests.ps1 -Filter Anastasis.HeroCanopy`).
  `Anastasis.HeroCanopy.Select` controle desormais : deux massifs sur trois (le troisieme sans hauteur
  rendue), le plus dense d'abord, sol = moyenne des troncs (500 cm), sommet <= mediane, base entre sol
  et sommet, rayon < 15 m malgre un tronc a 27 m au coin de la cellule, teinte = secheresse moyenne.
- Suite `Anastasis` complete : non lancee par cette mission (Alexandre a demande ce soir d'arreter
  les tests). Le lot de l'integrateur la rejoue.
- Enveloppe corrigee (C++) :
  - cellule de massif 30 m, au moins 4 troncs ; rayon = distance mediane des troncs x sqrt(2) + 2,5 m,
    borne 6 a 18 m ;
  - hauteur independante de la largeur : de 0,4 a 0,92 x la hauteur rendue mediane du massif ;
  - posee sur le sol moyen des troncs, lacet deterministe par massif ;
  - teinte : secheresse moyenne du massif en donnee d'instance 0 ;
  - plafond 600, les massifs les plus denses d'abord. Log reel : `stands=616 shells=600 placed_shells=597`.
- `anastasis.Dressing.CanopyShell` (nouvelle, defaut 0) : l'enveloppe ne se pose plus par defaut. Les
  huit heros restent sous `anastasis.Dressing.HeroCanopy` 1.

## PROOFS

Aucune preuve PIE du registre `tools/unreal/proofs.txt` ne cadre l'enveloppe ; la preuve est
l'A/B de captures ci-dessous, faite avant rebase sur `5fa5853`.

PROOFS: (aucune)

## SCN

`tools\unreal\capture-slice.ps1 -Mode 2`, sorties dans `Saved\SliceEvidence\` du worktree.

- Lisiere a 1,7 m, `-CamLoc '73614,113406,953' -CamRot '2,173.7'` :
  `canopy_fix_on.png` (geometrie corrigee, avant teinte) / `canopy_fix_off.png` (`HeroCanopy 0`).
  Plus de dome ni de nuage, mais des galettes vert vif sans tronc qui masquent les cypres.
- Vue oblique a 58 m, `-CamLoc '79583,87541,5869' -CamRot '-9.1,54.5'` : `far_on.png` / `far_off.png`
  (teinte corrigee). `compare.py` : 2,09 % de pixels > 16/255, sous la variance de capture (~3,6 %).
  Au-dela de 300 m l'enveloppe n'ajoute rien de mesurable ; son seul effet visible est l'artefact
  entre 70 et 200 m. D'ou le defaut 0.
- Etat par defaut, meme lisiere : `edge_default.png`.

## PLY

UNKNOWN. Pas de PIE rejoue sur la vue de la capture d'Alexandre.

## INTEGRATION_RISK

- `AnastasisWorldEmbodiment.cpp` est un fichier chaud (contact au sol, village).
- Qui attendait l'enveloppe par defaut la perd : `anastasis.Dressing.CanopyShell 1` la rend, avant
  l'incarnation.
- `SM_CanopyShell` n'est pas regenere : un mesh de 6,1 m x 6,7 m de rayon, etire jusqu'a ~2,7 x en
  largeur pour un massif de 18 m.

## STOP

- Pas de vraie masse lointaine (imposteurs, LOD de foret) : c'est ce qu'il faudrait pour que
  l'enveloppe serve, mission a part.
- Pas de retouche des heros, de `SM_Tree_*`, des materiaux, ni de `create-hero-trees.py`.
- Pas de suite complete, pas d'integration, pas de push.
