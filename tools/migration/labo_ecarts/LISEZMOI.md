# Laboratoire de jugement des écarts de portage

Méthode, format du relevé, raisons de l'emplacement : `docs/migration/ecarts/METHODE.md`.
Dossiers d'expérience : `docs/migration/ecarts/nNN.md`.

| Fichier | Rôle |
|---|---|
| `labo.py` | point d'entrée : `cone`, `lancer`, `analyser` (Python 3.10+, bibliothèque standard) |
| `cone.py` | cône causal statique d'un écart dans la référence JS (niveau 1, `INF`) |
| `observables.py` | grandeurs micro / comportement / monde tirées d'un relevé `labo-releve` v1 |
| `stats.py` | différences appariées, bootstrap, permutation, TOST |
| `tolerances.json` | δ du test d'équivalence et critères de rupture — **propositions**, déclarées avant la mesure |
| `bras.mjs` | un passage de la référence JS (Node), avec ou sans un écart ; écrit le relevé |
| `injections.mjs` | les réinjections d'écart dans la référence, quelques lignes chacune |
| `experiences/nNN.json` | déclaration d'une expérience : terrains, N, D, bras, comparaisons, cône, prédictions |

Pour juger un nouvel écart : une entrée dans `injections.mjs` (qui imite l'endroit exact du C++ et rend un
compteur d'activation), une déclaration `experiences/nNN.json` avec sa prédiction, le dossier
`docs/migration/ecarts/nNN.md` commité **avant** `labo.py lancer`, puis le renvoi dans la fiche
d'`ECARTS.md`. Le dépôt JS de référence ne se modifie jamais.
