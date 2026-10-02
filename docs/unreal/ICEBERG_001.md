# ICEBERG_001 — le territoire dit-il la vérité sur la simulation ?

Mission `iceberg-001`. Question : **est-ce que le monde visible dit la vérité sur le monde simulé ?**
Document court : registre de l'audit, cible retenue, frontière. Les preuves et les valeurs sont dans
la fiche `docs/unreal/handoffs/iceberg-001.md`.

## Constat de fond (OBS)

Il n'y a ni Landscape, ni PCG, ni Water, ni Foliage. Tout le territoire est une grammaire C++
déterministe (`WorldView/AnastasisWorldEmbodiment.cpp`) qui lit le monde **généré, immuable**
(`FVisualTile` : Type, Wetness, Fertility, Shore, Alt, ForestMargin…) et le relief rendu. Forêt,
sous-bois, herbe, berges, micro-écologie : **aucun ne lit d'état vivant du village**. La
géographie est causale (eau, pente, humidité, `RiparianAt`) mais statique : elle ne répond pas à la
société. Le seul pont vivant est `AnastasisAnthropicSubsystem` (traces de déplacement sur l'herbe),
expérimental, `anastasis.Anthropic.Memory` = 0.

## Registre

| ID | Promesse visible | Constat | Statut | Propriétaire |
|---|---|---|---|---|
| I1 | Maison : « quelqu'un vit ici » | le mesh ne porte que l'échelle du chantier ; maison vivante = maison vide | FALSE_PROMISE → **traité** | Village/ (libre) |
| I2 | Village : « une société est là » | `RemoveNpc` libère la maison, le rendu ne change pas : **ghost settlement invisible** | FALSE_PROMISE → **traité pour les maisons** | Village/ |
| I3 | Route : « on passe ici » | route de col = spline écrite à la main ; aucun compteur de passage ; trace d'herbe expérimentale, off | PARTIAL | `anthropic-paths-002` |
| I4 | Forêt : « elle pousse pour une raison » | humidité, pente, ripisylve réels ; clairières = seuil de bruit ; aucune coupe, aucune souche | PARTIAL | `ecotone-002`, `woodland-sequence-003`, `atmosphere-crusade-001` |
| I5 | Champ : « quelqu'un l'exploite » | `LiveTileAt` (nourriture, jachère) vit côté sim ; le sol « Worked » vient du monde généré ; seul un `DrawDebugBox` lit l'état vivant | DECORATIVE | terrain/sol (autres agents) |
| I6 | Rivière : « l'eau vient de quelque part » | bassin versant calculé (`AnastasisDrainage`), vitesse, berges selon la vitesse | REAL | `water-continuity-001` |
| I7 | Pluie : « il pleut » | météo sim → MPC (vent, humidité, couverture) ; le sol mouillé dépend d'un matériau non vérifié ici | PARTIAL | atmosphère |
| I8 | Mort de la société | **mortalité et famine non portées** (`starvingDays` « n'est pas porté », `RemoveNpc` n'est qu'une commande console) : le ghost settlement n'a aujourd'hui aucune voie de production en partie | NOT_REACHABLE | simulation |
| I9 | Passé du lieu : « ancien, usé » | la sim ne garde aucune date de vacance ni d'abandon ; `Hamlet`/`Vestige` sont des compositions décoratives | UNKNOWN | simulation |
| I10 | Orientation (aspect) | aucun champ d'exposition de versant | UNKNOWN | — |

## Cible retenue : I1 + I2, pas le reste

Raison, par ordre de poids : (1) la seule branche **sans collision** — aucun agent actif ne touche
`Village/` ; (2) la donnée est **déjà dans la simulation** et fiable (`CountShelterOccupants`,
`InsideOf`, `IsCompleted`) ; (3) elle est le **test mental** de la mission : une société qui meurt,
des murs qui restent ; (4) petite, réversible (`anastasis.Village.Metabolism 0`). Les autres
leviers (routes, forêt, berges) sont possédés par d'autres agents : pas de détournement.

Chaîne : `FVillage` (résidents, dedans, achevé) → `AnastasisMetabolism::Derive` (pur) →
`AAnastasisVillageBuilding::SetHearth` → `UPointLightComponent` dans le volume de la maison, la
lumière sort par la porte et la fenêtre (ouvertures du mesh) → le joueur voit une maison allumée
la nuit, ou noire.

## Ce qui n'est PAS fait, et pourquoi

- **Usure, mousse, toit qui cède** : demandent « depuis quand ce foyer est vide » ; la sim ne le sait
  pas. Le déduire côté Unreal serait une seconde vérité et ne survivrait pas à un rechargement.
  Pont minimal à proposer : un champ `VacatedDay` posé par la sim quand `Owner` se vide.
- **Fumée de cheminée** : pas de Niagara dans le projet ; le signal de jour reste manquant (la
  lumière ne dit rien à midi). Frontière, pas d'asset inventé.
- **Reprise végétale autour d'un hameau mort, champs, coupes** : appartiennent à des agents
  actifs ou exigent la même date de vacance.
- **Occurrence en partie** : tant que la mortalité n'est pas portée, une maison ne devient
  « vide » en jeu que par `RemoveNpc` ou démolition d'un propriétaire. Le mécanisme existe ;
  son occurrence naturelle, non.
