# HANDOFF: ma-cabane-001

## MISSION

« Ma cabane » (mandat d'Alexandre, 2026-10-09, `docs/unreal/MA_CABANE_001.md`) : le joueur se pose dans la vallée, lève
seul une petite cabane d'une pièce, et y vit — il passe la porte, il y dort, elle est à lui (personne d'autre n'y dort).
Réponse d'Alexandre : « Seul, une cabane ».


## FILES_OWNED

- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` (`CabinType`, `IsClosedCabin`, `OpenFamilySiteNear(..., Type)`) ; `Private/Village/AnastasisVillage.cpp` (la cabane connue, une place, fermée aux autres, son toit n'attend personne, donnée à son bâtisseur dès le tracé) ; `Private/Work/AnastasisBuild.cpp` (devis 8 / 2) ; `Private/Village/AnastasisVillageVoice.cpp` (`PlayerBuildHome` ouvre une cabane)
- `Source/AnastasisSim/Private/Tests/AnastasisCabinTests.cpp` (nouveau, `Anastasis.Sim.Cabane.Seul`) ; `Source/AnastasisSim/ECARTS.md` (n° 56)
- `Source/Anastasis_UnrealV2/Village/AnastasisArchitecture.{h,cpp}` (`EVariant::Cabin`, archétype `cabin`) ; `AnastasisArchitectureTests.cpp` ; `AnastasisSettlementLedger.cpp` ; `AnastasisVillagePresentation.cpp` (la cabane se présente comme une maison, son foyer s'allume)
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationPlayer.cpp` (le corps du joueur dans sa cabane ; `get_cabin_status`) ; `AnastasisSimulationSubsystem.h` ; `AnastasisVillageChronicle.cpp` (« cabane »)
- `Content/Anastasis/VillageArchitecture/SM_Arch_Cabin_01.uasset`, `SM_Arch_Cabin_01_Footing.uasset` (nouveaux, générés par `create-village-architecture.ps1 -Only SM_Arch_Cabin_01`)
- `tools/unreal/create-village-architecture.{py,ps1}` (recette `cabin()`, `-Only`), `docs/unreal/architecture/architecture-kit-001.json` (l'entrée de la cabane seule ajoutée)
- `tools/unreal/cabane-pie.py` (nouveau), `tools/unreal/proofs.txt`, `AGENTS.md` (une ligne d'index, la ligne du générateur)
- `tools/unreal/voix-pie.py` : le joueur demande de l'aide quatre jours avant de bâtir (sa cabane, seul, monte en quelques heures : sinon il n'aurait plus rien à demander)
- `docs/unreal/MA_CABANE_001.md`, `docs/unreal/ma-cabane-001/`, cette fiche

## COMMIT

Le dernier commit de la branche `agent/ma-cabane-001` (marqué par `finish`).

## MEC

- BUILD : `anastasis-unreal.ps1 build` -> `BUILD::PASS`.
- TESTS : `report-tests.ps1 -Filter "Anastasis.Sim.Cabane+Anastasis.Village.Architecture+Anastasis.Sim.Voix+Anastasis.Sim.Joueur"` -> PASS 15, FAIL 0 (`Anastasis.Sim.Cabane.Seul` : tracée à lui, 8 bois / 2 pierres, debout posée par lui seul sans attendre d'aidant, son foyer, le voisin sans toit n'y est pas logé, il y entre pour dormir, seul dedans).
- ASSET : `create-village-architecture.ps1 -Only SM_Arch_Cabin_01` -> `ARCH::PASS` (corps 11 364 triangles, assise 12 522) ; hors éditeur `python create-village-architecture.py` -> `ARCH_GEOMETRY PASS`, cabane 683 × 721 × 414 cm, 0 erreur d'échelle.
- PROOF : `editor-batch.ps1 -Proofs cabane-pie` -> `PROOF::PASS cabane-pie (137.9s)` ; `CABANE_PIE PASS built_days=1 pieces=22/22 workers=['npc-14:22'] checks=traced_cabin=1,standing=1,built_alone=1,few_days=1,his_home=1,sleeps_inside=1,body_inside=1,his_alone=1,shots=1`.
- ECARTS : `node tools/migration/check-ecarts.mjs -base main -handoff docs/unreal/handoffs/ma-cabane-001.md` -> `ECARTS::PASS`.
- STATE_FIELDS : `node tools/migration/check-state-fields.mjs -base main` -> `STATE_FIELDS::PASS structures=46` (aucun champ d'état nouveau : `SaveFormatVersion` inchangé).

## PROOFS

PROOFS: cabane-pie, voix-pie, house-rest-pie, npc-life-pie

## SCN

`cabane-pie`, échantillon dans `docs/unreal/ma-cabane-001/` (images réduites + `cabin.json`) :

- le joueur (npc-14) arrive à (38,5 ; 23,5), `Anastasis.Player.Build` trace la cabane en (40, 21), à lui ; il bâtit par sauts d'une heure, s'interrompt pour manger, dormir, boire ; 22 pièces sur 22, toutes de sa main, achevée au jour 2 (tracée au jour 1) ;
- à 22 h il choisit `rest` : il entre dans sa cabane, activité « dort » ; son corps est posé dedans, à (-110, -60) dans le repère de la cabane (emprise -391..432 × -429..432) ; personne d'autre n'y loge ni n'y est entré ;
- `02-cabane` : la cabane de jour, petite, planches et toit à deux pans sur soubassement de pierre, tas de bois contre le mur ; `05-dehors-nuit` : la nuit, la porte éclairée par le foyer, le joueur debout dans la pièce ;
- limites visibles : `01-chantier` montre la cabane écrasée en hauteur (le rendu d'un chantier pour tous les bâtiments, pas un montage pièce à pièce) ; `03-porte` est sombre et trop près ; `04-dedans-nuit` est surexposé (l'œil s'adapte à l'intérieur sombre) et le joueur est debout près du banc, pas couché (aucune animation de sommeil) ; des stries de pluie traversent les vues de jour.

## PLY

NOT_JUDGED — Alexandre regarde les images et joue (`Anastasis.Player.Build`, puis « bâtir », puis « se reposer » la nuit).

## ECARTS

- ouvert : n° 56 — La cabane du joueur (EXTENSION, A_TRANCHER)

## INTEGRATION_RISK

- `Anastasis.Player.Build` trace désormais une cabane (et plus une maison de famille) : `voix-pie` est ajusté en conséquence (il demande avant de bâtir).
- Numéro d'écart 56 réservé par `agent-worktree.ps1 ecart` (le n° 55 était pris aussi par movement-audit-001).

## STOP

- Le bois et la pierre de la cabane sont livrés au tracé : il ne les ramasse pas (cueillir / bûcher : autre mission).
- Une seule forme de cabane ; la variété des maisons n'est pas traitée.
- Le joueur ne peut inviter personne chez lui.
