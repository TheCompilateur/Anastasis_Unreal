# SETTLEMENT_MORPHOGENESIS_001 — le village devient ce que ses habitants en font

Mission `settlement-morphogenesis-001` (2026-10-07), mandat d'Alexandre : que la forme du village soit
la conséquence physique de la géographie, des foyers, du travail, du passage et du temps — et non
d'un tirage. Empilée sur `architecture-crusade-001` (bâti à l'échelle humaine, catalogue fonctionnel).

> RANDOMNESS CREATES VARIATION. CAUSALITY CREATES IDENTITY.

## 1. Porte 0 — ce qui existait (OBS, relevé dans le code le 2026-10-07)

| Domaine | Propriétaire canonique | État |
|---|---|---|
| Habitants, besoins, décisions | `AnastasisSim` `FVillage` (port fidèle du JS) | porté |
| Foyer / famille / parenté | JS `life/household.js`, `lineage.js` | **non porté** (écarts n°7, 8 ; `goals-family-001` planifié). `FNpc` n'a ni famille ni conjoint |
| Or, marché, loyers, achats | JS `simulation.js` (`resolveDailyRents`, `HOUSE_PRICE`) | **non porté** (« l'or n'existe pas encore ») |
| Maison → habitant | `FNpc::HomeId/ShelterId`, `FBuilding::Owner`, `AssignHome`, `AssignSheltersDaily` | porté (branche sans famille) |
| Agrandissement d'une maison | JS `resolveHouseUpgrades` (foyer plein **et** or + bois + pierre du marché) | **non porté** ; `HousePhase` reste 1 |
| Où bâtir | hôte Unreal : site du village par la géographie (`AnastasisSettlementSite`), maisons en anneaux autour d'un colon atteignable | ad hoc ; le planificateur (`planner-wiring-001`, écart n°27) n'est pas versé |
| Passage → chemins | JS `sim.traffic`, `recordPassage`, `trafficDecay.js`, `updateRoadEvolutionDaily` | **non porté** ; la file de minuit C++ réserve le rang 8 (« routes ») ; le type `Road` existe (« elles naissent du passage, plus tard ») |
| Mémoire du mouvement rendue | `WorldView/AnastasisAnthropicMemory` | présentation seule, expérimentale, défaut 0, rien en temps accéléré, aucune persistance, aucun retour vers la simulation |
| Bâti à l'échelle humaine | `architecture-crusade-001` (`AnastasisArchitecture`, `SM_Arch_*`) | en file d'intégration |
| Histoire d'un bâtiment | — | `CreatedDay`, `CompletedDay`, `VacantSinceDay` seulement ; aucune biographie |
| Persistance | — | aucune `USaveGame` ; `AnastasisJsSave` est un instrument du harnais |

Travaux concurrents qui touchent le même territoire : `village-fabric-001` (calades, terrasses, placette —
tracé de présentation, non causé par le passage), `lived-paths-001` (herbe couchée par la mémoire
anthropique), `planner-wiring-001` (`FBuilding::VacantSinceDay`, où bâtir), `abandon-001` / `iceberg-001`
(vacance, foyer allumé), `crossing-site-001` (passage de rivière d'une route auteur).

## 2. Ce que la mission a établi

```
SIMULATION (AnastasisSim)                    PRÉSENTATION (Anastasis_UnrealV2)
habitant marche ─► recordPassage (0,85 s) ─► sim.traffic (f32, ≤180)
minuit : decayFootTraffic                    │
nuit, rang 8 : effort de défrichage ─────────┤
  ≥ 14 passages, 18 nuits ─► Road (path)     │
  coût de marche 0,86 ─► même trajet ────┐   ├─► FAnastasisSettlementPaths : bande de terre battue
  moins cher, plus court en temps ◄──────┘   │   (1,2-1,7 m) entre cases foulées et jusqu'aux portes
                                             │
FBuilding.Owner, FNpc.JobId, abrités ───────►├─► FLedger (biographie, par observation)
CompletedDay, VacantSinceDay                 │   fondateur, métier, foyer, changements de mains,
                                             │   nuits pleines, vides ─► PROGRAMME (forme fixée)
                                             ├─► AAnastasisVillageBuilding : archétype = programme,
                                             │   patine = âge, abandon = vacance
```

### Système A — le passage fait le chemin (simulation, port de la référence)

| | |
|---|---|
| OWNER | `AnastasisVillage::FVillage` (`Traffic`, `Roads`, `RoadEfforts`), fonctions pures `AnastasisTraffic` |
| ENTRYPOINT | `MoveActor` / `DrivePlayer` (passage) ; `FAnastasisSimulation::OnNewDay` (décroissance) ; travail différé 8 (`UpdateRoadEvolutionDaily`) |
| STATE_READ | position des habitants, type de case vivant, blocage |
| STATE_WRITE | `Traffic`, `RoadEfforts`, `Roads`, `LiveTiles[i].Type = Road`, `Nav.MoveCost[i]`, `NavVersion` |
| CADENCE | passage : 0,85 s de marche ; décroissance : minuit ; sentier : une nuit (travail 8) |
| ACTIVATION | trafic : toujours (fidèle) ; sentier : `anastasis.Village.RoadEvolution` (hôte, défaut 1 ; écart n°42) |
| OBSERVABLE | `Anastasis.Village.SettlementReport`, `GetSettlementStatus` (JSON), `anastasis.Village.Debug 1` (points de passage, carrés de sentier) |
| TEST | `Anastasis.Sim.Village.Sentiers.{Passage, Naissance, Abandon, Renforcement}` |
| FALSIFIER | sentier hors de la route parcourue ; sentier sans passage ; sentier sans activation ; passage sans marche ; trace qui survit à l'abandon ; trajet qui suit l'usage sans passer par le sentier ni coûter moins |
| PROVENANCE | `src/sim/simulation.js` 4775-4880, 7171-7260 ; `src/sim/trafficDecay.js` |

### Système B — la biographie des bâtiments (présentation, par observation)

| | |
|---|---|
| OWNER | `AnastasisSettlement::FLedger`, membre de `FAnastasisVillagePresentation` |
| ENTRYPOINT | `FAnastasisVillagePresentation::Sync` → `Ledger.Observe(Village, Day)` |
| STATE_READ | `FBuilding` (type, propriétaire, achèvement, phase), `FNpc::JobId`, `CountShelterOccupants`, `ShelterCapacity` |
| STATE_WRITE | la biographie seule ; **rien dans la simulation** |
| CADENCE | transitions à chaque synchronisation ; compteurs journaliers une fois par jour simulé |
| RÈGLE | la forme d'une maison se FIXE le jour où un foyer la prend : cultivateur → ferme (grange, aire, cour close) ; bâtisseur → maison à rez maçonné ; foyer ≥ 4 → deux niveaux ; sinon une pièce ; la phase de la référence l'emporte si plus haute. Un nouveau propriétaire **hérite des murs** |
| OBSERVABLE | lignes `ANASTASIS_SETTLEMENT event / bio / reform`, texte de débogage au-dessus de chaque bâtiment |
| TEST | `Anastasis.Village.Settlement.{Programme, Biographie}` |
| FALSIFIER | deux foyers différents → même forme ; forme qui change avec le propriétaire ; forme fixée sans foyer ; nuit pleine comptée deux fois |

### Ce qui a été retiré

Le choix de typologie par **graine d'identifiant** (`architecture-crusade-001`, `ChooseVariant`) : supprimé.
`ChooseVariant` n'est plus que le repli de phase d'un bâtiment sans histoire.

## 3. Statut historique

| | |
|---|---|
| VÉRIFIÉ | rien de nouveau n'est revendiqué comme vérifié par cette mission |
| PLAUSIBLE | sentiers de désir nés de l'usage ; maisons rurales byzantines agrandies et réemployées par plusieurs générations ; la forme d'une exploitation agricole (grange, aire, cour) liée à l'activité du foyer |
| ABSTRACTION | métier du fondateur → typologie ; seuils de la référence JS (14 passages, 18 nuits, coût 0,86) ; patine `0,12 + 0,88·(1 − e^(−jours/60))` |
| INCONNU | densité et largeur réelles des sentiers d'un village pontique post-1204 ; rythme réel de l'usure |

## 4. Non prouvé, et pourquoi

- **Un sentier attire les trajets voisins** : RÉFUTÉ pour le moteur actuel (`Sentiers.Renforcement`).
  L'heuristique de l'A* de la référence compte 10 par case ; sous un sentier (8,6) elle surestime, et l'A*
  rend le premier chemin direct sans explorer le détour. Le sentier renforce l'usage qui le suit (trajet
  moins cher, plus court en temps avec l'écart n°29), il ne capte pas un trajet parallèle, même à une case.
  Une capture réelle demanderait une heuristique admissible (borne au coût minimal 0,52 × 10) : décision
  de simulation commune, hors de cette mission.
- **Taille du foyer → forme** : la règle existe, mais une maison possédée n'abrite que son propriétaire
  (`findOpenShelter` refuse l'intrusion, sans familles portées) : le foyer d'un fondateur vaut toujours 1.
  Seuls les refuges sans maître se remplissent (`Settlement.Biographie`).

- **Foyer → agrandissement** : BLOQUÉ. La référence exige un foyer plein (familles non portées : un
  propriétaire C++ vit seul) **et** de l'or et un marché (non portés). Le registre compte déjà les nuits
  pleines et écrit l'événement « pression d'agrandissement » : le levier attend `goals-family-001` et l'économie.
- **Où bâtir** : inchangé (anneaux autour d'un colon). Le planificateur est une autre mission (`planner-wiring-001`).
- **Strates temporelles par composant** (mur, toit, extension) : la patine est par bâtiment ; un âge par
  composant demande des meshes par composant. Prochain levier visuel.
- **Réemploi des matériaux (spolia)** : non engagé ; `FSiteMaterials` n'a pas de provenance.
- **Persistance** : aucune sauvegarde Unreal n'existe. Trafic et sentiers vivent dans `FVillage` (à
  sauvegarder avec lui le jour venu : `packTraffic` de la référence) ; la biographie n'est pas dérivable
  d'un instantané et devra l'être aussi.
