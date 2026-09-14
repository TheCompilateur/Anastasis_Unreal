# Harnais différentiel — « premier tick divergent »

Les vecteurs de parité répondent à « cette fonction rend-elle les bons bits ». Ils ne
répondent pas à « à quel moment les deux simulations ont-elles cessé d'être la même ».
Sur `simulation.js` — 8 663 lignes, une seule closure, 85 imports — on ne peut pas
fabriquer un vecteur pour « le tick de minuit ». Il faut une empreinte de l'état entier,
calculée des deux côtés, comparable tick par tick.

C'est ce que fait ce harnais : même graine, N ticks, une empreinte par tick de chaque
section de l'état. Le premier tick où les empreintes divergent nomme le système fautif.

## S'en servir

```bash
node tools/migration/emit-state-digests.mjs -seed 33344 -days 3 -out a.jsonl
node tools/migration/emit-state-digests.mjs -seed 33344 -days 3 -out b.jsonl
node tools/migration/compare-digests.mjs a.jsonl b.jsonl
```

Le comparateur sort en 0 si les traces sont identiques, 1 s'il trouve une divergence,
3 s'il refuse de comparer. Rapport réel, sur une divergence d'un seul ulp injectée au
tick 40 :

```
PREMIER TICK DIVERGENT : 40  (jour 1, temps 39.133333333333255)
La divergence persiste jusqu'a la fin de la trace.

Sections divergentes a ce tick (1) :
  actors                 A=6f745fab21c1ebe9  B=649849c1c437a240

Premiere divergence par section — l'ordre est l'ordre de travail :
  tick       40  actors

Sections restees identiques sur toute la trace (34) : animalState, animals, buildings, …

Forage au tick 40 :
  actors — 1 element(s) sur 5
    npc-0              empreinte differente
```

Deux passes, et c'est volontaire : la trace large d'abord, le forage ensuite. Garder
l'empreinte de chaque acteur à chaque tick multiplierait la trace par cent pour une
information dont on n'a besoin qu'une fois, au tick où la divergence apparaît. Quand le
rapport nomme un tick, on relance les deux émetteurs avec `-drill <tick>`.

## L'état vient de `serialize`, pas d'une sélection maison

`src/sim/save.js` fait autorité sur ce qu'est l'état. Ses commentaires racontent les
divergences déjà combattues : le `trafficTimer` qui décide de la formation des routes
donc du terrain, le cache A\* dont l'absence change la trajectoire dès la première
requête, la portion de repas qui reste réservée sur un bâtiment après rechargement.
Quelqu'un a déjà payé le prix de savoir ce qui doit figurer dans un état canonique. Le
harnais n'a pas à le repayer, ni à décider à sa place.

## Le format

`tools/migration/state-digest.mjs` **est** la spécification ;
`Source/AnastasisSim/Public/Core/AnastasisStateDigest.h` la réimplémente. Un flux
d'octets typés, haché en FNV-1a 64. Trois choix qui ne sont pas négociables :

1. **Les nombres par leur motif binaire**, jamais par leur écriture décimale. Même règle
   que les vecteurs de parité : un littéral décimal perd le dernier bit, et c'est ce bit
   qu'on surveille.
2. **Les clés d'objet triées** par unité de code UTF-16. L'ordre d'insertion d'un objet
   JS dépend de l'ordre d'écriture du code, pas de l'état ; le trier rend l'empreinte
   indépendante de la forme du code des deux côtés.
3. **Les clés `undefined` omises** d'un objet — `{a: undefined}` et `{}` décrivent le même
   état. Dans un tableau en revanche, un trou décale les indices : il compte.

NaN est normalisé (V8 comme le C++ peuvent porter plusieurs charges utiles). `-0` garde
ses bits et se distingue de `+0` : la doctrine du projet est qu'un bit qui bouge compte.
Si cela produit un faux positif un jour, ce sera une décision explicite à prendre, pas un
défaut subi.

## Ce qui est prouvé

**L'autotest de la chaîne** — `node tools/migration/selftest-harness.mjs`, 4 cas sur 4 :

| Cas | Attendu | Ce qu'il prouve |
| --- | --- | --- |
| même graine, deux traces | aucune divergence | l'empreinte ne bouge pas toute seule |
| un ulp au tick 0 | tick 0 | une divergence de genèse s'accuse avant le premier pas |
| un ulp sur `actors[0].x` au tick 40 | tick 40 | il voit le plus petit écart possible, au bon tick |
| graines différentes | refus de comparer | il ne rend pas un « tick 0 » qu'on mettrait sur le dos du portage |

**La parité de l'empreinte JS ↔ C++** — `Anastasis.Sim.Empreinte.Parite` et
`Anastasis.Sim.Empreinte.Invariants`, mesurés le 2026-09-13 : **PASS** (suite
`Anastasis.Sim` : 12 PASS, 2 KNOWN_EXPECTED_FAILURE, 0 FAIL).

C'est la pièce qui rend le harnais utilisable : le jour où une trace Unreal et une trace
JS divergeront au tick 812, il faut pouvoir dire « la simulation a dévié » sans avoir à se
demander d'abord si c'est le hacheur qui compte autrement. Les 32 vecteurs sont générés
par `tools/migration/gen-digest-vectors.mjs`, qui déclare chaque cas **une fois** en
instructions et en tire à la fois la valeur JS hachée et la fonction C++ qui la rejoue —
aucune transcription à la main entre les deux.

```bash
node tools/migration/gen-digest-vectors.mjs   # régénère le .inl
```

## Ce qui n'est pas encore là

**Il n'y a pas d'émetteur Unreal.** Le C++ sait calculer l'empreinte d'un état ; il n'a
pas encore d'état à décrire — `serialize` n'a pas de contrepartie tant que la vague 4
(état du monde et sauvegarde) n'est pas portée. Le harnais est donc aujourd'hui complet
d'un côté et prouvé des deux : dès que le C++ aura un état, `FStateWriter` le décrit, la
trace sort au même format, et le comparateur marche sans changer une ligne.

En attendant, il sert déjà à comparer deux états du dépôt JS entre eux — une refonte qui
ne devait rien changer, une sauvegarde rechargée qui doit reprendre à l'identique.

## Une trace = un processus neuf

Deux `new Simulation(graine)` dans le **même** processus divergent dès le tick 0 : les
identifiants du journal de village (`logs[].id`, `vlog-24` puis `vlog-48`) viennent d'un
compteur de module, pas de la simulation. Le harnais l'a trouvé à son premier essai.

Ce n'est pas un bug à corriger dans le dépôt JS, et ce n'est pas non plus un détail : c'est
une contrainte sur la façon de produire les traces. L'usage réel — JS d'un côté, Unreal de
l'autre, deux processus — y satisfait naturellement. L'autotest, lui, a dû être réécrit
pour lancer chaque trace dans son propre processus.

## Les fichiers

| Fichier | Rôle |
| --- | --- |
| `tools/migration/state-digest.mjs` | la spécification du format, et le hacheur du harnais |
| `tools/migration/emit-state-digests.mjs` | trace JSONL d'une exécution JS |
| `tools/migration/compare-digests.mjs` | rapport « premier tick divergent » |
| `tools/migration/selftest-harness.mjs` | autotest de la chaîne entière |
| `tools/migration/gen-digest-vectors.mjs` | vecteurs de parité de l'empreinte, JS → C++ |
| `Source/AnastasisSim/Public/Core/AnastasisStateDigest.h` | l'empreinte côté C++ |
| `Source/AnastasisSim/Private/Tests/AnastasisDigestTests.cpp` | la preuve que les deux côtés hachent pareil |
