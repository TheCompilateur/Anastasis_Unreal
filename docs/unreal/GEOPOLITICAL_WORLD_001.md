# GEOPOLITICAL_WORLD_001 — le monde extérieur comme pression qui se propage

Mission `geopolitical-world-001` (2026-10-07, demande d'Alexandre : ANASTASIS_GEOPOLITICAL_WORLD_V1).
Écart n° 38 (`Source/AnastasisSim/ECARTS.md`) : EXTENSION, hôte seulement, déchargée par défaut.

## Ce que c'est

Le village n'est plus un terrarium fermé. Un monde extérieur abstrait (des nœuds, des routes, des
acteurs) reçoit des **chocs** (une crise, une chute de ville, un impôt…). Un choc n'agit jamais
directement sur le village : il émet des **paquets** qui voyagent de nœud en nœud, avec un délai et
une atténuation, jusqu'à ce qu'ils atteignent le nœud du village. Une **nouvelle** du même choc voyage
à part, plus vite, et se déforme. Le village ne lit que son **exposition** (ce qui l'a atteint) et son
**savoir** (ce qu'on lui a dit ou ce qu'il a vécu). Un seul système local réagit en V1 : un groupe de
migrants arrivé au village devient des habitants.

```
CHOC -> PAQUETS -> ROUTES (délai, atténuation) -> NŒUDS -> NŒUD DU VILLAGE -> EXPOSITION -> ARRIVANTS
CHOC -> NOUVELLE (plus rapide, moins fiable, grossie) -> SAVOIR DU VILLAGE
```

Ce n'est ni une carte de provinces ni une IA diplomatique. Aucun « propriétaire » de nœud : un acteur a
une influence à plusieurs dimensions (politique, militaire, commerce, administration) sur des nœuds, et
plusieurs acteurs peuvent peser sur le même.

## Où

| Fichier | Rôle |
|---|---|
| `Source/AnastasisSim/Public/Geo/AnastasisGeo.h`, `Private/Geo/AnastasisGeo.cpp` | module pur : scénario, validation, propagation, information, exposition, trace, sauvegarde |
| `Source/AnastasisSim/Private/Sim/AnastasisSimulation.cpp` | le monde extérieur avance à `OnNewDay`, sur le jour de la simulation ; `AdmitGeoMigration` |
| `Source/AnastasisSim/Private/Village/AnastasisVillageGeo.cpp` | `FVillage::AdmitExternalArrivals` : `spawnNpc` des arrivants |
| `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationGeo.cpp` | commandes `Anastasis.Geo.*`, journal `ANASTASIS_GEO`, `get_geo_status`, `get_geo_trace` |
| `Content/Anastasis/Scenario/geo-pontos-1204.json` | le scénario historique (données, avec provenance) |
| `Source/AnastasisSim/Private/Tests/AnastasisGeoTests.cpp` | `Anastasis.Sim.Geo.*` |
| `tools/unreal/geo-remote-crisis-pie.py` | preuve PIE `geo-remote-crisis-pie` |

Propriétaire du temps : `FAnastasisSimulation` (jour entier). Propriétaire de la vérité géopolitique :
`FAnastasisSimulation::Geo` (C++). L'hôte Unreal ne décide rien.

## Physique (V1)

**Unités.** Toute pression est une intensité normalisée dans [0, 1]. Deux pressions indépendantes se
combinent en « ou bruité » : `P = 1 - (1 - P)(1 - m)`.

**Canaux** (peu nombreux, un sens causal chacun) :

| Canal | Sens pour le village | Branchement local V1 |
|---|---|---|
| `TradeDisruption` | échanges coupés, biens introuvables | exposé ; aucun système d'échange porté en C++ : `BLOCKED_BY_MISSING_LOCAL_OWNER` |
| `Insecurity` | danger sur les chemins et aux champs | exposé ; pas de consommateur local en V1 |
| `Migration` | gens en mouvement vers le village | **branché** : groupe → habitants (`spawnNpc`) |
| `Military` | troupes, réquisitions, passages | exposé |
| `Extraction` | impôt, tribut, corvée | exposé |

**Règles par canal** (données du scénario, jamais en dur) : `retentionPerDay` (part de l'écart à la base
qui survit à un jour), `speedFactor` (multiplie le temps de trajet d'une route), `hopAttenuation` (part
qui franchit un relais), `minMagnitude` (sous ce seuil, un paquet ne repart pas).

**Un jour** (`StepDay`) : 1. décroissance vers la base (une source de choc encore active garde au moins
son intensité) ; 2. les chocs du jour émettent ; 3. les paquets dus arrivent, dans l'ordre (jour, numéro
de création). Arrivée au jour `départ + max(1, ceil(trajet × vitesse du canal))`.

**Transmission sur une route** : `m' = m × transmission[canal] × hopAttenuation`, puis : insécurité
× (1 + `baseRisk`) (une route dangereuse amplifie), migration et militaire plafonnés par `capacity`.
À l'arrivée, l'insécurité est amortie par l'administration : × (1 − `administrationDamping` × portée
administrative max des acteurs sur ce nœud).

**Boucles.** Toute arrivée pèse sur le nœud (deux routes, deux courants). Seule la **première** arrivée
d'une même cause et d'un même canal s'y relaie ; aucun paquet ne repart vers un nœud déjà atteint ;
plafond `maxHops`. La propagation est donc finie, sans tirage, déterministe.

**Information.** Une nouvelle part de la source le jour du choc ; à chaque relais : fiabilité ×
`reliabilityPerHop` × transmission d'information de la route, ampleur rapportée × (1 + `exaggerationPerHop`),
bornée à 1 ; sous `minReliability` elle s'arrête. Au village : une entrée `rumor` (première nouvelle) ou
`corroboration` (la même par une autre route). Une pression vécue au village y laisse aussi une entrée
`experience`, de fiabilité 1.

**Migration.** Une arrivée `Migration` au village d'intensité ≥ `minMagnitude` crée un groupe de
`min(maxPersonsPerBatch, round(m × personsPerUnit))` personnes. À minuit, l'adaptateur local les pose sur
le premier sol libre d'un anneau à 7 cases du camp (angle d'or entre deux groupes) ; ensuite ce sont des
habitants comme les autres. Le groupe garde leurs identifiants et sa cause.

## Données : le scénario

Format : voir `geo-pontos-1204.json` ; chaque nœud, route, acteur et choc porte
`provenance { source, ref, status (VERIFIED | PLAUSIBLE | ABSTRACTION | UNKNOWN), confidence, note }`.
La validation refuse (sans jamais réparer) : identifiants en double, références vers rien, temps de trajet
≤ 0, valeurs hors [0, 1], ancre du village absente, chocs avant le jour 1 ou sans émission, cause mère
inconnue ou déclarée après, canal ou statut inconnu, JSON illisible.

Le scénario livré suit `docs/recherche/histoire-pontique/HP-001_societe-pontique-1000-1461.md` : lieux et
acteurs sourcés (Trébizonde, Matzouka, parcharia, Paipert, Cheriana, Konya, Sinope, Constantinople ; Grands
Comnènes, sultanat de Konya, Empire latin, monastères, seigneurs des marches, Turkmènes), distances et
intensités **abstraites** (`ABSTRACTION` / `PLAUSIBLE`). Le village lui-même est fictif. Les « Turcs » ne
sont pas un bloc : sultanat (État qui veut des paysans), tribus turkmènes (transhumance, `PLAUSIBLE` en
1204), et, à venir, émirs de marche et Grecs au service turc (HP-001 §6).

Choc initial : la prise de Constantinople par les Latins (`VERIFIED`), avec une nouvelle et une faible
migration (`PLAUSIBLE` : HP-001 ne source pas de fuite jusqu'au Pont). Règle PONT-HIS-02 : cette migration
est une hypothèse de conception, chargée seulement à la demande.

## Commandes (PIE)

| Commande | Effet |
|---|---|
| `Anastasis.Geo.Load [chemin]` | charge le scénario (défaut `anastasis.Geo.ScenarioPath`), refuse et journalise toute erreur |
| `Anastasis.Geo.Unload` | ferme le monde extérieur |
| `Anastasis.Geo.Status` | nœuds, routes, acteurs, chocs, paquets et nouvelles en route, exposition |
| `Anastasis.Geo.Node <id>` | pressions, bases, influences d'un nœud |
| `Anastasis.Geo.Exposure` | exposition et savoir du village |
| `Anastasis.Geo.Trace <nœud> <canal>` | pourquoi : arrivées, maillons (routes, jours), cause racine, source |
| `Anastasis.Geo.Inject <nœud> Canal=m … [info=] [duration=] [start=] [actor=] [id=] [label=] [tags=]` | choc de développement (provenance `dev-intervention`, `ABSTRACTION`) |
| `Anastasis.Geo.Route <id> <0\|1>` | ferme / rouvre une route |
| `Anastasis.Geo.Save [nom]` / `Anastasis.Geo.Restore [nom]` | état vivant dans `Saved/GeoState/<nom>.json` |

Journal : une ligne `ANASTASIS_GEO …` par événement (chargement, injection, début de choc, arrivée,
nouvelle reçue, groupe d'arrivants, admission). Lecture Python : `get_geo_status(world)` (JSON),
`get_geo_trace(world, node, channel)`.

## Persistance

`FGeoWorld::SaveState` / `LoadState` : jour, pressions, état des routes, chocs, paquets et nouvelles en
route (avec leur jour d'arrivée), archive des paquets (pour la trace), arrivées, savoir, groupes,
compteurs. Le scénario n'y est pas : il se recharge de sa source (même identifiant exigé). Un paquet dû
avant le jour sauvé est refusé comme corruption. **Limite** : le jeu n'a pas encore de sauvegarde C++
canonique (seul le harnais lit des sauvegardes JS) ; la persistance V1 passe par `Anastasis.Geo.Save`.

## Non-buts V1

Pas de diplomatie, pas de guerre tactique, pas de carte, pas d'IA d'acteur, pas de bandits apparus au
hasard (l'insécurité est une pression, compatible avec une future formation de bandes), pas d'échange
porté. La mémoire des relations village–acteurs (impôts payés, aide reçue…) n'existe pas encore : la
structure (acteurs, causes tracées, groupes) ne la réduit pas à un nombre.
