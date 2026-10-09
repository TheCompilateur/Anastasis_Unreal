# MA_CABANE_001 — « Ma cabane » : le joueur lève seul sa cabane, et y vit

## Décision d'Alexandre (2026-10-09)

> « Est-ce que le joueur peut construire sa propre maison et est-ce que la maison est accessible ? Car on va arrêter de
> penser au village. Il faut qu'un joueur puisse construire sa maison et y vivre. Elle doit être petite. »

| Question | Réponse |
|---|---|
| Ta petite maison, tu la bâtis comment ? | **seul, une cabane** |

> **Scène** : le joueur se pose dans la vallée : il lève seul sa cabane, et y vit.
> **Je dois voir** : je décide où, je bâtis de mes mains en quelques jours, une petite cabane d'une pièce ; j'y entre, j'y
> dors la nuit, et elle est à moi (personne d'autre n'y dort).
> **Le joueur** : `Anastasis.Player.Build` trace la cabane près de lui ; il la bâtit en choisissant « bâtir » ; une fois
> finie, « se reposer » l'y fait entrer.
> **Hors sujet** : les maisons des autres, la variété des maisons (plus tard), les meubles.
> **Fini quand** : en jeu, captures à l'appui, ma cabane est debout, je passe la porte, et je dors dedans.

## La cabane (`SM_Arch_Cabin_01`)

Une pièce de 16,8 m² (4,7 × 4,5 m dedans) sur un soubassement de moellons, murs de planches, toit à deux pans en
planches, faîtage à 4 m : 6,8 × 7,2 m hors tout, la plus petite maisonnée du catalogue (la maison pauvre fait 11 × 13 m).
Une porte de 96 × 192 cm au milieu de la façade, une fenêtre au fond, un foyer ouvert contre le mur du fond, un banc-lit
le long du mur gauche, un coffre, une étagère, un panier ; dehors, un tas de bois et un billot. Sol de terre battue à
18 cm au-dessus de la cour. Recette : `cabin()` dans `tools/unreal/create-village-architecture.py` ;
`create-village-architecture.ps1 -Only SM_Arch_Cabin_01` la régénère seule.

## Dans la simulation (écart n°56)

| Règle | Valeur |
|---|---|
| type | `cabin`, ouvert seulement par `PlayerBuildHome` |
| tracé | près du joueur (2 à 6 cases), sans couper personne du puits, comme la parcelle d'une famille |
| à qui | au joueur dès le tracé (`Owner`) ; lui seul y bâtit, sauf s'il demande de l'aide et qu'on lui dit oui |
| devis | 8 bois, 2 pierres (une maison : 24 et 8), livrés au tracé ; mêmes 22 pièces |
| toit | n'attend aucun aidant (la maison de famille, écart n°48, attend un oui à mi-hauteur) |
| achevée | son foyer (`HomeId`) ; une place |
| fermée | personne d'autre n'y dort, n'y mange, ni ne s'y abrite (`NearestHousing`, `FindOpenShelter`, `BuildingForIndoorAction`) |

Le corps du joueur : quand la simulation le dit dans sa cabane (il dort, il mange), le pawn passe la porte et se tient
près du banc-lit, sur le sol de la pièce (`PlacePlayerPawn`) ; ailleurs, il reste au seuil comme avant.

## Comment le voir

| Pour | Comment |
|---|---|
| la preuve rejouable | `tools\unreal\editor-batch.ps1 -Proofs cabane-pie` → `Saved/CabinEvidence/pie/` (cinq images + `cabin.json`) |
| jouer | PIE, `Anastasis.Player.Arrive`, `Anastasis.Player.Build`, touche « bâtir » (ou `Anastasis.Player.Goal build`) ; la nuit, « se reposer » |
| lire l'état | `AnastasisSimulationDebugLibrary.get_cabin_status` (JSON) |

## Ce qui n'est pas fait

- Le bois et la pierre sont livrés au tracé : les ramasser soi-même (cueillir, bûcher) est la mission suivante.
- Une seule cabane, un seul modèle : la variété des maisons viendra plus tard.
- Personne ne peut y être invité.
