# HANDOFF: annee-valmire-001

## MISSION

« Une année à Valmire » (« Oui » d'Alexandre, 2026-10-09), avec ses trois questions : « objectivement, le village dépend-il
du joueur pour se développer ? quelle croissance ? je veux des stats économiques et sociales ».

Cette mission livre **l'instrument** : `Anastasis.Etude.Annee <jours> <relevé> <graines>` (et `tools/unreal/year-study.ps1`,
éditeur sans rendu). Le vrai village du lancement (`SeedStartVillage`), sans rendu, quatre scénarios à même graine :

| Scénario | Quoi |
|---|---|
| `sans-joueur` | le village tel qu'il vit sans joueur (le monde extérieur s'ouvre avec les fondateurs, comme dans le jeu) |
| `joueur-qui-survit` | un joueur arrive et ne fait que boire, manger, dormir (intentions posées comme aux touches) |
| `joueur-qui-travaille` | il survit et prend le premier travail de sa table (bâtir, livrer, récolter) |
| `sans-joueur-village-ferme` | sans joueur, `anastasis.Geo.AutoLoad 0` : aucun arrivant possible |

Un relevé tous les N jours (défaut 10, sur 240 jours = deux années du jeu de 120 jours) : population, familles, morts
(causes), arrivants, sans-toit, maisons, puits, greniers, chantiers, portions au grenier et sur pied, soirs à grenier vide,
repas, récolte, livraisons, bois, pièces posées, métiers, besoins moyens, en crise, relation moyenne, amitiés, conversations,
souvenirs, histoires racontées, aide demandée / acceptée, réputation, et l'état du joueur. Un csv par scénario, sa chronique
et `summary.json` dans `Saved/YearStudy/<horodatage>/seed-<graine>/`.

L'arrivée automatique du joueur (player-start-002) passe par `TryStartVillage`, pas par `SeedStartVillage` : le scénario
« sans joueur » reste sans joueur.

## Ce que la première étude a trouvé (main `4ff9c34a`, 2026-10-09, mondes 12345, 7, 42, 1204)

- **Croissance nulle** : tout se bâtit en 10 jours (5 maisons), puis plus rien pendant 230 jours ; aucune naissance ; au plus
  1 arrivant en 240 jours (le monde extérieur se chargeait alors à la main).
- **Le village ne dépend pas du joueur** : mêmes 6 maisons avec ou sans lui ; un joueur qui ne fait que survivre peut faire
  basculer le village (monde 12345 : 3 chantiers abandonnés, 122 soirs de faim) — sensibilité au moindre habitant de plus,
  pas une cause ; le joueur scripté mourait de faim à côté d'un grenier plein (mondes 7 et 42).
- **Économie** : 3 cultivateurs pour 14 ; 6 à 10 sans-métier pour toujours ; grenier au plafond (300) dès le jour 90 ;
  ~52 000 portions jamais récoltées ; premier hiver dur (monde 12345 : grenier vide 35 soirs).
- **Vie sociale** : 11 à 19 amitiés, presque toutes le premier mois ; souvenirs de 112 à 0 ; entraide seulement pendant les
  10 jours de chantier.
- **Ce qui cassait**, chacun corrigé par sa propre mission : plantage des graines 99 et 2026 (`drainage-lac-vide-001`) ; morts
  de soif à côté du puits, monde 1204 entier en 6 jours (`soif-dabord-001`) ; famine des sans-métier (`faim-champs-001`) ;
  le joueur sans mains (`mains-joueur-001`).

## FILES_OWNED

- `Source/Anastasis_UnrealV2/Sim/AnastasisYearStudy.{h,cpp}` (nouveaux) : scénarios, relevé, csv, résumé, commande
- `Source/Anastasis_UnrealV2/Sim/AnastasisYearStudyTests.cpp` (nouveau) : `Anastasis.Village.Etude.Instrument`
- `tools/unreal/year-study.ps1` (nouveau), `AGENTS.md` (index)

## COMMIT

Voir `git log main..agent/annee-valmire-001`.

## MEC

- BUILD: PASS (worktree)
- TESTS: RESULTAT_TESTS
- Étude de deux années (240 jours, 4 scénarios, graines 12345, 7, 42, 1204) : ~2 min sans rendu, `YEAR_STUDY COMPLETE`.

## PROOFS

PROOFS: (aucune)

L'étude est un instrument sans rendu ; son test tient dans la suite (6 jours, 4 scénarios).

## SCN

Rien ne change dans le jeu. Pour les agents : `tools\unreal\year-study.ps1 -Seeds 12345,7,42,1204` après une mission qui
touche la vie du village, et lire `summary.json` et les csv avant de conclure.

## PLY

NOT_JUDGED.

## ECARTS

AUCUN — tout est dans l'hôte (`Source/Anastasis_UnrealV2/`), rien dans `Source/AnastasisSim/`.

## INTEGRATION_RISK

- Aucun conflit attendu (fichiers nouveaux, une ligne d'index).
- Le test d'instrument joue 4 × 6 jours du vrai village (environ 10 s de suite).

## STOP

- L'étude ne juge pas : elle compte. Ses conclusions vont dans les fiches des missions qu'elle motive.
