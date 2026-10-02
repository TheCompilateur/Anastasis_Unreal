# TIME_WARP_001 — accélérer le temps, pour le joueur comme pour les agents

Demande d'Alexandre (2026-10-01) : « un mécanisme permettant d'augmenter la vitesse du temps pour le
joueur comme pour les agents IA ; ça permettra d'augmenter la vitesse de production et de test ».
Puis la conséquence : « un joueur qui utilise trop la vitesse augmentée devient progressivement
invisible pour les PNJ, et perd des points de réputation car il ne fait rien à leurs yeux. Un PNJ qui
observe un joueur avancer le temps depuis une semaine voit juste un type planté là, qui ne fait rien. »

## Ce qui existait

| Réglage | Effet | Limite |
|---|---|---|
| `anastasis.Sim.TimeScale` (0.0375) | secondes simulées par seconde réelle ; un jour ≈ 40 min | bornée à 1 |
| `anastasis.Sim.Speed` (1) | le « 1/2/5/10 » de la référence JS : un *fat step* de N × FixedDt | 10 au plus : au-delà, les habitants se coincent (`AnastasisSimClock::MaxStepMult`) |

Plafond : 10 × le temps réel de la référence (un jour en 9 s), et seulement en abandonnant le rythme
joueur (`TimeScale 1`). Les preuves qui attendaient la nuit restaient bloquées jusqu'à 240 s.

## Ce qui est ajouté

Tout est dans l'hôte Unreal (`Source/Anastasis_UnrealV2/Sim/AnastasisTimeWarp.*`). Le socle
`AnastasisSim` est un port de parité : `PumpFrame` et `StepPlan` ne bougent pas.

### 1. `anastasis.Sim.Warp` — le temps réel accéléré

Multiplie le temps simulé par frame, après `TimeScale` et `Speed`. 0 = pause, 0.25 à 1000.

- Le **pas** reste celui que la référence autorise : `FixedDt × clamp(Speed × Warp, 1, 10)`. Ce qui
  monte, c'est le **nombre de pas par frame**. À ×1000 sous `TimeScale 1`, une centaine par frame.
- Un **budget mur** (`anastasis.Sim.WarpBudgetMs`, 8 ms) borne le coût par frame. Machine saturée :
  le retard est abandonné, pas reporté (sinon chaque frame suivante coupe aussi), et l'overlay
  affiche « machine saturée » à côté de la vitesse obtenue. `0` = illimité, pour un banc sans écran.
- `Warp 1` reprend exactement l'ancien chemin `PumpFrame` : aucune preuve existante ne change.

Joueur, en PIE (liaisons de debug du moteur dans `Config/DefaultInput.ini`, aucun code joueur) :

| Touche | Commande | Effet |
|---|---|---|
| **`8`** (rangée des chiffres ou pavé), ou pavé `+` | `Anastasis.Sim.Faster` | palier suivant : ×0.25 ×0.5 ×1 ×2 ×4 ×8 ×16 ×32 ×64 ×128 |
| **`9`** (rangée des chiffres ou pavé), ou pavé `-` | `Anastasis.Sim.Slower` | palier précédent |
| `Pause` | `Anastasis.Sim.Pause` | pause, puis retour à la vitesse d'avant |

Ces liaisons n'existent pas dans un build Shipping : quand PLAYER sera écrit, la vitesse passera
par son interface.

### 2. `Anastasis.Sim.Advance` — le saut instantané, pour les agents

```
Anastasis.Sim.Advance 45      45 secondes simulées
Anastasis.Sim.Advance 6h      6 heures du jour simulé (un quart de jour)
Anastasis.Sim.Advance 3d      3 jours
Anastasis.Sim.Advance @22     jusqu'à la prochaine 22:00 du jour simulé
Anastasis.Sim.Advance @6:30   jusqu'au prochain 6 h 30
```

La simulation avance dans la frame, en pas de 1/6 s (le ×10 de la référence), le dernier raccourci
pour tomber juste. Un jour = 540 pas. Déterministe : deux simulations qui avancent pareil ont les
mêmes bits de temps (`Anastasis.Sim.TimeWarp.Advance`). Ligne de log :

```
ANASTASIS_SIM advance from=37.8000 to=127.8000 day=1->2 ticks=540 wallMs=… presence=… idleDays=…
```

### 3. Lire l'état

- `Anastasis.Sim.TimeStatus` → `ANASTASIS_SIM time {…}` au log ;
- Python : `unreal.AnastasisSimulationDebugLibrary.get_time_warp_status(world)` →
  `{"time","day","warp","speed","timeScale","rate","budgetCut","presence","idleDays"}`.
  `rate` = secondes simulées par seconde réelle, lissé : la vitesse **obtenue**.

## La conséquence : le témoin

Un joueur qui accélère ne fait rien aux yeux du village. Pour une seconde simulée à l'accélération
`M = max(1, Speed) × Warp`, le village voit le joueur oisif pendant `1 − 1/M` seconde ; `Advance`
est oisif en entier. Ralentir (`Warp < 1`) n'est pas oisif.

`AnastasisTimeWarp::FWitness` tient deux valeurs :

| Valeur | Évolution | Lecture |
|---|---|---|
| `Presence` (1 → 0) | × `exp(−oisif / 2 jours)` ; remonte vers 1 en `exp(−actif / 1 jour)` en jouant à vitesse normale | combien les habitants **voient** encore le joueur |
| `IdleSeconds` | s'accumule, ne s'efface jamais | ce que la **réputation** retiendra : « il ne fait rien » |

Repères (tests `Anastasis.Sim.TimeWarp.Witness`) :

| Le joueur… | Présence | Jours oisifs |
|---|---|---|
| saute une semaine (`Advance 7d`) | 3 % — quasi invisible | 7 |
| puis joue un jour à ×1 | 64 % | 7 (reste) |
| joue deux jours à ×2 | — | 1 |
| ×64 pendant trois jours | moins visible qu'à ×4 sur la même durée | — |

L'overlay PIE affiche `TEMPS ×N  1 jour = …` ; la ligne `JOUEUR` dit la présence, la réputation et
les jours oisifs de l'habitant incarné.

**Branché depuis PLAYER_MINIMAL_001** (`docs/unreal/PLAYER_MINIMAL_001.md`) : le témoin regarde
l'habitant incarné, et lui seul — sans joueur il ne compte rien. Sa présence et ses secondes oisives
sont écrites sur la personne ; les habitants le voient jusqu'à présence × leur portée, et ses jours
oisifs font baisser sa réputation à chaque minuit.

## STOP — ce qui n'est pas fait, et pourquoi

- Le témoin vit dans l'hôte ; la présence et l'oisiveté sont sur la personne, pas dans une sauvegarde.
- Les constantes (2 jours, 1 jour) sont un premier réglage, à rejouer quand un habitant s'en servira.
- Les scripts de preuve existants (`*-pie.py`) ne sont pas convertis à `Advance` : chacun le sera
  dans sa propre mission, preuve refaite.
