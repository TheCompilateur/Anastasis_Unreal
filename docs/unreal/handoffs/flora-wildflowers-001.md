# HANDOFF: flora-wildflowers-001

## MISSION

La prairie n'avait qu'une couleur : de l'herbe verte puis paille, et un seul « petit blanc » de sous-bois. Mandat
d'Alexandre (2026-10-02) : « il manque beaucoup d'assets flore… pas assez de plante, la nature ne fait pas son
œuvre ». Livré : des **fleurs sauvages** qui poussent par dérives, comme dans une vraie prairie de début d'été.

- **Trois familles** dans `AnastasisGroundCover` (`EFamily::FlowerWarm / FlowerCool / FlowerWhite`), neuf
  espèces dessinées en géométrie (tige, cœur hexagonal, pétales en cerf-volant à l'inclinaison propre à
  l'espèce) : coquelicot, bouton-d'or, épervier ; bleuet, chicorée, sauge des prés ; marguerite, ombelle,
  achillée. Chaque mesh est une touffe de prairie (64 lames) piquée de 18 à 22 fleurs.
- **Dérives, pas semis** : un champ de taches d'environ 15 m décide où l'on fleurit (jusqu'à 1 cellule de
  prairie sur 4 dans une dérive, ~1 sur 100 hors dérive) ; un second champ plus large choisit l'espèce
  dominante de la tache, avec 20 % de mélange ; le sec et le chaud pour les coquelicots, le frais pour les
  bleuets, partout pour les claires. À l'ombre et sur sol piétiné : peu de fleurs ; jamais en lande, en
  sous-bois ni sur sol saturé (laiches).
- **Une fleur REMPLACE une touffe d'herbe** : même nombre d'instances (test), la prairie ne se troue pas.
- Tuiles de 480 m pour ces trois familles (clairsemées) : +96 HISM sur 1 872 (+5 %), pas +300.
- `anastasis.Dressing.Wildflowers` (1 par défaut, 0 = la prairie d'avant, `FlowerShare` 0,24).
- Sans l'asset d'une famille, l'incarnation pose la touffe d'herbe à sa place.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisGroundCover.h/.cpp`, `AnastasisGroundCoverTests.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp` (CVar, tuiles, repli, journal `ANASTASIS_WILDFLOWERS`)
- `tools/unreal/create-ground-cover.py/.ps1` (`-Only flowers`), `ground-cover-capture.py` (états `noflowers` / `flowers`), `proofs.txt`
- `Content/Anastasis/GroundCover/SM_Grass_FlowerWarm/Cool/White_01`
- `docs/visual/wildflowers-001/`, `AGENTS.md` (index)

## COMMIT

PENDING

## MEC

- BUILD: voir `finish`
- TESTS: `report-tests.ps1 -Filter Anastasis.GroundCover` → 9 PASS, 0 FAIL, dont le nouveau `Anastasis.GroundCover.Wildflowers`
  (rien sans `FlowerShare` ; les trois familles fleurissent, 2 à 20 % de la prairie ; dérives : la cellule de 15 m la
  plus fleurie a plus du double de la moins fleurie ; déterministe ; mêmes touffes qu'en référence ; aucune fleur en
  lande, sur sol saturé, ni sous les couronnes ; champ de couverture inchangé)
- COMMANDS:
  - `tools\unreal\create-ground-cover.ps1 -Only flowers` → `GROUND_COVER_ASSETS::PASS` : triangles LOD0 `[996,499,115]` / `[997,427,83]` / `[1141,583,124]` (v1, avant doublement de la taille des têtes ; v2 régénérée)
  - `tools\unreal\editor-batch.ps1 -Proofs wildflowers-capture` → `PROOF::PASS`
  - Journal : `ANASTASIS_WILDFLOWERS enabled=1 warm=91275 cool=36731 white=40151 share_of_meadow=0.1609`

## PROOFS

PROOFS: wildflowers-capture

## SCN

**KEEP.** Images regardées : `docs/visual/wildflowers-001/`. La v1 (têtes de 2 à 3 cm, 6 % de la prairie) était
**invisible** à 5 m : des points rouges et jaunes noyés dans l'herbe. La v2 (têtes ×1,9, 18 à 22 fleurs par touffe,
64 lames de fond, 16 % de la prairie) se lit : nappes de coquelicots, de boutons-d'or et de marguerites dans la
prairie, à hauteur d'œil. Vue aérienne : les fleurs ne se voient pas (trop petites), et ce n'est pas revendiqué.
Les nappes bleues (bleuets, chicorée) sont les moins fréquentes dans cette carte (sol humide ou sec selon l'hydrologie).
L'objet bleu dans la rivière (vue `riviere_eye`) existe sans fleurs : ce n'est pas le mien.

**Coût (RTX 3060, viewport éditeur 1280×720, p50 GPU)**, état `noflowers2` (témoin) → `flowers` : prairie 15,16 → 14,65 ;
lisière 12,93 → 12,41 ; rivière 16,09 → 16,07 ; aérien 13,16 → 13,10. Aucun surcoût mesurable (le premier état de la série,
`noflowers`, est un échauffement et n'est pas retenu). Aucune mesure en jeu packagé.

## PLY

UNKNOWN : aucune session de jeu.

## ECARTS

AUCUN — `Source/AnastasisSim` inchangé.

## INTEGRATION_RISK

- `AnastasisGroundCover.cpp` / `AnastasisWorldEmbodiment.cpp` / `create-ground-cover.py` / `ground-cover-capture.py` /
  `proofs.txt` sont chauds : `natural-history-001` et d'autres y ont écrit. Le branchement des fleurs est un bloc
  autonome dans la branche « prairie » de `Build`.
- `create-ground-cover.py` : sans `-Only`, il régénère TOUTES les familles et réécrit `M_AnastasisGrass`. N'utiliser
  que `-Only flowers` pour ne pas toucher les assets des autres.
- Les fleurs changent le champ de couverture du sol seulement comme l'herbe qu'elles remplacent (compté « prairie »).
- Les captures des autres missions qui montrent la prairie changent d'aspect par défaut.

## STOP

Ce que cette mission ne revendique pas : les fleurs dans les clairières de sous-bois, la lande (callune seule), la
saisonnalité (une seule saison fixe), des fleurs visibles de loin ou des airs, la flore des berges et des sols
humides, et un coût en jeu packagé.
