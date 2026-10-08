# SAVE_STATE_001 — sauvegarder la partie

Chantier 4 d'IRON_CRUSADE_001 (`docs/unreal/iron-crusade-001/NECRON_RECONSTRUCTION_BLUEPRINT.md`, § C4).
Avant : le jeu ne savait pas sauver. `AnastasisJsSave` lisait le format JS, dans les tests seulement.

## Ce que fait le joueur (ou un agent)

```
Anastasis.Sim.Save [slot]     Saved/SaveGames/<slot>.sav   (défaut : anastasis.Sim.SaveSlot = anastasis)
Anastasis.Sim.Load [slot]
```

Recharger rend la partie exactement où elle était :
- l'heure et le jour ;
- chaque habitant (besoins, chemin en cours, souvenirs, liens, métier, fatigue, réputation du joueur) ;
- chaque bâtiment, chantier et stock ;
- les sentiers, la météo forcée, le tirage aléatoire du village ;
- le monde extérieur.

**Le futur est le même.** Le village rechargé refait, pas pour pas, ce qu'il aurait fait sans la sauvegarde.
Un fichier refusé (autre version, tronqué, scénario extérieur manquant) ne change rien à la partie en cours.

## Architecture

| Couche | Rôle |
|---|---|
| `Core/AnastasisStateArchive.{h,cpp}` | `FStateArchive`, un visiteur à trois modes : **hacher**, **écrire**, **lire** |
| `Village/AnastasisVillageStateDigest.cpp` | les `VisitState(Ar, T&)` : la liste des champs, écrite **une seule fois** |
| `Sim/AnastasisSimulation.cpp` | `ArchiveState` (horloge, file de minuit, monde, village, monde extérieur), `SaveState`, `ReadSaveHeader`, `LoadState` |
| `Anastasis_UnrealV2/Sim/AnastasisSaveGame.h` | `UAnastasisSaveGame` : conteneur Unreal (`USaveGame`), pas le modèle de données |
| `Anastasis_UnrealV2/Sim/AnastasisSimulationSave.cpp` | `SaveGameToSlot` / `LoadGameFromSlot`, commandes console, `get_save_status` |

**Une seule liste de champs.** L'empreinte d'état de STATE_ORACLE_001 (`StateDigest`) lisait déjà tout ce qui
décide du futur, et `check-state-fields` interdit d'y oublier un champ. La sauvegarde est ce même parcours en
mode écriture. Un champ ajouté est donc haché et sauvé, ou classé hors état. Il n'existe pas de seconde liste
qui pourrait dériver. Le hachage est inchangé, bit pour bit : un test temporaire comparait l'ancien hacheur
au nouveau parcours, puis il a été retiré.

**Format** : signature `ANSV`, un en-tête (version, graine, taille du monde, heure, scénario extérieur requis),
puis le parcours. Chaque clé est écrite et relue avec son nom, chaque nombre avec sa taille (un `double` reste
un `double`), chaque objet et chaque tableau avec un marqueur. Une sauvegarde qui ne suit pas le parcours
actuel est refusée à la première différence, avec le chemin du champ (exemple réel :
`world.tiles.fertility : fin de fichier inattendue`). `SaveFormatVersion` monte à chaque changement du parcours
(règle dans AGENTS.md).

**Chargement** : relu une première fois sur une simulation d'essai. Refusé, il ne touche à rien. Accepté :
`Reset(graine)`, puis chaque champ relu, puis les caches classés `cache:` vidés (ils se refont), puis le monde
extérieur relu par son propre `LoadState`, sur le scénario relu depuis sa source.

**Hôte** : le `USaveGame` porte l'état de la simulation, plus ce que l'hôte garde lui-même et qui décide
de la suite :
- les verrous de scénario (chantier d'ouverture, fermier, premier chantier, village du lancement) ;
- le chemin du scénario extérieur.

Au chargement, la présentation (acteurs, cartes) est retirée puis refaite depuis l'état relu. Le témoin
du joueur repart de ce que la simulation sait de lui.

## Ce qui n'est PAS sauvé (connu, déclaré)

- **Mémoire des passages** (`UAnastasisAnthropicSubsystem`) : un historique de présentation pure, en
  centimètres rendus. Expérimentale et **désactivée par défaut** (`anastasis.Anthropic.Memory 0`). Elle
  repart de zéro au chargement, et une ligne `ANASTASIS_SAVE presentation history not restored` le dit au log.
  Le passage que la simulation compte (`sim.traffic`, les sentiers) est, lui, sauvé.
- ~~Biographie des bâtiments~~ : **sauvée depuis save-history-001** (écart n°46). Elle est observée par la
  simulation, à chaque pas, et entre dans le parcours ; la forme des maisons revient au chargement.
- `Colony` et `MarketStock` : les deux lacunes du registre, posées seulement par le harnais.
- Le format JS (`serialize(sim)`) : une sauvegarde JS ne se recharge pas par `LoadState`, ni l'inverse
  (écart n°45, `A_TRANCHER`).
- Le verrou d'ouverture de `main` vit encore dans l'hôte (`OpeningSiteId`) : il est sauvé par le
  `USaveGame`. Quand `opening-in-sim-001` sera versée, il passera dans la simulation et le parcours.

## Preuves

- `Anastasis.Sim.Sauvegarde.AllerRetour` : village vivant (puits, grenier, champs, chantier sec, porteur,
  sentiers, sol humide, écritures invisibles à `Digest()`). Sauvé, rechargé ailleurs :
  - même état complet ;
  - même fichier resauvé, à l'octet près ;
  - même état à chaque demi-journée, sur 1 puis 3 jours.
- `Anastasis.Sim.Sauvegarde.MondeExterieur` : la même chose avec le scénario `geo-pontos-1204`. Sans
  scénario fourni, refus, et la simulation qui charge reste intacte.
- `Anastasis.Sim.Sauvegarde.Refus` : signature, vide, version, tronqué, octets en trop, clé altérée. Chacun
  donne une raison lisible, et la simulation qui charge reste intacte.
- `save-load-pie` (PIE, registre) : sauvegarder, vivre 2 jours, recharger, revivre les 2 mêmes jours. Même
  empreinte, présentation refaite, slot absent refusé.
