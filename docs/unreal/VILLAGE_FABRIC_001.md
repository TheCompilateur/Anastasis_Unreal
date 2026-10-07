# VILLAGE_FABRIC_001 — le tissu entre les maisons

Mandat d'Alexandre, 2026-10-07 : une seconde croisade bâtiments, celle de l'organisation du village.
Elle a été coordonnée avec architecture-crusade-001, qui garde chaque bâtiment (échelle, kit, typologies,
assise, défrichement de l'emprise). Cette mission prend ce qui est **entre** les bâtiments.

## Ce que le village pontique apprend à la grammaire

Les villages des vallées de Trébizonde (Empire des Grands Comnènes, après 1204) ne sont pas des grilles.
Ce sont des maisonnées posées sur la pente, sur leurs terrasses, reliées par des **sentiers muletiers
caladés** (le *kaldırım* de la période ottomane, une chaussée de pierre héritée des voies byzantines). Ces
sentiers suivent les courbes de niveau et ne montent franchement qu'en marches. Le réseau pousse depuis un
lieu commun : la source ou le puits, la chapelle, le grand platane. Chaque nouveau seuil s'y greffe par le
chemin le moins pénible.

Quatre règles en sortent, toutes visibles dans le code (`Village/AnastasisVillageFabric.cpp`) :

| Règle | Où |
|---|---|
| **La pente coûte au carré.** Le coût d'un pas vaut `longueur × (1 + k·pente²)`. Pour gagner une hauteur donnée, la pente la moins coûteuse vaut `1/√k`. Avec `k = 40`, elle tombe sur 16 %, la pente d'un sentier muletier. Plus raide, la ruelle prend des lacets ; au-delà de `StepGrade` (16 %), la calade devient escalier (contremarches ≤ 18 cm). | `FParams::SlopeWeight`, `StepGrade` |
| **Le réseau est un arbre qui pousse depuis la placette.** À chaque tour, le seuil le moins coûteux rejoint le réseau déjà tracé, pas forcément la placette. Les troncs communs s'élargissent avec le nombre de seuils servis (150 → 240 cm). | boucle Dijkstra multi-source, `Usage` |
| **La terrasse se tient par la pierre sèche.** On prend le niveau de l'assise (la médiane de 25 sondages, la même que l'architecture). Là où le bord de la plate-forme domine le sol de plus de 45 cm, on pose un mur de soutènement aval, qui regarde l'aval. Là où le talus domine, on pose un mur de déblai amont, qui regarde la maison. Le mur s'ouvre au seuil. | `FWallRun` |
| **La placette a son platane.** Il est posé sur le bord le plus plat, hors chaussée, hors parcelle. | `FPlaza::TreeSpot` |

## Durabilité

- **Aucun asset généré.** Calades, marches, bordures, placette et murets sont construits à l'exécution en
  `UProceduralMeshComponent` et refaits dès que le village change (bâtiment ajouté, retiré, déplacé). Il
  n'y a pas de `.uasset` à régénérer ni de LFS qui grossit, et rien n'est retouché à la main pour être
  écrasé ensuite. Les matériaux, eux, existent déjà : `MI_WeatheredStone` (sinon `M_AnastasisStone`) et
  `SM_Tree_PlaneTree_0x`.
- **Aucun tirage.** La variation (assises de 20 à 30 cm, pierres de 35 à 75 cm, joints croisés, teinte)
  vient d'un hachage stable de l'identifiant. Même village, même tissu : c'est la signature (FNV-1a sur les
  positions au cm), vérifiée par les tests et par la preuve PIE.
- **Rien n'est écrit dans la simulation.** La grammaire lit la case, le type et le premier point d'accès
  de chaque bâtiment. Elle lit aussi le relief rendu (les mêmes maillages que `SimToUnreal`).
- **Le défrichement est réversible.** Les instances d'herbe et de sous-bois posées sur la chaussée passent
  à l'échelle zéro, et sont rendues à l'identique quand le tissu change ou disparaît. Le carré ±1020 cm
  d'une parcelle bâtie est exclu, car c'est le domaine du défrichement de l'architecture : les deux
  défrichements ne se recouvrent jamais.

## Réglages

| CVar / commande | Effet |
|---|---|
| `anastasis.Village.Fabric` 0/1 | tissu absent (herbe rendue) / présent (défaut) |
| `anastasis.Village.FabricClear` 0/1 | A/B : herbe laissée sous la calade / défrichée (défaut) |
| `anastasis.Village.FabricTree` 0/1 | placette nue / platane (défaut) |
| `Anastasis.Village.FabricReport` | ligne `ANASTASIS_FABRIC report …` et JSON au journal |
| `Anastasis.Village.FabricRebuild` | refait le tissu du même village (contrôle de déterminisme) |

## Preuves

- Tests `Anastasis.Village.Fabric.{Flat,Determinism,Slope,EdgeCases}` : grammaire pure, relief analytique.
  Ils vérifient la connexion de tous les seuils, l'absence de chaussée sur un corps, l'invariance à
  l'ordre, et que sur 30 % la ruelle suit les courbes (pente moyenne < 0,85 × la ligne droite). Ils
  vérifient aussi l'orientation des soutènements et des déblais, et les cas sans puits ou avec un trou
  dans le relief.
- `village-fabric-pie` (registre) : PIE réel, instrumental (voir l'en-tête du script). **Ce n'est pas un
  verdict visuel.** Le jugement d'image se fait à part (skill `anastasis-capture`), une fois le lot versé.

## Ce qui reste ouvert

- **Adaptateur architecture** (après le versement d'architecture-crusade-001) : poser le seuil sur
  `FArchetype::EntryLocal` plutôt que dans l'axe de l'accès, et caler les murets sur `GetPadOffset()` de
  l'acteur au lieu de recalculer la médiane. Ce sera un petit fichier séparé.
- **Matériau de calade** dédié (galets debout, joints sombres, mousse au pied des murs) : la géométrie
  porte déjà des couleurs de sommet par pierre, mais `MI_WeatheredStone` ne les lit pas.
- **Usure vivante** : élargir ou polir une ruelle selon le passage réel (`AnastasisAnthropicMemory`).
- **Chapelle** : la grammaire sait faire une placette au puits. La chapelle de l'architecture est encore
  au laboratoire ; sa place (au-dessus du village, au bout de la ruelle maîtresse) attendra son entrée dans
  la simulation.
