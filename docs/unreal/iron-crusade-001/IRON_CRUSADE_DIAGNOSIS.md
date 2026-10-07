# IRON_CRUSADE_DIAGNOSIS — état réel d'ANÁSTASIS Unreal

Mission `iron-crusade-001`, 2026-10-07. Lecture seule sur la racine canonique ; écritures limitées au
worktree `C:\dev\ANASTASIS_WORKTREES\iron-crusade-001`. Elles ont été commitées en fin de mission,
avec l'accord d'Alexandre. Les constats restent datés de la version étudiée ci-dessous.

## 0. Version étudiée et bornes

| Élément | Valeur |
|---|---|
| Racine canonique | `C:\dev\ANASTASIS_UNREAL`, branche `main` |
| Commit étudié | `e681e629` (2026-10-07 14:07, « Enable finite material courier for opening NPC site ») |
| Moteur | UE 5.8, validé par `anastasis-unreal.ps1 build` (`BUILD::PASS` sur ce commit, worktree) |
| Modules | `AnastasisSim` (Runtime, PreDefault, deps `Core` et `CoreUObject` seulement) ; `Anastasis_UnrealV2` (Runtime) |
| Taille | 86 244 lignes C++ : `AnastasisSim` 128 fichiers / 37 666 lignes ; `Anastasis_UnrealV2` 200 fichiers / 48 578 lignes, dont `WorldView` 33 772 |
| Tests d'automation | `AnastasisSim` ~135, tous purs ; `Anastasis_UnrealV2` 173, dont ~26 empruntent un monde ; **aucun ne lance PIE ni n'ouvre de carte** |
| Concurrence pendant l'étude | un lot `integrate-batch` tenait le verrou de `main` (pie-advance-001, soil-matrix-004, lived-paths-001, lake-shoreline-contour-001, integration-proof-closure-001). Le lot a avancé `main` à `d059a812` en fin de mission. Ce lot (7 commits) ne touche ni `AnastasisVillage.*`, ni `Sim/`, ni `AnastasisSettlementSurvey.cpp`. Il modifie `agent-worktree.ps1` dans `integrate-batch` (rejet des commits hérités non prouvés), mais pas dans `status`. F1 à F6 restent donc valables à `d059a812` ; le reste est borné à `e681e629` |

Légende : **V** = vérifié (code relu, commande lancée) · **I** = inféré · **E** = démontré par expérience.

## 1. Cartographie des propriétaires

```
                      ┌──────────────────────────────────────────────────────┐
  Config/DefaultEngine │ GameDefaultMap = Lvl_AnastasisSlice                  │
                      │ GameMode = BP_FirstPersonGameMode → redirigé vers     │
                      │            AAnastasis_UnrealV2GameMode                │
                      └──────────────┬───────────────────────────────────────┘
                                     │ BeginPlay : Atmosphere, Embodiment (si absent)
  ┌──────────────────────────────────▼─────────────────────────────────────────┐
  │ UAnastasisSimulationSubsystem (UTickableWorldSubsystem, mondes de jeu)       │
  │  possède FAnastasisSimulation ── seule autorité de la SOCIÉTÉ (V)             │
  │  + décide elle-même du village d'ouverture : SeedOpening*, AssignCompleted…  │
  │    (hors sim, hors ECARTS, hors empreinte, hors test d'automation)  (V)      │
  └───────┬──────────────────────────────▲──────────────────────────┬──────────┘
          │ Tick → PumpFrame / WarpPump    │ SetSettlement (ouverture) │ Sync
  ┌───────▼───────────────┐      ┌────────┴────────────────┐  ┌──────▼─────────────────┐
  │ AnastasisSim (portable)│      │ AnastasisSettlementSurvey│  │ FAnastasisVillagePresent.│
  │ FVillage : besoins,    │      │ lit le MAILLAGE RENDU    │  │ acteurs bâtiments, cartes,│
  │ Noûs, planner, nav A*, │      │ (ProcMesh sections 0-2)  │  │ corps, SmartObjects miroir│
  │ hydrologie de tuiles   │      └────────▲────────────────┘  └────────────────────────┘
  └───────────────────────┘               │
                               ┌──────────┴─────────────────────────────────┐
                               │ AAnastasisWorldEmbodiment (WorldView)        │
                               │ régénère le monde depuis la graine, puis     │
                               │ Forge ×3,6, HumanGeography, Drainage D8 :    │
                               │ SECONDE GÉOGRAPHIE, inconnue de la sim (V)    │
                               └─────────────────────────────────────────────┘
```

| Responsabilité | Propriétaire réel | Remarque |
|---|---|---|
| décisions des habitants | `AnastasisSim` (`FVillage::UpdateNpc` → `ChooseGoal` → Noûs, planner) | **V** : aucun second décideur ; StateTree et `AIController` absents du jeu |
| StateTree villageois | personne | **V** : 4 tâches `FAnastasis*SmartObjectTask` référencées par aucun code ni asset |
| SmartObjects | miroir de `Npc.Inside` | **V** : réclamés après coup par la présentation |
| géographie du jeu | tuiles de la sim | **V** |
| géographie vue | `WorldView` (Forge, HumanGeography, Drainage) | **V** : diverge par conception (`HYDRO_NETWORK_001.md`, « Gameplay ≠ rendu ») |
| site du village d'ouverture | **le rendu**, via `AnastasisSettlementSurvey::Read` | **V** : le maillage rendu choisit l'état initial de la société |
| village d'ouverture (maison, métiers, porteur, attribution) | **l'hôte Unreal** | **V** : `AnastasisSimulationSubsystem.cpp:189-385` |
| temps | sim (pas fixe) + hôte (`TimeScale`, `Warp`) | **V** : sain ; témoin déterministe sur 2 jours (**E**) |
| persistance | **personne** | **V** : aucun `USaveGame`, `SaveGameToSlot` ni `FArchive` dans `Source/` |
| parité JS | harnais (`AnastasisHarnessTrace`, `AnastasisJsSave`, lecture seule du format JS) | **V** : tests seulement |

## 2. Fragilités prouvées

### F1 — L'empreinte n'est pas un oracle d'égalité d'état (E, confiance haute)

`FVillage::Digest` est la projection de parité JS, figée. Les tests C++ s'en servent comme oracle de
déterminisme et de non-écriture. Elle lit 22 des ~109 champs de `FNpc` et 5 des 46 membres de `FVillage` ;
`sim.rng`, la météo, les compteurs d'identifiants et l'état du joueur n'y sont pas.

Expérience `Anastasis.Iron.Empreinte.AveugleAuxEcritures` :

| Écriture | Empreinte | Futur |
|---|---|---|
| aucune (témoin) | égale | égal pendant 2 jours |
| `Speed` | égale | diverge après 7,8 s |
| `sim.rng` | égale | diverge après 9,0 s |

Les assertions « la presentation n'ecrit pas dans la simulation », « observer mode: same digest » et
« l'empreinte ne bouge pas » sont aveugles à ces écritures. Détail : `PERTURABO_FIRST_BREACH.md`.

### F2 — L'hôte Unreal gouverne le village d'ouverture, sans test, hors protocole (V, confiance haute)

- **Décisions de jeu dans l'hôte** :
  - `SeedOpeningHousehold`, `SeedOpeningConstruction` et `SeedOpeningWorkforce` attribuent maison,
    métiers et porteur par pathfinding ;
  - `AssignCompletedOpeningHome` donne la maison achevée au bâtisseur le plus proche, à chaque `Tick`.
- **Ce code échappe à tous les garde-fous** :
  - au contrôle `ECARTS`, qui ne regarde que `Source/AnastasisSim/` (`check-ecarts.mjs:33-34`) ;
  - à l'empreinte ;
  - à toute sauvegarde : le loquet `OpeningSiteId` vit dans l'hôte.
- **Il n'est atteint par aucun test d'automation** (grep : aucun test n'appelle `OnWorldBeginPlay`,
  `Tick`, `TryStartVillage`, `SeedOpening*` ni `SyncVillagePresentation`). Il n'est vu que par des
  preuves PIE du lot (`npc-life-pie`, `material-courier-pie`).
- **C'est la zone la plus active** : 5 commits du 2026-10-07 sur ce seul fichier.
- **Défaut annexe** : `ResetCanonical` ne remet pas `FarmerGranaryId` ni `FarmerField` (`:408-429`).

### F3 — Le rendu fait remonter de l'état dans la société (V code ; effet I)

- **Le rendu choisit le site** : `TryStartVillage` choisit le site d'ouverture en lisant le maillage
  procédural rendu (`AnastasisSettlementSurvey.cpp:59-127`). Ce maillage dépend de CVars visuelles
  (`anastasis.Terrain.Forge`, `HumanGeography`, `Drainage`, `Shoreline`, `Surface`).
  - **I** : une mission visuelle qui modifie le relief peut déplacer le village de départ, et donc toute
    la trajectoire de la société, sans toucher une ligne de `AnastasisSim`.
- **Course au démarrage** : si le terrain n'est pas prêt dans les 10 s de `DeltaTime` cumulé
  (`AnastasisSimulationSubsystem.cpp:148-153`), le rapport passe à `unavailable` et **aucun village
  n'est posé**.
  - **I** : sur une machine chargée (premier boot, compilation de shaders), le jeu peut s'ouvrir vide.
- **Non mesuré** : aucune A/B de CVar de relief sur le site choisi n'a été faite.

### F4 — Deux géographies, et l'écart n'a jamais été chiffré (V structure ; ampleur inconnue)

- **Deux sources** : la sim marche sur ses tuiles. Le rendu creuse, exagère (×3,6), dessine des lits à
  la main et calcule son propre réseau de rivières (`AnastasisDrainage`).
- **Effet** : les habitants sont posés sur le maillage par trace (`AnastasisVillagePresentation.cpp:71-108`).
  - **I** : sur une tuile sèche de la sim, sous une rivière rendue, un habitant marche dans le lit.
- **Instruments sans mesure** : `geography-concordance-pie`, `terrain-access-pie` et `river-use-pie`
  existent, mais **aucun chiffre** n'a été relevé ; les trois fiches sont `QUEUED`.
- L'équipe connaît le problème. Ce qui manque, c'est la mesure.

### F5 — Aucune persistance (V, confiance haute)

Pas de sauvegarde de partie. `AnastasisJsSave` lit le format JS pour le harnais, sans l'écrire, et
n'est appelé que par des tests. L'ambition est une société « persistante » ; aujourd'hui une partie
meurt avec le processus. Trois états accumulent une histoire hors de la sim : le loquet d'ouverture
(F2), la mémoire anthropique (désactivée par défaut) et le témoin du joueur, réécrit dans la sim à
chaque tick selon le multiplicateur de `Warp`.

### F6 — La file d'intégration ment par omission (V, confiance haute)

- **Deux définitions de « intégrée »** : `status` classe par ascendance (`git branch --no-merged`),
  le lot verse par contenu (`git cherry`).
- **Mesure du 2026-10-07** : 55 branches `agent/*` affichées non intégrées, dont **16 déjà
  entièrement dans `main` par contenu** et 12 partiellement.
  - Exemples de branches déjà versées : `pontos-histoire-001`, `planner-module-001`,
    `terrain-access-001`, `resource-targets-001`.
  - Plusieurs ont 34 à 165 commits de retard.
- **Effet** : `PRETES_POUR_LE_LOT` en propose plusieurs qui n'apportent rien.

## 3. Systèmes portés mais inertes en jeu (V)

Ce n'est pas du code mort au sens strict : l'essentiel est déclaré dans `ECARTS.md` ou tient lieu
d'outil de parité. Mais rien de ceci ne tourne quand on joue.

| Système | Lignes (≈) | Pourquoi inerte | Déclaré ? |
|---|---|---|---|
| génome, phénotype, conditionnement, mode de vie | plusieurs centaines | posés seulement par `AnastasisJsSave` ; les habitants créés en C++ restent médians | oui, écart n°8 |
| `FNavService` | 1 140 | prouvé à parité (`Parite.NavService` : 5 473 opérations rejouées, `PORTAGE.md:138`), mais utilisé seulement par ses tests ; le jeu appelle `AnastasisPath::FindPath` directement | **non** sur `main` ; le branchement vit dans `agent/nav-wiring-001`, non intégré (13 commits non versés, 39 de retard) |
| budget de cadence (`AnastasisSimBudget`, `SetSimulationView`) | — | aucune vue posée en jeu : chaque habitant pense à chaque tick | oui (`ECARTS.md:144`) |
| tâches StateTree villageoises | ~100 | aucune référence | non |
| `WorldDressing` (Manager, Placement, Profile) | ~550 | jamais créé ni référencé par un asset | non |
| variantes du template (`Variant_Horror`, `Variant_Shooter`) | ~3 980 + 52 assets + 215 acteurs externes | compilées, jamais référencées | non |

**Conséquence systémique (I)** : la société jouée est plus pauvre que la société portée. Les missions
« wiring » branchent des couches que seuls les habitants lus d'une sauvegarde JS traversent. Une
validation verte de ces couches ne dit rien du jeu.

## 4. Hypothèses réfutées

| Hypothèse du mandat | Verdict | Preuve |
|---|---|---|
| plusieurs autorités de décision pour les PNJ | **réfutée** | seul `FVillage` décide ; StateTree inerte, aucun `AIController` |
| simulation non déterministe | **réfutée** (dans ce cadre) | témoin E : 2 jours, 8 habitants, aucune divergence |
| dépendances circulaires entre modules | **réfutée** | `AnastasisSim` ne dépend que de `Core` et `CoreUObject` ; sens unique `UnrealV2 → Sim` |
| écarts de portage cachés | **réfutée en grande partie** | `check-ecarts.mjs` : `ECARTS::PASS`, 30 ouverts, 6 sans marque dans le code |
| la présentation écrit dans la sim | **non trouvée**, mais indécidable avec l'oracle actuel | F1 |
| deux horloges du jour | **réfutée** | trois constantes `90.0` identiques (`AnastasisSimulation.h:31`, `AnastasisVillageRhythm.h:22`, `AnastasisSkyClock.h:34`) ; duplication sans garde, pas de divergence |

## 5. Zones encore inconnues

- **Le chiffre** de concordance simulation ↔ rendu (F4) : instruments `QUEUED`.
- **L'effet réel** d'une CVar de relief sur le site d'ouverture (F3) : demande une A/B en PIE.
- **Le temps** que met le terrain à être prêt au démarrage sur cette machine, comparé à la borne de 10 s.
- **Le coût par frame** du chemin réel (Tick de sim + présentation + abonnements) : non profilé. Aucune
  trace Unreal Insights n'a été prise (porte mémoire, lot en cours).
- **Les références d'assets** aux variantes du template : non vérifiées par l'Asset Registry (éditeur requis).
- **Le build packagé** : jamais tenté ici.

## 6. Preuves exécutables et limites

| Preuve | Commande | Résultat | Limite |
|---|---|---|---|
| build | `tools\unreal\anastasis-unreal.ps1 build` (worktree) | `BUILD::PASS`, 194,5 s | Editor Win64 Development seulement |
| brèche F1 | `tools\unreal\report-tests.ps1 -Filter Anastasis.Iron` | PASS 1 / KEF 0 / FAIL 0, run complet ; lignes `IRON_DIGEST` | un scénario (puits + 8 habitants), horizon 2 jours |
| écarts | `node tools/migration/check-ecarts.mjs` | `ECARTS::PASS fiches=30 ouvertes=30 fail=0 warn=6` | contrôle `AnastasisSim` seulement |
| file d'intégration F6 | `git cherry main agent/<m>` sur chaque branche non intégrée | 16 versées / 12 partielles / 27 à verser | photographie du 2026-10-07 |
| F2, F3, F5 | grep et lecture (citations dans le texte) | — | pas d'expérience runtime |

La suite complète n'a **pas** été relancée : elle relève du lot. Le seul test joué est celui de la brèche.
