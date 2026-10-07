# PERTURABO_FIRST_BREACH — l'empreinte n'est pas un oracle d'égalité d'état

Mission `iron-crusade-001`, 2026-10-07. Worktree `C:\dev\ANASTASIS_WORKTREES\iron-crusade-001`,
branche `agent/iron-crusade-001` créée depuis `main` = `e681e629` (« Enable finite material courier
for opening NPC site », 2026-10-07 14:07). Le test d'expérience et ces rapports ont été commités en
fin de mission, avec l'accord d'Alexandre, sur la branche remise à niveau sur `main`.

## 1. La faille visée

`FVillage::Digest()` (`Source/AnastasisSim/Private/Village/AnastasisVillage.cpp:2905-3086`) a deux
rôles qui ne sont écrits nulle part comme distincts :

| Rôle | Contrainte | Conséquence |
|---|---|---|
| projection de parité JS | périmètre **figé** pour que les vecteurs de la référence ne bougent pas | chaque nouveau champ est ajouté « hors empreinte », exprès (`docs/unreal/handoffs/player-minimal-001.md:66`) |
| oracle C++ d'égalité / de non-écriture | devrait voir **tout** l'état qui décide du futur | hérite du périmètre figé du premier rôle |

Les tests C++ qui l'emploient comme oracle général (relevé par grep, pas exhaustif sur l'intention) :

| Test | Assertion | Fichier |
|---|---|---|
| `Anastasis.Sim.Village.Repousse.Hote` | « deterministe » | `AnastasisVillageEnduranceTests.cpp:293` |
| `Puits.MultiAgents` (et voisins) | « empreinte deterministe » | `AnastasisVillageSimTests.cpp:545` |
| test des villageois (présentation) | « la presentation n'ecrit pas dans la simulation » | `Anastasis_UnrealV2/Village/AnastasisVillagerTests.cpp:263` |
| joueur, mode observateur | « same digest as a village with no player API touched » | `AnastasisPlayerTests.cpp:103` |
| mortalité | « l'empreinte ne bouge pas » | `AnastasisMortalityTests.cpp:79` |

Relevé statique du périmètre (script Python sur `Public/Village/AnastasisVillage.h`, champs de données
déclarés ; approximation à ±2 près, les structures imbriquées ont été écartées à la main) :

| Structure | Champs | Lus par `Digest` | Hors empreinte (exemples) |
|---|---|---|---|
| `FNpc` | ~109 | 22 | `Speed`, `AiThinkAt`, `Path`, `PathStep`, `StuckTimer`, `MaterialCarry`, `KnownStocks`, `WorkShift`, `AlgoDecision`, `Phenotype`, `Lifestyle`, `Moodlets`, `People`, `Tom`, `Talk*`, `RumorsHeard`, `Reputation`, `Presence`, `IdleSeconds` |
| `FBuilding` | 21 | 13 | `CreatedDay`, `VacantSinceDay`, `BuilderId`, `CompletedDay`, `LaborToday` |
| `FVillage` (privé) | 46 | 5 | `VillageRng` (= `sim.rng`), `WeatherSeed`, `ForcedWeather`, `Settlement`, `MaterialCourierId`, `Colony`, `NextNpcId`, `NextBuildingId`, `DeathLog`, état du joueur |

Lire le code ne suffit pas à conclure : un champ hors empreinte peut être inerte, ou se recopier dans
un champ lu avant que personne ne le remarque. D'où l'expérience.

## 2. L'expérience

Fichier : `Source/AnastasisSim/Private/Tests/AnastasisIronDigestBlindnessTests.cpp`
(test `Anastasis.Iron.Empreinte.AveugleAuxEcritures`, non commité).

1. Deux `FAnastasisSimulation` identiques : graine 12345, carte 96×96, un puits au centre du village,
   8 habitants assoiffés et affamés en couronne.
2. 1 200 pas (20 s simulées) dans les deux.
3. **Une seule** écriture, dans B seulement, sur un champ hors empreinte.
4. Mesure 1 : `A.Digest() == B.Digest()` juste après l'écriture (c'est l'assertion des tests ci-dessus).
5. Mesure 2 : les deux avancent jusqu'à 10 800 pas (2 jours) ; premier pas où les empreintes divergent.
6. Témoin : même protocole sans écriture.

Commandes :

```powershell
cd C:\dev\ANASTASIS_WORKTREES\iron-crusade-001
tools\unreal\anastasis-unreal.ps1 build                        # BUILD::PASS, 194,5 s
tools\unreal\report-tests.ps1 -Filter Anastasis.Iron           # UnrealEditor-Cmd via Start-AnastasisEditor
```

Un seul éditeur (UnrealEditor-Cmd, sans fenêtre), passé par la porte mémoire, refermé par le test.
Log : `Saved/CanonicalVerification/report-tests.log` du worktree.

## 3. Résultats (log du 2026-10-07 20:46:41 UTC)

```
Test Completed. Result={Success} Path={Anastasis.Iron.Empreinte.AveugleAuxEcritures}
IRON_DIGEST temoin (aucune ecriture)               : habitants=8 egale_apres_ecriture=1 premiere_divergence=-1
IRON_DIGEST FNpc::Speed x1,25 sur un habitant      : egale_apres_ecriture=1 premiere_divergence=470 pas (7.83 s simulees)
IRON_DIGEST FNpc::AiThinkAt +3 s sur un habitant   : egale_apres_ecriture=1 premiere_divergence=-1
IRON_DIGEST FNpc::Reputation +0,2 sur un habitant  : egale_apres_ecriture=1 premiere_divergence=-1
IRON_DIGEST etat du flux partage sim.rng ^ 1       : egale_apres_ecriture=1 premiere_divergence=539 pas (8.98 s simulees)
```

`report-tests` : PASS 1, KNOWN_EXPECTED_FAILURE 0, FAIL 0, TOTAL 1, run complet.

| Écriture | Vue par l'empreinte ? | Futur différent ? |
|---|---|---|
| aucune (témoin) | — | **non**, 2 jours |
| `Speed` ×1,25 | **non** | **oui**, après 7,8 s |
| `sim.rng` (1 bit) | **non** | **oui**, après 9,0 s |
| `AiThinkAt` +3 s | **non** | non observé en 2 jours |
| `Reputation` +0,2 | **non** | non — attendu : la réputation ne pèse que si l'autre est le joueur (`AnastasisVillagePlayer.cpp:269`) |

## 4. Ce qui est démontré

- **OBSERVATION** : deux villages d'empreinte égale peuvent avoir des futurs différents. Égalité
  d'empreinte ≠ égalité d'état. Le témoin borne l'effet : sans écriture, rien ne diverge en deux jours.
- **PREUVE** : une présentation, un hôte ou une commande qui consommerait un tirage de `sim.rng`, ou
  toucherait la vitesse d'un habitant, passerait l'assertion « la presentation n'ecrit pas dans la
  simulation » (`AnastasisVillagerTests.cpp:263`) et changerait la société neuf secondes simulées plus
  tard. Même cécité pour « observer mode: same digest » et « l'empreinte ne bouge pas ».
- **Les tests actuels l'auraient-ils détecté ?** Non, par construction : ils comparent avant/après
  sans avancer, et l'expérience montre que l'empreinte est égale juste après l'écriture.

## 5. Ce qui n'est PAS démontré

- **Aucun bug réel trouvé** : je n'ai trouvé aucune présentation qui écrive aujourd'hui dans ces
  champs. La faille porte sur la capacité des tests à le voir, pas sur une écriture constatée.
- Les tests « deterministe » (A et B construits pareil) restent probants pour ce qu'ils couvrent : une
  non-déterminisme dans un champ hors empreinte finit souvent par toucher la position ou les besoins.
  Mais la mesure montre qu'il faut des secondes simulées pour que ça se voie. Un test qui compare au
  même instant, ou un horizon court, peut passer.
- Un seul scénario : 8 habitants autour d'un puits, sans maison ni grenier ni joueur. `AiThinkAt` sans
  effet ici ne veut pas dire sans effet ailleurs.
- La parité JS n'est pas mise en cause : elle se juge sur des sections plus larges
  (`AnastasisHarnessTrace.cpp:336`, `DigestSections`, qui inclut rng, temps et jour).

## 6. Réparation candidate

> **Réalisée** après décision d'Alexandre (« Fait 1 ») dans la mission `state-oracle-001`, qui
> attend le lot :
> - `StateDigest()` ;
> - les assertions d'état complet ;
> - le garde-fou `check-state-fields.mjs`, branché sur `finish`.
>
> Le garde-fou a attrapé un premier cas réel dès le rebase : le champ `Geo`, ajouté par
> `geopolitical-world-001`. Le texte ci-dessous est la proposition d'origine.

Séparer les deux rôles plutôt qu'élargir l'empreinte de parité, qui doit rester figée :

- **A, minimale** : un `FVillage::StateDigest()` C++ qui lit **tout** l'état (y compris `VillageRng`, la
  météo, les compteurs d'identifiants, l'état du joueur), et faire passer les cinq assertions citées sur
  lui. `Digest()` reste la projection JS, intouchée. Le risque est faible : c'est un ajout pur, sans
  changement de comportement, et il ne touche pas la parité.
- **Garde-fou** pour que ça tienne : un contrôle (sur le modèle de `check-ecarts.mjs`) qui refuse un champ
  ajouté à `FNpc` / `FBuilding` / `FVillage` sans déclaration explicite `état` / `dérivé` / `cache`.
  Sinon la dérive actuelle reprend : un champ de plus à chaque mission, 87 champs `FNpc` déjà hors champ.
- Je ne l'ai pas implémentée : cela modifie l'oracle de cinq tests existants, et ces tests appartiennent
  à d'autres missions. Le test d'expérience, lui, se retire en supprimant un seul fichier.
