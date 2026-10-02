# HANDOFF: aaa-contact-realism-001

## MISSION

Couche de **contact** additive : decalques DBuffer et quelques cailloux qui raccordent au sol les objets deja
poses (arbres, rochers, roseaux, billes, souches) et la ligne d'eau (cinq etats de rive). Ne touche ni le
relief, ni l'hydrologie, ni la distribution des arbres, ni l'atmosphere. Documentation :
`docs/unreal/AAA_CONTACT_REALISM_001.md`. **Verdict : PARTIEL** (effet net a 1-3 m, quasi nul a 80 m).

## FILES_OWNED

Nouveaux (tous) :

- `Source/Anastasis_UnrealV2/WorldView/AnastasisContactRealism.{h,cpp}` (planificateur pur)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisContactRealismSubsystem.{h,cpp}` (UWorldSubsystem)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisContactRealismTests.cpp`
- `Content/Anastasis/AAAContactRealism/` : `M_ACR_Decal` + 11 `MI_ACR_*` (source d'autorite : `tools/unreal/aaa-contact-realism.py`)
- `tools/unreal/aaa-contact-realism.{ps1,py}`, `tools/unreal/contact-realism-capture.{ps1,py}`
- `docs/unreal/AAA_CONTACT_REALISM_001.md`, `docs/visual/aaa-contact-realism-001/`

Fichiers d'autres proprietaires touches (deux seulement) :

- `AGENTS.md` : deux lignes ajoutees a l'index de `tools/unreal/` (exigees par `finish`).
- `Source/Anastasis_UnrealV2/WorldView/AnastasisMicroEcologyTests.cpp` ligne 384 : `FPlan` -> `AnastasisMicroEcology::FPlan`
  (un jeton). Ajouter mes fichiers a `Source/` a redecoupe les groupes unity, et `FPlan` est devenu ambigu avec
  `AnastasisWorldView::FPlan` (erreur C2872). Sans ce jeton le build echoue ; si l'agent de micro-ecologie l'a
  deja corrige a l'identique, le merge est trivial.

## COMMIT

Voir `git log` de la branche `agent/aaa-contact-realism-001`.

## MEC

- BUILD: `BUILD::PASS` (`anastasis-unreal.ps1 build`, worktree, apres la derniere modification de `Source/`).
- TESTS: `Anastasis.ContactRealism.Anchors`, `.ShorePlan`, `.ShoreStates` : voir la ligne `ACR_TESTS::` ci-dessous
  (relancees sur le code final). Suite `Anastasis` complete : **non rejouee ici**, elle attend le lot.
- COMMANDS:
  - `tools\unreal\aaa-contact-realism.ps1 -Tests` -> `ACR_MATERIAL::PASS`, `ACR_TESTS::PASS 3/3`
  - `tools\unreal\contact-realism-capture.ps1 -Label sixth -Materials` -> `CAPTURE::PASS` (12 vues x 3 etats)
  - mesures : `docs/unreal/AAA_CONTACT_REALISM_001.md`, section « Preuve »

## PROOFS

PROOFS: (aucune)

(la capture n'est pas une preuve PIE du registre ; elle est faite dans l'editeur, aux memes cameras, couche 1 / 0 / 1.)

## SCN

Capture `sixth`, graine 12345, ciel epingle (jour 1, 16 h 30), 2 zones x (arbre, rive, 20 m, 80 m, pierre, rocher).
Plan : 23 826 decalques pour tout le monde, 345-640 vivants aux cameras, 6 000 cailloux (plafond atteint), `plan_ms` 210-330.
GPU p50 contact / base : -0,2 a +0,7 ms, dans la derive de la machine (jusqu'a 1,9 ms) ; un seul echantillon par vue.
Planches : `docs/visual/aaa-contact-realism-001/*.png`.

## PLY

Sans objet : aucune preuve en jeu. Le comportement du joueur qui marche (rediffusion des decalques toutes les 15 m)
n'est pas mesure.

## ECARTS

Sans objet : ne touche pas `Source/AnastasisSim`.

## INTEGRATION_RISK

- **Actif par defaut** (`anastasis.Contact.Realism 1`) des que l'incarnation est stable en jeu : change l'image de
  chaque rive et de chaque pied d'arbre. Retour arriere : `anastasis.Contact.Realism 0` (releve au prochain sondage).
- Lit les HISM de `AAnastasisWorldEmbodiment` (noms de maillages `SM_Tree_*`, `SM_Rock_*`, `SM_Ecotone_*`,
  `SM_Shrub_*`) : un renommage de maillage retire silencieusement l'ancre. Met `bReceivesDecals=false` sur ces HISM
  tant que la couche est active (restitue a `Clear()`), ce qui touche aussi les decalques de sentier de
  `human-occupation-001` s'ils visent de la vegetation (ils visent le sol).
- Depend de `AnastasisTerrainForge::SampleActive*`, `AnastasisDrainage::GetActive()` et
  `AnastasisRiverbank::{BuildSpeedField,Calmness}` (API publiques, lues seulement).
- **Materiau** : `M_ACR_Decal` doit etre regenere par `aaa-contact-realism.ps1` si un autre agent change le
  pipeline de decalques ; `-nullrhi` ne compile aucun shader, seul `contact-realism-capture.ps1` detecte un
  materiau casse (il refuse la capture).
- Les tests `Anastasis.ContactRealism.*` sont nouveaux : ils ont ete joues ici (3/3), pas dans la suite complete.
  Selon la memoire projet, une mission qui ajoute des tests peut faire echouer le lot si ses tests n'ont jamais
  tourne dans la suite ; ils ont tourne seuls.
- Modifier `AnastasisMicroEcologyTests.cpp` (un jeton) peut entrer en conflit avec la branche qui le touche.

## STOP

Ne revendique ni un verdict visuel final (PARTIEL), ni le cout GPU fin (un echantillon par vue), ni une preuve en
jeu, ni la suite complete, ni un effet a 80 m. Ne revendique pas non plus l'usage de RVT, PCG ou de decalques de
maillage (non utilises, voir la doc). Ne pousse pas, n'integre pas : s'arrete a `HANDOFF_READY`.
