# HANDOFF: relay-mains-001

RELAIS: mains-joueur-001

## MISSION

Relais de `mains-joueur-001` (son agent a fini ; branche `agent/mains-joueur-001`, tip `b64b4ac94`, laissée intacte), sur
mandat de l'intégrateur (autorité d'Alexandre, 2026-10-10). Au lot sur `main` `6733c3d67`, `player-hands-pie` échouait
5 vérifications sur 17 (« looking at the granary: deposit or take », « ... within reach », « granary: bag and granary
change by the same amount », « looking at the wood: it offers to chop », « holding the click on wood: a log in the bag
0 -> 0 »). Les cinq commits de `mains-joueur-001` sont repris tels quels (`git cherry-pick -x`, sans conflit), puis
corrigés ici. La mission d'origine est décrite dans `docs/unreal/handoffs/mains-joueur-001.md` (reprise par ce relais).

### La cause (reproduite à l'identique dans ce worktree avant correction)

L'hypothèse du lot (field-grain-001 et granary-provisions-001 auraient changé le grenier ou la récolte) **ne tient pas** :
ces deux missions ne touchent que la présentation (épis 3D sur les champs, caisses 3D sans collision près du grenier) ;
la simulation du grenier et des tuiles est inchangée. La preuve n'avait jamais été jouée (fiche d'origine : « écrite pour
le lot, jamais jouée ») et portait deux défauts, plus un défaut réel du code des mains :

1. **Grenier** — `Anastasis.Player.Arrive X Y` fait arriver un **nouvel** habitant, sac vide. La preuve récoltait une
   portion, la mangeait, puis faisait arriver un autre corps au grenier du village d'ouverture, **vide** (`food=0`) : ni
   sac ni grenier n'avaient rien, la main n'offrait rien — à raison. Aucune version de `main` ne remplit ce grenier au
   lancement.
2. **Bois (code des mains, simulation)** — `PlayerHandOffer` proposait « couper du bois » sur une tuile **où se dresse un
   bâtiment** : la maison d'ouverture `building-1` est posée sur la tuile (31,23), qui garde `Wood 8` dans le monde. L'outil
   des cibles la désignait ; le regard, lui, touchait la maison (`focus_building=building-1`). Le bois sous une maison
   n'est atteignable par aucun regard (l'écran abat les arbres de la parcelle).
3. **Visée (hôte et preuve)** — le rayon de visée (30 m) était plus court que la portée de la main (1,6 case ≈ 32 m), et la
   preuve visait 60 cm au-dessus du sol d'une case à 20 m : à hauteur d'œil le rayon ne touchait le sol qu'au-delà de 30 m
   (`focus_tile=[-1,-1]`), constaté au deuxième essai sur la tuile (30,24).

### Ce qui change

- **Simulation** (`AnastasisVillageHands.cpp`, écart n°57) : une tuile où se dresse un bâtiment (achevé ou chantier)
  n'offre aucun geste de tuile ; la main vise le bâtiment. Test `Anastasis.Sim.Joueur.Mains` : une maison posée sur une
  tuile de bois → rien à couper, geste forcé refusé, le bois reste sur la tuile.
- **Hôte** (`AnastasisPlayerHands.cpp`) : le regard porte `max(30 m, (portée + 1,5) cases)` — au-delà de la main, pour que
  « … : approchez-vous » puisse se lire ; `GetPlayerHandTargets` donne aussi, par bâtiment, `available` (portions libres)
  et `access` (seuils).
- **Preuve** (`player-hands-pie.py`), plus stricte, pas affaiblie (17 → 22 contrôles) : le clic tenu se **refait** (deux
  portions), on en mange une, le **même corps** marche jusqu'au seuil du grenier (`Anastasis.Player.Move dx dy` +
  `Anastasis.Sim.Advance 0.2` pas à pas, simulation gelée entre deux), sac porté ; grenier : offre exactement « déposer »,
  dépôt conservé (grenier 0→1, sac 1→0), puis « prendre », reprise conservée (grenier 1→0, sac 0→1), mesures prises juste
  avant et juste après chaque geste ; bois : visée au sol (20 cm, comme le champ), côté le plus dégagé, `in_reach` contrôlé.

## FILES_OWNED

Ceux de `mains-joueur-001` (voir sa fiche), plus, dans ce relais :

- `Source/AnastasisSim/Private/Village/AnastasisVillageHands.cpp` (garde de la tuile bâtie)
- `Source/AnastasisSim/Private/Tests/AnastasisPlayerHandsTests.cpp` (maison sur une tuile de bois)
- `Source/AnastasisSim/ECARTS.md` (fiche n°57 : une phrase)
- `Source/Anastasis_UnrealV2/Sim/AnastasisPlayerHands.cpp` (longueur du regard, `available` / `access`)
- `tools/unreal/player-hands-pie.py`
- `docs/unreal/handoffs/relay-mains-001.md`

## COMMIT

`git log main..agent/relay-mains-001` : les cinq commits de `mains-joueur-001` (`cherry-pick -x`), puis le correctif de
ce relais. Base `main` = `6733c3d67`.

## MEC

- BUILD: `BUILD::PASS` (worktree, après correctif).
- Reproduction avant correctif : `tools\unreal\editor-batch.ps1 -Proofs player-hands-pie` → `PROOF::FAIL`, les mêmes 5
  contrôles sur 17 qu'au lot (`Saved/EditorBatch/20261010-141451/`).
- Après correctif : `PROOF::PASS player-hands-pie (74.9s)`, `PLAYER_HANDS_PIE PASS checks=22 failed=0`
  (`Saved/EditorBatch/20261010-142401/`) : boire 0→1 ; récolter 0→2 au clic tenu ; manger, 1 portion portée ; marche
  `npc-16` 9 pas jusqu'au seuil (32.5, 25.5), sac 1 ; déposer grenier 0→1 sac 1→0 ; prendre grenier 1→0 sac 0→1 ; bois
  tuile (30,24), debout en (29.5, 24.5), offre « couper du bois », à portée, bûche 0→1.
- `tools\unreal\editor-batch.ps1 -Proofs player-pie,player-food-loop-pie` → `PROOF::PASS player-pie (76.9s)`,
  `PROOF::PASS player-food-loop-pie (72.2s)` (`Saved/EditorBatch/20261010-142628/`).
- TESTS : suite sans rendu jouée par `finish` (verdict ci-dessous, à lire dans le journal de passation).
- Format de sauvegarde inchangé (aucun champ d'état nouveau).

## PROOFS

PROOFS: player-hands-pie, player-pie, player-food-loop-pie

## SCN

Celui de `mains-joueur-001`. En plus : devant une maison bâtie sur un bosquet, la main vise la maison, jamais « couper du
bois » sous elle ; un champ, un grenier, un arbre visés à une case se voient (le regard ne s'arrête plus avant la main).

## PLY

NOT_JUDGED — Alexandre joue. Les durées de maintien et la portée restent celles de `mains-joueur-001`.

## ECARTS

- n° 57 — **modifiée** (fiche venue de `mains-joueur-001`, pas encore dans `main`), OUVERT, A_TRANCHER, EXTENSION : une phrase
  ajoutée, « une tuile où se dresse un bâtiment n'offre aucun geste de tuile ». Marques `ecart n°57` inchangées. Aucun
  autre numéro touché ; fiche n°57 toujours à la fin du registre, sans fusion par union.

## INTEGRATION_RISK

- **Relais** : `RELAIS: mains-joueur-001` ; ses trois preuves sont reprises dans `PROOFS:` (3 ≤ 4). Ne pas verser
  `mains-joueur-001` seule : sa preuve échoue (5/17).
- **Fichiers partagés** : ceux de `mains-joueur-001` (`AnastasisSimulationSubsystem.{h,cpp}`, `Anastasis_UnrealV2Character`,
  `Config/DefaultInput.ini` : buts sur Maj + chiffre, `proofs.txt`, `AGENTS.md`, `ECARTS.md`). Cherry-pick sur
  `6733c3d67` sans conflit.
- **`player-hands-pie` dépend du village d'ouverture** (puits, maison, grenier, une tuile de nourriture et une de bois à
  moins de 12 cases du joueur). Un changement du site ou du relief rendu (« le rendu déplace le village ») la fait choisir
  d'autres cases ; elle les cherche, ne les code pas en dur, et la marche au grenier échoue explicitement
  (« walked to the granary » FAIL) si le chemin direct est barré.
- La preuve avance la simulation de ~2 s (`Advance 0.2` × 9) pendant la marche : temps oisif du joueur, sans effet sur
  les contrôles (mesures avant / après prises à simulation gelée).

## STOP

- Ne revendique ni la jouabilité (PLY) ni les durées ; ne touche ni la parole, ni le dormir à la main.
- Ne défait rien de `field-grain-001` ni de `granary-provisions-001` (aucun de leurs fichiers touché).
- Ne verse rien : s'arrête à `HANDOFF_READY::YES`.
