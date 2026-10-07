# NECRON_RECONSTRUCTION_BLUEPRINT — campagne de reconstruction

Base étudiée : `main` = `e681e629` (2026-10-07). Aucune ligne de ce plan n'est exécutée : chaque
chantier attend une décision d'Alexandre. Les preuves sont dans `IRON_CRUSADE_DIAGNOSIS.md` et
`PERTURABO_FIRST_BREACH.md`.

Légende des verdicts : **CONSERVER** · **CONSOLIDER** · **APPROFONDIR** (mesurer avant de décider) ·
**SUPPRIMER**.

## Ordre recommandé

```
0. Sécuriser l'oracle          (C1)  ── sans lui, aucune refonte suivante ne se prouve
1. Mesurer la géographie       (C2)  ── instruments déjà écrits, jamais lancés sur le monde réel
2. Rapatrier l'autorité        (C3)  ── dépend de C1 (oracle) ; prépare C4
3. Persistance                 (C4)  ── dépend de C1 + C3 : on ne sauve que ce que la sim possède
4. Hygiène de file d'intégration (C5) ── indépendant, peut partir tout de suite
5. Ménage                      (C6)  ── indépendant, faible rendement
```

C1 passe en premier parce que C3 et C4 sont des déplacements d'état, et que l'oracle actuel ne
verrait pas un état perdu en route. C'est exactement la faille démontrée par la brèche.

---

## C1 — Un oracle d'état complet, séparé de la projection JS · **CONSOLIDER**

> **Statut au 2026-10-07 : réalisé** dans `state-oracle-001` (`HANDOFF_READY::YES (queued)`, en attente du
> lot). Le garde-fou est différentiel : dans `finish`, seuls les champs que la branche ajoute la bloquent.

- **Défaut** : `FVillage::Digest` sert à la fois de projection de parité figée et d'oracle d'égalité
  dans les tests C++. Environ 87 champs `FNpc` et 41 membres `FVillage` lui échappent.
- **Preuve** : `Anastasis.Iron.Empreinte.AveugleAuxEcritures`. Une écriture invisible dans `Speed` ou
  `sim.rng` donne un futur différent après 7,8 s et 9,0 s simulées ; le témoin ne diverge pas en 2 jours.
- **Architecture cible** : `Digest()` reste la projection JS, inchangée. On ajoute un `StateDigest()` qui
  couvre tout l'état décisionnel. Les tests d'isolement et de déterminisme passent sur `StateDigest()`.
- **Natif Unreal envisageable** : convertir `FNpc` / `FBuilding` en `USTRUCT` et hacher par réflexion
  (`TFieldIterator<FProperty>`) couvrirait chaque nouveau champ automatiquement. C'est **déconseillé
  maintenant** : `AnastasisSim` est un module portable sous contrat de parité, et la réflexion apporterait
  UHT, du GC et des contraintes de types (`TOptional`, `TSet` imbriqués) pour un seul consommateur.
  Préférer une liste explicite, plus un garde-fou de script.
- **Garde-fou** : un `tools/migration/check-state-fields.mjs`, appelé par `finish` comme
  `check-ecarts.mjs`. Il exige qu'un champ ajouté à ces trois structures soit classé `etat` (lu par
  `StateDigest`), `derive` ou `cache` (justifié).
- **Fichiers** :
  - `Source/AnastasisSim/Public/Village/AnastasisVillage.h`, `Private/Village/AnastasisVillage.cpp`
  - les 5 tests listés dans la brèche
  - `tools/unreal/agent-worktree.ps1` (appel du garde-fou)
- **Dépendances** : aucune.
- **Risque** : faible. C'est un ajout pur, sans effet de jeu. Seul risque : un test existant passe au
  rouge parce qu'il aurait dû l'être. C'est le but, mais cela déplace des verdicts d'autres missions.
- **Effort** : 1 à 2 jours d'agent, plus un lot.
- **Séquence** :
  1. `StateDigest` et un test qui prouve qu'il voit les quatre écritures de la brèche.
  2. Basculer les 5 assertions.
  3. Ajouter le garde-fou de champs.

## C2 — Mesurer l'écart simulation ↔ rendu avant d'y toucher · **APPROFONDIR**

- **Défaut** : deux autorités géographiques.
  - **Simulation** : les tuiles `Type`/`Alt`, qui décident de tout le jeu.
  - **Rendu** : `AnastasisTerrainForge` (×3,6, ravines, érosion), `AnastasisHumanGeography` (lits et
    vallées dessinés à la main) et `AnastasisDrainage` (réseau D8 propre, sim non informée).
  - L'équipe le sait et l'écrit (« Gameplay ≠ rendu », `HYDRO_NETWORK_001.md`). Mais **aucun chiffre de
    concordance n'a jamais été relevé** : `geography-concordance-001`, `terrain-access-001` et
    `river-use-001` sont tous en état `QUEUED`.
- **Preuve** : lecture de code (`AnastasisDrainage.cpp:1842-1844`, `AnastasisSettlementSurvey.cpp`,
  `AnastasisVillagePresentation.cpp:71-108`). L'ampleur est inconnue.
- **Architecture cible** (à décider après mesure). Deux familles :
  - **(a)** la sim reste l'autorité, et le rendu s'interdit de créer de l'eau ou des pentes
    franchissables là où la sim ne les a pas : contrainte côté `WorldView`, sans toucher à la parité ;
  - **(b)** le relief rendu devient l'autorité, et la sim lit une grille dérivée : écart de parité
    massif, `ECARTS` lourd.

  Sans chiffres, ne choisir ni l'une ni l'autre.
- **Natif Unreal** : aucun à ce stade. Le maillage procédural est déjà la bonne source pour la mesure.
- **Fichiers** : `tools/unreal/geography-concordance-pie.py`, `terrain-access-pie.py`,
  `river-use-pie.py` (déjà au registre `proofs.txt`).
- **Dépendances** : aucune. Il suffit de verser ces missions et de lire les trois JSON.
- **Risque** : nul. Ce sont des lectures.
- **Effort** : un lot d'intégration.
- **Séquence** : verser, lancer, publier les trois tables. Décision (a)/(b) seulement après.

## C3 — Rapatrier dans la simulation les décisions prises par l'hôte · **CONSOLIDER**

- **Défaut** : dans le chemin de jeu normal, `UAnastasisSimulationSubsystem` décide de la société
  elle-même.
  - `SeedOpeningHousehold`, `SeedOpeningConstruction` et `SeedOpeningWorkforce` attribuent maison,
    métiers et porteur à coups de pathfinding (`AnastasisSimulationSubsystem.cpp:189-385`).
  - `AssignCompletedOpeningHome` donne la maison achevée au bâtisseur le plus proche. Il tient sur un
    loquet `OpeningSiteId`, membre de l'hôte, absent de la sim (`:321-358`, appelé à chaque `Tick` :491).
  - Ce code est hors du protocole `ECARTS`, qui ne regarde que `Source/AnastasisSim/`
    (`tools/migration/check-ecarts.mjs:33-34`). Il est aussi hors de toute empreinte et de toute
    sauvegarde. `ResetCanonical` oublie de remettre `FarmerGranaryId` et `FarmerField` (`:408-429`).
- **Preuve** : lecture de code (lignes citées) et relevé de l'agent de persistance. Pas encore
  d'expérience runtime.
- **Architecture cible** :
  - un `FVillage::SeedOpening(const FOpeningPlan&)` déterministe, dans `AnastasisSim` ;
  - l'attribution de la maison achevée devient une règle de la sim (événement d'achèvement), plus un
    loquet de l'hôte ;
  - l'hôte ne garde que la lecture du terrain (C2) et l'appel.
  - Écart déclaré dans `ECARTS.md` s'il n'a pas d'équivalent JS.
- **Natif Unreal** : non. Ce qui doit migrer, c'est la responsabilité, pas la technologie.
- **Fichiers** : `Sim/AnastasisSimulationSubsystem.*`, `AnastasisSim/Public|Private/Village/AnastasisVillage*`,
  `ECARTS.md`, `npc-life-pie.py`, `material-courier-pie.py`.
- **Dépendances** : C1 (prouver que l'état migré est le même).
- **Risque** : moyen. C'est le village du lancement que voit Alexandre. Il faut un A/B
  `StateDigest` avant/après sur la même graine.
- **Effort** : 2 à 3 jours.
- **Séquence** :
  1. Déplacer l'attribution de la maison (le plus petit morceau, avec un loquet d'état).
  2. Puis l'ouverture du ménage, du chantier et des métiers.
  3. Puis retirer les loquets de l'hôte.

## C4 — Persistance de la société · **APPROFONDIR, puis CONSTRUIRE**

- **Défaut** : aucune sauvegarde. Il n'y a ni `USaveGame`, ni `SaveGameToSlot`, ni `FArchive` dans
  `Source/`. `AnastasisJsSave` ne fait que **lire** le format JS, et seulement dans les tests. Pour un
  projet dont l'ambition est une société « persistante », c'est un trou de fondation, pas un détail.
- **Preuve** : grep (agent de persistance) et relecture de `AnastasisJsSave.h:9`.
- **Architecture cible** : sauvegarder **la sim seule** (le temps, `sim.rng`, l'état complet de C1).
  La présentation se reconstruit, comme elle le fait déjà, de façon dérivée. La mémoire anthropique est
  l'exception : c'est la seule présentation qui accumule une histoire propre ; à sauver, ou à déclarer
  éphémère.
- **Natif Unreal** : `USaveGame` + `UGameplayStatics::SaveGameToSlot` pour le conteneur, avec un
  `TArray<uint8>` produit par un sérialiseur de la sim (`FMemoryWriter`, ou le format JSON déjà écrit
  pour le harnais). Ne pas faire de `USaveGame` le modèle de données.
- **Test indispensable** : sauver, recharger, puis vérifier que `StateDigest` est égal **et** que le
  futur est égal sur N jours. C'est exactement le protocole de la brèche.
- **Dépendances** : C1 (sans oracle complet, un champ oublié passe) et C3 (sinon le loquet de l'hôte se
  perd au rechargement).
- **Risque** : moyen à élevé (migrations de format futures). Versionner le format dès le premier jour.
- **Effort** : 3 à 5 jours.

## C5 — Vérité de la file d'intégration · **CONSOLIDER**

- **Défaut** : `agent-worktree.ps1 status` classe les branches par ascendance
  (`git branch --no-merged`, `:428`), alors que le lot verse par contenu (`git cherry`, `:707`).
  Résultat le 2026-10-07 :
  - 55 branches affichées « non intégrées » ;
  - **16 sont déjà entièrement dans `main` par contenu**, 12 partiellement ;
  - certaines ont jusqu'à 165 commits de retard.

  `PRETES_POUR_LE_LOT` en propose plusieurs qui n'apporteraient rien (`NOTHING_TO_INTEGRATE`). Le signal
  donné à l'intégrateur et aux autres agents est faux, et `prune` n'est jamais suggéré pour elles.
- **Preuve** : boucle `git cherry main agent/<m>` sur toutes les branches (diagnostic, §5).
- **Cible** : `status` utilise `git cherry` comme `prune` et `integrate-batch`, classe en
  `versee` / `partielle` / `a_verser`, et propose `prune` pour les versées.
- **Fichiers** : `tools/unreal/agent-worktree.ps1` (`status`), `test-agent-worktree.ps1`.
- **Dépendances** : aucune. **Risque** : faible (affichage). **Effort** : quelques heures.

## C6 — Ménage · **SUPPRIMER / CONSOLIDER**, faible rendement

| Élément | Constat | Verdict |
|---|---|---|
| `Source/Anastasis_UnrealV2/Variant_Horror`, `Variant_Shooter` | ~4 000 lignes du template First Person, compilées dans le jeu, chemins d'inclusion publics (`Anastasis_UnrealV2.Build.cs`), assets sous `Content/Variant_*` | **SUPPRIMER** après vérification par l'Asset Registry / Reference Viewer qu'aucune carte du jeu ne les référence (non vérifié, éditeur requis) |
| `GlobalDefaultGameMode=/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode` | mode de jeu du template par défaut | **APPROFONDIR** : vérifier le mode du niveau `Lvl_AnastasisSlice` (World Settings) |
| `DayLength = 90.0` | écrit trois fois (`AnastasisSimulation.h:31`, `AnastasisVillageRhythm.h:22`, `AnastasisSkyClock.h:34`) sans `static_assert` | **CONSOLIDER** : une seule constante, ou un `static_assert` croisé |
| tâches StateTree `FAnastasis*SmartObjectTask` (`Village/AnastasisVillageStateTree.*`) | aucune référence, ni code, ni test, ni asset | **SUPPRIMER**, ou les brancher sur un mandat explicite (la sim décide déjà seule) |
| `WorldDressing/` (Manager, Placement, Profile, ~550 lignes) | jamais créé, testé seulement | **APPROFONDIR** : demander à son auteur s'il a été remplacé par `AnastasisPlaces` avant de supprimer |
| `FNavService` | prouvé à parité, pas branché ; le branchement attend dans `agent/nav-wiring-001` | **CONSERVER** ; à verser ou à déclarer comme écart, pas à supprimer |
| `static TWeakObjectPtr<AAnastasisWorldAtmosphere>` local à une fonction (`AnastasisSimulationSubsystem.cpp:515`) | partagé entre mondes (PIE multi-clients), gardé par un test de monde | **CONSERVER**, noter |

---

## Forteresses à préserver (ne pas réécrire)

- **`AnastasisSim`, module portable** : dépendances `Core` et `CoreUObject` seulement. Le témoin de la
  brèche le montre déterministe sur deux jours et huit habitants.
- **Le protocole `ECARTS`** : 30 écarts ouverts et tracés, contrôle mécanique (`ECARTS::PASS`). Le
  défaut, c'est son périmètre (C3), pas son principe.
- **Le pas fixe et l'accélération (`TIME_WARP_001`)** : mêmes pas que la référence, plus de pas par frame.
- **`report-tests.ps1` et ses trois catégories** : il refuse les runs tronqués, et il a rendu un verdict
  propre sur l'expérience.
- **La porte mémoire et la file d'un seul éditeur** : la brèche a tourné sans gêner le lot en cours.
