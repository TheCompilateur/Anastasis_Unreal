# HANDOFF: relay-wood-001

## MISSION

Relais d'integration de anthropic-wood-001, option (c) decidee par Alexandre : verser sur main la recolte de
bois par les habitants et le retour visuel de la foret utilisee, SANS le second systeme de transport du bois
vers le chantier. La branche d'origine `agent/anthropic-wood-001` n'est pas touchee ; ses commits sont cites
par `cherry-pick -x`.

RELAIS: anthropic-wood-001

Tri des 12 commits de la branche (4cd85b17..96af5622) :

| Commit | Contenu | Sort |
|---|---|---|
| 4cd85b17 | fiche de passation initiale | non repris (remplacee par cette fiche) |
| 87f836be | recolte bornee : metier woodcutter, but gatherWood, session chop, sac conserve, console, tests `Anastasis.Sim.Wood.*` | **verse**, adapte (voir ci-dessous) |
| 295fa2c8 | fiche : build de la recolte | non repris (doc) |
| fbc757b9 | transport du bois porte vers le chantier (`DeliverWoodToSite`, `AnastasisVillageWoodDelivery.cpp`), fiche du numero trente et un, retouche de la fiche du chantier sec, tests `Anastasis.Sim.WoodDelivery.*`, pression de chantier dans le score de recolte, totaux chantier dans WoodStatus | **retire en entier** |
| 69125e55, 0061757c | fiche : ecart et build de la livraison | retires |
| 71f27016 | foret utilisee : `AnastasisForestUse`, liaison des instances d'arbres, CVar `anastasis.Dressing.ForestUse`, tests `Anastasis.ForestUse.*` | **verse** tel quel |
| 2db6c927 | test ForestUse : espace local explicite | **verse** (meme commit que 71f27016) |
| 3bf5b5ad | fiche : build de la foret | non repris (doc) |
| 5d2c2aa5 | renumerotation des ecarts au versement | refaite ici pour le seul ecart n° 30 |
| 6aeaf9e0, 96af5622 | reconstitution des fiches d'ecarts de la pile d'alors, section ECARTS | sans objet (fiche n° 30 posee directement a sa place) |

## FILES_OWNED

- `Source/AnastasisSim/Private/Village/AnastasisVillageWood.cpp` (nouveau ; + `IsWoodHarvester`)
- `Source/AnastasisSim/Public/Work/AnastasisWoodHarvest.h` (nouveau)
- `Source/AnastasisSim/Private/Tests/AnastasisVillageWoodTests.cpp` (nouveau)
- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp`, `Public/Village/AnastasisVillage.h` (crochets gatherWood)
- `Source/AnastasisSim/Private/Work/AnastasisGather.cpp` (priorites et biais du woodcutter)
- `Source/AnastasisSim/ECARTS.md` (fiche n° 30 seulement)
- `Source/Anastasis_UnrealV2/Sim/AnastasisWoodHarvestConsole.cpp` (nouveau, version recolte seule)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisForestUse.{h,cpp}`, `AnastasisForestUseTests.cpp`,
  `AnastasisWorldEmbodimentForestUse.cpp` (nouveaux), `AnastasisWorldEmbodiment.{h,cpp}` (liaison, Tick 1 s)

## COMMIT

Voir `git log main..agent/relay-wood-001` ; marque par `finish`.

## MEC

- BUILD: `Build.bat Anastasis_UnrealV2Editor Win64 Development` sur le worktree -- voir finish (build seul, `queued`)
- TESTS: au lot. Recolte : `Anastasis.Sim.Wood.Bounds`, `Anastasis.Sim.Wood.LocalConservation`,
  `Anastasis.Sim.Wood.ExhaustedStand`. Foret : `Anastasis.ForestUse.LocalMonotonic`,
  `Anastasis.ForestUse.InstancesAndReset`, `Anastasis.ForestUse.RealHarvestReadOnly`. Aucun n'a ete joue
  dans cette mission (pas d'editeur) ; la mission d'origine non plus (`queued`).
  Les trois tests `Anastasis.Sim.WoodDelivery.*` ne sont pas verses.
- COMMANDS:
  - `git cherry-pick -x 87f836be` puis resolution (voir INTEGRATION_RISK)
  - `git cherry-pick -x -n 71f27016 2db6c927`
  - `node tools/migration/check-ecarts.mjs -base main -handoff docs/unreal/handoffs/relay-wood-001.md`

## PROOFS

La mission d'origine n'en declare aucune (`PROOFS: (aucune)` dans sa fiche) ; le relais n'en ajoute pas.

PROOFS: (aucune)

## SCN

Aucune capture. Le retour visuel de la foret (arbres masques a mesure que la tuile perd son bois) n'a pas de
preuve d'image ; seul le test d'automatisation `Anastasis.ForestUse.*` le juge.

## PLY

Sans objet.

## ECARTS

- ouvert : n° 30 -- Recolte de bois bornee, sans logistique ni doctrine forestiere complete (REDUIT,
  A_TRANCHER, anthropic-wood-001 verse par relay-wood-001). Numero reserve a cette mission ; fiche placee
  entre la n° 29 et la n° 32, marques `ecart n°30` dans AnastasisVillage.cpp/.h et AnastasisVillageWood.cpp.
- Aucun autre ecart ouvert, modifie ou ferme. La fiche du chantier sec et la logique du porteur de main ne
  sont pas touchees ; le numero trente et un de la branche (transport) n'est pas verse.

## INTEGRATION_RISK

- **Retire et pourquoi** : tout `fbc757b9` (et ses deux commits de fiche). Main a deja un porteur de
  materiaux opt-in (e681e629, `ProgressMaterialCourier`, `MaterialCarry`) qui prend bois et pierre et credite
  un chantier sec ; `DeliverWoodToSite` aurait ete un second chemin concurrent vers `CreditSiteMaterials`.
  Retires avec lui : le but `deliver` porte par le bois, l'engagement force du trajet charge, le score de
  pression de chantier dans `WoodRowScore`, `DeliveredWood` / `WoodDeliverySite`, le cle `deliveredWood` du
  digest, les totaux chantier de `Anastasis.Village.WoodStatus`.
- **Puits du bois** : il n'y en a pas, et c'est voulu. Le bois coupe reste dans `InventoryWood` ; la recolte
  s'arrete au seuil de sac existant (> 9, nourriture comprise) et le bucheron passe a `observer`. La vue du
  planificateur garde `InventoryWood = 0` (ce bois n'est mobilisable par aucun chemin). Un vrai depot ou un
  rattachement au porteur reste une mission a mandater.
- **Ajout du relais (non present dans la branche)** : `FVillage::IsWoodHarvester` = metier woodcutter ET
  pas `MaterialCourierId`. Sur main, le porteur pose lui-meme `Goal = gatherWood` en route vers le bois ;
  sans cette garde, la cible `WoodTarget`, la fatigue de travail et la session chop de la branche se
  seraient appliquees au porteur (et `WoodTarget` a tout habitant portant ce but). Avec elle, le porteur et
  tout non-bucheron gardent exactement le comportement de main.
- `IsPortedGoalFor` : `gatherWood` devient porte pour un bucheron. Le digest `actors` n'ecrit
  `inventoryWood` / `gatheredWood` que si non nuls : le harnais JS (aucun bucheron) reste inchange.
- `JobTraitBiasGather("woodcutter")` passe de 1.0 a 1.35 dans `AnastasisGather.cpp` (valeur du catalogue
  JS, deja celle du catalogue du planificateur).
- Foret : `AAnastasisWorldEmbodiment` tique desormais (intervalle 1 s) pour appliquer `ForestUse` ; la
  liaison ne couvre que les arbres de `PlaceDressing` (masse, heros/slots, coquilles lointaines). Le
  sous-bois, le contact des troncs et l'herbe, ajoutes sur main depuis, restent visibles sous une couronne
  ouverte. Toute baisse de bois d'une tuile foret ouvre la couronne, y compris celle faite par le porteur
  de main (c'est du vrai bois retire).
- Fichiers chauds : `AnastasisVillage.cpp/.h`, `AnastasisWorldEmbodiment.cpp`, `ECARTS.md`.

## STOP

- Pas d'economie du bois : ni depot, ni livraison au chantier, ni vente, ni regeneration de la foret.
- Aucun test joue dans cette mission ; aucun verdict visuel sur la foret.
- Ne revendique aucune fermeture d'ecart ; ne touche pas au porteur de main.
