# HANDOFF: tree-leafcards-001

## MISSION

Sortir le chêne vert du « poly » : remplacer ses lames de feuilles opaques (8 triangles chacune, des
centaines par couronne) par des CARTES de grappes de feuilles à masque alpha. Mandat d'Alexandre
(2026-10-02) : « arbres », dans la suite de « il manque beaucoup d'assets flore… sois brutal comme un
graphiste numérique Unreal ». `tree-canopy-002` avait conclu qu'amincir les lames dépeuple les couronnes
et qu'il faut « une meilleure solution de géométrie/LOD » : voici cette solution, sur une espèce.

- **Atlas dessiné, pas téléchargé** (`tools/unreal/leaf-atlas.py`, Python système) : 1024², quatre cases
  (chêne vert ×2, olivier, platane) + un bloc opaque. Déterministe. Albédo stocké à DETAIL_MEAN 0,45 :
  la teinte reste dans la couleur de sommet, comme le sol.
- **Matériau** `M_AnastasisFoliageCard` : duplicata de `M_AnastasisVegetation` (même vent, même teinte par
  instance, même transmission) + albédo de l'atlas + masque alpha (Masked, seuil 0,42, couverture alpha
  conservée dans les mips) + normale du sommet.
- **Mesh** `SM_Tree_HolmOak_Card_01..03` : même tronc, mêmes branches, même graine que `SM_Tree_HolmOak_01..03`
  (fonctions de `create_tree_asset.py`) ; seule la couronne change : 78 cartes par lobe (≈ 780), un quad à
  trois rangées incurvé, **normales sphériques** de la couronne entière (dôme doux), **occlusion peinte**
  (sombre au cœur, clair en surface), axes de gerbe mélangés au hasard (des gerbes purement radiales
  faisaient un palmier à 70 m). LOD1 : 75 % des cartes ×1,15 ; LOD2 : masses fermées sur le bloc opaque.
- **Branchement** (`AnastasisPresentationResolver.cpp`, une seule fonction) : `anastasis.Dressing.TreeCards`
  (1 par défaut, 0 = lames) remplace le mesh et le matériau de fente 0 des variantes HolmOak ; sans l'asset,
  l'arbre garde ses lames. Aucune donnée (`DA_AnastasisPresentation`) n'est modifiée.

## FILES_OWNED

- `tools/unreal/leaf-atlas.py`, `create-tree-cards.py/.ps1`, `tree-cards-lab.py/.ps1` (nouveaux)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationResolver.cpp`
- `tools/unreal/ground-cover-capture.py` (états `nocards` / `cards` / `nocards2`), `tools/unreal/proofs.txt`
- `Content/Anastasis/Vegetation/Cards/*` (atlas importé, 3 meshes), `Content/Anastasis/Materials/M_AnastasisFoliageCard`
- `SourceArt/Vegetation/leaf-atlas.png`, `docs/visual/tree-leafcards-001/`, `AGENTS.md` (index)

## COMMIT

PENDING

## MEC

- BUILD: voir `finish`
- TESTS: aucun test C++ ajouté (le branchement est de l'incarnation, pas du Build pur) ; les tests existants du résolveur ne touchent pas les chemins `SM_Tree_HolmOak_0N`
- COMMANDS:
  - `python tools/unreal/leaf-atlas.py`
  - `tools\unreal\create-tree-cards.ps1` → `TREE_CARDS_ASSETS::PASS` ; triangles LOD0/1/2 : Card_01 `[6378,5586,2424]`, Card_02 `[6444,5652,2424]`, Card_03 `[6378,5582,2424]`, contre `[18458,9229,1884]` pour les lames
  - `tools\unreal\tree-cards-lab.ps1 -Label lab2` → `TREE_CARDS::PASS` (8 vues)
  - `tools\unreal\editor-batch.ps1 -Proofs tree-cards-capture` → `PROOF::PASS`

## PROOFS

PROOFS: tree-cards-capture

## SCN

**KEEP, pour le chêne vert, de près et à moyenne distance.** Images regardées : `docs/visual/tree-leafcards-001/`.

- Banc (même lumière, même hauteur 11 m, tronc identique) : à 9 m et à contre-jour, la couronne à cartes est un
  feuillage dense à bords aérés, avec une ombre portée de vrai feuillage ; les lames sont des éclats
  jaunes sur des branches nues. Deux futaies de 20 arbres côte à côte : coupole sombre et continue contre
  éclats épars.
- Monde (Lvl_AnastasisSlice, ciel à 11 h, lames / cartes / lames) : en sous-bois (`sousbois_eye`) les chênes ont
  une vraie masse de feuillage. À 50 m et plus (`aerien`), pas de différence nette : ni gain, ni régression.
- Défauts connus : la couronne est un peu claire contre le ciel à 25 m ; un liseré clair à contre-jour sur les
  bords (transmission) ; les feuilles de près font 6 à 9 cm, un peu grandes pour un chêne vert.
- Le monde n'est pas identique à l'octet près entre les deux états : les rayons de couronne (bornes du mesh)
  alimentent la micro-écologie, qui place un peu autrement ses bois morts. Ce n'est pas le feuillage.

**Coût (RTX 3060, viewport éditeur 1280×720, p50 de `GetFrameTimingsMs`)**, lames → cartes, dérive du témoin
`nocards2` entre parenthèses : rivière 15,41 → 15,68 (+0,23) ; lisière 12,41 → 12,86 (+0,17) ; aérien
12,68 → 12,96 (+0,22) ; sous-bois 17,58 → 19,80 (+2,22, contre 0,27). Pour UNE essence. Le coût suit
l'overdraw du masque alpha : étendre aux autres essences l'additionne. Aucune mesure en jeu packagé.

## PLY

UNKNOWN : aucune session de jeu.

## ECARTS

AUCUN — `Source/AnastasisSim` inchangé.

## INTEGRATION_RISK

- **Première texture de végétation du projet.** C'est un changement de direction artistique (jusqu'ici la
  couleur de sommet seule), même s'il est cadré par la planche `AAA_VISUAL_TARGET_LAB.md` (« garder les
  cartes pour > 15 m »). Retour arrière : `anastasis.Dressing.TreeCards 0`, ou ne pas verser.
- Assets binaires (LFS) nouveaux : aucun écrasement d'un asset existant ; `create_tree_asset.py` n'est pas
  modifié et ne touche pas `Cards/`.
- `M_AnastasisFoliageCard` est un DUPLICATA : si `M_AnastasisVegetation` change (vent, teinte), relancer
  `create-tree-cards.ps1` pour le régénérer.
- Les captures des autres missions qui montrent des chênes verts changent d'aspect par défaut.

## STOP

Ce que cette mission ne revendique pas : les autres essences (olivier et platane ont leur case d'atlas,
pas de mesh), les arbres « héros » (`SM_Hero_HolmOak`, branche à part dans l'incarnation), un coût en jeu
packagé, la feuille à 1,7 m sous un regard de près (leur taille est à reprendre), le vent des cartes
(hérité tel quel, non observé).
