# SHORE_REED_003 — étude de silhouette sur une rive

## Contrat

Étude bornée, scène éditeur temporaire. Base terrain `85e0627` (étape 3 de
Claude, cases de **1 m**). Branche `agent/shore-reed-003`, dans le worktree
Codex existant `asset-map-002`. Le correctif du registre et l'audit précédent
restent conservés sur `agent/asset-map-002` (`592a35a`, `f40e94d`).

Le 29 septembre, pendant cette étude, le worktree `terrain-relief-001` a repris
un changement non commité vers des cases de **4 m**. Cette étude n'en constitue
pas une validation. Le script refuse une emprise incompatible avec la base 1 m.

Référence examinée : `docs/visual/reference/pontique-etat-zero-5-lisieres-transitions.png`.
Cible locale : silhouettes fines, hauteurs irrégulières, groupes laissant des
ouvertures et contact crédible avec la rive.

## Variable évaluée

Un seul mesh existant repris sans modification :
`Content/Anastasis/Ecotone/SM_Ecotone_Reed_01.uasset`, depuis `fb68398`.
SHA256 : `E6FF20029251238348BDFA510D161607AF0F7A1348D2F898A21EAEE12C140D3E`.
Matériau : celui de végétation déjà présent dans la base terrain.

- `bare` : rive sans les roseaux d'étude.
- `A` : mesh à son échelle native, six tiges par touffe.
- `B` : largeur XY ×0,45 ; hauteur Z ×0,62 à 1,0, tirage déterministe.

A et B ont les mêmes racines, rotations, mesh et matériau. La variation testée
est l'échelle des instances. La distribution est commune, en groupes discontinus,
sur des racines proches de l'eau réellement rendue. Les arbres/ruines existants
restent présents ; leurs racines sont évitées, sans garantie complète d'occlusion.

KEEP du réglage si les silhouettes sont plus fines/lisibles et l'ancrage conservé.
REJECT comme qualité finale si les tiges restent des bâtons, sans feuilles/courbure,
ou si le sol et la rive empêchent la continuité demandée par la référence.

## Limites de la mesure

Le support est échantillonné au centre et sur huit points d'un cercle de 16 cm :
écart maximal admis 8 cm, base posée au minimum. C'est un contrôle discret,
pas une preuve de contact de chaque sommet. Le déplacement matériel par le vent
(WPO) existant n'est pas gelé et n'entre pas dans le calcul du support.

Deux caméras fixes : œil à 165 cm du sol sec et vue de contexte plus haute.
Le trajet œil-cible est contrôlé contre le relief ; les autres meshes peuvent
encore masquer des objets. Soleil 75 000 lux, rotation (-38,-35,0), EV100=14,
bloom/vignette/flou de mouvement nuls, brouillard exponentiel retiré en mémoire.
Aucun niveau ni asset sauvegardé. Acteurs statiques temporaires, pas un nouveau
système de dressing ; performance non mesurée. PLAYER reste NOT_IMPLEMENTED.

## Reproduction

Build Editor requis. L'opérateur actuel refuse le chemin des worktrees gérés par
Codex ; cette étude a utilisé directement sa commande Build.bat, moteur vérifié
UE 5.8.2 CL 56702186. Compilation complète réussie, code de sortie 0, 82,67 s.
Aucun changement C++, aucune nouvelle suite C++ revendiquée.

Hydrater les pointeurs LFS locaux si nécessaire : `git lfs checkout`.
Puis exécuter `tools/unreal/capture-shore-reeds.ps1 -OutDir <nouveau-dossier-absolu>`.
Le script refuse un dossier existant, attend six captures et produit leurs SHA256.
Le succès de capture ne vaut pas acceptation artistique.

## Résultat observé — 2026-09-29

**Décision : REJECT du réglage B comme correction artistique suffisante.**
B réduit la masse et varie les hauteurs ; les tiges deviennent cependant plus
fragmentées et moins lisibles. A conserve une allure de piquets à têtes rondes.
Aucune des deux versions n'apporte les feuilles, courbures et transitions basses
visibles dans la référence. Les ombres restent striées. La cause exacte de la
fragmentation (matériau/vent/anticrénelage) n'est pas isolée par cette comparaison.

La scène garde un sol peu détaillé, des berges anguleuses et un horizon noir.
Ces défauts sont visibles dès `bare` et ne sont pas des effets des roseaux.
La répartition sur la grille de candidats à 40 cm reste elle aussi perceptible.
Une scène composée crédible n'est donc **pas acquise** par cette étude.

Preuve retenue :
`C:/Users/alex_/.codex/visualizations/2026/09/29/01a0eab3-eaab-7610-8f6d-bb6dd660a518/shore-reed-003d/`

- Six PNG 1600×900 : `bare`, `A`, `B`, chacun en vues `eye` et `context`.
- `capture.log` : `REED_REVIEW COMPLETE shots=6`, six `CAMERA_VERIFIED`.
- `manifest.json` : les 39 placements, échelles, caméras et conditions.
- `capture-hashes.json` : SHA256 des six images, écrit après fermeture de l'éditeur.
- 120 racines candidates ; 39 retenues ; six tiges par mesh, soit 234 tiges.
- Point visé XY=(938,51 ; 8671,28), sol Z=273,99 uu.
- Écart de support maximal observé : 7,952 cm (seuil 8 cm).
- Dégagement minimal échantillonné caméra/cible au-dessus du relief : 66,915 cm.
- Hauteurs B : facteur 0,6213 à 0,9876 ; largeur facteur 0,45.

`shore-reed-003` : échec avant scène (pointeurs LFS non hydratés).
`shore-reed-003b` : interrompu, callback réentrant pendant le chargement de carte.
`shore-reed-003c` : six images, mais angles positionnels Unreal incorrects ;
**rejetées**. `003d` utilise des angles nommés et vérifie la caméra effectivement
appliquée. Aucun de ces essais précédents ne constitue la preuve retenue.

[MEC] compilation et exécution de capture observées ; pas de nouvelle suite C++.
[SCN] comparaison obtenue ; qualité finale REJECT. [PLY] UNKNOWN.
Aucun merge, aucun changement au travail concurrent de Claude ou au canonique.

## Prochaine décision

Après livraison de l'échelle 4 m par Claude, recaler la petite rive. Pour les
roseaux, ouvrir un seul chantier : géométrie courbe avec feuilles et lisibilité
du matériau à distance, puis comparer à cette base. Ne pas augmenter la densité
pour masquer les défauts de silhouette. Le présent script reste une étude 1 m,
pas un système de placement à intégrer au gameplay.
