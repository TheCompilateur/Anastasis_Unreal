# HANDOFF: world-theatre-threat-001

## MISSION

WORLD_THEATRE v2.2, la menace. Une carte de menace lue dans la simulation, montrée au loin par des fumées et des
feux de signaux, sans rien inventer (PONT-HIS-02). Seule source : le monde extérieur simulé
(geopolitical-world-001, `geo-pontos-1204.json`) — insécurité et troupes réelles chez les voisins du village et en
route vers lui, nouvelles en route. Conception : section « v2.2 » de `docs/unreal/world-theatre-001/WORLD_THEATRE_001.md`.
Brief : `docs/historicity/briefs/world-theatre-threat-001.json` (BRIEF::PASS, traçabilité seulement).

Livré :
1. `AnastasisWorldTheatreThreat` (pur) : `ReadThreatMap` (monde extérieur -> carte de menace), `Evaluate` (règles),
   géométrie des colonnes de fumée et des lueurs.
2. Sous-système : un composant par site (14 : deux voisins × 3 fumées + 4 feux), nuit = feu, jour = fumée claire
   (élévation réelle du soleil), lueur rouge au pied des fumées la nuit, effroi ajouté à l'assombrissement v2.1.
   `anastasis.Theatre.Threat` **0 par défaut** ; `.Dread`, `.Glow`, `.FireGain` ; `anastasis.Theatre.Threat.Status` ;
   `UAnastasisWorldTheatreLibrary::GetThreatStatus` (JSON, preuve).
3. Analyse : sites placés par mesure (fumées à source cachée derrière une crête ; chaîne de 4 collines à portée de vue
   du village sur la canopée relevée), plan C++ régénéré (masses inchangées, `ThreatSites`, `ThreatEye`).
4. `M_WorldTheatreSmoke`, `M_WorldTheatreFire` (`/Game/WorldTheatre/`, `world-theatre-threat-material.ps1` + `.py`).
5. Preuve PIE `world-theatre-threat-pie`, au registre.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldTheatre/AnastasisWorldTheatreThreat.{h,cpp}`, `AnastasisWorldTheatreThreatTests.cpp`
- `Content/WorldTheatre/M_WorldTheatreSmoke.uasset`, `Content/WorldTheatre/M_WorldTheatreFire.uasset` (LFS)
- `tools/unreal/world-theatre-threat-material.{ps1,py}`, `tools/unreal/world-theatre-threat-pie.py`
- `docs/historicity/briefs/world-theatre-threat-001.json`
- modifiés, du même théâtre (même auteur) : `AnastasisWorldTheatre.h`, `AnastasisWorldTheatrePlan.inl` (généré),
  `AnastasisWorldTheatreSubsystem.{h,cpp}`, `tools/unreal/world-theatre-analyze.py`, `WORLD_THEATRE_001.md`
- lignes ajoutées : `AGENTS.md` (index, 2 lignes), `tools/unreal/proofs.txt` (1 entrée),
  `docs/historicity/SOURCES.md` (HIS-07, Pattenden 1983)

## COMMIT

PENDING

## MEC

- BUILD: PASS (worktree).
- TESTS: `report-tests.ps1 -Filter Anastasis.WorldTheatre` -> PASS 6 / KNOWN_EXPECTED_FAILURE 0 / FAIL 0, dont
  `ThreatRules` et `ThreatNewsBeforeRaid` (scénario 1204 réel, raid injecté à Paipert : nouvelle en route vers le
  village au jour 2, raid en route au jour 3, ressenti au jour 4). Suite complète : au lot.
- `world-theatre-threat-material.ps1` : `WORLD_THEATRE_THREAT_MATERIAL::PASS` (feu : EyeAdaptationInverse).
- `editor-batch.ps1 -Proofs world-theatre-threat-pie` : `PROOF::PASS (165.9s)`. Chronologie (run 3) :
  calme 0 signe ; jour 3 11 h : 2 feux, 0 fumée ; jour 3 22 h : 4 feux, 0 fumée ; jour 4 : 2 fumées ;
  jour 5 et après : 4 feux tenus, 2-3 fumées, effroi 1,0 ; menace 0 : 0 signe visible à chaque point.

## PROOFS

PROOFS: world-theatre-threat-pie

## SCN

`Saved/WorldTheatreEvidence/threat/` du worktree (runs précédents : `threat-run1`, `threat-run2`). Vu du village vers
Parcharia : la nuit, trois feux nets sur les collines (le quatrième derrière un arbre) ; le jour, deux panaches gris
derrière la crête et les fumées claires des postes ; la nuit, panaches sombres à lueur orangée au pied.
Itérations : run 1, feux invisibles (foyer de 8 m à 10 km, sous le pixel) et fumée de nuit en trapèze noir ; run 2,
lueurs visibles mais fumées de nuit en colonnes blanches ; run 3, lueur au premier sixième, couleur bornée.

## PLY

UNKNOWN : aucun joueur n'a vu ni agi sur l'alerte. La simulation ne fait pas encore réagir les habitants à
l'insécurité (`bDangerNear` toujours faux) : la menace se voit, elle ne se vit pas encore.

## ECARTS

AUCUN — `Source/AnastasisSim/` non touché (lecture seule de `FGeoWorld` ; `FScenario::FindNode`, non exporté, n'est pas utilisé).

## INTEGRATION_RISK

- **Dépendance** : créée depuis `agent/world-theatre-light-001` (versé au lot 8, `main` 3f945847) ; rebasée sur `main`.
- Couche **coupée par défaut** ; et même allumée, inerte sans `Anastasis.Geo.Load` (le monde extérieur n'est pas chargé
  au démarrage aujourd'hui : décision du propriétaire geopolitical-world-001).
- Caps des voisins (Parcharia 108°, Matzouka 40°) : hypothèse de conception dans `world-theatre-analyze.py`
  (`THREAT_ANCHORS`) ; un nouveau voisin dans le scénario n'aura pas de site tant qu'on ne lui donne pas de cap.
- Assets LFS neufs dans `Content/WorldTheatre/`.
- Fichiers partagés : `AGENTS.md`, `proofs.txt`, `docs/historicity/SOURCES.md` (ligne HIS-07 ajoutée après HIS-06).

## STOP

- Aucune menace turque n'est revendiquée pour 1204-1225 ; la preuve injecte un raid **sans acteur**.
- Les feux de signaux sont une **analogie** (IXe siècle, HIS-07).
- Les saisons (transhumance d'été, calendrier des raids de HIS-05) ne sont pas simulées : la couche ne les montre pas.
- Lueur de nuit encore pâle et striée par les volutes ; aucune mesure de coût GPU.
