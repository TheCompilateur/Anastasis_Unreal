# Protocole des écarts — absorber vite, stabiliser ensuite

Décidé le 2026-10-01. Registre : `Source/AnastasisSim/ECARTS.md`. Contrôleur :
`tools/migration/check-ecarts.mjs`. Portail : `agent-worktree.ps1 finish`.

## Pourquoi

La stratégie d'Alexandre : **intégrer d'abord la grande masse** du simulateur JS (code qui a survécu
à trois moteurs), le laisser évoluer, **puis lancer la croisade de stabilisation**. Le désordre de la
phase d'absorption est accepté.

Une seule chose peut faire échouer cette stratégie. Pendant la stabilisation, le harnais
(`compare-digests.mjs`) dira « `actors` diverge au tick N ». Il faut alors savoir tout de suite si
c'est :

1. **un bug de portage** : à corriger ;
2. **un écart provisoire** : connu, déjà attribué à une mission ;
3. **une évolution voulue** : la référence JS ne fait plus foi sur ce point.

Si l'écart n'a pas été écrit au moment où il est entré, les trois se ressemblent, et chaque
divergence devient une enquête dans du code dont plus personne ne se souvient. **Un écart déclaré
coûte une ligne le jour où il entre ; un écart oublié coûte une enquête pendant la stabilisation.**

Ce protocole ne ralentit pas l'absorption : il ne demande pas de fermer les écarts, seulement de
les **nommer** au moment où ils entrent.

## La règle

> Tout comportement C++ de `Source/AnastasisSim/` qui n'est pas la copie fidèle de la référence JS
> épinglée a une fiche dans `Source/AnastasisSim/ECARTS.md`, **dans le commit qui l'introduit**.

« Fidèle » veut dire : en parité bit à bit, prouvée par des vecteurs (`gen-parity.mjs`) ou par le
harnais. Tout le reste est un écart, y compris :

- une branche de la référence sautée (« non porté ») : `REDUIT` ;
- un repli inventé à la place d'un but non porté (`observer` au lieu d'`explore`), un flux aléatoire
  propre, un hôte qui décide à la place des habitants : `SUBSTITUT` ;
- un comportement qui n'existe pas dans la référence (démolition, circuit vivrier) : `EXTENSION` ;
- un écart entre la référence épinglée et le dépôt JS vivant : `REFERENCE`.

Ce qui **n'est pas** un écart : le rendu, l'éditeur, les acteurs, tout ce qui vit dans
`Source/Anastasis_UnrealV2/`. Ce module peut évoluer librement ; il lit la simulation sans l'écrire.

## Les trois destins

Un écart a un destin, et c'est ce qui rend la stabilisation mesurable :

| Destin | Sens | Qui le pose |
|---|---|---|
| `A_FERMER` | provisoire : une mission nommée le fermera (`P3_PLAN.md`) | l'agent qui l'introduit |
| `A_TRANCHER` | personne n'a encore décidé si c'est provisoire ou voulu | l'agent, par défaut s'il ne sait pas |
| `ASSUME` | **évolution voulue et définitive** : sur ce point, Unreal devient la référence | **Alexandre seul**, avec une date |

C'est le destin `ASSUME` qui rend l'évolution possible pendant l'absorption : Alexandre peut faire
diverger le jeu de l'ancien simulateur autant qu'il le veut. Il suffit que ce soit **écrit**, pour
que le harnais ne le compte plus comme un bug.

**Fin de la croisade de stabilisation** : `check-ecarts.mjs -bilan` affiche
`STABILISATION::ATTEINTE`, c'est-à-dire plus aucun écart `A_FERMER` ni `A_TRANCHER` ouvert. Tous sont
`FERME` ou `ASSUME`.

## Pendant une mission : ce que fait l'agent

1. **Il porte.** S'il saute une branche, invente un repli ou ajoute un comportement :
   - il **réserve** son numéro par `tools\unreal\agent-worktree.ps1 ecart -Mission <m>` (ECARTS_UNION_001 :
     le plus grand numéro de `main`, des branches `agent/*` et des réservations, plus un), puis ouvre sa fiche
     **à la fin** de `ECARTS.md` sous ce numéro — jamais un numéro réutilisé, jamais un « max + 1 » deviné :
     deux missions parallèles prenaient le même. Pas de fusion par union (ECARTS_REPAIR_001 : elle entrelaçait deux
     fiches aux lignes identiques) ; le lot contrôle tout le registre avant le build ;
   - il pose la marque `ecart n°N` en commentaire à l'endroit du code ;
   - il choisit `A_FERMER` s'il sait quelle mission le fermera, sinon `A_TRANCHER`. **Jamais
     `ASSUME`.**
2. **Il ferme.** S'il porte enfin ce qu'un écart déclarait manquant, il passe la fiche à
   `statut : FERME` avec `ferme_par`, et il retire les marques `ecart n°N` du code. Le contrôleur
   refuse une marque vers une fiche fermée.
3. **Il passe la main.** Sa fiche de passation a une section `## ECARTS` :

   ```markdown
   ## ECARTS

   - ouvert : n° 20 — <titre> (A_FERMER, goals-work-001)
   - modifié : n° 4 — la file de porte est portée, reste l'hésitation
   - fermé : n° 5 — cadence branchée, `Anastasis.Sim.Harnais.Trace` : actors identique jusqu'au tick 120
   ```

   ou, si la mission n'a rien introduit :

   ```markdown
   ## ECARTS

   AUCUN — portage fidèle, prouvé par Anastasis.Sim.Parite.Xxx (N vecteurs, 0 échec).
   ```

4. **Il ne crée aucun nouveau flux aléatoire** (`P3_PLAN.md` §7). Un tirage hors `sim.rng` est un
   écart, et un tirage non déterministe (`FMath::Rand`, `FRandomStream`…) est refusé tout court.

## Les portails

`agent-worktree.ps1 finish` lance, avant le build :

```powershell
node tools\migration\check-ecarts.mjs -base main -handoff docs\unreal\handoffs\<mission>.md
```

| Verdict | Quand |
|---|---|
| `ECARTS::NON_CONCERNE` | la mission ne touche ni le C++ de `Source/AnastasisSim/` (hors `Private/Tests/`), ni le registre |
| `ECARTS::PASS` | registre bien formé, marques du code valides, section `## ECARTS` présente et cohérente |
| `ECARTS::FAIL` | voir ci-dessous ; `finish` refuse `HANDOFF_READY::YES` |

Ce qui fait échouer :

- section `## ECARTS` absente, restée au gabarit, ou `AUCUN` sans justification ;
- une fiche nouvelle, ou dont le statut ou le destin change, que la section ne nomme pas ;
- un numéro cité qui n'a pas de fiche ;
- une marque `ecart n°N` dans le code vers une fiche absente ou `FERME` ;
- un tirage non déterministe ajouté dans `Source/AnastasisSim/` ;
- une ligne ajoutée qui dit « non porté » alors que la section dit `AUCUN` ;
- une fiche mal formée : champ requis manquant, `A_FERMER` sans `fermeture`, `ASSUME` sans décision
  datée d'Alexandre, `EXTENSION` sans `activation`, masque inconnu de `masks.mjs`.

Ce qui avertit seulement (`WARN`) :

- un nouveau `FAnastasisRng` (un flux de plus, à déclarer comme le n° 16) ;
- « non porté » sans marque `ecart n°N` sur la ligne ;
- un écart ouvert sans marque dans le code (six au 2026-10-01 : n° 5, 6, 7, 14, 15, 19) ;
- une fermeture « à attribuer ».

Un défaut du registre déjà présent sur `main` ne bloque pas une mission qui n'y touche pas : il
s'affiche en `WARN (registre)`.

## Pendant la stabilisation : lire un rapport du harnais

Le rapport de `compare-digests.mjs` nomme la première section divergente. Avant de chercher un bug :

```bash
node tools/migration/check-ecarts.mjs -section actors
```

La commande liste les écarts ouverts qui font diverger cette section, avec leur classe, leur destin,
leurs masques et la mission qui les fermera. Exemple réel : le premier rapport (tick 1, `actors`,
`P3_PREMIER_RAPPORT.md`) s'explique par le n° 5, déjà déclaré (« pas de cadence LOD ») mais **sans
marque dans le code**. C'est le cas que ce protocole veut rendre impossible pour les écarts à venir.

- La divergence est expliquée par un écart `A_FERMER` : rien à corriger, c'est sa mission qui la
  fermera.
- Elle est expliquée par un écart `ASSUME` : le scénario doit l'exclure (périmètre ou masque, à la
  création du scénario).
- Elle n'est expliquée par aucun écart : **c'est un bug de portage.** C'est la seule catégorie qui
  demande une enquête.

Le harnais dit **si** une section diverge, pas si l'écart **compte**. Pour le savoir, le laboratoire
de jugement (`docs/migration/ecarts/METHODE.md`, `tools/migration/labo_ecarts/`) réinjecte l'écart dans
la référence JS et mesure son effet sur la vie du village, avec un verdict au bit et un verdict
statistique. Le résultat s'inscrit dans le champ `jugement` de la fiche ; il éclaire le `destin`, il ne
le décide pas.

## Ce que ce protocole ne fait pas

- Il ne prouve pas qu'un écart non déclaré n'existe pas. Le contrôleur repère les aveux (« non
  porté »), les marques et les tirages, pas une formule subtilement différente. Les vecteurs de
  parité et le harnais restent la preuve ; le registre dit seulement ce qu'on sait déjà.
- Il ne couvre pas `Source/Anastasis_UnrealV2/`, qui n'est pas soumis à la parité.
- Il ne range pas l'historique : les n° 1 à 18 gardent leur détail dans `AnastasisVillage.h`, et six
  écarts ouverts n'ont pas encore de marque dans le code. Une mission qui repasse dans ce code pose
  la marque.
