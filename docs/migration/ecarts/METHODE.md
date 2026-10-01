# Laboratoire de jugement des écarts — méthode

Mission `labo-ecarts-001`, 2026-10-01. Discipline de claim (`P3_PLAN.md`) : `OBS` observé · `EVD` mesuré ·
`INF` déduit · `DEC` décidé · `UNK` inconnu.

## Dossiers jugés

| Écart | Au bit | Statistique (règle v2) | Nature mesurée | Prédiction | Dossier |
|---|---|---|---|---|---|
| n° 5, cadence | `DIVERGE` tick 1 | `RUPTURE` (ampleur) contre la référence du harnais ; indiscernable du bruit A/A contre la référence vue du village | rupture | dérivant faible : **réfutée** | `n05.md` |
| n° 16, flux propre des rumeurs | `DIVERGE` au premier tirage détourné | `INDETERMINE`, profil de la calibration A/A | chaotique (`INF`) | chaotique : non démontrée formellement, aucun attendu contredit hormis « peut-être dormant » | `n16.md` |

Les deux archétypes se séparent nettement : la méthode distingue un écart qui change la vie du village d'un
écart qui ne fait que décaler les tirages. Elle ne sait pas encore **certifier** le second (`NEUTRE`).

## La question

Le harnais (`P3_PLAN.md` §2) répond à une seule question : la trace C++ est-elle identique **au bit près**
à la trace JS ? Il ne dit pas si un écart **compte**. Un écart qui décale un tirage aléatoire peut rendre
le harnais rouge pour toujours sans rien changer à la vie du village ; un plancher de score peut au
contraire changer la nature du village. Le laboratoire mesure l'effet de chaque écart, à part, **dans la
référence JS** : on réinjecte l'écart dans la référence et on compare à la référence intacte.

Question ouverte posée à Alexandre, **non tranchée ici** : la stabilisation vise-t-elle la parité au bit
(but actuel de `P3_PLAN.md` : 0 divergence) ou l'équivalence statistique ? Le laboratoire rend donc
**deux verdicts** pour chaque écart, côte à côte :

| Verdict | Question | Valeurs |
|---|---|---|
| au bit | la trajectoire avec l'écart est-elle celle de la référence ? | `IDENTIQUE` · `DIVERGE` (premier tick) |
| statistique | le village avec l'écart vit-il comme celui de la référence ? | `NEUTRE` · `DERIVE` (sur quoi, de combien) · `RUPTURE` · `INDETERMINE` · `DORMANT` |

## Les trois niveaux

**1. Effet théorique (`INF`) — écrit AVANT la mesure.** `labo.py cone` lit le code de la référence et
trace le cône causal du code omis ou remplacé : fonctions atteintes, champs écrits, tirages `sim.rng`,
appels qui reçoivent `dt`, lecteurs des champs écrits. Analyse par expressions régulières (limites en tête
de `cone.py`) : elle sert à ne rien oublier d'évident, pas à remplacer la lecture. La prédiction range
l'écart dans une **nature** et liste des **attendus réfutables** (grandeur, sens) ; elle est commitée
avant la première expérience à N répliques (l'historique git en fait foi).

| Nature | Ce qu'elle prédit |
|---|---|
| chaotique | décale l'ordre des tirages : trajectoires décorrélées, statistiques intactes |
| dérivant | biaise une quantité de façon systématique |
| rupture | change un comportement qualitatif |
| dormant | ne s'active pas dans le scénario |

**2. Effet réel dans la référence (`EVD`) — l'expérience contrefactuelle.** Deux bras par réplique, même
état de départ, même suite de tirages au départ :

- `A` : la référence (`-ecart aucun`) ;
- `B` : la référence + l'écart (`-ecart nNN`), réinjecté par `injections.mjs` sans jamais modifier le dépôt
  JS : méthode remplacée sur l'instance `sim`, étape de `tickRecompose` remplacée sur l'objet `ref`,
  enveloppe de `sim.rng` — les techniques de `tools/migration/scenarios/masks.mjs`.

Deux terrains :

| Terrain | État de départ | Réplique k |
|---|---|---|
| `endurance` | le scénario du harnais (`scenarios/endurance.json`, 24 masques) : le contexte où le C++ est jugé | même village ; k = 0 garde l'état `rng` du scénario, k > 0 pose `sim.rng.setState(graineReplique(k))` |
| `genese` | `new Simulation(1000 + k)`, la référence entière, sans masque : le jeu | un monde par graine |

Un troisième bras, le **témoin-bis** (`aucun` + `-reseed 100000+k` : même départ, suite de tirages
indépendante), mesure ce qu'est une trajectoire complètement décorrélée.

Grandeurs (`observables.py`), à trois échelles :

- **micro** : premier tick divergent (empreinte réduite à chaque tick : état `sim.rng` + position, besoins,
  but de chaque habitant ; empreinte complète du harnais tous les 900 ticks), vitesse d'écartement
  (distance moyenne des positions et des besoins entre A et B au fil du temps), **décorrélation** =
  séparation A/B au dernier jour ÷ séparation témoin/témoin-bis (≥ 0,8 : décorrélé) ;
- **comportement** : part de chaque but, part en `observer`, part du temps dedans, durée des épisodes de
  but, changements de but par habitant et par jour ;
- **monde** : survie, faim, soif et autres besoins, santé minimale, part du temps en faim / soif forte
  (≥ 70), stocks bâtis, nourriture, bois, pierre prélevés sur les tuiles, liens, oui-dire, bâtiments, trésor.

Statistique (`stats.py`, bibliothèque standard) : pour chaque grandeur, différences appariées
d_k = B_k − A_k ; IC à 95 % de leur moyenne par bootstrap percentile (4 000 tirages, graine fixe) ; test de
permutation par inversion de signe ; **équivalence (TOST à 5 %)** : l'IC à 90 % tient-il dans
[−δ, +δ] ? Les δ sont **déclarés avant la mesure** dans `tolerances.json` — ce sont des **propositions de
l'agent, non validées par Alexandre**. Verdict d'une grandeur : `EQUIVALENT` (TOST passe), `DIFFERENT`
(IC 95 % exclut 0, TOST échoue), `INDETERMINE` sinon. Verdict de l'écart : `RUPTURE` si un critère
qualitatif se déclenche (survie, but qui apparaît ou disparaît, ampleur > 3 δ), sinon `DERIVE` si une
grandeur est `DIFFERENT`, sinon `INDETERMINE` si une grandeur l'est, sinon `NEUTRE` ; `DORMANT` si
l'injection ne s'active dans aucun passage.

**Règles de décision et calibration A/A.** La règle ci-dessus, déclarée avant la première mesure, est la
**v1**. Une comparaison sans aucun écart (témoin contre témoin-bis, puis quatre jeux de témoins deux à deux)
l'a prise en défaut le jour même : **6 paires A/A sur 6 déclarées `DERIVE`** sur `endurance`, faute de
correction pour ~45 grandeurs testées ensemble (`n05.md`). La **v2** exige pour `DIFFERENT` un p corrigé
par Holm < 0,05 ; sur les mêmes paires : 1/6 `DERIVE`, 5/6 `INDETERMINE`, 0/6 `NEUTRE`. Les deux règles
sont calculées et rapportées ; les verdicts des dossiers sont en v2. Conséquences, à garder en tête :

- chaque expérience juge aussi ses paires A/A (`calibration` dans `experiences/nNN.json`, par défaut
  témoin / témoin-bis) : un verdict ne se lit qu'à côté du bruit A/A du même terrain ;
- à N = 40, D = 3, l'instrument **reconnaît un effet fort mais ne sait pas conclure `NEUTRE`** ; un écart
  chaotique sort `INDETERMINE` avec le même profil que la calibration. Pour atteindre `NEUTRE` : plus de
  répliques, ou des tolérances v2 sur les grandeurs bruitées — **déclarées avant** la mesure suivante.

Les interactions entre écarts viennent dans un second temps (deux injections dans le même bras : le
format le permet, aucune expérience ne le fait encore).

**3. Effet observé dans Unreal (`UNK`, plus tard) — le résidu.** Les mêmes grandeurs, calculées sur une
trace du harnais C++, comparées à la somme des effets connus. Le résidu est ce que personne n'a déclaré.
Ce niveau demande le poste Windows et un émetteur C++ du **relevé** (format ci-dessous) ; il n'existe pas
encore. Ce qui est prêt : `observables.py` lit un relevé sans savoir qui l'a écrit.

## Le format `labo-releve` v1

JSONL. Première ligne, l'en-tête :

```json
{"kind":"labo-releve","format":1,"source":"js|unreal","refCommit":"fee66ae…","terrain":"endurance",
 "replique":0,"graine":12345,"ecart":"aucun","dt":0.016666666666666666,"dayLength":90,"ticks":16200,
 "every":60,"empreinteEvery":900}
```

Puis un échantillon tous les `every` ticks (et au dernier) :

```json
{"t":60,"day":1,"time":38.8,"treasury":420,"tiles":{"food":35564,"wood":40982,"stone":49918},
 "actors":[{"id":"npc-0","job":"farmer","x":48.5,"y":59.5,"goal":"rest","act":"dort","in":1,
            "hu":10.2,"th":10.1,"en":85,"so":80,"le":80,"hy":80,"he":95,"mo":60,"food":5,"gold":0,
            "relN":0,"relSum":0,"relPos":0,"spots":26,"ouiDire":0,"home":"building-2"}],
 "buildings":[{"id":"building-0","type":"granary","progress":1,"food":0}],
 "empreinte":"<digestState du périmètre, quand t % empreinteEvery == 0>"}
```

Puis la ligne de fin : `{"kind":"fin","ticks":…,"activation":{…},"empreintesReduites":["…" × ticks+1],"secondes":…}`.

Pour le niveau 3, l'émetteur C++ (`Harness/AnastasisHarnessTrace`) devra écrire ce relevé avec
`source: "unreal"`, les mêmes champs lus sur `FNpc` / `FBuilding` / le monde, et la même empreinte réduite
(FNV-1a 64 de `state-digest.mjs`, même ordre : état `rng`, puis par habitant id, x, y, six besoins, santé,
moral, `_simBudgetAccum` ou −1, but, dedans). Le résidu d'une grandeur sera alors
`unreal − (référence + Σ effets mesurés des écarts ouverts)`, avec l'hypothèse — à tester — d'additivité.

## Où vit le laboratoire, et pourquoi

`tools/migration/labo_ecarts/` : à côté des outils du harnais qu'il réutilise sans les copier
(`scenarios/masks.mjs`, `scenarios/endurance.json`, `state-digest.mjs`), hors de `tools/unreal/` (il ne
lance pas Unreal ; l'index d'`AGENTS.md` ne le concerne pas). Les dossiers d'expérience et leurs données
résumées sont sous `docs/migration/ecarts/` ; les relevés bruts (~1 Mo par passage) ne sont **pas**
commités : ils se refont (`labo.py lancer`), et les chiffres se rejouent au bit (graines fixes partout).
