# HANDOFF: relay-settlement-001

RELAIS: settlement-morphogenesis-001

## MISSION

Relayer `settlement-morphogenesis-001`, commit `23113b54` (« le passage fait le sentier, l'histoire fait la
maison ») sur `main`, qui porte déjà `sim.traffic` / `recordPassage` depuis nav-wiring-001. Le commit
d'architecture de la même branche (`334b7897`) est déjà sur `main` par son contenu (`c83d4099`) : il n'est
pas relayé. Un premier relais avait renoncé pour conflit sémantique ; celui-ci unifie :

- **un seul `RecordPassage`** : celui de main (`AnastasisVillageNav.cpp`), appelé par `MoveActor` (main) et,
  désormais, par `DrivePlayer` (joueur, seulement s'il a bougé, comme `drivePlayerActor`). Celui que la
  mission ajoutait dans `AnastasisVillage.cpp` est retiré ;
- **un seul `FNpc::TrafficTimer`** (celui de main) ;
- **`Traffic` passe de `TArray<int32>` à `TArray<float>`** (Float32Array de la référence) : `decayFootTraffic`
  (×0,88 − 0,55) l'exige. `RecordPassage` écrit `AnastasisTraffic::AddPassage` (min(180, n + 1) en f32) et
  compte `PassageCount` ;
- **`NextWaypoint` / ancien `MoveActor`** de la mission : abandonnés (main les a déplacés dans le service de
  navigation ; le pas de passage y est déjà, fidèle) ;
- **test « bâtiment »** : vérifié contre la référence (`src/sim/simulation.js`). `recordPassage` lit
  `this.buildingAt(tile.x, tile.y)`, c'est-à-dire `buildingIndex[floor(y) * w + floor(x)]`, rempli par
  `addBuilding` à la seule case d'ancrage et jamais vidé (la référence ne démolit pas). La variante de main
  (liste vivante des bâtiments, case d'ancrage) est le port fidèle ; celle de la mission (`Nav.Blocked` hors
  eau) lui est équivalente tant que `Blocked` ne porte que l'eau et les ancrages, mais lit un dérivé. Gardée :
  celle de main, avec `floor(x), floor(y)` (un bâtiment relu d'une sauvegarde JS peut avoir une position non
  entière ; `B.X == TX` le manquait). `UpdateRoadEvolutionDaily` lit la même chose (ensemble des cases
  d'ancrage, construit une fois par nuit).

Contenu porté tel quel : `World/AnastasisTraffic.{h,cpp}` (décroissance, effort de défrichage, classes,
profils), `DecayTrafficDaily` à minuit, travail 8 `roadEvolution`, tests `Anastasis.Sim.Village.Sentiers.*`,
registre de biographie (`AnastasisSettlementLedger`), sentiers rendus (`AnastasisSettlementPaths`), patine,
archétype = programme de la biographie, `Anastasis.Village.SettlementReport`, preuve PIE
`settlement-morphogenesis-pie`, assets `VillageArchitecture` régénérés (paramètre `Weathering`).

État : `StateDigest` lit désormais `traffic`, `roadEvolutionEnabled`, `roads`, `roadEfforts` et
`FNpc::TrafficTimer` (le compteur décide maintenant des sentiers et de leur coût) ; `PassageCount` et
`LastRoadsBuilt` sont classés `observation:` dans `tools/migration/state-fields.json`.

## FILES_OWNED

- `Source/AnastasisSim/Public/World/AnastasisTraffic.h`, `Source/AnastasisSim/Private/World/AnastasisTraffic.cpp`
- `Source/AnastasisSim/Private/Tests/AnastasisTrafficTests.cpp`
- `Source/AnastasisSim/ECARTS.md` (fiche nouvelle en fin de registre)
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h`, `Private/Village/AnastasisVillage.cpp`,
  `Private/Village/AnastasisVillageNav.cpp` (`RecordPassage` en f32), `Private/Village/AnastasisVillagePlayer.cpp`,
  `Private/Village/AnastasisVillageStateDigest.cpp`
- `Source/AnastasisSim/Public/Sim/AnastasisSimulation.h`, `Private/Sim/AnastasisSimulation.cpp`
- `Source/Anastasis_UnrealV2/Village/AnastasisSettlement{Ledger,Paths}.{h,cpp}`, `AnastasisSettlementTests.cpp`,
  `AnastasisVillagePresentation.{h,cpp}`, `AnastasisVillageBuilding.{h,cpp}`, `AnastasisArchitecture.{h,cpp}`,
  `AnastasisArchitectureTests.cpp`
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.{h,cpp}`
- `Content/Anastasis/VillageArchitecture/**` (régénéré par la mission d'origine), `tools/unreal/create-village-architecture.py`
- `tools/unreal/settlement-morphogenesis-pie.{py,ps1}`, `tools/unreal/proofs.txt` (une ligne), `AGENTS.md` (index)
- `tools/migration/state-fields.json`
- `docs/unreal/SETTLEMENT_MORPHOGENESIS_001.md`, `docs/unreal/handoffs/settlement-morphogenesis-001.md` (fiche d'origine, annotée)

## COMMIT

voir `git log agent/relay-settlement-001` (un commit de relais sur `main` 423955c5)

## MEC

- BUILD: PASS (`Build.bat Anastasis_UnrealV2Editor Win64 Development`, worktree, apres rebase sur main 423955c5, 2026-10-08)
- TESTS: `report-tests.ps1 -Filter "Anastasis.Sim+Anastasis.Village.Settlement+Anastasis.Village.Architecture"` (worktree,
  MAIN_LOCK libre) : 173 PASS, 2 KNOWN_EXPECTED_FAILURE (Parite.Fbm, Parite.SemantiqueJs), 0 FAIL, 175 annonces.
  Dont `Sentiers.{Passage,Naissance,Abandon,Renforcement}`, `Recolte.RouteCostDelivery`, `Parite.Nav{Chemin,Cout,Invariants,Service,ServiceFonctions}`, `Village.Navigation`,
  `Village.Settlement.{Programme,Biographie}` : Success. Valeurs : `Passage` 105 passages, 105,0 sur la route, 0 hors
  route ; `Naissance` 24 sentiers, premier au jour 21, 0 hors route, cout 0,860 ; `Abandon` passage 0,000, efforts 0,
  sentiers 0 ; `Renforcement` 23 cases de route, cout 230,0 -> 197,8, trajet parallele 0 case, 230,0 -> 230,0 ;
  `Biographie` refuge 3 dormeurs, 2 nuits pleines, maison du cultivateur 1 dormeur. Suite complete : au lot.
- ECARTS: `node tools/migration/check-ecarts.mjs -base main -handoff docs/unreal/handoffs/relay-settlement-001.md` -> `ECARTS::PASS fiches=39 ouvertes=39 fail=0 warn=9`
- STATE_FIELDS: `node tools/migration/check-state-fields.mjs -base main` -> `STATE_FIELDS::PASS structures=37 lacunes=2 warn=31`
- Doublons index AGENTS.md / proofs.txt : aucun ; marqueurs de conflit : 0.

## PROOFS

Preuves de la fiche d'origine, toutes reprises : `architecture-pie` reste due à CE commit (il remplace le
choix de typologie par graine par le programme de la biographie, régénère les meshes et le matériau
d'architecture avec `Weathering`, et pose un MID permanent sur les corps d'archétype), pas seulement à
`334b7897`.

PROOFS: settlement-morphogenesis-pie, architecture-pie, site-stock-visual-pie

## SCN

Village d'ouverture inchangé. `Anastasis.Village.Hamlet` : les foyers reçoivent des métiers (cultivateur au
grenier, deux bâtisseurs). `Anastasis.Village.SettlementReport`.

## PLY

Le joueur foule aussi (`DrivePlayer`, seulement s'il bouge, comme `drivePlayerActor`).

## ECARTS

- ouvert : n° 42 OUVERT — sentiers de désir posés sans paiement de bois, sans raser une case porteuse d'une
  ressource, sans classes formelles (REDUIT, A_TRANCHER, entrée settlement-morphogenesis-001 où il portait un
  autre numéro, déjà pris sur main). Activation `anastasis.Village.RoadEvolution` (hôte, défaut 1), village
  C++ nu à 0. Marques dans `AnastasisTraffic.{h,cpp}`, `AnastasisVillage.{h,cpp}`, `AnastasisSimulation.h`,
  `AnastasisVillageStateDigest.cpp`, `AnastasisSettlementPaths.h`, `AnastasisSimulationSubsystem.cpp`,
  `AGENTS.md`, la doc et le script PIE.
- Porté fidèlement (sans écart) : `recordPassage` (celui de nav-wiring-001, compteur en f32), `trafficTimer`
  (habitants et joueur), `decayFootTraffic`, effort de défrichage, `roadClassForTraffic`, coût du profil.

## INTEGRATION_RISK

- Branche d'origine `agent/settlement-morphogenesis-001` : NE PAS la verser (elle porte encore `334b7897`
  re-hashé et l'ancien `RecordPassage` en double). Ce relais la remplace ; elle peut être archivée.
- `Traffic` change de type (`int32` → `float`) : toute mission en vol qui lit `FVillage::Traffic` en entier
  devra suivre. Aucun lecteur sur main hors `RecordPassage`.
- `StateDigest` lit maintenant le trafic et les sentiers : deux villages qui ne différaient que par le passage
  ne sont plus égaux pour l'oracle (c'est voulu). La projection JS (`Digest`) n'est pas touchée.
- La décroissance de minuit (fidèle) tourne désormais dans tout village C++, harnais compris : le compteur ne
  sature plus à 180 comme sur main.
- `AnastasisVillageBuilding.cpp` cohabite avec site-stock-visual-001 (déjà sur main) : fusion textuelle propre,
  `site-stock-visual-pie` non rejouée ici.
- Deux systèmes de chemins coexistent (village-fabric-001 et ces sentiers) : non réconciliés (voir la fiche d'origine).

## STOP

- Pas de preuve PIE jouée ici (EDITOR_QUEUE_001) : `settlement-morphogenesis-pie` et `architecture-pie`
  attendent le lot. Le dernier run de la mission d'origine était un `MORPH_PIE FAIL` dû à son propre contrôle
  (corrigé, jamais rejoué).
- Foyer → agrandissement : toujours bloqué (familles, or, marché non portés). Un sentier n'attire pas un trajet
  parallèle (constat épinglé par `Sentiers.Renforcement`).
- Aucune revendication de lisibilité visuelle des sentiers.
